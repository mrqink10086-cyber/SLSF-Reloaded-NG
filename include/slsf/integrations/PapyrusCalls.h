#pragma once

#include "slsf/core/Types.h"
#include "slsf/interfaces/VmPort.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace slsf {

enum class PapyrusCallKind : std::uint8_t {
    Ready = 0,
    Stale,
    Failed,
    TimedOut,
    Cancelled,
};

struct PapyrusCallResult {
    PapyrusCallKind kind{PapyrusCallKind::Failed};
    Status status{Status::NotReady};
    std::vector<VmValue> values;
    std::string error;
};

struct PapyrusCallOptions {
    std::int64_t timeoutMs{0};
    std::size_t maxInFlight{256};
};

class PapyrusCalls {
public:
    using Outcome = std::function<void(PapyrusCallResult)>;
    using EpochProbe = std::function<Epoch()>;

    PapyrusCalls(IVmPort& transport, EpochProbe currentEpoch, PapyrusCallOptions options);
    ~PapyrusCalls();

    RequestId Submit(VmTargetKind target, const StableFormKey& instance,
                     std::string script, std::string function,
                     std::vector<VmValue> arguments, Outcome onDone);

    RequestId SubmitGlobal(std::string script, std::string function,
                           std::vector<VmValue> arguments, Outcome onDone);

    bool Cancel(RequestId id);

    void Advance(std::int64_t nowMs);

    std::size_t InFlight() const;

    bool LastTargetWasInstance() const;

private:
    struct Call;

    static bool DeliverCall(const std::shared_ptr<Call>& call, PapyrusCallKind kind,
                            Status status, std::vector<VmValue> values,
                            std::string error);

    void VacuumDelivered();

    IVmPort& transport_;
    EpochProbe epoch_;
    PapyrusCallOptions options_;
    mutable std::mutex mutex_;
    std::vector<std::shared_ptr<Call>> calls_;
    std::int64_t nowMs_{0};
    bool lastInstanceTarget_{false};
};

}