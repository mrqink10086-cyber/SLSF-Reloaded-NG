#include "slsf/integrations/PapyrusCalls.h"

#include <algorithm>
#include <cstddef>
#include <utility>

namespace slsf {

struct PapyrusCalls::Call {
    mutable std::mutex mutex;
    bool delivered{false};
    RequestId id{};
    bool hasId{false};
    std::int64_t deadlineMs{0};
    bool hasDeadline{false};
    PapyrusCallKind terminal{PapyrusCallKind::Failed};
    Outcome onDone;
};

bool PapyrusCalls::DeliverCall(const std::shared_ptr<Call>& call,
                               PapyrusCallKind kind, Status status,
                               std::vector<VmValue> values, std::string error) {
    if (!call) {
        return false;
    }
    Outcome handler;
    {
        std::lock_guard<std::mutex> guard(call->mutex);
        if (call->delivered) {
            return false;
        }
        call->delivered = true;
        call->terminal = kind;
        handler = std::move(call->onDone);
    }
    if (handler) {
        PapyrusCallResult result;
        result.kind = kind;
        result.status = status;
        result.values = std::move(values);
        result.error = std::move(error);
        try {
            handler(std::move(result));
        } catch (...) {
        }
    }
    return true;
}
void PapyrusCalls::VacuumDelivered() {
    calls_.erase(std::remove_if(calls_.begin(), calls_.end(),
                                [](const std::shared_ptr<Call>& call) {
                                    return !call || call->delivered;
                                }),
                 calls_.end());
}
PapyrusCalls::~PapyrusCalls() {
    std::vector<std::shared_ptr<Call>> pending;
    {
        std::lock_guard<std::mutex> guard(mutex_);
        pending.reserve(calls_.size());
        for (const auto& call : calls_) {
            if (call && !call->delivered) {
                pending.push_back(call);
            }
        }
        calls_.clear();
    }
    for (const auto& call : pending) {
        transport_.RequestCancellation(call->id);
        PapyrusCalls::DeliverCall(call, PapyrusCallKind::Cancelled, Status::NotReady, {},
                                  "call destroyed with the owning PapyrusCalls instance");
    }
}
PapyrusCalls::PapyrusCalls(IVmPort& transport, EpochProbe currentEpoch,
                           PapyrusCallOptions options)
    : transport_(transport),
      epoch_(std::move(currentEpoch)),
      options_(std::move(options)) {}

RequestId PapyrusCalls::Submit(VmTargetKind target, const StableFormKey& instance,
                               std::string script, std::string function,
                               std::vector<VmValue> arguments, Outcome onDone) {
    bool saturated = false;
    const std::shared_ptr<Call> call = std::make_shared<Call>();
    call->onDone = std::move(onDone);
    {
        std::lock_guard<std::mutex> guard(mutex_);
        VacuumDelivered();
        if (options_.maxInFlight > 0) {
            std::size_t pending = 0;
            for (const auto& entry : calls_) {
                if (entry && !entry->delivered) {
                    ++pending;
                }
            }
            if (pending >= options_.maxInFlight) {
                saturated = true;
            }
        }
        if (!saturated && options_.timeoutMs > 0) {
            call->hasDeadline = true;
            call->deadlineMs = nowMs_ + options_.timeoutMs;
        }
    }
    if (saturated) {
        PapyrusCallResult result;
        result.kind = PapyrusCallKind::Failed;
        result.status = Status::Invalid;
        result.error = "too many in-flight calls";
        if (call->onDone) {
            Outcome rejected = std::move(call->onDone);
            try {
                rejected(std::move(result));
            } catch (...) {
            }
        }
        return RequestId{};
    }

    const Epoch submittedEpoch = epoch_ ? epoch_() : Epoch{};

    VmRequest request;
    request.epoch = submittedEpoch;
    request.target = target;
    request.instance = instance;
    request.script = std::move(script);
    request.function = std::move(function);
    request.arguments = std::move(arguments);

    VmCompletion completion = [call, probe = epoch_, submittedEpoch](VmResult result) {
        PapyrusCallKind kind = PapyrusCallKind::Failed;
        Status base = result.status;
        std::vector<VmValue> values = std::move(result.values);
        std::string error = std::move(result.error);
        bool terminal = true;
        {
            std::lock_guard<std::mutex> guard(call->mutex);
            terminal = call->delivered;
        }
        if (terminal) {
            return;
        }
        if (result.status == Status::Ready) {
            if (!probe) {
                kind = PapyrusCallKind::Failed;
                base = Status::Invalid;
                error = "epoch probe unavailable";
            } else if (probe() != submittedEpoch) {
                kind = PapyrusCallKind::Stale;
                base = Status::Stale;
                error = "result epoch is not the current session epoch";
            } else {
                kind = PapyrusCallKind::Ready;
            }
        }
        PapyrusCalls::DeliverCall(call, kind, base, std::move(values), std::move(error));
    };

    const RequestId id = transport_.Submit(std::move(request), std::move(completion));
    {
        std::lock_guard<std::mutex> guard(mutex_);
        call->id = id;
        call->hasId = true;
        calls_.push_back(call);
        lastInstanceTarget_ = (target == VmTargetKind::Instance);
    }
    return id;
}
RequestId PapyrusCalls::SubmitGlobal(std::string script, std::string function,
                                     std::vector<VmValue> arguments, Outcome onDone) {
    return Submit(VmTargetKind::Global, StableFormKey{}, std::move(script),
                  std::move(function), std::move(arguments), std::move(onDone));
}
bool PapyrusCalls::Cancel(RequestId id) {
    std::shared_ptr<Call> call;
    {
        std::lock_guard<std::mutex> guard(mutex_);
        for (const auto& entry : calls_) {
            if (entry && entry->hasId && entry->id == id) {
                call = entry;
                break;
            }
        }
    }
    if (!call) {
        return false;
    }
    return PapyrusCalls::DeliverCall(call, PapyrusCallKind::Cancelled, Status::NotReady,
                                     {}, "call cancelled");
}
void PapyrusCalls::Advance(std::int64_t nowMs) {
    std::vector<std::shared_ptr<Call>> expired;
    {
        std::lock_guard<std::mutex> guard(mutex_);
        if (nowMs > nowMs_) {
            nowMs_ = nowMs;
        }
        for (const auto& call : calls_) {
            if (call && !call->delivered && call->hasDeadline && nowMs_ >= call->deadlineMs) {
                expired.push_back(call);
            }
        }
    }
    for (const auto& call : expired) {
        PapyrusCalls::DeliverCall(call, PapyrusCallKind::TimedOut, Status::NotReady, {},
                                  "call timed out");
    }
}
std::size_t PapyrusCalls::InFlight() const {
    std::lock_guard<std::mutex> guard(mutex_);
    std::size_t count = 0;
    for (const auto& call : calls_) {
        if (call && !call->delivered) {
            ++count;
        }
    }
    return count;
}
bool PapyrusCalls::LastTargetWasInstance() const {
    std::lock_guard<std::mutex> guard(mutex_);
    return lastInstanceTarget_;
}
}