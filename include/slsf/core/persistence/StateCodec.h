#pragma once

#include "slsf/core/persistence/PersistentState.h"

#include <cstddef>
#include <cstdint>

namespace slsf {

inline constexpr std::uint32_t kStateCodecMagic = 0x534C5331u;
inline constexpr std::uint32_t kStateCodecVersion = kPersistentStateCodecVersion;
inline constexpr std::uint32_t kLegacyStateCodecVersion = kLegacyPersistentStateCodecVersion;
inline constexpr std::size_t kStateCodecMaxLocations = 65535;
inline constexpr std::size_t kStateCodecMaxProviders = 1u << 20;
inline constexpr std::size_t kStateCodecMaxCustomRegions = 65535;
inline constexpr std::size_t kStateCodecMaxGraphEdits = 1u << 20;
inline constexpr std::size_t kStateCodecMaxSettings = 1u << 16;
inline constexpr std::size_t kStateCodecMaxStringBytes = 65535;

Result<void> ValidateSaveMetadata(const PersistentState& state);

}
