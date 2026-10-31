#include "slsf/integrations/PapyrusCalls.h"

#include "slsf/interfaces/VmPort.h"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <vector>

namespace {

using slsf::Epoch;
using slsf::IVmPort;
using slsf::PapyrusCallKind;
using slsf::PapyrusCallOptions;
using slsf::PapyrusCallResult;
using slsf::PapyrusCalls;
using slsf::RequestId;
using slsf::StableFormKey;
using slsf::Status;
using slsf::VmCompletion;
using slsf::VmRequest;
using slsf::VmResult;
using slsf::VmTargetKind;
using slsf::VmValue;
using slsf::VmValueType;

VmValue IntVm(std::int64_t v) {
    VmValue value;
    value.type = VmValueType::Int;
    value.intValue = v;
    return value;
}
class FakeVmPort final : public IVmPort {
public:
    struct Pending {
        VmRequest request;
        VmCompletion completion;
    };

    RequestId Submit(VmRequest request, VmCompletion completion) override {
        const RequestId id{nextId_++};
        pending_.push_back(Pending{std::move(request), std::move(completion)});
        return id;
    }

    void Complete(RequestId id, Status status, std::vector<VmValue> values = {},
                  const std::string& error = "") {
        auto& entry = pending_[id.value - 1];
        VmResult result;
        result.epoch = entry.request.epoch;
        result.status = status;
        result.values = std::move(values);
        result.error = error;
        VmCompletion completion = std::move(entry.completion);
        entry.completion = nullptr;
        if (completion) {
            completion(std::move(result));
        }
    }

    std::size_t Count() const { return pending_.size(); }
    const VmRequest* RequestAt(std::size_t index) const {
        return index < pending_.size() ? &pending_[index].request : nullptr;
    }

    void RequestCancellation(RequestId id) override { cancelledIds_.push_back(id); }

    const std::vector<RequestId>& Cancelled() const { return cancelledIds_; }

private:
    std::vector<Pending> pending_;
    std::vector<RequestId> cancelledIds_;
    std::uint64_t nextId_{1};
};

struct EpochBox {
    Epoch current{1, 0, 0};
};

PapyrusCalls::EpochProbe EpochProbe(EpochBox& box) {
    return [&box] { return box.current; };
}
class SharedPort final : public IVmPort {
public:
    struct Pending {
        VmRequest request;
        VmCompletion completion;
    };

    RequestId Submit(VmRequest request, VmCompletion completion) override {
        std::lock_guard lock(mutex_);
        const RequestId id{nextId_++};
        pending_.push_back(Pending{std::move(request), std::move(completion)});
        return id;
    }

    void Complete(RequestId id, Status status, std::vector<VmValue> values = {},
                  const std::string& error = "") {
        VmCompletion completion;
        VmResult result;
        {
            std::lock_guard lock(mutex_);
            Pending& entry = pending_[static_cast<std::size_t>(id.value - 1)];
            result.epoch = entry.request.epoch;
            result.status = status;
            result.values = std::move(values);
            result.error = error;
            completion = std::move(entry.completion);
            entry.completion = nullptr;
        }
        if (completion) {
            completion(std::move(result));
        }
    }

    void RequestCancellation(RequestId) override {}

private:
    mutable std::mutex mutex_;
    std::vector<Pending> pending_;
    std::uint64_t nextId_{1};
};

VmValue MakeString(const std::string& value) {
    VmValue out;
    out.type = VmValueType::String;
    out.stringValue = value;
    return out;
}
TEST_CASE("papyrus success delivers Ready exactly once with values") {
    FakeVmPort port;
    EpochBox box;
    PapyrusCalls calls(port, EpochProbe(box), PapyrusCallOptions{});

    std::atomic<int> delivered{0};
    PapyrusCallResult seen;
    auto onDone = [&](PapyrusCallResult result) {
        delivered++;
        seen = std::move(result);
    };

    const RequestId id =
        calls.SubmitGlobal("SLSF_Reloaded_DataManager", "GetFameValue",
                           {MakeString("Whiterun"), MakeString("Whore")}, onDone);
    REQUIRE(calls.InFlight() == 1);

    const auto* sent = port.RequestAt(0);
    REQUIRE(sent != nullptr);
    REQUIRE(sent->target == VmTargetKind::Global);
    REQUIRE(sent->script == "SLSF_Reloaded_DataManager");
    REQUIRE(sent->function == "GetFameValue");
    REQUIRE(sent->arguments.size() == 2);
    REQUIRE(sent->arguments[0].type == VmValueType::String);
    REQUIRE(sent->arguments[0].stringValue == "Whiterun");
    REQUIRE_FALSE(calls.LastTargetWasInstance());

    port.Complete(id, Status::Ready, {IntVm(42)});
    REQUIRE(delivered == 1);
    REQUIRE(seen.kind == PapyrusCallKind::Ready);
    REQUIRE(seen.status == Status::Ready);
    REQUIRE(seen.values.size() == 1);
    REQUIRE(seen.values[0].intValue == 42);
    REQUIRE(calls.InFlight() == 0);

    port.Complete(id, Status::Ready);
    REQUIRE(delivered == 1);
}
TEST_CASE("papyrus instance target carries the bound form") {
    FakeVmPort port;
    EpochBox box;
    PapyrusCalls calls(port, EpochProbe(box), PapyrusCallOptions{});

    const StableFormKey instance{"SLSF Reloaded.esp", 0x80A};
    std::atomic<int> delivered{0};
    const RequestId id = calls.Submit(
        VmTargetKind::Instance, instance, "SLSF_Reloaded_FameManager",
        "UpdateFame", {IntVm(1)},
        [&delivered](PapyrusCallResult) { delivered++; });

    const auto* sent = port.RequestAt(0);
    REQUIRE(sent != nullptr);
    REQUIRE(sent->target == VmTargetKind::Instance);
    REQUIRE(sent->instance == instance);
    REQUIRE(calls.LastTargetWasInstance());

    port.Complete(id, Status::Ready);
    REQUIRE(delivered == 1);
}
TEST_CASE("papyrus timeout fires once, late completion is ignored") {
    FakeVmPort port;
    EpochBox box;
    PapyrusCalls calls(port, EpochProbe(box),
                      PapyrusCallOptions{.timeoutMs = 100});

    std::atomic<int> delivered{0};
    PapyrusCallResult seen;
    const RequestId id = calls.SubmitGlobal(
        "S", "F", {}, [&delivered, &seen](PapyrusCallResult r) {
            delivered++;
            seen = std::move(r);
        });

    calls.Advance(99);
    REQUIRE(delivered == 0);
    REQUIRE(calls.InFlight() == 1);

    calls.Advance(100);
    REQUIRE(delivered == 1);
    REQUIRE(seen.kind == PapyrusCallKind::TimedOut);
    REQUIRE(calls.InFlight() == 0);

    calls.Advance(500);
    REQUIRE(delivered == 1);
    port.Complete(id, Status::Ready);
    REQUIRE(delivered == 1);
}
TEST_CASE("papyrus cancel fires once, late completion is ignored") {
    FakeVmPort port;
    EpochBox box;
    PapyrusCalls calls(port, EpochProbe(box), PapyrusCallOptions{});

    std::atomic<int> delivered{0};
    PapyrusCallResult seen;
    const RequestId id = calls.SubmitGlobal(
        "S", "F", {}, [&delivered, &seen](PapyrusCallResult r) {
            delivered++;
            seen = std::move(r);
        });

    REQUIRE(calls.Cancel(id));
    REQUIRE(delivered == 1);
    REQUIRE(seen.kind == PapyrusCallKind::Cancelled);
    REQUIRE(calls.InFlight() == 0);
    REQUIRE(port.Cancelled().size() == 1);
    REQUIRE(port.Cancelled().front() == id);

    REQUIRE_FALSE(calls.Cancel(RequestId{999}));
    REQUIRE_FALSE(calls.Cancel(id));
    REQUIRE(delivered == 1);

    port.Complete(id, Status::Ready);
    REQUIRE(delivered == 1);
}
TEST_CASE("papyrus stale epoch result is demoted to Stale") {
    FakeVmPort port;
    EpochBox box;
    PapyrusCalls calls(port, EpochProbe(box), PapyrusCallOptions{});

    std::atomic<int> delivered{0};
    PapyrusCallResult seen;
    const RequestId id = calls.SubmitGlobal(
        "S", "F", {}, [&delivered, &seen](PapyrusCallResult r) {
            delivered++;
            seen = std::move(r);
        });

    box.current = Epoch{2, 0, 0};
    port.Complete(id, Status::Ready, {IntVm(7)});

    REQUIRE(delivered == 1);
    REQUIRE(seen.kind == PapyrusCallKind::Stale);
    REQUIRE(seen.status == Status::Stale);
    REQUIRE_FALSE(seen.values.empty());
}
TEST_CASE("papyrus transport stale status is not reported as Failed") {
    FakeVmPort port;
    EpochBox box;
    PapyrusCalls calls(port, EpochProbe(box), PapyrusCallOptions{});

    PapyrusCallResult seen;
    const RequestId id = calls.SubmitGlobal(
        "S", "F", {}, [&seen](PapyrusCallResult r) { seen = std::move(r); });

    port.Complete(id, Status::Stale);
    REQUIRE(seen.kind == PapyrusCallKind::Stale);
    REQUIRE(seen.status == Status::Stale);
}
TEST_CASE("papyrus transport failure surfaces as Failed") {
    FakeVmPort port;
    EpochBox box;
    PapyrusCalls calls(port, EpochProbe(box), PapyrusCallOptions{});

    PapyrusCallResult seen;
    const RequestId id =
        calls.SubmitGlobal("S", "F", {}, [&seen](PapyrusCallResult r) {
            seen = std::move(r);
        });

    port.Complete(id, Status::Invalid, {}, "function not found");
    REQUIRE(seen.kind == PapyrusCallKind::Failed);
    REQUIRE(seen.status == Status::Invalid);
    REQUIRE(seen.error == "function not found");
}
TEST_CASE("papyrus in-flight calls complete independently") {
    FakeVmPort port;
    EpochBox box;
    PapyrusCalls calls(port, EpochProbe(box), PapyrusCallOptions{});

    std::atomic<int> a{0};
    std::atomic<int> b{0};
    PapyrusCallResult aSeen;
    PapyrusCallResult bSeen;
    const RequestId idA = calls.SubmitGlobal(
        "S", "A", {}, [&a, &aSeen](PapyrusCallResult r) {
            a++;
            aSeen = std::move(r);
        });
    const RequestId idB = calls.SubmitGlobal(
        "S", "B", {}, [&b, &bSeen](PapyrusCallResult r) {
            b++;
            bSeen = std::move(r);
        });
    REQUIRE(calls.InFlight() == 2);

    port.Complete(idB, Status::Ready);
    REQUIRE(b == 1);
    REQUIRE(a == 0);
    REQUIRE(calls.InFlight() == 1);

    port.Complete(idA, Status::Ready);
    REQUIRE(a == 1);
    REQUIRE(b == 1);
    REQUIRE(calls.InFlight() == 0);
    REQUIRE(aSeen.kind == PapyrusCallKind::Ready);
    REQUIRE(bSeen.kind == PapyrusCallKind::Ready);

    REQUIRE(port.RequestAt(0)->function == "A");
    REQUIRE(port.RequestAt(1)->function == "B");
}
TEST_CASE("papyrus overflow of the in-flight cap rejects with Failed") {
    FakeVmPort port;
    EpochBox box;
    PapyrusCalls calls(port, EpochProbe(box),
                       PapyrusCallOptions{.maxInFlight = 1});

    std::atomic<int> delivered{0};
    PapyrusCallResult firstSeen;
    PapyrusCallResult rejectedSeen;
    const RequestId idA = calls.SubmitGlobal(
        "S", "A", {},
        [&](PapyrusCallResult r) {
            delivered++;
            firstSeen = std::move(r);
        });
    REQUIRE(idA.value != 0);
    REQUIRE(calls.InFlight() == 1);
    REQUIRE(port.Count() == 1);

    const RequestId idB = calls.SubmitGlobal(
        "S", "B", {},
        [&](PapyrusCallResult r) {
            delivered++;
            rejectedSeen = std::move(r);
        });
    REQUIRE(idB.value == 0);
    REQUIRE(port.Count() == 1);
    REQUIRE(delivered == 1);
    REQUIRE(rejectedSeen.kind == PapyrusCallKind::Failed);
    REQUIRE(rejectedSeen.status == Status::Invalid);
    REQUIRE(rejectedSeen.error == "too many in-flight calls");
    REQUIRE(calls.InFlight() == 1);

    port.Complete(idA, Status::Ready);
    REQUIRE(delivered == 2);
    REQUIRE(firstSeen.kind == PapyrusCallKind::Ready);

    std::atomic<int> extra{0};
    const RequestId idC = calls.SubmitGlobal(
        "S", "C", {}, [&extra](PapyrusCallResult) { extra++; });
    REQUIRE(idC.value != 0);
    REQUIRE(port.Count() == 2);
    port.Complete(idC, Status::Ready);
    REQUIRE(extra == 1);
}
TEST_CASE("papyrus destructor delivers Cancelled once and ignores late completions") {
    FakeVmPort port;
    EpochBox box;
    std::atomic<int> delivered{0};
    PapyrusCallResult seen;
    RequestId idA{};
    RequestId idB{};
    {
        PapyrusCalls calls(port, EpochProbe(box), PapyrusCallOptions{});
        idA = calls.SubmitGlobal(
            "S", "A", {}, [&](PapyrusCallResult r) {
                delivered++;
                seen = std::move(r);
            });
        idB = calls.SubmitGlobal("S", "B", {}, [&](PapyrusCallResult) { delivered++; });
        REQUIRE(calls.InFlight() == 2);
    }
    REQUIRE(delivered == 2);
    REQUIRE(seen.kind == PapyrusCallKind::Cancelled);
    REQUIRE(seen.error.find("destroyed") != std::string::npos);
    REQUIRE(port.Cancelled().size() == 2);
    REQUIRE(port.Cancelled()[0].value == idA.value);
    REQUIRE(port.Cancelled()[1].value == idB.value);

    port.Complete(idA, Status::Ready, {IntVm(5)});
    port.Complete(idB, Status::Ready);
    REQUIRE(delivered == 2);
    REQUIRE(seen.values.empty());
}
TEST_CASE("papyrus concurrent submit/cancel/complete race delivers exactly once") {
    SharedPort port;
    EpochBox box;
    PapyrusCalls calls(port, EpochProbe(box),
                       PapyrusCallOptions{.timeoutMs = 120, .maxInFlight = 0});

    constexpr int kPerSubmitter = 250;
    std::atomic<int> outcomes{0};
    std::mutex idsMutex;
    std::vector<RequestId> ids;

    auto submitter = [&] {
        for (int i = 0; i < kPerSubmitter; ++i) {
            const RequestId id = calls.SubmitGlobal(
                "S", "F", {}, [&outcomes](PapyrusCallResult) { ++outcomes; });
            std::lock_guard lock(idsMutex);
            ids.push_back(id);
        }
    };
    auto completer = [&](std::uint32_t seed) {
        std::mt19937 rng{seed};
        for (int i = 0; i < 600; ++i) {
            RequestId pick{};
            {
                std::lock_guard lock(idsMutex);
                if (!ids.empty()) {
                    pick = ids[rng() % ids.size()];
                }
            }
            if (pick.value != 0) {
                port.Complete(pick, Status::Ready);
            }
        }
    };
    auto canceller = [&](std::uint32_t seed) {
        std::mt19937 rng{seed};
        for (int i = 0; i < 400; ++i) {
            RequestId pick{};
            {
                std::lock_guard lock(idsMutex);
                if (!ids.empty()) {
                    pick = ids[rng() % ids.size()];
                }
            }
            if (pick.value != 0) {
                calls.Cancel(pick);
            }
        }
    };
    auto advancer = [&] {
        for (int n = 1; n <= 400; ++n) {
            calls.Advance(n * 7);
        }
    };

    std::vector<std::thread> threads;
    threads.emplace_back(submitter);
    threads.emplace_back(submitter);
    threads.emplace_back(completer, 11u);
    threads.emplace_back(completer, 31u);
    threads.emplace_back(canceller, 53u);
    threads.emplace_back(advancer);
    for (auto& thread : threads) {
        thread.join();
    }

    for (const RequestId id : ids) {
        port.Complete(id, Status::Ready);
    }
    REQUIRE(outcomes.load() == 2 * kPerSubmitter);
    REQUIRE(calls.InFlight() == 0);
}
}
