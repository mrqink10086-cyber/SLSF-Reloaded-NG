#pragma once

#include "slsf/core/Types.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace slsf::integrations {

enum class ExternalAvailability : std::uint8_t {
    Unknown = 0,
    Available,
    Missing,
    Incompatible,
};

[[nodiscard]] const char* ExternalAvailabilityName(ExternalAvailability value) noexcept;

enum class ExternalGate : std::uint8_t {
    Ready = 0,
    Waiting,
    Changed,
    Stale,
    Missing,
    Incompatible,
    Failed,
    Closed,
};

[[nodiscard]] const char* ExternalGateName(ExternalGate value) noexcept;
[[nodiscard]] bool BlocksNewScoring(ExternalGate value) noexcept;
[[nodiscard]] ExternalGate GateForAvailability(ExternalAvailability value) noexcept;
[[nodiscard]] FactState FactStateForAvailability(ExternalAvailability value) noexcept;

struct ExternalTimingPolicy {
    std::int64_t timeoutMs{5000};
    std::int64_t retryIntervalMs{250};
    std::uint32_t maxAttempts{20};
};

struct ExternalProviderInfo {
    std::string name;
    std::string version;
    std::uint32_t apiVersion{};
};

inline constexpr std::size_t kMaxExternalActors = 64;
inline constexpr std::size_t kMaxExternalTags = 512;

}
