#pragma once

#include "slsf/interfaces/VmPort.h"
#include <atomic>

namespace slsf::boundary {
class EngineVm final : public IVmPort {
public:
    using Probe = std::function<bool(Epoch)>;
    explicit EngineVm(Probe probe) : probe_(std::move(probe)) {}
    RequestId Submit(VmRequest request, VmCompletion completion) override;
private:
    Probe probe_;
    std::atomic<std::uint64_t> next_{0};
};
}
