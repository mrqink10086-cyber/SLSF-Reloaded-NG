#pragma once

#include "slsf/core/Snapshots.h"
#include "slsf/core/Types.h"

#include <optional>
#include <vector>

namespace slsf::test {

std::optional<LocationId> FindLocationReference(const std::vector<LocationRecord>& records,
                                                const StableFormKey& formId);
bool IsAncestorReference(const std::vector<LocationRecord>& records, LocationId ancestor,
                         LocationId location, bool includeSelf);
std::optional<RegionId> ResolveRegionReference(const LocationIndexSnapshot& index,
                                              const RegionDefinitions& definitions,
                                              LocationId location);

}
