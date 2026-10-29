#include "slsf/core/location/RegionRegistry.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <limits>
#include <map>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

namespace slsf {
namespace {

std::string Describe(const StableRegionKey& key) {
    return key.plugin + ":" + std::to_string(key.localFormId);
}

std::vector<LocationId> ParentIds(const LocationIndexSnapshot& index) {
    std::vector<LocationId> parents(index.records.size(), LocationId{});
    if (index.parents.size() == index.records.size()) {
        for (std::size_t i = 0; i < index.parents.size(); ++i) {
            if (index.parents[i] && index.parents[i].value < index.records.size()) {
                parents[i] = index.parents[i];
            }
        }
        return parents;
    }

    std::map<StableFormKey, LocationId> byFormId;
    for (std::size_t i = 0; i < index.records.size(); ++i) {
        byFormId.emplace(index.records[i].formId, LocationId(static_cast<std::uint32_t>(i)));
    }
    for (std::size_t i = 0; i < index.records.size(); ++i) {
        const auto& key = index.records[i].parentFormId;
        if (key.plugin.empty() && key.localFormId == 0) {
            continue;
        }
        const auto it = byFormId.find(key);
        if (it != byFormId.end()) {
            parents[i] = it->second;
        }
    }
    return parents;
}

bool ParentsFormForest(const LocationIndexSnapshot& index,
                       const std::vector<LocationId>& parents) {
    const std::size_t count = index.records.size();
    if (parents.size() != count) {
        return false;
    }
    std::vector<std::uint8_t> state(count, 0);
    std::vector<LocationId> path;
    path.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        if (state[i] == 2) {
            continue;
        }
        path.clear();
        auto node = LocationId(static_cast<std::uint32_t>(i));
        while (node) {
            if (node.value >= count) {
                return false;
            }
            if (state[node.value] == 2) {
                break;
            }
            if (state[node.value] == 1) {
                return false;
            }
            state[node.value] = 1;
            path.push_back(node);
            node = parents[node.value];
        }
        for (const auto entry : path) {
            state[entry.value] = 2;
        }
    }
    return true;
}

std::vector<LocationId> ParentFirstOrder(const LocationIndexSnapshot& index,
                                         const std::vector<LocationId>& parents) {
    std::vector<LocationId> order;
    order.reserve(index.records.size());
    if (index.entry.size() == index.records.size()) {
        order.resize(index.records.size());
        for (std::size_t i = 0; i < index.records.size(); ++i) {
            order[i] = LocationId(static_cast<std::uint32_t>(i));
        }
        std::sort(order.begin(), order.end(), [&index](LocationId lhs, LocationId rhs) {
            return index.entry[lhs.value] < index.entry[rhs.value];
        });
        return order;
    }

    std::vector<std::vector<LocationId>> children(index.records.size());
    std::vector<LocationId> roots;
    for (std::size_t i = 0; i < index.records.size(); ++i) {
        const auto node = LocationId(static_cast<std::uint32_t>(i));
        if (parents[i]) {
            children[parents[i].value].push_back(node);
        } else {
            roots.push_back(node);
        }
    }
    for (const auto root : roots) {
        std::vector<LocationId> stack{root};
        while (!stack.empty()) {
            const auto node = stack.back();
            stack.pop_back();
            order.push_back(node);
            for (auto it = children[node.value].rbegin(); it != children[node.value].rend(); ++it) {
                stack.push_back(*it);
            }
        }
    }
    return order;
}

}

std::string NormalizeAlias(std::string_view alias) {
    std::string out;
    out.reserve(alias.size());
    bool pendingSpace = false;
    for (const char ch : alias) {
        const auto value = static_cast<unsigned char>(ch);
        if (std::isspace(value) != 0) {
            pendingSpace = !out.empty();
            continue;
        }
        if (pendingSpace) {
            out.push_back(' ');
            pendingSpace = false;
        }
        if (value >= 'A' && value <= 'Z') {
            out.push_back(static_cast<char>(value - 'A' + 'a'));
        } else {
            out.push_back(static_cast<char>(value));
        }
    }
    return out;
}

Result<RegionSnapshot> BindRegions(const LocationIndexSnapshot& index,
                                   const RegionDefinitions& definitions) {
    if (definitions.keys.size() > kApiLocationIndexLimit) {
        return Fail(Error::Code::OutOfRange, "too many registered regions");
    }

    std::vector<StableRegionKey> keys = definitions.keys;
    std::sort(keys.begin(), keys.end());
    std::map<StableRegionKey, RegionId> regionByKey;
    for (std::size_t i = 0; i < keys.size(); ++i) {
        if (keys[i].plugin.empty()) {
            return Fail(Error::Code::InvalidArgument, "region key has no plugin");
        }
        const auto inserted = regionByKey.emplace(keys[i], RegionId(static_cast<std::uint32_t>(i)));
        if (!inserted.second) {
            return Fail(Error::Code::InvalidArgument, "duplicate region key " + Describe(keys[i]));
        }
    }

    std::map<StableRegionKey, bool> ownerSeen;
    std::map<StableRegionKey, StableRegionKey> childOwner;
    for (const auto& rule : definitions.ownerRules) {
        if (!regionByKey.contains(rule.owner)) {
            return Fail(Error::Code::NotFound, "owner rule names an unknown region " +
                                                   Describe(rule.owner));
        }
        if (ownerSeen.contains(rule.owner)) {
            return Fail(Error::Code::InvalidArgument, "duplicate owner rule " + Describe(rule.owner));
        }
        ownerSeen.emplace(rule.owner, true);
        std::map<StableRegionKey, bool> childSeen;
        for (const auto& child : rule.children) {
            if (!regionByKey.contains(child)) {
                return Fail(Error::Code::NotFound, "owner rule names an unknown child " +
                                                       Describe(child));
            }
            if (child == rule.owner) {
                return Fail(Error::Code::InvalidArgument, "region owns itself " + Describe(child));
            }
            if (!childSeen.emplace(child, true).second) {
                return Fail(Error::Code::InvalidArgument, "duplicate child region " +
                                                              Describe(child));
            }
            if (!childOwner.emplace(child, rule.owner).second) {
                return Fail(Error::Code::InvalidArgument, "region has multiple owners " +
                                                              Describe(child));
            }
        }
    }

    std::vector<StableRegionKey> customOrder = definitions.customOrder;
    if (customOrder.empty()) {
        customOrder = keys;
    }
    if (customOrder.size() > kApiLocationIndexLimit) {
        return Fail(Error::Code::OutOfRange, "too many custom regions");
    }

    std::map<StableRegionKey, bool> customSeen;
    std::map<StableRegionKey, ApiLocationIndex> customPosition;
    for (std::size_t i = 0; i < customOrder.size(); ++i) {
        const auto& key = customOrder[i];
        if (!regionByKey.contains(key)) {
            return Fail(Error::Code::NotFound, "custom order names an unknown region " +
                                                   Describe(key));
        }
        if (!customSeen.emplace(key, true).second) {
            return Fail(Error::Code::InvalidArgument, "duplicate custom region " + Describe(key));
        }
        customPosition.emplace(key, ApiLocationIndex(static_cast<std::uint16_t>(i)));
    }

    // A partial customOrder is a priority prefix.  Append the remaining keys so
    // regionApiIndex stays injective without changing alias membership or the
    // priority computed from the supplied order above.
    auto publicOrder = customOrder;
    auto publicPosition = customPosition;
    for (const auto& key : keys) {
        if (!publicPosition.contains(key)) {
            publicPosition.emplace(
                key, ApiLocationIndex(static_cast<std::uint16_t>(publicOrder.size())));
            publicOrder.push_back(key);
        }
    }

    RegionSnapshot snapshot;
    snapshot.regions = keys;
    snapshot.ownership = definitions.ownerRules;
    snapshot.apiAliases = definitions.apiAliases;
    snapshot.customOrder = std::move(publicOrder);
    snapshot.regionApiIndex.assign(keys.size(), ApiLocationIndex{});
    for (std::size_t i = 0; i < keys.size(); ++i) {
        snapshot.regionApiIndex[i] = publicPosition.at(keys[i]);
    }

    std::vector<RegionId> ownerOf(keys.size());
    for (const auto& [child, owner] : childOwner) {
        ownerOf[regionByKey.at(child).value] = regionByKey.at(owner);
    }
    std::vector<RegionId> canonical(keys.size());
    std::vector<std::uint8_t> visiting(keys.size(), 0);
    std::vector<RegionId> path;
    path.reserve(keys.size());
    for (std::size_t i = 0; i < keys.size(); ++i) {
        if (canonical[i]) {
            continue;
        }
        path.clear();
        auto current = RegionId(static_cast<std::uint32_t>(i));
        while (!canonical[current.value]) {
            if (visiting[current.value] != 0) {
                return Fail(Error::Code::InvalidState, "region ownership cycle at " +
                                                           Describe(keys[i]));
            }
            visiting[current.value] = 1;
            path.push_back(current);
            if (!ownerOf[current.value]) {
                canonical[current.value] = current;
                break;
            }
            current = ownerOf[current.value];
        }
        const auto root = canonical[current.value];
        for (const auto node : path) {
            canonical[node.value] = root;
            visiting[node.value] = 0;
        }
    }

    std::vector<std::uint32_t> priority(keys.size(),
                                        std::numeric_limits<std::uint32_t>::max());
    std::uint32_t rank = 0;
    // Priority uses the supplied order only.  The default order above exists
    // for the public ApiLocationIndex and must not outrank owner rules.
    for (const auto& key : definitions.customOrder) {
        priority[regionByKey.at(key).value] = rank++;
    }
    for (const auto& rule : definitions.ownerRules) {
        auto& slot = priority[regionByKey.at(rule.owner).value];
        if (slot == std::numeric_limits<std::uint32_t>::max()) {
            slot = rank++;
        }
    }
    for (const auto& key : definitions.keys) {
        auto& slot = priority[regionByKey.at(key).value];
        if (slot == std::numeric_limits<std::uint32_t>::max()) {
            slot = rank++;
        }
    }

    const auto parents = ParentIds(index);
    if (!ParentsFormForest(index, parents)) {
        return Fail(Error::Code::InvalidState, "location index has a parent cycle");
    }
    const auto order = ParentFirstOrder(index, parents);
    if (order.size() != index.records.size()) {
        return Fail(Error::Code::InvalidState, "location index has a parent cycle");
    }
    snapshot.locationOwners.assign(index.records.size(), RegionId{});
    std::vector<std::uint32_t> bestPriority(index.records.size(),
                                            std::numeric_limits<std::uint32_t>::max());
    for (const auto node : order) {
        auto best = RegionId{};
        auto bestRank = std::numeric_limits<std::uint32_t>::max();

        const auto& stableKey = index.records[node.value].stableKey;
        if (!stableKey.plugin.empty()) {
            const auto region = regionByKey.find(stableKey);
            if (region != regionByKey.end()) {
                const auto owner = canonical[region->second.value];
                const auto ownerRank = std::min(priority[region->second.value],
                                                priority[owner.value]);
                if (ownerRank != std::numeric_limits<std::uint32_t>::max()) {
                    best = owner;
                    bestRank = ownerRank;
                }
            }
        }

        if (parents[node.value]) {
            const auto parent = parents[node.value];
            if (bestPriority[parent.value] < bestRank) {
                best = snapshot.locationOwners[parent.value];
                bestRank = bestPriority[parent.value];
            }
        }

        snapshot.locationOwners[node.value] = best;
        bestPriority[node.value] = bestRank;
    }

    std::map<std::string, bool> aliasSeen;
    snapshot.aliasIndex.reserve(definitions.apiAliases.size());
    for (const auto& alias : definitions.apiAliases) {
        if (!regionByKey.contains(alias.target)) {
            return Fail(Error::Code::NotFound, "alias names an unknown region " +
                                                   Describe(alias.target));
        }
        const auto position = customPosition.find(alias.target);
        if (position == customPosition.end()) {
            return Fail(Error::Code::NotFound, "alias target is not a custom region " +
                                                   Describe(alias.target));
        }
        const auto normalized = NormalizeAlias(alias.alias);
        if (normalized.empty()) {
            return Fail(Error::Code::InvalidArgument, "empty region alias");
        }
        if (!aliasSeen.emplace(normalized, true).second) {
            return Fail(Error::Code::InvalidArgument, "duplicate region alias " + alias.alias);
        }
        snapshot.aliasIndex.push_back(ApiAliasIndexEntry{normalized, position->second});
    }
    std::sort(snapshot.aliasIndex.begin(), snapshot.aliasIndex.end(),
              [](const ApiAliasIndexEntry& lhs, const ApiAliasIndexEntry& rhs) {
                  return lhs.alias < rhs.alias;
              });

    return snapshot;
}

std::optional<RegionId> FindRegion(const RegionSnapshot& snapshot, const StableRegionKey& key) {
    const auto it = std::lower_bound(snapshot.regions.begin(), snapshot.regions.end(), key);
    if (it == snapshot.regions.end() || *it != key) {
        return std::nullopt;
    }
    return RegionId(static_cast<std::uint32_t>(it - snapshot.regions.begin()));
}

std::optional<RegionId> ResolveRegion(const RegionSnapshot& snapshot, LocationId location) {
    if (!location || location.value >= snapshot.locationOwners.size()) {
        return std::nullopt;
    }
    const auto owner = snapshot.locationOwners[location.value];
    if (!owner || owner.value >= snapshot.regions.size()) {
        return std::nullopt;
    }
    return owner;
}

Result<ApiLocationIndex> ResolveLegacyLocation(const RegionSnapshot& snapshot,
                                               std::string_view alias) {
    const auto normalized = NormalizeAlias(alias);
    const auto it = std::lower_bound(
        snapshot.aliasIndex.begin(), snapshot.aliasIndex.end(), normalized,
        [](const ApiAliasIndexEntry& entry, std::string_view value) {
            return entry.alias < value;
        });
    if (it == snapshot.aliasIndex.end() || it->alias != normalized) {
        return Fail(Error::Code::NotFound, "unknown legacy location");
    }
    return it->apiIndex;
}

}
