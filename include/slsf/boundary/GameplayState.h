#pragma once

#include "slsf/core/LegacyState.h"
#include "slsf/core/Snapshots.h"
#include "slsf/core/gameplay/Anonymity.h"
#include "slsf/core/location/RegionRegistry.h"
#include <span>
#include <vector>

namespace slsf::boundary {
inline void RestoreRecognition(gameplay::AnonymityRules& rules, std::optional<RegionId>& current,
                               std::span<const RegionRecognitionState> rows, const RegionSnapshot& regions) {
    gameplay::AnonymityState state;
    current.reset();
    for (const auto& row : rows) if (const auto region = FindRegion(regions, row.key)) {
        state.regions[*region] = {row.localAnonymous, row.startedAnonymous, row.recognitionDays, GameTime{row.lastEnter}, GameTime{row.lastExit}};
        if (row.resident) current = *region;
    }
    rules.Restore(std::move(state));
}
inline void TransitionRecognition(gameplay::AnonymityRules& rules, std::optional<RegionId>& current,
                                  std::optional<RegionId> next, GameTime now) {
    if (current == next) return;
    if (rules.ObserveRegion(current, next, now) == Status::Ready) current = next;
}
inline std::vector<RegionRecognitionState> CaptureRecognition(const gameplay::AnonymityRules& rules,
                                                             std::optional<RegionId> current, const RegionSnapshot& regions) {
    std::vector<RegionRecognitionState> rows;
    for (const auto& [region, memory] : rules.State().regions) if (region.value < regions.regions.size())
        rows.push_back({regions.regions[region.value], memory.recognitionDays, memory.lastEnter.days, memory.lastExit.days, memory.localAnonymous, memory.startedAnonymous, current == region});
    return rows;
}
}
