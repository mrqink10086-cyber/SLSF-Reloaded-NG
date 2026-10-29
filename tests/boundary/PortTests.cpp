#include <catch2/catch_test_macros.hpp>

#include "slsf/core/Types.h"
#include "slsf/interfaces/EnginePort.h"
#include "slsf/interfaces/ProviderPort.h"
#include "slsf/interfaces/VmPort.h"
#include "slsf/interfaces/WakeSource.h"

#include <cstdint>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

namespace {

class FakeVmPort final : public slsf::IVmPort {
public:
    slsf::RequestId Submit(slsf::VmRequest request, slsf::VmCompletion completion) override {
        lastRequest = std::move(request);
        slsf::VmResult result;
        result.epoch = lastRequest.epoch;
        result.status = slsf::Status::Ready;
        slsf::VmValue value;
        value.type = slsf::VmValueType::String;
        value.stringValue = "ok";
        result.values.push_back(std::move(value));
        completion(std::move(result));
        return slsf::RequestId{7};
    }

    slsf::VmRequest lastRequest;
};

class FakeEnginePort final : public slsf::IEnginePort {
public:
    slsf::RequestId Post(slsf::EngineRequest request) override {
        lastRequest = std::move(request);
        return slsf::RequestId{9};
    }

    slsf::EngineRequest lastRequest;
};

class FakeWakeSource final : public slsf::IWakeSource {
public:
    bool Start(slsf::WakeCallback callback) override {
        callback_ = std::move(callback);
        started = true;
        return true;
    }

    void Stop() override {
        callback_ = nullptr;
        started = false;
    }

    bool started{};
    slsf::WakeCallback callback_;
};

class FakeProvider final : public slsf::IProviderPort {
public:
    slsf::Result<slsf::ProviderFactState> Snapshot() const override {
        slsf::ProviderFactState state;
        state.flags.push_back(slsf::ProviderFlagFact{"mod.esp", slsf::FameCategory::Whore, true});
        return state;
    }

    slsf::Result<void> Submit(const slsf::ProviderFactTransition& transition) override {
        submitted = transition;
        return {};
    }

    slsf::ProviderFactTransition submitted;
};

class FakeRandom final : public slsf::IRandom {
public:
    std::int32_t UniformInt(std::int32_t lo, std::int32_t hi) override {
        lastLo = lo;
        lastHi = hi;
        return static_cast<std::int32_t>(std::midpoint(lo, hi));
    }

    std::int32_t lastLo{};
    std::int32_t lastHi{};
};

} // namespace

TEST_CASE("vm completion keeps the submitted epoch", "[ports]") {
    FakeVmPort vm;
    const slsf::Epoch epoch{11, 12, 13};

    slsf::VmRequest request;
    request.epoch = epoch;
    request.script = "SLSF_Reloaded_DataManager";
    request.function = "GetFameValue";

    slsf::Epoch seenEpoch;
    std::string seenValue;
    const auto requestId = vm.Submit(std::move(request), [&](slsf::VmResult result) {
        seenEpoch = result.epoch;
        REQUIRE(result.status == slsf::Status::Ready);
        REQUIRE(result.values.size() == 1);
        seenValue = result.values.front().stringValue;
    });

    REQUIRE(requestId.value == 7);
    REQUIRE(vm.lastRequest.epoch == epoch);
    REQUIRE(seenEpoch == epoch);
    REQUIRE(seenValue == "ok");
}

TEST_CASE("engine and wake ports separate request ids from revisions", "[ports]") {
    FakeEnginePort engine;
    slsf::EngineRequest request;
    request.epoch = slsf::Epoch{1, 2, 3};
    request.kind = slsf::EngineRequestKind::Notify;
    request.actor = slsf::StableFormKey{"Skyrim.esm", 0x14};
    const auto engineId = engine.Post(request);

    REQUIRE(engineId.value == 9);
    REQUIRE(engine.lastRequest.kind == slsf::EngineRequestKind::Notify);
    REQUIRE(engine.lastRequest.actor.localFormId == 0x14);

    FakeWakeSource wake;
    slsf::GameTime seenTime;
    REQUIRE(wake.Start([&](slsf::GameTime now) { seenTime = now; }));
    REQUIRE(wake.started);
    wake.callback_(slsf::GameTime{2.5});
    REQUIRE(seenTime.AsDays() == 2.5);
    wake.Stop();
    REQUIRE_FALSE(wake.started);
    REQUIRE_FALSE(static_cast<bool>(wake.callback_));
}

TEST_CASE("provider ports do not expose third-party containers", "[ports]") {
    FakeProvider provider;
    const auto snapshot = provider.Snapshot();
    REQUIRE(snapshot.has_value());
    REQUIRE(snapshot->flags.size() == 1);
    REQUIRE(snapshot->flags.front().category == slsf::FameCategory::Whore);

    slsf::ProviderFactTransition transition;
    transition.wipeOnce.push_back(slsf::FameCategory::Cuck);
    const auto submitted = provider.Submit(transition);
    REQUIRE(submitted.has_value());
    REQUIRE(provider.submitted.wipeOnce.size() == 1);
}

TEST_CASE("random interface is deterministic for tests", "[ports]") {
    FakeRandom random;
    REQUIRE(random.UniformInt(10, 20) == 15);
    REQUIRE(random.lastLo == 10);
    REQUIRE(random.lastHi == 20);
}
