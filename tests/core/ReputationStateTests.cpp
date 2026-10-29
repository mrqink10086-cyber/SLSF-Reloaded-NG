#include <catch2/catch_test_macros.hpp>

#include "slsf/core/Commands.h"
#include "slsf/core/Configuration.h"
#include "slsf/core/Snapshots.h"
#include "slsf/core/Types.h"
#include "slsf/core/reputation/ReputationState.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace {

slsf::State MakeState(std::size_t locations) {
    slsf::State state;
    state.accounts.resize(locations);
    state.accountKeys.resize(locations);
    state.locationAccounts.resize(locations);
    for (std::size_t i = 0; i < locations; ++i) {
        state.accountKeys[i] = slsf::StableRegionKey{"location.esp",
                                                     static_cast<std::uint32_t>(i + 1)};
        state.locationAccounts[i] = slsf::RegionId(static_cast<std::uint32_t>(i));
    }
    return state;
}

slsf::ExplicitCommand MakeCommand(slsf::ExplicitCommandKind kind, std::uint32_t location,
                                  slsf::FameCategory category, double value = 0.0) {
    slsf::ExplicitCommand command;
    command.kind = kind;
    command.target = slsf::RegionId(location);
    command.category = category;
    command.value = value;
    return command;
}

slsf::RegionId Account(const slsf::State& state, std::uint32_t location) {
    return state.locationAccounts[location];
}

double At(const slsf::State& state, std::uint32_t location, slsf::FameCategory category) {
    const auto account = Account(state, location);
    return state.accounts[account.value].fame[category];
}

slsf::ConfigurationSnapshot Config() {
    auto data = std::make_shared<slsf::ConfigurationData>();
    data->relationships.forbiddenByFriend =
        slsf::CategoryMask::FromCategory(slsf::FameCategory::Whore);
    data->relationships.forbiddenByLover =
        slsf::CategoryMask::FromCategory(slsf::FameCategory::Cuck);
    return slsf::ConfigurationSnapshot{slsf::ConfigurationRevision{7}, std::move(data), {}};
}

slsf::RegionSnapshot Regions(std::vector<slsf::StableRegionKey> keys,
                             std::vector<slsf::RegionId> owners) {
    slsf::RegionSnapshot snapshot;
    snapshot.regions = std::move(keys);
    snapshot.locationOwners = std::move(owners);
    return snapshot;
}

}

TEST_CASE("explicit numeric writes preserve exact values", "[state]") {
    auto state = MakeState(2);
    const auto category = slsf::FameCategory::Whore;
    const auto account = Account(state, 0);
    slsf::RuleContext context;

    const auto set = [&](double value) {
        const auto result = slsf::ApplyExplicit(
            state, MakeCommand(slsf::ExplicitCommandKind::Set, 0, category, value), context);
        REQUIRE(result.status == slsf::Status::Ready);
        REQUIRE(state.accounts[account.value].local[category] == value);
        REQUIRE(At(state, 0, category) == value);
        const auto published = slsf::MakePublishedState(state);
        REQUIRE(published.fame[slsf::ReputationSlotIndex(account, category)] == value);
    };

    set(-1.0);
    state.accounts[account.value].remote[category] = 80.0;
    set(20.0);
    REQUIRE(state.accounts[account.value].remote[category] == 20.0);

    state.accounts[account.value].remote[category] = 80.0;
    set(200.0);
    REQUIRE(state.accounts[account.value].remote[category] == 80.0);

    set(16777217.0);
    set(static_cast<double>(std::numeric_limits<std::int32_t>::min()));
    set(static_cast<double>(std::numeric_limits<std::int32_t>::max()));
}

TEST_CASE("published snapshot ties mapping values and configuration to one revision",
          "[state][publish]") {
    auto state = MakeState(2);
    auto& first = state.accounts[0];
    first.local[slsf::FameCategory::Whore] = 12.5;
    first.remote[slsf::FameCategory::Whore] = 9.0;
    slsf::RecomputeAccount(first);
    auto& second = state.accounts[1];
    second.local[slsf::FameCategory::Cuck] = 7.0;
    second.flags[static_cast<std::size_t>(slsf::FameCategory::Cuck)] = 1;
    slsf::RecomputeAccount(second);
    state.locationAccounts = {slsf::RegionId(1), slsf::RegionId(0), slsf::RegionId(0)};
    state.revision = slsf::Revision(41);
    state.epoch = slsf::Epoch(3);
    state.configurationRevision = 19;

    const auto published = slsf::MakePublishedState(state);
    REQUIRE(published.revision == slsf::Revision(41));
    REQUIRE(published.epoch == slsf::Epoch(3));
    REQUIRE(published.configurationRevision == 19);
    REQUIRE(published.regionIds == std::vector<slsf::RegionId>{slsf::RegionId(0), slsf::RegionId(1)});
    REQUIRE(published.locationAccounts ==
            std::vector<slsf::RegionId>{slsf::RegionId(1), slsf::RegionId(0), slsf::RegionId(0)});
    REQUIRE(published.fame[slsf::ReputationSlotIndex(slsf::RegionId(0), slsf::FameCategory::Whore)] ==
            12.5);
    REQUIRE(published.flags[second.flags.size() + static_cast<std::size_t>(slsf::FameCategory::Cuck)] ==
            1);

    state.accounts[0].local[slsf::FameCategory::Whore] = 99.0;
    state.locationAccounts[0] = slsf::RegionId(0);
    state.revision = slsf::Revision(42);
    state.configurationRevision = 20;

    REQUIRE(published.fame[slsf::ReputationSlotIndex(slsf::RegionId(0), slsf::FameCategory::Whore)] ==
            12.5);
    REQUIRE(published.locationAccounts[0] == slsf::RegionId(1));
    REQUIRE(published.revision == slsf::Revision(41));
    REQUIRE(published.configurationRevision == 19);
}

TEST_CASE("explicit clear zeroes local remote and published", "[state]") {
    auto state = MakeState(1);
    const auto category = slsf::FameCategory::Slut;
    const auto account = Account(state, 0);
    auto& row = state.accounts[account.value];
    row.local[category] = 40.0;
    row.remote[category] = 30.0;
    slsf::RuleContext context;

    const auto result = slsf::ApplyExplicit(
        state, MakeCommand(slsf::ExplicitCommandKind::Clear, 0, category), context);
    REQUIRE(result.status == slsf::Status::Ready);
    REQUIRE(row.local[category] == 0.0);
    REQUIRE(row.remote[category] == 0.0);
    REQUIRE(row.fame[category] == 0.0);
    REQUIRE_FALSE(row.positiveMask.Test(category));
}

TEST_CASE("explicit clear all and category wipe follow legacy scope", "[state]") {
    auto state = MakeState(3);
    slsf::RuleContext context;
    const auto populate = [&] {
        for (std::uint32_t location = 0; location < 3; ++location) {
            REQUIRE(slsf::ApplyExplicit(
                        state,
                        MakeCommand(slsf::ExplicitCommandKind::Set, location,
                                    slsf::FameCategory::Nasty, 10.0 + location),
                        context)
                        .status == slsf::Status::Ready);
            REQUIRE(slsf::ApplyExplicit(
                        state,
                        MakeCommand(slsf::ExplicitCommandKind::Set, location,
                                    slsf::FameCategory::Cuck, 20.0 + location),
                        context)
                        .status == slsf::Status::Ready);
            REQUIRE(slsf::ApplyExplicit(
                        state,
                        MakeCommand(slsf::ExplicitCommandKind::SetFlag, location,
                                    slsf::FameCategory::Nasty),
                        context)
                        .status == slsf::Status::Ready);
        }
    };
    populate();

    for (auto& row : state.accounts) {
        row.protectionUntilDay.fill(9.0);
    }
    slsf::ExplicitCommand clearAll;
    clearAll.kind = slsf::ExplicitCommandKind::ClearAll;
    REQUIRE(slsf::ApplyExplicit(state, clearAll, context).status == slsf::Status::Ready);
    for (const auto& row : state.accounts) {
        for (const double value : row.local.values) {
            REQUIRE(value == 0.0);
        }
        for (const double value : row.remote.values) {
            REQUIRE(value == 0.0);
        }
        for (const double value : row.fame.values) {
            REQUIRE(value == 0.0);
        }
        for (const double deadline : row.protectionUntilDay) {
            REQUIRE(deadline == 0.0);
        }
    }

    populate();
    slsf::ExplicitCommand wipe;
    wipe.kind = slsf::ExplicitCommandKind::Wipe;
    wipe.category = slsf::FameCategory::Nasty;
    REQUIRE(slsf::ApplyExplicit(state, wipe, context).status == slsf::Status::Ready);
    for (std::uint32_t location = 0; location < 3; ++location) {
        const auto account = Account(state, location);
        const auto& row = state.accounts[account.value];
        REQUIRE(row.local[slsf::FameCategory::Nasty] == 0.0);
        REQUIRE(row.remote[slsf::FameCategory::Nasty] == 0.0);
        REQUIRE(row.fame[slsf::FameCategory::Nasty] == 0.0);
        REQUIRE(row.local[slsf::FameCategory::Cuck] == 20.0 + location);
        REQUIRE(row.flags[static_cast<std::size_t>(slsf::FameCategory::Nasty)] == 1);
    }

    wipe.category = slsf::FameCategory::Count;
    REQUIRE(slsf::ApplyExplicit(state, wipe, context).status == slsf::Status::Invalid);
}

TEST_CASE("explicit commands reject invalid input without writing", "[state]") {
    auto state = MakeState(1);
    const auto category = slsf::FameCategory::Oral;
    const auto account = Account(state, 0);
    auto& row = state.accounts[account.value];
    row.local[category] = 12.0;
    row.remote[category] = 7.0;
    slsf::RecomputeAccount(row);
    slsf::RuleContext context;

    auto invalidLocation = MakeCommand(slsf::ExplicitCommandKind::Set, 9, category, 5.0);
    invalidLocation.target = slsf::RegionId{};
    REQUIRE(slsf::ApplyExplicit(state, invalidLocation, context).status == slsf::Status::Invalid);

    const auto invalidCategory =
        MakeCommand(slsf::ExplicitCommandKind::Set, 0, slsf::FameCategory::Count, 5.0);
    REQUIRE(slsf::ApplyExplicit(state, invalidCategory, context).status == slsf::Status::Invalid);

    const auto unallocated =
        MakeCommand(slsf::ExplicitCommandKind::Set, 1, category, 5.0);
    REQUIRE(slsf::ApplyExplicit(state, unallocated, context).status == slsf::Status::Invalid);

    const auto nan = MakeCommand(slsf::ExplicitCommandKind::Set, 0, category,
                                 std::numeric_limits<double>::quiet_NaN());
    REQUIRE(slsf::ApplyExplicit(state, nan, context).status == slsf::Status::Invalid);

    const auto inf = MakeCommand(slsf::ExplicitCommandKind::Set, 0, category,
                                 std::numeric_limits<double>::infinity());
    REQUIRE(slsf::ApplyExplicit(state, inf, context).status == slsf::Status::Invalid);

    auto unknownKind = MakeCommand(static_cast<slsf::ExplicitCommandKind>(99), 0, category, 5.0);
    REQUIRE(slsf::ApplyExplicit(state, unknownKind, context).status == slsf::Status::Invalid);

    REQUIRE(row.local[category] == 12.0);
    REQUIRE(row.remote[category] == 7.0);
    REQUIRE(row.fame[category] == 12.0);
    REQUIRE(state.revision.value == 0);
}

TEST_CASE("flags set and clear are independent", "[state]") {
    auto state = MakeState(1);
    slsf::RuleContext context;
    const auto category = slsf::FameCategory::CumDump;
    const auto account = Account(state, 0);
    auto& row = state.accounts[account.value];

    REQUIRE(slsf::ApplyExplicit(
                state, MakeCommand(slsf::ExplicitCommandKind::SetFlag, 0, category), context)
                .status == slsf::Status::Ready);
    REQUIRE(row.flags[static_cast<std::size_t>(category)] == 1);
    REQUIRE(row.local[category] == 0.0);
    REQUIRE(row.remote[category] == 0.0);
    REQUIRE(At(state, 0, category) == 0.0);
    REQUIRE(state.derived.flagged.Test(category));

    REQUIRE(slsf::ApplyExplicit(
                state, MakeCommand(slsf::ExplicitCommandKind::ClearFlag, 0, category), context)
                .status == slsf::Status::Ready);
    REQUIRE(row.flags[static_cast<std::size_t>(category)] == 0);
    REQUIRE_FALSE(state.derived.flagged.Test(category));
}

TEST_CASE("category summaries follow the last region containing each value or flag",
          "[state][regression]") {
    auto state = MakeState(3);
    slsf::RuleContext context;
    const auto category = slsf::FameCategory::Nasty;
    const auto apply = [&](slsf::ExplicitCommandKind kind, std::uint32_t region, double value = 0) {
        REQUIRE(slsf::ApplyExplicit(state, MakeCommand(kind, region, category, value), context)
                    .status == slsf::Status::Ready);
    };
    for (std::uint32_t region = 0; region < 2; ++region) {
        apply(slsf::ExplicitCommandKind::Set, region, 10);
        apply(slsf::ExplicitCommandKind::SetFlag, region);
    }
    apply(slsf::ExplicitCommandKind::Clear, 0);
    apply(slsf::ExplicitCommandKind::ClearFlag, 0);
    REQUIRE(state.derived.numericPositive.Test(category));
    REQUIRE(state.derived.numericLocal.Test(category));
    REQUIRE(state.derived.flagged.Test(category));
    apply(slsf::ExplicitCommandKind::Clear, 1);
    apply(slsf::ExplicitCommandKind::ClearFlag, 1);
    REQUIRE_FALSE(state.derived.numericPositive.Test(category));
    REQUIRE_FALSE(state.derived.numericLocal.Test(category));
    REQUIRE_FALSE(state.derived.flagged.Test(category));

    apply(slsf::ExplicitCommandKind::Set, 2, 15);
    const auto saved = slsf::CaptureState(state);
    REQUIRE(saved);
    REQUIRE(slsf::RestoreState(state, *saved, Regions(state.accountKeys, state.locationAccounts),
                              Config()));
    apply(slsf::ExplicitCommandKind::Clear, 2);
    REQUIRE_FALSE(state.derived.numericPositive.Test(category));
    apply(slsf::ExplicitCommandKind::Set, 0, 5);
    apply(slsf::ExplicitCommandKind::Wipe, 0);
    apply(slsf::ExplicitCommandKind::Set, 1, 7);
    apply(slsf::ExplicitCommandKind::Clear, 1);
    REQUIRE_FALSE(state.derived.numericLocal.Test(category));
    apply(slsf::ExplicitCommandKind::SetFlag, 0);
    REQUIRE(slsf::BindStateToRegions(state, Regions(state.accountKeys, state.locationAccounts)));
    apply(slsf::ExplicitCommandKind::ClearFlag, 0);
    REQUIRE_FALSE(state.derived.flagged.Test(category));
}

TEST_CASE("locations owned by one region share one numeric account", "[state][accounts]") {
    auto state = MakeState(2);
    state.locationAccounts[1] = state.locationAccounts[0];
    slsf::RuleContext context;

    REQUIRE(slsf::ApplyExplicit(
                state, MakeCommand(slsf::ExplicitCommandKind::Set, 0,
                                   slsf::FameCategory::Nasty, 33.0),
                context)
                .status == slsf::Status::Ready);
    REQUIRE(At(state, 1, slsf::FameCategory::Nasty) == 33.0);
    REQUIRE(Account(state, 0) == Account(state, 1));
    REQUIRE(state.derived.numericPositive.Test(slsf::FameCategory::Nasty));
}

TEST_CASE("explicit writes address a region without an actual location record",
          "[state][regression]") {
    auto state = MakeState(2);
    state.locationAccounts.clear();
    slsf::RuleContext context;
    REQUIRE(slsf::ApplyExplicit(state, MakeCommand(slsf::ExplicitCommandKind::Set, 1,
                                  slsf::FameCategory::Nasty, 33.0), context).status ==
            slsf::Status::Ready);
    REQUIRE(state.accounts[1].fame[slsf::FameCategory::Nasty] == 33.0);
    REQUIRE(state.accounts[0].fame[slsf::FameCategory::Nasty] == 0.0);
}

TEST_CASE("binding migrates rows by stable region identity", "[state][accounts]") {
    auto state = MakeState(2);
    slsf::RuleContext context;
    REQUIRE(slsf::ApplyExplicit(
                state, MakeCommand(slsf::ExplicitCommandKind::Set, 1,
                                   slsf::FameCategory::Cuck, 42.0),
                context)
                .status == slsf::Status::Ready);
    const auto keptKey = state.accountKeys[1];

    const auto regions = Regions({slsf::StableRegionKey{"location.esp", 2},
                                  slsf::StableRegionKey{"location.esp", 3}},
                                 {slsf::RegionId(0), slsf::RegionId(1)});
    const auto before = state.revision;
    REQUIRE(slsf::BindStateToRegions(state, regions));
    REQUIRE(state.revision.value == before.value + 1);
    REQUIRE(state.accountKeys[0] == keptKey);
    REQUIRE(state.accounts[0].fame[slsf::FameCategory::Cuck] == 42.0);
    REQUIRE(state.accounts[1].fame[slsf::FameCategory::Cuck] == 0.0);
    REQUIRE(state.locationAccounts.size() == 2);
}

TEST_CASE("capture and restore rebuild derived sets from current configuration",
          "[state][capture]") {
    auto state = MakeState(2);
    slsf::RuleContext context;
    REQUIRE(slsf::ApplyExplicit(
                state, MakeCommand(slsf::ExplicitCommandKind::Set, 0,
                                   slsf::FameCategory::Nasty, 15.0),
                context)
                .status == slsf::Status::Ready);
    state.accounts[0].flags[static_cast<std::size_t>(slsf::FameCategory::Nasty)] = 1;
    state.accounts[1].protectionUntilDay[static_cast<std::size_t>(slsf::FameCategory::Whore)] =
        12.5;
    state.providers.contributions.push_back(slsf::ProviderContribution{
        99, slsf::CategoryMask::FromCategory(slsf::FameCategory::Cuck)});
    state.providers.RebuildActiveCounts();
    slsf::RebuildDerivedSets(state);

    const auto captured = slsf::CaptureState(state);
    REQUIRE(captured);
    REQUIRE(captured->codecVersion == slsf::kPersistentStateCodecVersion);
    REQUIRE(captured->locations.size() == 2);
    REQUIRE(captured->contributions.size() == 1);

    const auto config = Config();
    const auto regions = Regions(state.accountKeys, state.locationAccounts);
    const auto bytes = slsf::EncodeState(*captured);
    REQUIRE(bytes);
    const auto decoded = slsf::DecodeState(*bytes);
    REQUIRE(decoded);
    slsf::State restored;
    const auto report = slsf::RestoreState(restored, *decoded, regions, config);
    REQUIRE(report);
    REQUIRE(report->restoredAccounts == 2);
    REQUIRE(report->unresolvedAccounts == 0);
    REQUIRE(restored.accounts[0].fame[slsf::FameCategory::Nasty] == 15.0);
    REQUIRE(restored.accounts[0].flags[static_cast<std::size_t>(slsf::FameCategory::Nasty)] == 1);
    REQUIRE(restored.accounts[1].protectionUntilDay[static_cast<std::size_t>(
                slsf::FameCategory::Whore)] == 12.5);
    REQUIRE(restored.providers.ActiveProviderCount(slsf::FameCategory::Cuck) == 1);
    REQUIRE(restored.derived.providerActive.Test(slsf::FameCategory::Cuck));
    REQUIRE(restored.derived.relationshipForbidden.Test(slsf::FameCategory::Whore));
    REQUIRE(restored.derived.relationshipForbidden.Test(slsf::FameCategory::Cuck));
    REQUIRE(restored.configurationRevision == 7);
    REQUIRE(restored.revision.value == 0);
}

TEST_CASE("restore keeps unknown accounts unresolved and ignores v1 settings",
          "[state][capture]") {
    auto state = MakeState(1);
    const auto captured = slsf::CaptureState(state);
    REQUIRE(captured);

    slsf::PersistentState legacy = *captured;
    legacy.codecVersion = slsf::kLegacyPersistentStateCodecVersion;
    legacy.locations[0].key = slsf::StableRegionKey{"removed.esp", 7};
    legacy.settings.push_back(slsf::SettingState{"fame.scale", 4.0});
    legacy.graphEdits.push_back(slsf::GraphEditState{
        slsf::StableRegionKey{"a.esp", 1}, slsf::StableRegionKey{"b.esp", 2}, 0.5, true});
    legacy.providerFlags.push_back(
        slsf::ProviderFlagState{"provider.one", slsf::FameCategory::Cuck, true, 0});

    slsf::RegionSnapshot regions;
    slsf::State restored;
    const auto report = slsf::RestoreState(restored, legacy, regions, Config());
    REQUIRE(report);
    REQUIRE(report->unresolvedAccounts == 1);
    REQUIRE(report->legacySettingsIgnored);
    REQUIRE(report->legacyGraphEditsIgnored);
    REQUIRE(restored.unresolvedAccounts.size() == 1);
    REQUIRE(restored.accounts.empty());
    REQUIRE(restored.providers.ActiveProviderCount(slsf::FameCategory::Cuck) == 1);
}

TEST_CASE("temporarily missing regions retain their full reputation across saves and rebinds",
          "[state][capture][regression]") {
    auto original = MakeState(1);
    auto& row = original.accounts[0];
    row.local[slsf::FameCategory::Cuck] = 42.0;
    row.remote[slsf::FameCategory::Cuck] = 9.0;
    row.flags[static_cast<std::size_t>(slsf::FameCategory::Cuck)] = 1;
    row.protectionUntilDay[static_cast<std::size_t>(slsf::FameCategory::Cuck)] = 123.5;
    row.settledAt = 77.25;
    row.remoteSettledAt = 78.5;
    row.anonymity = 3;
    row.auxiliaryState = {1, 2, 3, 4};
    const auto active = Regions(original.accountKeys, original.locationAccounts);
    const auto captured = slsf::CaptureState(original);
    REQUIRE(captured);

    slsf::State missing;
    const auto absent = slsf::RestoreState(missing, *captured, {}, Config());
    REQUIRE(absent);
    REQUIRE(absent->unresolvedAccounts == 1);
    const auto recaptured = slsf::CaptureState(missing);
    REQUIRE(recaptured);
    REQUIRE(recaptured->locations == captured->locations);
    const auto bytes = slsf::EncodeState(*recaptured);
    REQUIRE(bytes);
    const auto decoded = slsf::DecodeState(*bytes);
    REQUIRE(decoded);

    slsf::State returned;
    const auto rebound = slsf::RestoreState(returned, *decoded, active, Config());
    REQUIRE(rebound);
    REQUIRE(rebound->restoredAccounts == 1);
    REQUIRE(returned.accounts[0].fame[slsf::FameCategory::Cuck] == 42.0);
    REQUIRE(returned.accounts[0].remoteSettledAt == 78.5);
    REQUIRE(returned.derived.flagged.Test(slsf::FameCategory::Cuck));
    REQUIRE(slsf::CaptureState(returned)->locations == captured->locations);

    REQUIRE(slsf::BindStateToRegions(returned, {}));
    REQUIRE(slsf::CaptureState(returned)->locations == captured->locations);
    REQUIRE(slsf::BindStateToRegions(returned, active));
    REQUIRE(slsf::CaptureState(returned)->locations == captured->locations);
    REQUIRE(returned.unresolvedAccounts.empty());
}

TEST_CASE("mod registration can be saved before selecting reputation categories",
          "[state][capture][providers][regression]") {
    auto state = MakeState(1);
    const auto registered = slsf::UpdateProvider(
        state.providers, slsf::ProviderCommand{81, slsf::ProviderPhase::Enabled, {}});
    REQUIRE(registered.status == slsf::Status::Ready);
    REQUIRE(registered.changed);
    const auto captured = slsf::CaptureState(state);
    REQUIRE(captured);
    const auto bytes = slsf::EncodeState(*captured);
    REQUIRE(bytes);
    const auto decoded = slsf::DecodeState(*bytes);
    REQUIRE(decoded);
    slsf::State restored;
    REQUIRE(slsf::RestoreState(restored, *decoded,
                              Regions(state.accountKeys, state.locationAccounts), Config()));
    REQUIRE(restored.providers.contributions.size() == 1);
    REQUIRE(restored.providers.contributions[0].registrationId == 81);
    REQUIRE_FALSE(restored.providers.ActiveCategories().Any());
    const auto repeated = slsf::UpdateProvider(
        restored.providers, slsf::ProviderCommand{81, slsf::ProviderPhase::Enabled, {}});
    REQUIRE_FALSE(repeated.changed);
    const auto enabled = slsf::UpdateProvider(restored.providers,
        slsf::ProviderCommand{81, slsf::ProviderPhase::Enabled, {slsf::FameCategory::Cuck}});
    REQUIRE(enabled.changed);
    REQUIRE(restored.providers.ActiveProviderCount(slsf::FameCategory::Cuck) == 1);
}

TEST_CASE("provider invalidation wipes only complete optional categories",
          "[state][providers]") {
    auto state = MakeState(1);
    slsf::RuleContext context;
    REQUIRE(slsf::ApplyExplicit(
                state, MakeCommand(slsf::ExplicitCommandKind::Set, 0,
                                   slsf::FameCategory::Cuck, 9.0),
                context)
                .status == slsf::Status::Ready);
    state.accounts[0].flags[static_cast<std::size_t>(slsf::FameCategory::Cuck)] = 1;
    slsf::RecomputeAccount(state.accounts[0]);
    state.providers.contributions = {
        slsf::ProviderContribution{1, slsf::CategoryMask::FromCategory(slsf::FameCategory::Cuck)},
        slsf::ProviderContribution{2, slsf::CategoryMask::FromCategory(slsf::FameCategory::Nasty)}};
    state.providers.contributions.erase(state.providers.contributions.begin());
    state.providers.RebuildActiveCounts();
    slsf::RebuildDerivedSets(state);

    slsf::ProviderTransition transition;
    transition.status = slsf::Status::Ready;
    transition.kind = slsf::ProviderTransitionKind::Invalidated;
    transition.wipeCategories = {slsf::FameCategory::Cuck};
    const auto applied = slsf::ApplyProviderTransition(state, transition);
    REQUIRE(applied);
    REQUIRE(state.accounts[0].fame[slsf::FameCategory::Cuck] == 0.0);
    REQUIRE(state.accounts[0].flags[static_cast<std::size_t>(slsf::FameCategory::Cuck)] == 0);
    REQUIRE_FALSE(state.derived.providerActive.Test(slsf::FameCategory::Cuck));
    REQUIRE(state.derived.providerActive.Test(slsf::FameCategory::Nasty));
}

TEST_CASE("provider wipe validates every category before committing", "[state][providers]") {
    auto state = MakeState(1);
    auto& account = state.accounts[0];
    account.local[slsf::FameCategory::Cuck] = 9.0;
    account.flags[static_cast<std::size_t>(slsf::FameCategory::Cuck)] = 1;
    slsf::RecomputeAccount(account);
    slsf::RebuildDerivedSets(state);
    state.revision = slsf::Revision(5);

    slsf::ProviderTransition transition;
    transition.status = slsf::Status::Ready;
    transition.kind = slsf::ProviderTransitionKind::Invalidated;
    transition.wipeCategories = {slsf::FameCategory::Cuck,
                                 static_cast<slsf::FameCategory>(slsf::kFameCategoryCount)};
    const auto applied = slsf::ApplyProviderTransition(state, transition);
    REQUIRE_FALSE(applied);
    REQUIRE(applied.error().code == slsf::Error::Code::InvalidArgument);
    REQUIRE(state.accounts[0].local[slsf::FameCategory::Cuck] == 9.0);
    REQUIRE(state.accounts[0].fame[slsf::FameCategory::Cuck] == 9.0);
    REQUIRE(state.accounts[0].flags[static_cast<std::size_t>(slsf::FameCategory::Cuck)] == 1);
    REQUIRE(state.derived.numericPositive.Test(slsf::FameCategory::Cuck));
    REQUIRE(state.revision == slsf::Revision(5));
}

TEST_CASE("global cleanup also clears unresolved persisted accounts", "[state][capture]") {
    slsf::PersistentState persistent;
    persistent.codecVersion = slsf::kPersistentStateCodecVersion;
    slsf::LocationPersistentState saved;
    saved.key = slsf::StableRegionKey{"removed.esp", 9};
    saved.local[static_cast<std::size_t>(slsf::FameCategory::Cuck)] = 5.0;
    saved.remote[static_cast<std::size_t>(slsf::FameCategory::Cuck)] = 4.0;
    saved.flags[static_cast<std::size_t>(slsf::FameCategory::Cuck)] = 1;
    saved.protectionUntilDay[static_cast<std::size_t>(slsf::FameCategory::Cuck)] = 2.0;
    persistent.locations.push_back(saved);

    const auto restored = [&] {
        slsf::State state;
        const auto report = slsf::RestoreState(state, persistent, {}, Config());
        REQUIRE(report);
        return state;
    };

    auto state = restored();
    REQUIRE(state.unresolvedAccounts.size() == 1);
    slsf::RuleContext context;
    REQUIRE(slsf::ApplyExplicit(state, MakeCommand(slsf::ExplicitCommandKind::ClearAll, 0,
                                  slsf::FameCategory::Count), context)
                .status == slsf::Status::Ready);
    REQUIRE(state.unresolvedAccounts[0].local[static_cast<std::size_t>(
                slsf::FameCategory::Cuck)] == 0.0);
    REQUIRE(state.unresolvedAccounts[0].remote[static_cast<std::size_t>(
                slsf::FameCategory::Cuck)] == 0.0);
    REQUIRE(state.unresolvedAccounts[0].protectionUntilDay[static_cast<std::size_t>(
                slsf::FameCategory::Cuck)] == 0.0);
    REQUIRE(state.unresolvedAccounts[0].flags[static_cast<std::size_t>(
                slsf::FameCategory::Cuck)] == 1);

    state = restored();
    REQUIRE(slsf::ApplyExplicit(state, MakeCommand(slsf::ExplicitCommandKind::Wipe, 0,
                                  slsf::FameCategory::Cuck), context)
                .status == slsf::Status::Ready);
    REQUIRE(state.unresolvedAccounts[0].local[static_cast<std::size_t>(
                slsf::FameCategory::Cuck)] == 0.0);
    REQUIRE(state.unresolvedAccounts[0].flags[static_cast<std::size_t>(
                slsf::FameCategory::Cuck)] == 1);

    state = restored();
    slsf::ProviderTransition transition;
    transition.status = slsf::Status::Ready;
    transition.kind = slsf::ProviderTransitionKind::Invalidated;
    transition.wipeCategories = {slsf::FameCategory::Cuck};
    REQUIRE(slsf::ApplyProviderTransition(state, transition));
    REQUIRE(state.unresolvedAccounts[0].flags[static_cast<std::size_t>(
                slsf::FameCategory::Cuck)] == 0);
    REQUIRE(state.unresolvedAccounts[0].local[static_cast<std::size_t>(
                slsf::FameCategory::Cuck)] == 0.0);
}

TEST_CASE("restore rejects provider contributions above runtime capacity",
          "[state][capture][providers]") {
    slsf::PersistentState persistent;
    persistent.codecVersion = slsf::kPersistentStateCodecVersion;
    for (std::uint64_t id = 1; id <= slsf::kMaxProviderRegistrations + 1; ++id) {
        persistent.contributions.push_back(slsf::ProviderContributionState{id, {}});
    }
    slsf::State state;
    const auto report = slsf::RestoreState(state, persistent, {}, Config());
    REQUIRE_FALSE(report);
    REQUIRE(report.error().code == slsf::Error::Code::OutOfRange);
}

TEST_CASE("restore caps legacy provider flags before expanding them",
          "[state][capture][providers][regression]") {
    slsf::PersistentState legacy;
    legacy.codecVersion = slsf::kLegacyPersistentStateCodecVersion;
    for (std::uint32_t id = 0; id <= slsf::kMaxProviderRegistrations; ++id) {
        legacy.providerFlags.push_back(slsf::ProviderFlagState{
            "provider." + std::to_string(id), slsf::FameCategory::Whore, true, 0});
    }
    slsf::State state;
    const auto report = slsf::RestoreState(state, legacy, {}, Config());
    REQUIRE_FALSE(report);
    REQUIRE(report.error().code == slsf::Error::Code::OutOfRange);
    REQUIRE(state.accounts.empty());
}

TEST_CASE("restore accepts exactly the provider registration capacity",
          "[state][capture][providers][regression]") {
    slsf::PersistentState legacy;
    legacy.codecVersion = slsf::kLegacyPersistentStateCodecVersion;
    for (std::size_t id = 0; id < slsf::kMaxProviderRegistrations; ++id) {
        legacy.providerFlags.push_back(slsf::ProviderFlagState{
            "provider." + std::to_string(id), slsf::FameCategory::Whore, true, 0});
    }
    slsf::State state;
    const auto report = slsf::RestoreState(state, legacy, {}, Config());
    REQUIRE(report);
    REQUIRE(state.providers.contributions.size() == slsf::kMaxProviderRegistrations);
    REQUIRE(state.providers.ActiveProviderCount(slsf::FameCategory::Whore) ==
            slsf::kMaxProviderRegistrations);
}

TEST_CASE("binding rejects duplicate account identities and bad owners",
          "[state][accounts][regression]") {
    auto duplicate = MakeState(2);
    duplicate.accountKeys[1] = duplicate.accountKeys[0];
    const auto keys = std::vector<slsf::StableRegionKey>{
        duplicate.accountKeys[0], slsf::StableRegionKey{"location.esp", 2}};
    auto regions = Regions(keys, {slsf::RegionId(0), slsf::RegionId(1)});
    const auto duplicateResult = slsf::BindStateToRegions(duplicate, regions);
    REQUIRE_FALSE(duplicateResult);
    REQUIRE(duplicateResult.error().code == slsf::Error::Code::InvalidState);
    REQUIRE(duplicate.revision.value == 0);
    REQUIRE(duplicate.accounts.size() == 2);
    REQUIRE(duplicate.accountKeys[1] == duplicate.accountKeys[0]);
    REQUIRE(duplicate.locationAccounts[1] == slsf::RegionId(1));

    auto state = MakeState(1);
    const auto badOwner = Regions({state.accountKeys[0]}, {slsf::RegionId(5)});
    const auto badOwnerResult = slsf::BindStateToRegions(state, badOwner);
    REQUIRE_FALSE(badOwnerResult);
    REQUIRE(badOwnerResult.error().code == slsf::Error::Code::InvalidArgument);
}

TEST_CASE("restore rejects an out-of-range public location owner",
          "[state][capture][regression]") {
    const auto captured = slsf::CaptureState(MakeState(1));
    REQUIRE(captured);
    const auto regions = Regions({captured->locations[0].key}, {slsf::RegionId(5)});
    slsf::State state;
    const auto report = slsf::RestoreState(state, *captured, regions, Config());
    REQUIRE_FALSE(report);
    REQUIRE(report.error().code == slsf::Error::Code::InvalidArgument);

    slsf::RegionSnapshot emptyKey;
    emptyKey.regions = {slsf::StableRegionKey{"", 0}};
    const auto emptyReport = slsf::RestoreState(state, *captured, emptyKey, Config());
    REQUIRE_FALSE(emptyReport);
    REQUIRE(emptyReport.error().code == slsf::Error::Code::InvalidArgument);

    slsf::RegionSnapshot duplicateKey;
    duplicateKey.regions = {captured->locations[0].key, captured->locations[0].key};
    const auto duplicateReport = slsf::RestoreState(state, *captured, duplicateKey, Config());
    REQUIRE_FALSE(duplicateReport);
    REQUIRE(duplicateReport.error().code == slsf::Error::Code::InvalidArgument);
}
