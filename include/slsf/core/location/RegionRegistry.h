#pragma once

#include "slsf/core/Snapshots.h"

#include <optional>
#include <string>
#include <string_view>

namespace slsf {

inline constexpr std::size_t kApiLocationIndexLimit = ApiLocationIndex::invalid;

std::optional<RegionId> FindRegion(const RegionSnapshot& snapshot, const StableRegionKey& key);
std::string NormalizeAlias(std::string_view alias);

}
