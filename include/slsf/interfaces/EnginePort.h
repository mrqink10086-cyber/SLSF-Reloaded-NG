#pragma once

#include "slsf/core/Types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace slsf {

enum class EngineRequestKind : std::uint8_t {
    ApplyActorState = 0,
    ApplyVisibility,
    Notify,
    Wake,
};

struct EngineRequest {
    Epoch epoch;
    EngineRequestKind kind{EngineRequestKind::Notify};
    StableFormKey actor;
    std::string payload;
    std::vector<std::string> tags;
};

class IEnginePort {
public:
    virtual ~IEnginePort() = default;
    virtual RequestId Post(EngineRequest request) = 0;
};

} // namespace slsf
