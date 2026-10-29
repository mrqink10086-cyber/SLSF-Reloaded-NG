#include <catch2/catch_test_macros.hpp>

#include "slsf/core/Commands.h"
#include "slsf/core/Configuration.h"
#include "slsf/core/Snapshots.h"
#include "slsf/core/Types.h"
#include "slsf/core/persistence/PersistentState.h"
#include "slsf/core/reputation/GainRules.h"
#include "slsf/core/reputation/ReputationState.h"
#include "slsf/core/reputation/Settlement.h"
#include "slsf/engine/EventBridge.h"
#include "slsf/persistence/StableKeys.h"
#include "slsf/runtime/Lifecycle.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using slsf::ConfigurationData;
using slsf::ConfigurationRevision;
using slsf::ConfigurationSnapshot;
using slsf::ConfigurationUpdateResult;
using slsf::ConfigurationUpdateStatus;
using slsf::ExplicitCommand;
using slsf::ExplicitCommandKind;
using slsf::FameCategory;
using slsf::Lifecycle;
using slsf::LocationId;
using slsf::LocationRecord;
using slsf::PersistentState;
using slsf::RegionDefinitions;
using slsf::RegionId;
using slsf::RegionSnapshot;
using slsf::SessionContext;
using slsf::StableRegionKey;
using slsf::StaticWorld;
using slsf::Status;

StableRegionKey Key(std::uint32_t id) {
    return StableRegionKey{"lifecycle.esp", id};
}

class TestConfiguration final : public slsf::IConfigurationSource {
public:
    TestConfiguration() {
        auto data = std::make_shared<ConfigurationData>();
        data->parameters = slsf::LegacyDefaultParameters();
        snapshot_.revision = ConfigurationRevision{1};
        snapshot_.data = std::move(data);
        snapshot_.lastUpdate = ConfigurationUpdateResult{
            .status = ConfigurationUpdateStatus::Applied,
            .revision = snapshot_.revision,
            .message = "test configuration",
        };
    }

    ConfigurationSnapshot Current() const override { return snapshot_; }

    ConfigurationUpdateResult Reload() override {
        ++reloads;
        auto update = snapshot_.lastUpdate;
        update.revision = snapshot_.revision;
        return update;
    }

    void Publish(const std::function<void(ConfigurationData&)>& mutate) {
        auto next = std::make_shared<ConfigurationData>(*snapshot_.data);
        mutate(*next);
        ++revision;
        snapshot_.revision = ConfigurationRevision{revision};
        snapshot_.data = std::move(next);
        snapshot_.lastUpdate = ConfigurationUpdateResult{
            .status = ConfigurationUpdateStatus::Applied,
            .revision = snapshot_.revision,
            .message = "published",
        };
    }

    std::uint64_t reloads{};

private:
    ConfigurationSnapshot snapshot_;
    std::uint64_t revision{1};
};

RegionSnapshot MakeWorld() {
    std::vector<LocationRecord> records;
    for (std::uint32_t id = 1; id <= 3; ++id) {
        records.push_back(LocationRecord{
            .formId = slsf::StableFormKey{"lifecycle.esp", id},
            .stableKey = Key(id),
            .displayName = "location " + std::to_string(id),
        });
    }
    auto index = slsf::BuildLocationIndex(records);
    REQUIRE(index.has_value());

    RegionDefinitions definitions;
    definitions.keys = {Key(1), Key(2), Key(3)};
    definitions.ownerRules.push_back(slsf::OwnerRule{Key(2), {Key(3)}});
    definitions.customOrder = definitions.keys;
    auto regions = slsf::BindRegions(*index, definitions);
    REQUIRE(regions.has_value());
    return std::move(*regions);
}

PersistentState EmptySave() {
    PersistentState state;
    state.codecVersion = slsf::kPersistentStateCodecVersion;
    return state;
}

PersistentState SaveWith(const StableRegionKey& key, double value) {
    PersistentState state;
    state.codecVersion = slsf::kPersistentStateCodecVersion;
    slsf::LocationPersistentState location;
    location.key = key;
    location.local[static_cast<std::size_t>(FameCategory::Whore)] = value;
    location.remote[static_cast<std::size_t>(FameCategory::Whore)] = value;
    state.locations.push_back(location);
    return state;
}

PersistentState SaveWithProvider(const StableRegionKey& key, double value,
                                 std::uint64_t registrationId) {
    auto state = SaveWith(key, value);
    state.contributions.push_back(slsf::ProviderContributionState{
        .registrationId = registrationId,
        .categories = slsf::CategoryMask::FromCategory(FameCategory::Whore),
    });
    return state;
}

ExplicitCommand SetCommand(std::uint32_t region, double value) {
    ExplicitCommand command;
    command.kind = ExplicitCommandKind::Set;
    command.target = RegionId{region};
    command.category = FameCategory::Whore;
    command.value = value;
    return command;
}

double Fame(const Lifecycle& lifecycle, std::uint32_t location) {
    slsf::QueryRequest request;
    request.kind = slsf::QueryRequest::Kind::Fame;
    request.location = LocationId{location};
    request.category = FameCategory::Whore;
    const auto result = lifecycle.Executor().Query(request);
    REQUIRE(result.has_value());
    REQUIRE_FALSE(result->fame.empty());
    return result->fame.front();
}

struct Fixture {
    Fixture() : world(MakeWorld()) {
        lifecycle = std::make_unique<Lifecycle>(StaticWorld{.regions = &world}, configuration);
    }

    RegionSnapshot world;
    TestConfiguration configuration;
    std::unique_ptr<Lifecycle> lifecycle;
};

} // namespace

TEST_CASE("new game stays closed until the complete bind is activated") {
    Fixture fixture;
    auto& lifecycle = *fixture.lifecycle;

    CHECK_FALSE(lifecycle.BusinessOpen());
    CHECK(lifecycle.Status().phase == slsf::LifecyclePhase::Detached);
    CHECK_FALSE(lifecycle.Capture().has_value());
    CHECK_FALSE(lifecycle.ActivatePrepared());

    const auto epochBefore = lifecycle.Status().epoch;
    REQUIRE(lifecycle.PrepareNewGame(SessionContext{.randomSeed = 42}));
    CHECK_FALSE(lifecycle.BusinessOpen());
    CHECK(lifecycle.Status().phase == slsf::LifecyclePhase::Preparing);
    CHECK(lifecycle.Status().epoch.session > epochBefore.session);
    CHECK(lifecycle.Status().restoredAccounts == 0);
    CHECK(lifecycle.Status().configurationRevision == ConfigurationRevision{1});

    REQUIRE(lifecycle.ActivatePrepared());
    REQUIRE(lifecycle.BusinessOpen());
    CHECK(lifecycle.Status().phase == slsf::LifecyclePhase::Active);
    CHECK(lifecycle.CurrentEpoch().session > epochBefore.session);
    const auto epoch = lifecycle.CurrentEpoch();
    CHECK(lifecycle.IsEpochCurrent(epoch));
    CHECK_FALSE(lifecycle.IsEpochCurrent(slsf::Epoch{epoch.session + 1, 0, 0}));

    const auto command = lifecycle.Executor().Execute(SetCommand(0, 12.5));
    CHECK(command.status == Status::Ready);
    CHECK(Fame(lifecycle, 0) == 12.5);
}

TEST_CASE("A then B then A restores each save's reputation and registration") {
    Fixture fixture;
    auto& lifecycle = *fixture.lifecycle;

    REQUIRE(lifecycle.PrepareNewGame(SessionContext{.randomSeed = 1}));
    REQUIRE(lifecycle.ActivatePrepared());
    const auto epochA = lifecycle.CurrentEpoch();
    REQUIRE(lifecycle.Executor().Execute(SetCommand(0, 10.0)).status == Status::Ready);
    REQUIRE(lifecycle.Executor().Execute(SetCommand(1, 20.0)).status == Status::Ready);

    auto saveA = lifecycle.Capture();
    REQUIRE(saveA.has_value());
    REQUIRE(saveA->locations.size() == 3);
    const auto aRevision = lifecycle.Status().epoch;

    lifecycle.ReturnToMenu();
    CHECK_FALSE(lifecycle.BusinessOpen());
    CHECK_FALSE(lifecycle.Executor().Execute(SetCommand(0, 1.0)).status == Status::Ready);

    REQUIRE(lifecycle.PrepareLoad(*saveA, SessionContext{.randomSeed = 2}));
    CHECK_FALSE(lifecycle.BusinessOpen());
    REQUIRE(lifecycle.ActivatePrepared());
    REQUIRE(lifecycle.BusinessOpen());
    CHECK(lifecycle.CurrentEpoch() != epochA);
    CHECK(lifecycle.CurrentEpoch().session > epochA.session);
    CHECK(Fame(lifecycle, 0) == 10.0);
    CHECK(Fame(lifecycle, 1) == 20.0);

    REQUIRE(lifecycle.Executor().Execute(SetCommand(0, 99.0)).status == Status::Ready);
    auto saveB = lifecycle.Capture();
    REQUIRE(saveB.has_value());
    const auto epochB = lifecycle.CurrentEpoch();

    lifecycle.ReturnToMenu();
    REQUIRE(lifecycle.PrepareLoad(*saveA, SessionContext{.randomSeed = 3}));
    REQUIRE(lifecycle.ActivatePrepared());
    CHECK(Fame(lifecycle, 0) == 10.0);
    CHECK(lifecycle.CurrentEpoch().session > epochB.session);

    lifecycle.ReturnToMenu();
    REQUIRE(lifecycle.PrepareLoad(*saveB, SessionContext{.randomSeed = 4}));
    REQUIRE(lifecycle.ActivatePrepared());
    CHECK(Fame(lifecycle, 0) == 99.0);
    CHECK(Fame(lifecycle, 1) == 20.0);
    CHECK(lifecycle.CurrentEpoch().session > aRevision.session);
}

TEST_CASE("provider registrations travel with each save and rebind per session") {
    Fixture fixture;
    auto& lifecycle = *fixture.lifecycle;
    const auto saveA = SaveWithProvider(Key(1), 4.0, 42);
    const auto saveB = SaveWithProvider(Key(1), 8.0, 77);

    REQUIRE(lifecycle.PrepareLoad(saveA, SessionContext{.randomSeed = 12}));
    REQUIRE(lifecycle.ActivatePrepared());
    auto capturedA = lifecycle.Capture();
    REQUIRE(capturedA.has_value());
    REQUIRE(capturedA->contributions.size() == 1);
    CHECK(capturedA->contributions.front().registrationId == 42);

    lifecycle.ReturnToMenu();
    REQUIRE(lifecycle.PrepareLoad(saveB, SessionContext{.randomSeed = 13}));
    REQUIRE(lifecycle.ActivatePrepared());
    auto capturedB = lifecycle.Capture();
    REQUIRE(capturedB.has_value());
    REQUIRE(capturedB->contributions.size() == 1);
    CHECK(capturedB->contributions.front().registrationId == 77);

    lifecycle.ReturnToMenu();
    REQUIRE(lifecycle.PrepareLoad(*capturedA, SessionContext{.randomSeed = 14}));
    REQUIRE(lifecycle.ActivatePrepared());
    auto capturedAgain = lifecycle.Capture();
    REQUIRE(capturedAgain.has_value());
    REQUIRE(capturedAgain->contributions.size() == 1);
    CHECK(capturedAgain->contributions.front().registrationId == 42);
}

TEST_CASE("missing regions keep the complete record and rebind in another session") {
    Fixture fixture;
    auto& lifecycle = *fixture.lifecycle;
    const auto missingKey = StableRegionKey{"absent.esp", 0xABC};
    auto save = SaveWith(missingKey, 33.5);
    save.locations.front().anonymity = 9;
    save.locations.front().auxiliaryState = {1, 2, 3, 4};

    REQUIRE(lifecycle.PrepareLoad(save, SessionContext{.randomSeed = 7}));
    CHECK_FALSE(lifecycle.BusinessOpen());
    CHECK(lifecycle.Status().unresolvedAccounts == 1);
    CHECK(lifecycle.Status().restoredAccounts == 0);
    REQUIRE(lifecycle.ActivatePrepared());

    auto retained = lifecycle.Capture();
    REQUIRE(retained.has_value());
    const auto found = std::find_if(retained->locations.begin(), retained->locations.end(),
                                    [&](const auto& location) {
                                        return location.key == missingKey;
                                    });
    REQUIRE(found != retained->locations.end());
    CHECK(*found == save.locations.front());

    // The same process-static table is reused; adding the region to a new
    // composition root is how a later load-order makes the record resolvable.
    RegionSnapshot larger = fixture.world;
    larger.regions.push_back(missingKey);
    std::sort(larger.regions.begin(), larger.regions.end());
    larger.locationOwners.push_back(slsf::RegionId{0});
    larger.customOrder.push_back(missingKey);
    std::sort(larger.customOrder.begin(), larger.customOrder.end());

    TestConfiguration configuration;
    Lifecycle later(StaticWorld{.regions = &larger}, configuration);
    REQUIRE(later.PrepareLoad(*retained, SessionContext{.randomSeed = 8}));
    CHECK(later.Status().unresolvedAccounts == 0);
    CHECK(later.Status().restoredAccounts >= 1);
    REQUIRE(later.ActivatePrepared());
    CHECK(later.BusinessOpen());
}

TEST_CASE("failed load never opens the switch and the next session recovers") {
    Fixture fixture;
    auto& lifecycle = *fixture.lifecycle;
    REQUIRE(lifecycle.PrepareNewGame(SessionContext{.randomSeed = 1}));
    REQUIRE(lifecycle.ActivatePrepared());
    REQUIRE(lifecycle.Executor().Execute(SetCommand(0, 5.0)).status == Status::Ready);

    auto badSchema = EmptySave();
    badSchema.schemaVersion = 99;
    CHECK_FALSE(lifecycle.PrepareLoad(badSchema, SessionContext{.randomSeed = 2}));
    CHECK_FALSE(lifecycle.BusinessOpen());
    CHECK(lifecycle.Status().phase == slsf::LifecyclePhase::Failed);
    CHECK_FALSE(lifecycle.Status().message.empty());

    auto duplicate = SaveWith(Key(1), 1.0);
    duplicate.locations.push_back(duplicate.locations.front());
    CHECK_FALSE(lifecycle.PrepareLoad(duplicate, SessionContext{.randomSeed = 3}));
    CHECK_FALSE(lifecycle.BusinessOpen());

    lifecycle.RejectPreparation("truncated record");
    CHECK_FALSE(lifecycle.BusinessOpen());
    CHECK(lifecycle.Status().phase == slsf::LifecyclePhase::Failed);

    REQUIRE(lifecycle.PrepareNewGame(SessionContext{.randomSeed = 4}));
    REQUIRE(lifecycle.ActivatePrepared());
    CHECK(lifecycle.BusinessOpen());
    CHECK(Fame(lifecycle, 0) == 0.0);
}

TEST_CASE("each new session reloads and adopts the current cross-save configuration") {
    Fixture fixture;
    auto& lifecycle = *fixture.lifecycle;
    REQUIRE(lifecycle.PrepareNewGame(SessionContext{.randomSeed = 1}));
    REQUIRE(lifecycle.ActivatePrepared());
    const auto reloadsAfterFirst = fixture.configuration.reloads;
    CHECK(reloadsAfterFirst >= 1);

    fixture.configuration.Publish([](ConfigurationData& data) {
        data.parameters.decayPerDay = 3.5;
        data.categories.multipliers[static_cast<std::size_t>(FameCategory::Whore)] = 2.0;
        data.connections.push_back(slsf::RegionConnectionSetting{Key(1), Key(2), true});
    });
    lifecycle.ReturnToMenu();
    REQUIRE(lifecycle.PrepareNewGame(SessionContext{.randomSeed = 2}));
    CHECK(fixture.configuration.reloads > reloadsAfterFirst);
    CHECK(lifecycle.Status().configurationRevision == ConfigurationRevision{2});
    CHECK(lifecycle.Status().droppedConnectionEdges == 0);
    REQUIRE(lifecycle.ActivatePrepared());

    // New JSON connections are compiled into this session's graph; an
    // unknown endpoint is counted instead of failing the session.
    CHECK(lifecycle.Status().droppedConnectionEdges == 0);

    // A save carries per-save business state, never the old JSON revision.
    auto captured = lifecycle.Capture();
    REQUIRE(captured.has_value());
    CHECK(captured->settings.empty());
    CHECK(captured->graphEdits.empty());

    fixture.configuration.Publish([](ConfigurationData& data) {
        data.parameters.decayPerDay = 4.5;
        data.connections.clear();
        data.connections.push_back(slsf::RegionConnectionSetting{
            StableRegionKey{"unknown.esp", 1}, Key(2), true});
    });
    lifecycle.ReturnToMenu();
    REQUIRE(lifecycle.PrepareLoad(*captured, SessionContext{.randomSeed = 3}));
    CHECK(lifecycle.Status().configurationRevision == ConfigurationRevision{3});
    CHECK(lifecycle.Status().droppedConnectionEdges == 1);
    REQUIRE(lifecycle.ActivatePrepared());
}

TEST_CASE("async messages are committed only while their epoch is current") {
    Fixture fixture;
    auto& lifecycle = *fixture.lifecycle;
    std::vector<std::string> handled;
    slsf::engine::EventBridge bridge(lifecycle, [&](const slsf::engine::AsyncMessage& message) {
        handled.push_back(message.topic + ":" + message.payload);
    });

    CHECK_FALSE(bridge.Enqueue("closed", "never"));
    CHECK(bridge.Stats().rejectedClosed == 1);

    REQUIRE(lifecycle.PrepareNewGame(SessionContext{.randomSeed = 10}));
    REQUIRE(lifecycle.ActivatePrepared());
    const auto epochA = lifecycle.CurrentEpoch();
    REQUIRE(bridge.Enqueue("a", "one"));
    REQUIRE(bridge.Enqueue("a", "two"));
    CHECK(bridge.Pending() == 2);

    lifecycle.ReturnToMenu();
    REQUIRE(lifecycle.PrepareNewGame(SessionContext{.randomSeed = 11}));
    REQUIRE(lifecycle.ActivatePrepared());
    const auto epochB = lifecycle.CurrentEpoch();
    REQUIRE(epochB != epochA);
    REQUIRE(bridge.Enqueue("b", "current"));

    CHECK(bridge.Flush() == 1);
    REQUIRE(handled.size() == 1);
    CHECK(handled.front() == "b:current");
    const auto stats = bridge.Stats();
    CHECK(stats.enqueued == 3);
    CHECK(stats.committed == 1);
    CHECK(stats.droppedStale == 2);
    CHECK(stats.rejectedClosed == 1);
    CHECK(bridge.Pending() == 0);
}

TEST_CASE("one flush drains only the batch it started with") {
    Fixture fixture;
    auto& lifecycle = *fixture.lifecycle;
    std::vector<std::string> handled;
    slsf::engine::EventBridge bridge(lifecycle, [&](const slsf::engine::AsyncMessage& message) {
        handled.push_back(message.topic);
        if (handled.size() < 3) {
            REQUIRE(bridge.Enqueue("follow", "next"));
        }
    });

    REQUIRE(lifecycle.PrepareNewGame(SessionContext{.randomSeed = 20}));
    REQUIRE(lifecycle.ActivatePrepared());
    REQUIRE(bridge.Enqueue("seed", "first"));

    CHECK(bridge.Flush() == 1);
    CHECK(handled.size() == 1);
    CHECK(bridge.Pending() == 1);
    CHECK(bridge.Flush() == 1);
    CHECK(handled.size() == 2);
    CHECK(bridge.Pending() == 1);
    CHECK(bridge.Flush() == 1);
    CHECK(handled.size() == 3);
    CHECK(bridge.Pending() == 0);
}

TEST_CASE("typed configuration projects onto legacy settlement keys") {
    ConfigurationData data;
    data.features.reduceFameAtNight = false;
    data.categories.multipliers[static_cast<std::size_t>(FameCategory::Whore)] = 2.5;
    data.timing.nightStart = 19.5;
    data.timing.nightEnd = 5.25;
    data.timing.decayPeriodDays = 0.75;

    const auto rules = slsf::MakeRuleConfiguration(data);
    CHECK(rules.Get(slsf::kGainConfigKeyUseGlobal, 1.0) == 0.0);
    CHECK(rules.Get(slsf::kGainConfigKeyNightReduction, 1.0) == 0.0);
    CHECK(rules.Get(slsf::kGainConfigKeyNightStart, 0.0) == 19.5);
    CHECK(rules.Get(slsf::kGainConfigKeyNightEnd, 0.0) == 5.25);
    CHECK(rules.Get(slsf::kGainConfigKeyDecayTimeNeeded, 0.0) == 36.0);
    std::string categoryKey{slsf::kGainConfigKeyCategoryMultiplierPrefix};
    categoryKey += std::to_string(static_cast<unsigned>(FameCategory::Whore));
    CHECK(rules.Get(categoryKey, 0.0) == 2.5);
}

TEST_CASE("host load failure discards a successfully prepared save", "[fix05-06]") {
    Fixture fixture;
    auto& lifecycle = *fixture.lifecycle;
    REQUIRE(lifecycle.PrepareLoad(SaveWith(Key(1), 37.5), SessionContext{}));
    REQUIRE(lifecycle.Status().phase == slsf::LifecyclePhase::Preparing);
    CHECK_FALSE(lifecycle.ActivatePrepared(false));
    CHECK(lifecycle.Status().phase == slsf::LifecyclePhase::Failed);
    CHECK(lifecycle.Status().message == "host load failed; prepared session discarded");
    CHECK_FALSE(lifecycle.BusinessOpen());
    CHECK_FALSE(lifecycle.Executor().Attached());
    CHECK_FALSE(lifecycle.Capture());
    CHECK_FALSE(lifecycle.ActivatePrepared());
    REQUIRE(lifecycle.PrepareLoad(SaveWith(Key(1), 12), SessionContext{}));
    REQUIRE(lifecycle.ActivatePrepared(true));
    CHECK(Fame(lifecycle, 0) == 12);
}

TEST_CASE("repeated activation closes an already attached session", "[fix05-06]") {
    Fixture fixture;
    auto& lifecycle = *fixture.lifecycle;
    REQUIRE(lifecycle.PrepareNewGame(SessionContext{}));
    REQUIRE(lifecycle.ActivatePrepared());
    bool aborted = false;
    auto flow = lifecycle.Executor().SpawnFlow([&]() -> slsf::Dispatcher::Flow {
        aborted = !(co_await slsf::Dispatcher::Delay{
            lifecycle.Executor(), 100, slsf::time::ClockDomain::Real});
    });
    CHECK_FALSE(lifecycle.ActivatePrepared());
    CHECK_FALSE(lifecycle.BusinessOpen());
    CHECK_FALSE(lifecycle.Executor().Attached());
    CHECK(lifecycle.Executor().Timers().PendingCount() == 0);
    CHECK(aborted);
    CHECK(flow.Done());
}

TEST_CASE("historical light keys restore and recapture canonical local IDs", "[fix05-06]") {
    Fixture fixture;
    auto save = SaveWith(Key(0x012001), 37.5);
    SECTION("v1 payload") { save.codecVersion = slsf::kLegacyPersistentStateCodecVersion; }
    SECTION("v2 payload") {}
    slsf::persistence::NormalizeLightPluginKeys(save, [](std::string_view plugin) {
        return plugin == "lifecycle.esp";
    });
    REQUIRE(save.locations.front().key == Key(1));
    REQUIRE(fixture.lifecycle->PrepareLoad(save, SessionContext{}));
    REQUIRE(fixture.lifecycle->ActivatePrepared());
    CHECK(fixture.lifecycle->Status().restoredAccounts == 1);
    CHECK(fixture.lifecycle->Status().unresolvedAccounts == 0);
    CHECK(Fame(*fixture.lifecycle, 0) == 37.5);
    auto captured = fixture.lifecycle->Capture();
    REQUIRE(captured);
    CHECK(captured->locations.front().key == Key(1));
    CHECK(captured->locations.front().local == save.locations.front().local);
}

TEST_CASE("light key migration preserves full and unknown plugins and rejects collisions", "[fix05-06]") {
    auto save = SaveWith(Key(0x012001), 37.5);
    save.locations.push_back(SaveWith({"full.esm", 0x123ABC}, 20).locations.front());
    save.locations.push_back(SaveWith({"absent.esl", 0x037ABC}, 10).locations.front());
    save.customRegions.push_back({"custom", Key(0x012001)});
    save.graphEdits.push_back({Key(0x012001), {"full.esm", 0x123ABC}, 0.6, true});
    const auto isLight = [](std::string_view plugin) { return plugin == "lifecycle.esp"; };
    slsf::persistence::NormalizeLightPluginKeys(save, isLight);
    CHECK(save.locations[0].key == Key(1));
    CHECK(save.locations[1].key.localFormId == 0x123ABC);
    CHECK(save.locations[2].key.localFormId == 0x037ABC);
    CHECK(save.customRegions.front().key == Key(1));
    CHECK(save.graphEdits.front().from == Key(1));
    CHECK(save.graphEdits.front().to.localFormId == 0x123ABC);
    const auto once = save;
    slsf::persistence::NormalizeLightPluginKeys(save, isLight);
    CHECK(save == once);

    Fixture fixture;
    auto collision = SaveWith(Key(0x012001), 37.5);
    collision.locations.push_back(SaveWith(Key(1), 20).locations.front());
    slsf::persistence::NormalizeLightPluginKeys(collision, isLight);
    CHECK_FALSE(fixture.lifecycle->PrepareLoad(collision, SessionContext{}));
    CHECK_FALSE(fixture.lifecycle->BusinessOpen());
    CHECK_FALSE(fixture.lifecycle->Executor().Attached());
}
