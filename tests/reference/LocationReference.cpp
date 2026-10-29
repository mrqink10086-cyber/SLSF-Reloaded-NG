#include "LocationReference.h"

#include <algorithm>
#include <limits>
#include <map>

namespace slsf::test {
namespace {

std::optional<LocationId> ParentOf(const std::vector<LocationRecord>& records,
                                   LocationId location) {
    if (!location || location.value >= records.size()) {
        return std::nullopt;
    }
    const auto& parentKey = records[location.value].parentFormId;
    if (parentKey.plugin.empty() && parentKey.localFormId == 0) {
        return std::nullopt;
    }
    return FindLocationReference(records, parentKey);
}

}

std::optional<LocationId> FindLocationReference(const std::vector<LocationRecord>& records,
                                                const StableFormKey& formId) {
    for (std::size_t i = 0; i < records.size(); ++i) {
        if (records[i].formId == formId) {
            return LocationId(static_cast<std::uint32_t>(i));
        }
    }
    return std::nullopt;
}

bool IsAncestorReference(const std::vector<LocationRecord>& records, LocationId ancestor,
                         LocationId location, bool includeSelf) {
    if (!ancestor || !location) {
        return false;
    }
    if (ancestor == location) {
        return includeSelf;
    }
    auto current = ParentOf(records, location);
    std::size_t guard = 0;
    while (current && guard++ <= records.size()) {
        if (*current == ancestor) {
            return true;
        }
        current = ParentOf(records, *current);
    }
    return false;
}

std::optional<RegionId> ResolveRegionReference(const LocationIndexSnapshot& index,
                                              const RegionDefinitions& definitions,
                                              LocationId location) {
    std::map<StableRegionKey, RegionId> ids;
    for (std::size_t i = 0; i < definitions.keys.size(); ++i) {
        ids.emplace(definitions.keys[i], RegionId(static_cast<std::uint32_t>(i)));
    }
    std::map<StableRegionKey, std::uint32_t> priority;
    std::uint32_t rank = 0;
    for (const auto& key : definitions.customOrder) {
        priority.emplace(key, rank++);
    }
    for (const auto& rule : definitions.ownerRules) {
        priority.emplace(rule.owner, rank++);
    }
    for (const auto& key : definitions.keys) {
        priority.emplace(key, rank++);
    }

    std::map<StableRegionKey, StableRegionKey> childOwner;
    for (const auto& rule : definitions.ownerRules) {
        for (const auto& child : rule.children) {
            childOwner.emplace(child, rule.owner);
        }
    }
    std::map<StableRegionKey, StableRegionKey> canonical;
    for (const auto& key : definitions.keys) {
        auto current = key;
        std::size_t guard = 0;
        while (true) {
            const auto owner = childOwner.find(current);
            if (owner == childOwner.end()) {
                break;
            }
            current = owner->second;
            if (++guard > definitions.keys.size()) {
                break;
            }
        }
        canonical.emplace(key, current);
    }

    if (!location || location.value >= index.records.size()) {
        return std::nullopt;
    }
    auto current = std::optional<LocationId>(location);
    std::size_t guard = 0;
    std::optional<RegionId> best;
    std::uint32_t bestPriority = std::numeric_limits<std::uint32_t>::max();
    while (current && guard++ <= index.records.size()) {
        const auto it = ids.find(index.records[current->value].stableKey);
        if (it != ids.end()) {
            const auto& stableKey = index.records[current->value].stableKey;
            const auto canonicalKey = canonical.at(stableKey);
            const auto rankIt = priority.find(stableKey);
            const auto ownerRank = priority.find(canonicalKey);
            if (rankIt != priority.end() && ownerRank != priority.end() &&
                std::min(rankIt->second, ownerRank->second) < bestPriority) {
                best = ids.at(canonicalKey);
                bestPriority = std::min(rankIt->second, ownerRank->second);
            }
        }
        current = ParentOf(index.records, *current);
    }
    return best;
}

}
