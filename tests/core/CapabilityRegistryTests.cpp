#include <catch2/catch_test_macros.hpp>

#include "slsf/integrations/CapabilityRegistry.h"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

using slsf::integrations::CapabilityProbeResult;
using slsf::integrations::CapabilityRecord;
using slsf::integrations::CapabilityRegistry;
using slsf::integrations::CapabilityRequirement;
using slsf::integrations::CapabilityState;

CapabilityRequirement Requirement() {
    return CapabilityRequirement{
        .id = "ddng",
        .displayName = "Devious Devices NG",
        .business = "equipment visibility",
        .minimumVersion = 2,
        .maximumVersion = 2,
        .requiredApiVersion = 1,
        .minimumStructSize = 16,
        .requiredFunctionCount = 1,
    };
}

CapabilityProbeResult GoodProbe() {
    static int function{};
    CapabilityProbeResult probe;
    probe.loaded = true;
    probe.version = 2;
    probe.apiVersion = 1;
    probe.structSize = 16;
    probe.functions.push_back(&function);
    return probe;
}

} // namespace

TEST_CASE("capability requirements are validated and locked on first registration") {
    CapabilityRegistry registry([](const CapabilityRequirement&) { return GoodProbe(); });
    CHECK_FALSE(registry.Require(CapabilityRequirement{}));
    CHECK_FALSE(registry.Require(CapabilityRequirement{
        .id = "bad",
        .displayName = "Bad",
        .business = "business",
        .minimumVersion = 3,
        .maximumVersion = 2,
    }));
    REQUIRE(registry.Require(Requirement()));
    CHECK(registry.Require(Requirement()));
    auto changed = Requirement();
    changed.minimumVersion = 1;
    CHECK_FALSE(registry.Require(changed));
    CHECK(registry.Size() == 1);
    CHECK(registry.Find("missing") == std::nullopt);
    CHECK(registry.Ensure("missing") == CapabilityState::Incompatible);
}

TEST_CASE("late capability completes after a waiting query") {
    std::atomic<int> calls{0};
    auto available = std::make_shared<bool>(false);
    CapabilityRegistry registry([&](const CapabilityRequirement&) {
        ++calls;
        if (!*available) {
            CapabilityProbeResult pending;
            pending.pending = true;
            return pending;
        }
        return GoodProbe();
    });
    REQUIRE(registry.Require(Requirement()));
    CHECK(registry.Ensure("ddng") == CapabilityState::Waiting);
    auto waiting = registry.Find("ddng");
    REQUIRE(waiting.has_value());
    CHECK(waiting->reason == "module not loaded yet");
    CHECK(waiting->queryCount == 1);
    CHECK_FALSE(registry.IsAvailable("ddng"));

    *available = true;
    CHECK(registry.Ensure("ddng") == CapabilityState::Available);
    CHECK(registry.IsAvailable("ddng"));
    CHECK(calls == 2);
    const auto record = registry.Find("ddng");
    REQUIRE(record.has_value());
    CHECK(record->version == 2);
    CHECK(record->apiVersion == 1);
    CHECK(record->structSize == 16);
    CHECK(record->functionCount == 1);
    CHECK(record->queryCount == 2);
    CHECK(record->requirement.business == "equipment visibility");
}

TEST_CASE("missing and incompatible capabilities are terminal and explain the lock") {
    std::atomic<int> missingCalls{0};
    CapabilityRegistry missing([&](const CapabilityRequirement&) {
        ++missingCalls;
        return CapabilityProbeResult{};
    });
    REQUIRE(missing.Require(Requirement()));
    CHECK(missing.Ensure("ddng") == CapabilityState::Missing);
    CHECK(missing.Ensure("ddng") == CapabilityState::Missing);
    CHECK(missingCalls == 1);

    const auto setProbe = [](auto mutate) {
        auto probe = GoodProbe();
        mutate(probe);
        CapabilityRegistry registry([probe](const CapabilityRequirement&) { return probe; });
        REQUIRE(registry.Require(Requirement()));
        CHECK(registry.Ensure("ddng") == CapabilityState::Incompatible);
        const auto record = registry.Find("ddng");
        REQUIRE(record.has_value());
        CHECK_FALSE(record->reason.empty());
        return *record;
    };

    CHECK(setProbe([](CapabilityProbeResult& probe) { probe.version = 1; }).reason ==
          "version is older than the required minimum");
    CHECK(setProbe([](CapabilityProbeResult& probe) { probe.version = 3; }).reason ==
          "version is newer than the supported maximum");
    CHECK(setProbe([](CapabilityProbeResult& probe) { probe.apiVersion = 2; }).reason ==
          "API version mismatch");
    CHECK(setProbe([](CapabilityProbeResult& probe) { probe.structSize = 8; }).reason ==
          "published structure is smaller than the validated layout");
    CHECK(setProbe([](CapabilityProbeResult& probe) { probe.functions.clear(); }).reason ==
          "published function table is incomplete");
    CHECK(setProbe([](CapabilityProbeResult& probe) { probe.functions[0] = nullptr; }).reason ==
          "published function table contains a null entry");
}

TEST_CASE("duplicate concurrent requests share one provider query") {
    std::atomic<int> calls{0};
    CapabilityRegistry registry([&](const CapabilityRequirement&) {
        ++calls;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        return GoodProbe();
    });
    REQUIRE(registry.Require(Requirement()));

    std::vector<std::thread> threads;
    std::atomic<int> available{0};
    for (int index = 0; index < 8; ++index) {
        threads.emplace_back([&] {
            if (registry.Ensure("ddng") == CapabilityState::Available) {
                ++available;
            }
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }
    CHECK(available == 8);
    CHECK(calls == 1);
    CHECK(registry.Find("ddng")->queryCount == 1);
}

TEST_CASE("provider exceptions become incompatible without escaping Ensure") {
    CapabilityRegistry registry([](const CapabilityRequirement&) -> CapabilityProbeResult {
        throw std::runtime_error("provider crashed");
    });
    REQUIRE(registry.Require(Requirement()));
    CHECK(registry.Ensure("ddng") == CapabilityState::Incompatible);
    CHECK(registry.Find("ddng")->reason == "capability query threw");
}
