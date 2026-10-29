#include "slsf/core/gameplay/Visibility.h"

#include <array>
#include <iostream>

using namespace slsf;
using namespace slsf::gameplay;

namespace {

Fact Known(bool value) { return Fact::Known(value); }

VisibilityFacts Fixture() {
    VisibilityFacts facts;
    facts.status = Status::Ready;
    facts.anonymous = Known(false);
    facts.playerIsMale = Known(false);
    facts.mods.deviousDevices = Known(true);
    facts.mods.armorKeywords = Known(true);
    facts.mods.sls = Known(true);
    facts.mods.slacs = Known(true);
    facts.mods.slaveTats = Known(true);

    for (ItemFacts& item : facts.items) {
        item.present = Known(false);
    }

    facts.worn.deviousHeavyBondage = Known(false);
    facts.worn.deviousLockable = Known(false);
    facts.worn.deviousBeltWorn = Known(false);
    facts.worn.deviousCollarWorn = Known(false);
    facts.worn.devious.hood = Known(false);
    facts.worn.devious.gag = Known(false);
    facts.worn.devious.gagPanel = Known(false);
    facts.worn.devious.armCuffs = Known(false);
    facts.worn.devious.armCuffsFront = Known(false);
    facts.worn.devious.armbinder = Known(false);
    facts.worn.devious.armbinderElbow = Known(false);
    facts.worn.devious.gloves = Known(false);
    facts.worn.devious.legCuffs = Known(false);
    facts.worn.devious.boots = Known(false);
    facts.worn.devious.suit = Known(false);
    facts.worn.devious.corset = Known(false);
    facts.worn.devious.blindfold = Known(false);

    facts.andRanks.installed = Known(false);
    for (Fact& rank : facts.andRanks.rankIsOne) {
        rank = Known(false);
    }

    facts.tattoos.body.installed = Known(false);
    facts.tattoos.body.appliedCountKnown = Known(true);
    facts.tattoos.body.appliedCount = 0;
    facts.tattoos.face.installed = Known(false);
    facts.tattoos.face.appliedCountKnown = Known(true);
    facts.tattoos.hands.installed = Known(false);
    facts.tattoos.hands.appliedCountKnown = Known(true);
    facts.tattoos.feet.installed = Known(false);
    facts.tattoos.feet.appliedCountKnown = Known(true);
    return facts;
}

const char* StatusName(Status status) {
    switch (status) {
    case Status::Ready: return "Ready";
    case Status::NotReady: return "NotReady";
    case Status::Missing: return "Missing";
    case Status::Incompatible: return "Incompatible";
    case Status::Invalid: return "Invalid";
    case Status::Stale: return "Stale";
    }
    return "?";
}

} // namespace

int main() {
    VisibilityRules rules;

    auto heavy = Fixture();
    heavy.worn.deviousHeavyBondage = Known(true);
    heavy.worn.devious.hood = Known(true);
    const BondageResult heavyResult = rules.EvaluateBondage(heavy);
    std::cout << "heavy_harness_wearingUnhideable="
              << StatusName(heavyResult.wearingUnhideable.status) << ':'
              << heavyResult.wearingUnhideable.value << '\n';
    std::cout << "heavy_harness_visiblyBound="
              << StatusName(heavyResult.visiblyBound.status) << ':'
              << heavyResult.visiblyBound.value << '\n';
    std::cout << "legacy_expected_heavy_harness_wearingUnhideable=Ready:0\n";
    const bool heavyMatchesLegacy =
        heavyResult.wearingUnhideable.status == Status::Ready &&
        !heavyResult.wearingUnhideable.value;
    std::cout << "heavy_harness_matches_legacy="
              << (heavyMatchesLegacy ? "true" : "false") << '\n';

    auto tattoo = Fixture();
    tattoo.andRanks.installed = Fact{};
    tattoo.andRanks.rankIsOne[static_cast<std::size_t>(AndLocation::Chest)] =
        Known(true);
    tattoo.tattoos.body.installed = Known(true);
    tattoo.tattoos.body.appliedCountKnown = Known(true);
    tattoo.tattoos.body.appliedCount = 1;
    tattoo.tattoos.body.slots.push_back(
        TattooSlotFacts{Known(true), TattooSubcategory::Chest});
    const RuleDecision tattooResult = rules.BodyTattooVisible(tattoo, 0);
    auto andInstalled = tattoo;
    andInstalled.andRanks.installed = Known(true);
    const RuleDecision branchAResult = rules.BodyTattooVisible(andInstalled, 0);
    auto andMissing = tattoo;
    andMissing.andRanks.installed = Known(false);
    const RuleDecision branchCResult = rules.BodyTattooVisible(andMissing, 0);
    std::cout << "chest_tattoo_unknown_AND=" << StatusName(tattooResult.status)
              << ':' << tattooResult.value << '\n';
    std::cout << "branch_A_AND_true=" << StatusName(branchAResult.status) << ':'
              << branchAResult.value << '\n';
    std::cout << "branch_C_AND_false=" << StatusName(branchCResult.status) << ':'
              << branchCResult.value << '\n';
    const bool reproduced = heavyMatchesLegacy &&
        tattooResult.status == Status::NotReady &&
        branchAResult.status == Status::Ready && branchAResult.value &&
        branchCResult.status == Status::Ready && branchCResult.value;
    std::cout << "probe_result=" << (reproduced ? "REPRODUCED" : "MISMATCH")
              << '\n';
    return reproduced ? 0 : 1;
}
