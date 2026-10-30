#include "slsf/persistence/StableKeys.h"

#include <map>
#include <string>

namespace slsf::persistence {

void NormalizeLightPluginKeys(PersistentState& state,
                              const std::function<bool(std::string_view)>& isLightPlugin) {
    std::map<std::string, bool, std::less<>> lightPlugins;
    const auto normalize = [&](StableRegionKey& key) {
        if (key.localFormId <= 0xFFFu || key.localFormId > 0xFFFFFFu) {
            return;
        }
        auto [found, inserted] = lightPlugins.try_emplace(key.plugin);
        if (inserted) {
            found->second = isLightPlugin(key.plugin);
        }
        if (found->second) {
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
}