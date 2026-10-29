#include "slsf/integrations/CapabilityRegistry.h"

#include <algorithm>
#include <utility>

namespace slsf::integrations {
namespace {

bool ValidText(std::string_view value) {
    return !value.empty() && value.size() <= 4096;
}

} // namespace

const char* CapabilityStateName(CapabilityState state) noexcept {
    switch (state) {
    case CapabilityState::Unqueried:
        return "Unqueried";
    case CapabilityState::Waiting:
        return "Waiting";
    case CapabilityState::Available:
        return "Available";
    case CapabilityState::Missing:
        return "Missing";
    case CapabilityState::Incompatible:
        return "Incompatible";
    }
    return "Unknown";
}

bool ValidateProbe(const CapabilityRequirement& requirement,
                   const CapabilityProbeResult& probe,
                   std::string& reason) {
    if (!probe.loaded) {
        reason = probe.pending ? "module not loaded yet" : "module not loaded";
        return false;
    }
    if (probe.apiVersion != requirement.requiredApiVersion) {
        reason = "API version mismatch";
        return false;
    }
    if (probe.version < requirement.minimumVersion) {
        reason = "version is older than the required minimum";
        return false;
    }
    if (probe.version > requirement.maximumVersion) {
        reason = "version is newer than the supported maximum";
        return false;
    }
    if (probe.structSize < requirement.minimumStructSize) {
        reason = "published structure is smaller than the validated layout";
        return false;
    }
    if (probe.functions.size() < requirement.requiredFunctionCount) {
        reason = "published function table is incomplete";
        return false;
    }
    for (std::size_t index = 0; index < requirement.requiredFunctionCount; ++index) {
        if (probe.functions[index] == nullptr) {
            reason = "published function table contains a null entry";
            return false;
        }
    }
    reason.clear();
    return true;
}

CapabilityRecord EvaluateCapability(const CapabilityRequirement& requirement,
                                    const CapabilityProbeResult& probe) {
    CapabilityRecord record;
    record.requirement = requirement;
    record.version = probe.version;
    record.apiVersion = probe.apiVersion;
    record.structSize = probe.structSize;
    record.functionCount = probe.functions.size();
    if (probe.pending) {
        record.state = CapabilityState::Waiting;
        record.reason = probe.loaded ? "module API not ready yet" : "module not loaded yet";
    } else if (ValidateProbe(requirement, probe, record.reason)) {
        record.state = CapabilityState::Available;
    } else if (!probe.loaded) {
        record.state = CapabilityState::Missing;
    } else {
        record.state = CapabilityState::Incompatible;
    }
    return record;
}

CapabilityRegistry::CapabilityRegistry(CapabilityQuery query) : query_(std::move(query)) {}

bool CapabilityRegistry::Require(CapabilityRequirement requirement) {
    if (!query_ || !ValidText(requirement.id) || !ValidText(requirement.displayName) ||
        !ValidText(requirement.business) ||
        requirement.minimumVersion > requirement.maximumVersion ||
        requirement.requiredApiVersion == 0) {
        return false;
    }
    std::lock_guard lock(mutex_);
    const auto found = std::find_if(entries_.begin(), entries_.end(), [&](const Entry& entry) {
        return entry.requirement.id == requirement.id;
    });
    if (found != entries_.end()) {
        return found->requirement.displayName == requirement.displayName &&
               found->requirement.business == requirement.business &&
               found->requirement.minimumVersion == requirement.minimumVersion &&
               found->requirement.maximumVersion == requirement.maximumVersion &&
               found->requirement.requiredApiVersion == requirement.requiredApiVersion &&
               found->requirement.minimumStructSize == requirement.minimumStructSize &&
               found->requirement.requiredFunctionCount == requirement.requiredFunctionCount;
    }
    entries_.emplace_back();
    entries_.back().requirement = std::move(requirement);
    entries_.back().record.requirement = entries_.back().requirement;
    return true;
}

CapabilityState CapabilityRegistry::Ensure(std::string_view id) {
    std::unique_lock lock(mutex_);
    const auto found = std::find_if(entries_.begin(), entries_.end(), [&](const Entry& entry) {
        return entry.requirement.id == id;
    });
    if (found == entries_.end()) {
        return CapabilityState::Incompatible;
    }
    Entry& entry = *found;
    if (entry.record.state == CapabilityState::Available ||
        entry.record.state == CapabilityState::Missing ||
        entry.record.state == CapabilityState::Incompatible) {
        return entry.record.state;
    }
    if (entry.querying) {
        const auto caller = std::this_thread::get_id();
        const Entry* dependency = &entry;
        while (dependency->querying) {
            if (dependency->queryThread == caller) {
                return CapabilityState::Incompatible;
            }
            const auto waiting = waitingOn_.find(dependency->queryThread);
            if (waiting == waitingOn_.end()) {
                break;
            }
            dependency = waiting->second;
        }
        waitingOn_.emplace(caller, &entry);
        try {
            entry.completed.wait(lock, [&entry] { return !entry.querying; });
        } catch (...) {
            waitingOn_.erase(caller);
            throw;
        }
        waitingOn_.erase(caller);
        return entry.record.state;
    }

    const CapabilityRequirement requirement = entry.requirement;
    entry.querying = true;
    entry.queryThread = std::this_thread::get_id();
    entry.record.state = CapabilityState::Waiting;
    ++entry.record.queryCount;
    const std::uint32_t queryCount = entry.record.queryCount;
    // Provider code often queries the registry for sibling capabilities.  Do
    // not hold the registry mutex across that call: duplicate callers wait on
    // this entry and then consume the single result.
    lock.unlock();

    CapabilityRecord evaluated;
    try {
        const CapabilityProbeResult probe = query_(requirement);
        evaluated = EvaluateCapability(requirement, probe);
    } catch (...) {
        evaluated = CapabilityRecord{};
        evaluated.requirement = requirement;
        evaluated.state = CapabilityState::Incompatible;
        evaluated.reason = "capability query threw";
    }
    evaluated.queryCount = queryCount;

    lock.lock();
    entry.record = std::move(evaluated);
    entry.querying = false;
    entry.queryThread = {};
    entry.completed.notify_all();
    return entry.record.state;
}

void CapabilityRegistry::EnsureAll() {
    std::vector<std::string> ids;
    {
        std::lock_guard lock(mutex_);
        ids.reserve(entries_.size());
        for (const Entry& entry : entries_) {
            ids.push_back(entry.requirement.id);
        }
    }
    for (const std::string& id : ids) {
        Ensure(id);
    }
}

std::optional<CapabilityRecord> CapabilityRegistry::Find(std::string_view id) const {
    std::lock_guard lock(mutex_);
    const auto found = std::find_if(entries_.begin(), entries_.end(), [&](const Entry& entry) {
        return entry.requirement.id == id;
    });
    if (found == entries_.end()) {
        return std::nullopt;
    }
    return found->record;
}

bool CapabilityRegistry::IsAvailable(std::string_view id) const {
    const auto record = Find(id);
    return record.has_value() && record->state == CapabilityState::Available;
}

std::vector<CapabilityRecord> CapabilityRegistry::Snapshot() const {
    std::lock_guard lock(mutex_);
    std::vector<CapabilityRecord> result;
    result.reserve(entries_.size());
    for (const Entry& entry : entries_) {
        result.push_back(entry.record);
    }
    return result;
}

std::size_t CapabilityRegistry::Size() const {
    std::lock_guard lock(mutex_);
    return entries_.size();
}

} // namespace slsf::integrations
