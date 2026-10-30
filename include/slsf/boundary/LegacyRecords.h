#pragma once

#include "slsf/compat/LegacyOutputs.h"
#include "slsf/compat/PapyrusBindings.h"

#include <functional>

namespace RE {
class TESQuest;
}

namespace slsf::boundary {

// Construct/use after DataLoaded on the game thread.
class EngineLegacyOutputSink final : public compat::LegacyOutputSink {
public:
    using StorageWriter = std::function<void(const std::string&, std::size_t, int)>;
    explicit EngineLegacyOutputSink(StorageWriter storageWriter);

    void SetCurrentFameGlobal(const StableFormKey& key, int value) override;
    void SetStorageIntList(const std::string& key, std::size_t locationIndex,
                           int value) override;
    void SetLegacyBridgeGlobal(const StableFormKey& key, int value) override;

private:
    StorageWriter storageWriter_;
};

[[nodiscard]] RE::TESQuest* LookupLegacyQuest(const StableFormKey& key);
[[nodiscard]] compat::PapyrusInstance PapyrusInstanceForQuest(
    const RE::TESQuest* quest, std::int32_t aliasIndex = -1);

}
