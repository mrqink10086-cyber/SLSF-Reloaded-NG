#pragma once

#include "slsf/core/CategoryMask.h"
#include "slsf/core/Types.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace slsf {

inline constexpr std::uint32_t kPersistentStateSchemaVersion = 1;
// v1 kept settings and user graph edits in the native save.  v2 keeps only
// per-save business state; both now live in the cross-save JSON configuration.
inline constexpr std::uint32_t kPersistentStateCodecVersion = 2;
inline constexpr std::uint32_t kLegacyPersistentStateCodecVersion = 1;

struct LocationPersistentState {
    StableRegionKey key;
    std::array<double, kFameSlotCount> local{};
    std::array<double, kFameSlotCount> remote{};
    std::array<double, kFameSlotCount> protectionUntilDay{};
    std::array<std::uint8_t, kFameSlotCount> flags{};
    double settledAt{};
    // Fully anchored game-time of the last remote relaxation pass.  v1 saves
    // predate this field and restore it as zero.
    double remoteSettledAt{};
    std::uint8_t anonymity{};
    std::array<std::uint8_t, 4> auxiliaryState{};

    friend bool operator==(const LocationPersistentState&, const LocationPersistentState&) = default;
};

struct ProviderFlagState {
    std::string provider;
    FameCategory category{FameCategory::Count};
    bool enabled{};
    std::uint8_t phase{};

    friend bool operator==(const ProviderFlagState&, const ProviderFlagState&) = default;
};

// A durable provider contribution.  The category mask is the low 25-bit set
// already used by the runtime registry; registration ids are stable for the
// lifetime of the save.
struct ProviderContributionState {
    std::uint64_t registrationId{};
    CategoryMask categories;

    friend bool operator==(const ProviderContributionState&,
                           const ProviderContributionState&) = default;
};

struct CustomRegionState {
    std::string name;
    StableRegionKey key;

    friend bool operator==(const CustomRegionState&, const CustomRegionState&) = default;
};

struct GraphEditState {
    StableRegionKey from;
    StableRegionKey to;
    double retention{};
    bool enabled{};

    friend bool operator==(const GraphEditState&, const GraphEditState&) = default;
};

struct SettingState {
    std::string key;
    double value{};

    friend bool operator==(const SettingState&, const SettingState&) = default;
};

struct PersistentState {
    std::uint32_t schemaVersion{kPersistentStateSchemaVersion};
    // Codec version of the payload this state came from.  New captures use
    // kPersistentStateCodecVersion; decoding a v1 payload preserves version 1
    // so a decode/encode roundtrip is byte-exact.
    std::uint32_t codecVersion{kPersistentStateCodecVersion};
    std::vector<LocationPersistentState> locations;
    std::vector<ProviderContributionState> contributions;
    std::vector<CustomRegionState> customRegions;
    // Legacy v1-only sections.  v2 encodes no graph edits or settings; loaders
    // must not apply these over the current cross-save JSON configuration.
    std::vector<ProviderFlagState> providerFlags;
    std::vector<GraphEditState> graphEdits;
    std::vector<SettingState> settings;

    friend bool operator==(const PersistentState&, const PersistentState&) = default;
};

Result<PersistentState> DecodeState(std::span<const std::byte> bytes);
Result<std::vector<std::byte>> EncodeState(const PersistentState& state);

} // namespace slsf
