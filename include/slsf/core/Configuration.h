#pragma once

#include "slsf/core/CategoryMask.h"
#include "slsf/core/Commands.h"
#include "slsf/core/Types.h"

#include <array>
#include <compare>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace slsf {

inline constexpr std::uint32_t kConfigurationSchemaVersion = 1;

constexpr std::array<double, kFameCategoryCount> MakeUnitCategoryMultipliers() noexcept {
    std::array<double, kFameCategoryCount> multipliers{};
    for (auto& multiplier : multipliers) {
        multiplier = 1.0;
    }
    return multipliers;
}
// Pure compiled configuration values; JSON parsing and validation stay in the adapter.
struct FeatureSwitches {
    bool reduceFameAtNight{true};
    bool fameSpreadRestrictions{true};
    bool dynamicAnonymity{true};

    friend bool operator==(const FeatureSwitches&, const FeatureSwitches&) = default;
};

struct CategorySettings {
    std::array<double, kFameCategoryCount> multipliers{MakeUnitCategoryMultipliers()};
    double minimumFameToSpread{};
    std::uint32_t maximumSpreadCategories{};
    double maximumSpreadPercentage{};

    friend bool operator==(const CategorySettings&, const CategorySettings&) = default;
};

struct RelationshipSettings {
    CategoryMask forbiddenByFriend;
    CategoryMask forbiddenByLover;
    double friendHighFameThreshold{};
    double loverHighFameThreshold{};
    double friendBetrayChance{};
    double loverBetrayChance{};

    friend bool operator==(const RelationshipSettings&, const RelationshipSettings&) = default;
};

struct TimingSettings {
    double decayPeriodDays{};
    double spreadPeriodDays{};
    double nightStart{};
    double nightEnd{};
    double failedSpreadIncrease{};
    double successfulSpreadReduction{};

    friend bool operator==(const TimingSettings&, const TimingSettings&) = default;
};

struct RegionConnectionSetting {
    StableRegionKey from;
    StableRegionKey to;
    bool enabled{true};

    friend bool operator==(const RegionConnectionSetting&, const RegionConnectionSetting&) = default;
};

struct NotificationSettings {
    bool fameIncrease{true};
    bool fameDecay{true};
    bool fameSpread{true};

    friend bool operator==(const NotificationSettings&, const NotificationSettings&) = default;
};

struct MenuSettings {
    NotificationSettings notifications;
    std::vector<StableRegionKey> selectedRegions;

    friend bool operator==(const MenuSettings&, const MenuSettings&) = default;
};

struct ConfigurationData {
    std::uint32_t schemaVersion{kConfigurationSchemaVersion};
    FeatureSwitches features;
    CategorySettings categories;
    RelationshipSettings relationships;
    TimingSettings timing;
    Parameters parameters;
    std::vector<RegionConnectionSetting> connections;
    MenuSettings menu;

    friend bool operator==(const ConfigurationData&, const ConfigurationData&) = default;
};

struct ConfigurationRevision {
    std::uint64_t value{};

    constexpr explicit operator bool() const noexcept { return value != 0; }
    friend constexpr bool operator==(const ConfigurationRevision&,
                                     const ConfigurationRevision&) noexcept = default;
    friend constexpr auto operator<=>(const ConfigurationRevision&,
                                      const ConfigurationRevision&) noexcept = default;
};

enum class ConfigurationUpdateStatus : std::uint8_t {
    Applied = 0,
    Unchanged,
    Invalid,
    IoError,
};

// Failed updates keep the last valid data/revision; status/message describe why.
struct ConfigurationUpdateResult {
    ConfigurationUpdateStatus status{ConfigurationUpdateStatus::Invalid};
    ConfigurationRevision revision;
    std::string message;

    constexpr bool Succeeded() const noexcept {
        return status == ConfigurationUpdateStatus::Applied ||
               status == ConfigurationUpdateStatus::Unchanged;
    }

    friend bool operator==(const ConfigurationUpdateResult&,
                           const ConfigurationUpdateResult&) = default;
};

// Immutable consumer view; data and revision change together.
struct ConfigurationSnapshot {
    ConfigurationRevision revision;
    std::shared_ptr<const ConfigurationData> data;
    ConfigurationUpdateResult lastUpdate;

    bool HasData() const noexcept { return data != nullptr; }
};

}