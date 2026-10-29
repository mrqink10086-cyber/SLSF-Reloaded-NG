#pragma once

#include "slsf/core/Snapshots.h"

#include <optional>

namespace slsf {

std::optional<LocationId> FindLocation(const LocationIndexSnapshot& snapshot,
                                       const StableFormKey& formId);

}
