#pragma once

#include "slsf/core/CategoryMask.h"
#include "slsf/core/Types.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace slsf {

inline constexpr std::size_t kMaxProviderRegistrations = 255;

enum class ProviderPhase : std::uint8_t {
    Enabled = 0,
    Disabled,
    Missing,
    Incompatible,
    Stale,
    NotReady,
    Unregistered,
};

struct ProviderCommand {
    std::uint64_t registrationId{};
    ProviderPhase phase{ProviderPhase::Enabled};
    std::vector<FameCategory> categories;
};

enum class ProviderTransitionKind : std::uint8_t {
    None = 0,
    Enabled,
    Disabled,
    Invalidated,
    Unregistered,
};

struct ProviderTransition {
    Status status{Status::Invalid};
    ProviderTransitionKind kind{ProviderTransitionKind::None};
    std::vector<FameCategory> wipeCategories;
    bool changed{};
};

struct ProviderContribution {
    std::uint64_t registrationId{};
    CategoryMask categories;
};

struct ProviderState {
    std::vector<ProviderContribution> contributions;
    std::vector<std::uint64_t> stopped;
    std::array<std::uint32_t, kFameSlotCount> activeCounts{};

    bool IsActive(FameCategory category) const noexcept;
    std::size_t ActiveProviderCount(FameCategory category) const noexcept;

    CategoryMask ActiveCategories() const noexcept;
    void RebuildActiveCounts() noexcept;
};

ProviderTransition UpdateProvider(ProviderState& state, const ProviderCommand& command);

}
