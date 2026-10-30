#pragma once

#include "slsf/core/Types.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>

namespace slsf::compat {

using FameValueResolver =
    std::function<int(const std::string& locationName, FameCategory category)>;

class LegacyOutputSink {
public:
    virtual ~LegacyOutputSink() = default;
    virtual void SetCurrentFameGlobal(const StableFormKey& key, int value) = 0;
    virtual void SetStorageIntList(const std::string& key, std::size_t locationIndex,
                                  int value) = 0;
    virtual void SetLegacyBridgeGlobal(const StableFormKey& key, int value) = 0;
};

class LegacyOutputs {
public:
    static constexpr std::size_t kCurrentFameGlobalCount = 25;
    static constexpr std::size_t kLegacyFameSlotCount = 20;
    static constexpr std::size_t kLegacyLocationIndexCount = 24;

    static StableFormKey CurrentFameGlobalKey(FameCategory category);
    static const char* CurrentFameGlobalEditorId(FameCategory category) noexcept;

    static std::string StorageFameKey(std::size_t fameIndex) noexcept;
    static std::optional<FameCategory> StorageFameCategory(std::size_t fameIndex) noexcept;

    static std::optional<std::string> LegacyLocationName(std::size_t locationIndex) noexcept;

    static StableFormKey LegacyBridgeKey(std::size_t fameIndex);
    static std::optional<FameCategory> LegacyBridgeCategory(std::size_t fameIndex) noexcept;

    std::size_t PublishCurrentFameGlobals(LegacyOutputSink& sink,
                                          const FameValueResolver& resolve,
                                          const std::string& currentLocationName) const;

    std::size_t PublishStorageUtilLists(LegacyOutputSink& sink,
                                        const FameValueResolver& resolve) const;

    std::size_t PublishLegacyBridge(LegacyOutputSink& sink,
                                    const FameValueResolver& resolve,
                                    const std::string& currentLocationName) const;
};

}
