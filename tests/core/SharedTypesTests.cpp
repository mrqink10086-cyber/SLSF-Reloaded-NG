#include <catch2/catch_test_macros.hpp>

#include "slsf/core/CategoryMask.h"
#include "slsf/core/Commands.h"
#include "slsf/core/Configuration.h"
#include "slsf/core/NumericRow.h"
#include "slsf/core/Snapshots.h"
#include "slsf/core/Types.h"
#include "slsf/core/reputation/ProviderRegistry.h"
#include "slsf/core/reputation/ReputationState.h"
#include "slsf/interfaces/ProviderPort.h"

#include <type_traits>

static_assert(!std::is_same_v<slsf::ProviderState, slsf::ProviderFactState>);
static_assert(!std::is_same_v<slsf::ProviderTransition, slsf::ProviderFactTransition>);
static_assert(!std::is_same_v<slsf::ProviderFlagFact, slsf::ProviderState>);
static_assert(std::is_same_v<decltype(slsf::ProviderContribution::categories), slsf::CategoryMask>);
static_assert(std::is_same_v<decltype(slsf::ExplicitCommand::target), slsf::RegionId>);

static_assert(slsf::ReputationSlotIndex(slsf::RegionId(0), slsf::FameCategory::Whore) == 0);
static_assert(slsf::ReputationSlotIndex(slsf::RegionId(2), slsf::FameCategory::Group) ==
              2 * slsf::kFameSlotCount + 18);
static_assert(slsf::ReputationSlotIndex(slsf::RegionId(2), slsf::FameCategory::Airhead) ==
              2 * slsf::kFameSlotCount + 24);

TEST_CASE("provider facts and core contributions coexist in one translation unit",
          "[shared][providers]") {
    slsf::ProviderFactState facts;
    facts.flags.push_back(
        slsf::ProviderFlagFact{"provider.esp", slsf::FameCategory::Whore, true});

    slsf::ProviderState contributions;
    contributions.contributions.push_back(
        slsf::ProviderContribution{7, {}});
    contributions.activeCounts[0] = 1;

    REQUIRE(facts.flags.size() == 1);
    REQUIRE(facts.flags.front().provider == "provider.esp");
    REQUIRE(contributions.contributions.size() == 1);
    REQUIRE(contributions.IsActive(slsf::FameCategory::Whore));

    slsf::ProviderFactTransition transition;
    transition.wipeOnce.push_back(slsf::FameCategory::Cuck);
    REQUIRE(transition.wipeOnce.size() == 1);
}

TEST_CASE("shared category, row and configuration headers compose", "[shared]") {
    slsf::CategoryMask mask;
    REQUIRE(mask.Set(slsf::FameCategory::Whore));
    REQUIRE(mask.Set(slsf::FameCategory::Airhead));

    slsf::NumericRow row{};
    row[slsf::FameCategory::Whore] = 5.0;

    slsf::ConfigurationData configuration;
    configuration.relationships.forbiddenByFriend = mask;

    slsf::State state;
    state.accounts.resize(1);
    state.accounts[0].local[slsf::FameCategory::Whore] = 5.0;

    REQUIRE(mask.Count() == 2);
    REQUIRE(row[slsf::FameCategory::Whore] == 5.0);
    REQUIRE(configuration.relationships.forbiddenByFriend == mask);
    REQUIRE(state.accounts.size() == 1);
    REQUIRE(state.accounts[0].local[slsf::FameCategory::Whore] == 5.0);
}

TEST_CASE("region account slots are independent of place slots", "[shared][account]") {
    slsf::NumericRowBuffer accounts(3);
    accounts[0][slsf::FameCategory::Whore] = 1.0;
    accounts[1][slsf::FameCategory::Whore] = 2.0;

    const auto slot = slsf::ReputationSlotIndex(slsf::RegionId(1), slsf::FameCategory::Whore);
    REQUIRE(slot == slsf::kFameSlotCount);
    REQUIRE(accounts[1].values[slot % slsf::kFameSlotCount] == 2.0);
    REQUIRE(accounts[0][slsf::FameCategory::Whore] == 1.0);
}
