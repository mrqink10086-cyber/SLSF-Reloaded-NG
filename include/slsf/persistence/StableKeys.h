#pragma once

#include "slsf/core/persistence/PersistentState.h"

#include <functional>
#include <string_view>

namespace slsf::persistence {

// Repairs historical low-24-bit keys only for plugins identified as light by
// current engine metadata. Unknown plugins retain their unresolved identities.
void NormalizeLightPluginKeys(PersistentState& state,
                              const std::function<bool(std::string_view)>& isLightPlugin);

} // namespace slsf::persistence
