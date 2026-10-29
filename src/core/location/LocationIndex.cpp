#include "slsf/core/location/LocationIndex.h"

#include <algorithm>
#include <limits>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace slsf {
namespace {

bool IsRootParent(const StableFormKey& key) {
    return key.plugin.empty() && key.localFormId == 0;
}

std::string Describe(const StableFormKey& key) {
    return key.plugin + ":" + std::to_string(key.localFormId);
}

}

Result<LocationIndexSnapshot> BuildLocationIndex(std::span<const LocationRecord> records) {
    if (records.size() > (std::numeric_limits<std::uint32_t>::max() / 2u) - 1u) {
        return Fail(Error::Code::OutOfRange, "too many locations");
    }

    std::vector<LocationRecord> ordered(records.begin(), records.end());
    std::map<StableFormKey, LocationId> byFormId;
    for (std::size_t i = 0; i < ordered.size(); ++i) {
        if (ordered[i].formId.plugin.empty()) {
            return Fail(Error::Code::InvalidArgument, "location form id has no plugin");
        }
        const auto inserted = byFormId.emplace(ordered[i].formId, LocationId(static_cast<std::uint32_t>(i)));
        if (!inserted.second) {
            return Fail(Error::Code::InvalidArgument,
                        "duplicate location form id " + Describe(ordered[i].formId));
        }
    }

    std::sort(ordered.begin(), ordered.end(), [](const LocationRecord& lhs, const LocationRecord& rhs) {
        return lhs.formId < rhs.formId;
    });

    byFormId.clear();
    for (std::size_t i = 0; i < ordered.size(); ++i) {
        byFormId.emplace(ordered[i].formId, LocationId(static_cast<std::uint32_t>(i)));
    }

    LocationIndexSnapshot snapshot;
    snapshot.records = ordered;
    snapshot.parents.assign(ordered.size(), LocationId{});
    std::vector<std::vector<LocationId>> children(ordered.size());
    std::vector<LocationId> roots;

    for (std::size_t i = 0; i < ordered.size(); ++i) {
        const auto& parentKey = ordered[i].parentFormId;
        if (IsRootParent(parentKey)) {
            roots.push_back(LocationId(static_cast<std::uint32_t>(i)));
            continue;
        }
        if (parentKey.plugin.empty()) {
            return Fail(Error::Code::InvalidArgument,
                        "location " + Describe(ordered[i].formId) + " has an invalid parent");
        }
        const auto parent = byFormId.find(parentKey);
        if (parent == byFormId.end()) {
            return Fail(Error::Code::NotFound,
                        "location " + Describe(ordered[i].formId) + " has a missing parent " +
                            Describe(parentKey));
        }
        snapshot.parents[i] = parent->second;
        children[parent->second.value].push_back(LocationId(static_cast<std::uint32_t>(i)));
    }

    snapshot.entry.assign(ordered.size(), 0);
    snapshot.exit.assign(ordered.size(), 0);
    std::vector<std::uint8_t> state(ordered.size(), 0);
    std::vector<std::pair<LocationId, std::size_t>> stack;
    std::uint32_t clock = 0;

    for (const auto root : roots) {
        if (state[root.value] != 0) {
            continue;
        }
        state[root.value] = 1;
        snapshot.entry[root.value] = clock++;
        stack.emplace_back(root, 0);

        while (!stack.empty()) {
            auto& frame = stack.back();
            const auto node = frame.first;
            auto& next = frame.second;
            const auto& nodeChildren = children[node.value];
            if (next < nodeChildren.size()) {
                const auto child = nodeChildren[next++];
                if (state[child.value] == 1) {
                    return Fail(Error::Code::InvalidState,
                                "location parent cycle at " + Describe(ordered[child.value].formId));
                }
                if (state[child.value] == 2) {
                    return Fail(Error::Code::InvalidState,
                                "location appears under multiple parents " +
                                    Describe(ordered[child.value].formId));
                }
                state[child.value] = 1;
                snapshot.entry[child.value] = clock++;
                stack.emplace_back(child, 0);
            } else {
                snapshot.exit[node.value] = clock++;
                state[node.value] = 2;
                stack.pop_back();
            }
        }
    }

    for (std::size_t i = 0; i < state.size(); ++i) {
        if (state[i] == 0) {
            return Fail(Error::Code::InvalidState,
                        "location parent cycle at " + Describe(ordered[i].formId));
        }
        if (state[i] == 1) {
            return Fail(Error::Code::InvalidState,
                        "unterminated location traversal at " + Describe(ordered[i].formId));
        }
    }

    return snapshot;
}

bool IsAncestor(const LocationIndexSnapshot& snapshot, LocationId ancestor, LocationId location,
                bool includeSelf) {
    if (!ancestor || !location) {
        return false;
    }
    if (ancestor.value >= snapshot.entry.size() || location.value >= snapshot.entry.size() ||
        ancestor.value >= snapshot.exit.size() || location.value >= snapshot.exit.size()) {
        return false;
    }
    if (ancestor == location) {
        return includeSelf;
    }
    return snapshot.entry[ancestor.value] <= snapshot.entry[location.value] &&
           snapshot.exit[location.value] <= snapshot.exit[ancestor.value];
}

std::optional<LocationId> FindLocation(const LocationIndexSnapshot& snapshot,
                                       const StableFormKey& formId) {
    // records are sorted by stable form id at build time; keep that ordering
    // as the only lookup invariant so large location tables stay O(log n).
    const auto it = std::lower_bound(
        snapshot.records.begin(), snapshot.records.end(), formId,
        [](const LocationRecord& record, const StableFormKey& key) {
            return record.formId < key;
        });
    if (it == snapshot.records.end() || it->formId != formId) {
        return std::nullopt;
    }
    return LocationId(static_cast<std::uint32_t>(it - snapshot.records.begin()));
}

}
