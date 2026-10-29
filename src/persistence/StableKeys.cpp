#include "slsf/persistence/StableKeys.h"

namespace slsf::persistence {

void NormalizeLightPluginKeys(PersistentState& state,
                              const std::function<bool(std::string_view)>& isLightPlugin) {
    const auto normalize = [&](StableRegionKey& key) {
        if (key.localFormId > 0xFFFu && key.localFormId <= 0xFFFFFFu &&
            isLightPlugin(key.plugin)) {
            key.localFormId &= 0xFFFu;
        }
    };
    for (auto& location : state.locations) {
        normalize(location.key);
    }
    for (auto& region : state.customRegions) {
        normalize(region.key);
    }
    for (auto& edit : state.graphEdits) {
        normalize(edit.from);
        normalize(edit.to);
    }
}

} // namespace slsf::persistence
