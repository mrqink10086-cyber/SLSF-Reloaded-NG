#pragma once

#include "slsf/core/Types.h"

#include <functional>

namespace slsf {

using WakeCallback = std::function<void(GameTime)>;

class IWakeSource {
public:
    virtual ~IWakeSource() = default;
    virtual bool Start(WakeCallback callback) = 0;
    virtual void Stop() = 0;
};

}