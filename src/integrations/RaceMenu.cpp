#include "slsf/integrations/RaceMenu.h"

#include <mutex>
#include <utility>

namespace slsf::integrations {

namespace {

static_assert(kSkeeInterfaceExchangeMessage == 0x9E3779B9u);

using QueryInterfaceFn = void* (*)(void* map, const char* name);
using GetVersionFn = std::uint32_t (*)(void* interface);
using GetOverlayCountFn = std::uint32_t (*)(void* interface, std::uint32_t type,
                                            std::uint32_t location);

void* BindTableEntry(void* object, std::size_t slot) noexcept {
    if (object == nullptr) {
        return nullptr;
    }
    return reinterpret_cast<void***>(object)[0][slot];
}

void Record(RaceMenuCapabilityRecord& record, CapabilityState state, std::uint32_t version,
            std::string reason) {
    record.state = state;
    record.version = version;
    record.reason = std::move(reason);
}

}

const char* RaceMenuOverlayLocationName(RaceMenuOverlayLocation value) noexcept {
    switch (value) {
    case RaceMenuOverlayLocation::Body: return "Body";
    case RaceMenuOverlayLocation::Hand: return "Hand";
    case RaceMenuOverlayLocation::Feet: return "Feet";
    case RaceMenuOverlayLocation::Face: return "Face";
    }
    return "Unknown";
}

struct RaceMenu::Impl {
    RaceMenuExchangePort& port_;
    RaceMenuOptions options_;
    mutable std::mutex mutex_;
    std::array<RaceMenuCapabilityRecord, 3> records_{};
    RaceMenuSnapshot snapshot_{};
    ExternalGate gate_{ExternalGate::Waiting};
    Epoch epoch_{};
    bool epochSeen_{};
    bool dirty_{true};
    bool shuttingDown_{false};
    std::uint32_t attempts_{};
    std::string lastError_{};

    explicit Impl(RaceMenuExchangePort& port, RaceMenuOptions options)
        : port_{port}, options_{options} {
        records_.at(0).name = "InterfaceMap";
        records_.at(1).name = "OverlayInterface";
        records_.at(2).name = "GetOverlayCount";
    }

    void RecordFailureLocked(const std::string& reason) {
        attempts_ += 1;
        lastError_ = reason;
        gate_ = attempts_ >= options_.timing.maxAttempts ? ExternalGate::Failed
                                                         : ExternalGate::Waiting;
    }

    RaceMenuEnsureResult EnsureLocked(const Epoch& epoch) {
        if (shuttingDown_) {
            return {ExternalGate::Closed, false};
        }
        if (epoch != epoch_) {
            if (epochSeen_) {
                return {ExternalGate::Stale, false};
            }
            epochSeen_ = true;
            epoch_ = epoch;
        }
        if (!dirty_ && snapshot_.availability == ExternalAvailability::Available) {
            return {gate_, false};
        }
        void* map = port_.Exchange();
        if (map == nullptr) {
            Record(records_.at(0), CapabilityState::Waiting, 0,
                   "skee interface exchange has not delivered a map");
            RecordFailureLocked("RaceMenu interface exchange returned no map");
            return {gate_, false};
        }
        Record(records_.at(0), CapabilityState::Available, 1, {});
        const auto query = reinterpret_cast<QueryInterfaceFn>(BindTableEntry(map, 0));
        if (query == nullptr) {
            RecordFailureLocked("interface map has no query entry at the verified slot");
            return {gate_, false};
        }
        void* overlay = query(map, "Overlay");
        if (overlay == nullptr) {
            Record(records_.at(1), CapabilityState::Missing, 0,
                   "RaceMenu overlay interface is not registered");
            const bool changed = snapshot_.availability != ExternalAvailability::Missing;
            snapshot_.availability = ExternalAvailability::Missing;
            snapshot_.epoch = epoch_;
            gate_ = ExternalGate::Missing;
            dirty_ = false;
            attempts_ = 0;
            lastError_ = "RaceMenu overlay interface is not registered";
            return {gate_, changed};
        }
        const auto getVersion = reinterpret_cast<GetVersionFn>(BindTableEntry(overlay, 1));
        if (getVersion == nullptr) {
            Record(records_.at(1), CapabilityState::Incompatible, 0,
                   "overlay interface has no version entry at the verified slot");
            const bool changed = snapshot_.availability != ExternalAvailability::Incompatible;
            snapshot_.availability = ExternalAvailability::Incompatible;
            snapshot_.epoch = epoch_;
            gate_ = ExternalGate::Incompatible;
            dirty_ = false;
            attempts_ = 0;
            lastError_ = "overlay interface has no version entry";
            return {gate_, changed};
        }
        const std::uint32_t version = getVersion(overlay);
        if (version < options_.requiredOverlayVersion) {
            Record(records_.at(1), CapabilityState::Incompatible, version,
                   "overlay interface version is below the required ABI version");
            const bool changed = snapshot_.availability != ExternalAvailability::Incompatible;
            snapshot_.availability = ExternalAvailability::Incompatible;
            snapshot_.epoch = epoch_;
            gate_ = ExternalGate::Incompatible;
            dirty_ = false;
            attempts_ = 0;
            lastError_ = "overlay interface version is below the required ABI version";
            return {gate_, changed};
        }
        Record(records_.at(1), CapabilityState::Available, version, {});
        const auto getCount =
            reinterpret_cast<GetOverlayCountFn>(BindTableEntry(overlay, 11));
        if (getCount == nullptr) {
            Record(records_.at(2), CapabilityState::Incompatible, version,
                   "overlay count entry is missing at the verified vtable slot");
            const bool changed = snapshot_.availability != ExternalAvailability::Incompatible;
            snapshot_.availability = ExternalAvailability::Incompatible;
            snapshot_.epoch = epoch_;
            gate_ = ExternalGate::Incompatible;
            dirty_ = false;
            attempts_ = 0;
            lastError_ = "overlay count entry is missing at the verified vtable slot";
            return {gate_, changed};
        }
        Record(records_.at(2), CapabilityState::Available, version, {});
        RaceMenuSnapshot next;
        next.availability = ExternalAvailability::Available;
        next.overlayInterfaceVersion = version;
        next.epoch = epoch_;
        for (std::size_t index = 0; index < kRaceMenuOverlayLocationCount; ++index) {
            next.overlays.counts.at(index) =
                getCount(overlay, 0u, static_cast<std::uint32_t>(index));
        }
        bool changed = next.availability != snapshot_.availability ||
                       next.overlayInterfaceVersion != snapshot_.overlayInterfaceVersion ||
                       next.overlays != snapshot_.overlays;
        snapshot_ = next;
        dirty_ = false;
        attempts_ = 0;
        gate_ = ExternalGate::Ready;
        lastError_.clear();
        return {gate_, changed};
    }

    [[nodiscard]] ExternalGate GateLocked() const noexcept {
        if (shuttingDown_) {
            return ExternalGate::Closed;
        }
        if (epochSeen_ && gate_ == ExternalGate::Changed) {
            return ExternalGate::Changed;
        }
        if (snapshot_.availability == ExternalAvailability::Unknown) {
            return gate_ == ExternalGate::Failed ? ExternalGate::Failed
                                                 : ExternalGate::Waiting;
        }
        if (snapshot_.availability == ExternalAvailability::Missing) {
            return ExternalGate::Missing;
        }
        if (snapshot_.availability == ExternalAvailability::Incompatible) {
            return ExternalGate::Incompatible;
        }
        return gate_;
    }
};

RaceMenu::RaceMenu(RaceMenuExchangePort& port, RaceMenuOptions options)
    : impl_{std::make_shared<Impl>(port, std::move(options))} {}

RaceMenu::~RaceMenu() = default;

RaceMenuEnsureResult RaceMenu::Ensure(const Epoch& epoch) {
    const auto state = impl_;
    std::scoped_lock lock{state->mutex_};
    return state->EnsureLocked(epoch);
}

RaceMenuSnapshot RaceMenu::Snapshot() const {
    const auto state = impl_;
    std::scoped_lock lock{state->mutex_};
    return state->snapshot_;
}

ExternalGate RaceMenu::Gate() const {
    const auto state = impl_;
    std::scoped_lock lock{state->mutex_};
    return state->GateLocked();
}

std::uint32_t RaceMenu::Attempts() const {
    const auto state = impl_;
    std::scoped_lock lock{state->mutex_};
    return state->attempts_;
}

bool RaceMenu::Dirty() const {
    const auto state = impl_;
    std::scoped_lock lock{state->mutex_};
    return state->dirty_;
}

void RaceMenu::Capabilities(std::vector<RaceMenuCapabilityRecord>& out) const {
    const auto state = impl_;
    std::scoped_lock lock{state->mutex_};
    out.assign(state->records_.begin(), state->records_.end());
}

std::string RaceMenu::LastError() const {
    const auto state = impl_;
    std::scoped_lock lock{state->mutex_};
    return state->lastError_;
}

void RaceMenu::MarkDirty(const Epoch& eventEpoch) {
    const auto state = impl_;
    std::scoped_lock lock{state->mutex_};
    if (eventEpoch != state->epoch_) {
        return;
    }
    state->dirty_ = true;
    state->attempts_ = 0;
    state->gate_ = ExternalGate::Changed;
}

void RaceMenu::OnSessionChanged(const Epoch& epoch) {
    const auto state = impl_;
    std::scoped_lock lock{state->mutex_};
    state->epoch_ = epoch;
    state->epochSeen_ = true;
    state->snapshot_ = RaceMenuSnapshot{};
    state->snapshot_.epoch = epoch;
    state->dirty_ = true;
    state->attempts_ = 0;
    state->gate_ = ExternalGate::Changed;
    state->lastError_.clear();
}

void RaceMenu::Cancel() {
    const auto state = impl_;
    std::scoped_lock lock{state->mutex_};
    state->shuttingDown_ = true;
    state->gate_ = ExternalGate::Closed;
}

void RaceMenu::Reset() {
    const auto state = impl_;
    std::scoped_lock lock{state->mutex_};
    state->shuttingDown_ = false;
    state->snapshot_ = RaceMenuSnapshot{};
    state->gate_ = ExternalGate::Waiting;
    state->dirty_ = true;
    state->attempts_ = 0;
    state->lastError_.clear();
}

}
