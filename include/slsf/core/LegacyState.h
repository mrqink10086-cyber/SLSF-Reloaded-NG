#pragma once

#include "slsf/core/Types.h"

#include <cstdint>
#include <string>

namespace slsf {

struct CustomRegionState {
    std::string name;
    StableRegionKey key;
    friend bool operator==(const CustomRegionState&, const CustomRegionState&) = default;
};

struct NamedProviderState {
    std::string name;
    std::uint32_t flags{};
    friend bool operator==(const NamedProviderState&, const NamedProviderState&) = default;
};

struct RegionSpreadState {
    StableRegionKey key;
    std::int32_t chance{};
    friend bool operator==(const RegionSpreadState&, const RegionSpreadState&) = default;
};

struct RegionRecognitionState {
    StableRegionKey key;
    double recognitionDays{};
    double lastEnter{};
    double lastExit{};
    bool localAnonymous{};
    bool startedAnonymous{};
    bool resident{};
    friend bool operator==(const RegionRecognitionState&, const RegionRecognitionState&) = default;
};

}
