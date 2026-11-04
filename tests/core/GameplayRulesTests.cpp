#include <catch2/catch_test_macros.hpp>

#include "slsf/core/Types.h"
#include "slsf/core/gameplay/Anonymity.h"
#include "slsf/core/gameplay/AnimationClassifier.h"
#include "slsf/core/gameplay/Comments.h"
#include "slsf/core/gameplay/Common.h"
#include "slsf/core/gameplay/Visibility.h"
#include "slsf/core/gameplay/Witness.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace slsf;
using namespace slsf::gameplay;

class ScriptedRandom final : public IRandom {
public:
    explicit ScriptedRandom(std::initializer_list<std::int32_t> samples) : samples_(samples) {}
    explicit ScriptedRandom(std::vector<std::int32_t> samples) : samples_(std::move(samples)) {}

    std::int32_t UniformInt(std::int32_t lo, std::int32_t hi) override {
        ranges.emplace_back(lo, hi);
        ++calls;
        const std::int32_t raw = next_ < samples_.size() ? samples_[next_] : lo;
        ++next_;
        const std::int32_t value = std::clamp(raw, lo, hi);
        values.push_back(value);
        return value;
    }

    std::vector<std::pair<std::int32_t, std::int32_t>> ranges;
    std::vector<std::int32_t> values;
    std::size_t calls{};

private:
    std::vector<std::int32_t> samples_;
    std::size_t next_{};
};

Fact B(bool value) { return Fact::Known(value); }
Fact U() { return Fact{}; }

void SetAllEligible(FameGainRollFacts& facts, bool value) {
    facts.eligible.fill(B(value));
}
RegionId R(std::uint32_t id) { return RegionId(id); }

VisibilityFacts VisibilityFixture() {
    VisibilityFacts facts;
    facts.status = Status::Ready;
    facts.anonymous = B(false);
    facts.playerIsMale = B(false);
    facts.mods.deviousDevices = B(true);
    facts.mods.armorKeywords = B(true);
    facts.mods.sls = B(true);
    facts.mods.slacs = B(true);
    facts.mods.slaveTats = B(true);
    facts.settings.anonymityEnabled = B(true);
    facts.settings.dynamicAnonymity = B(false);
    facts.settings.allowCollarBoundFame = B(true);
    facts.settings.allowSlscCursedCollarBoundFame = B(true);

    for (auto& item : facts.items) {
        item = ItemFacts{};
        item.present = B(false);
    }
    facts.worn.hidesIdentity = B(false);
    facts.worn.coversBack = B(false);
    facts.worn.coversHands = B(false);
    facts.worn.coversFeet = B(false);
    facts.worn.deviousHood = B(false);
    facts.worn.deviousHeavyBondage = B(false);
    facts.worn.deviousLockable = B(false);
    facts.worn.deviousBeltWorn = B(false);
    facts.worn.deviousCollarWorn = B(false);
    facts.worn.slsCollarCurse = B(false);
    facts.worn.devious = DeviousWornFacts{};
    facts.worn.devious.hood = B(false);
    facts.worn.devious.gag = B(false);
    facts.worn.devious.gagPanel = B(false);
    facts.worn.devious.armCuffs = B(false);
    facts.worn.devious.armCuffsFront = B(false);
    facts.worn.devious.armbinder = B(false);
    facts.worn.devious.armbinderElbow = B(false);
    facts.worn.devious.gloves = B(false);
    facts.worn.devious.legCuffs = B(false);
    facts.worn.devious.boots = B(false);
    facts.worn.devious.suit = B(false);
    facts.worn.devious.corset = B(false);
    facts.worn.devious.blindfold = B(false);

    facts.andRanks = AndFacts{};
    facts.andRanks.installed = B(false);
    for (auto& rank : facts.andRanks.rankIsOne) {
        rank = B(false);
    }

    facts.tattoos.body = TattooAreaFacts{};
    facts.tattoos.face = TattooAreaFacts{};
    facts.tattoos.hands = TattooAreaFacts{};
    facts.tattoos.feet = TattooAreaFacts{};
    for (TattooAreaFacts* area : {&facts.tattoos.body, &facts.tattoos.face,
                                  &facts.tattoos.hands, &facts.tattoos.feet}) {
        area->installed = B(false);
        area->appliedCountKnown = B(true);
        area->appliedCount = 0;
    }

    facts.cum.anal = CountFacts{};
    facts.cum.anal.known = B(true);
    facts.cum.anal.value = 0;
    facts.cum.vaginal = facts.cum.anal;
    facts.cum.oral = facts.cum.anal;
    return facts;
}
ItemFacts& Slot(VisibilityFacts& facts, ArmorSlot slot) {
    return facts.items[static_cast<std::size_t>(slot)];
}
void PresentAt(VisibilityFacts& facts, ArmorSlot slot) {
    Slot(facts, slot).present = B(true);
}
void SetTattoo(VisibilityFacts& facts, TattooSubcategory subcategory,
               std::size_t slot = 0) {
    TattooAreaFacts& area = facts.tattoos.body;
    area.installed = B(true);
    area.appliedCountKnown = B(true);
    area.appliedCount = static_cast<int>(slot) + 1;
    area.slots.assign(slot + 1, TattooSlotFacts{});
    area.slots[slot].applied = B(true);
    area.slots[slot].subcategory = subcategory;
}
void SetAndRank(VisibilityFacts& facts, AndLocation location) {
    facts.andRanks.installed = B(true);
    facts.andRanks.rankIsOne[static_cast<std::size_t>(location)] = B(true);
}
AnimationFacts AnimationFixture() {
    AnimationFacts facts;
    facts.status = Status::Ready;
    facts.anonymous = B(false);
    facts.validLocation = B(true);
    facts.startGraceElapsed = B(true);
    facts.sceneEnded = B(false);
    facts.bodySearch = B(false);
    facts.leadIn = B(false);
    facts.allowForeplayFame = B(false);
    facts.playerIsFemale = B(false);
    facts.maleCount = ObservedNumber::Known(0);
    facts.femaleCount = ObservedNumber::Known(0);
    facts.aggressor = B(false);
    facts.victim = B(false);
    facts.usingStrapon = B(false);
    facts.sexWorker = B(false);
    facts.publicWhore = B(false);
    facts.submissiveDefault = B(false);
    facts.dominantDefault = B(false);
    facts.canGainBoundFame = B(false);
    facts.victimsAreMasochist = B(false);
    facts.allowLikeFameWhenRaped = B(false);
    facts.allowBestialityWhenRaped = B(false);
    facts.deviousDevicesInstalled = B(false);
    facts.friendKissAndTellChance = 100;
    facts.loverKissAndTellChance = 100;
    facts.actors.push_back(AnimationActorFacts{});
    facts.actors.back().isPlayer = true;
    return facts;
}
AnimationActorFacts Partner(int rank, AnimationRace race = AnimationRace::Other) {
    AnimationActorFacts actor;
    actor.isPlayer = false;
    actor.relationshipKnown = B(true);
    actor.relationshipRank = rank;
    actor.race = race;
    return actor;
}
NpcScanTargetFacts NpcFixture(std::string plugin = "Skyrim.esm",
                              std::uint32_t form = 0x100) {
    NpcScanTargetFacts target;
    target.actor = StableFormKey{std::move(plugin), form};
    target.validLocation = B(true);
    target.dead = B(false);
    target.inCombat = B(false);
    target.sexLabActive = B(false);
    target.lineOfSight = B(false);
    target.distance = ObservedDouble::Known(1.0);
    target.playerDetected = B(true);
    target.relationshipKnown = B(true);
    target.relationshipRank = 0;
    return target;
}
WitnessSettings WitnessFixture() {
    WitnessSettings settings;
    settings.enemyChance = 50;
    settings.neutralChance = 50;
    settings.friendChance = 50;
    settings.loverChance = 50;
    settings.npcNeedsLineOfSight = true;
    settings.minimumNpcLosDistance = 50.0;
    return settings;
}
CommentFacts CommentFixture() {
    CommentFacts facts;
    facts.status = Status::Ready;
    facts.anonymous = B(false);
    facts.combatState = ObservedNumber::Known(0);
    facts.commentFrequency = ObservedNumber::Known(50);
    facts.publicWhore = B(false);
    facts.disableNakedCommentsWhilePW = B(false);
    facts.andInstalled = B(true);
    return facts;
}
}
TEST_CASE("anonymity consumes each region transition and preserves the legacy memory formula",
          "[gameplay][anonymity]") {
    AnonymityRules rules;
    const RegionId a = R(1);
    const RegionId b = R(2);

    SECTION("first entry becomes anonymous with the seven day floor") {
        REQUIRE(rules.ObserveRegion(std::nullopt, a, GameTime::FromDays(10.0)) ==
                Status::Ready);
        const AnonymityMemory* memory = rules.Find(a);
        REQUIRE(memory != nullptr);
        REQUIRE(memory->localAnonymous);
        REQUIRE(memory->startedAnonymous);
        REQUIRE(memory->recognitionDays == 7.0);
        REQUIRE(rules.State().transitionsConsumed == 1);
    }

    SECTION("a visit shorter than three game hours does not consume recognition") {
        REQUIRE(rules.ObserveRegion(std::nullopt, a, GameTime::FromDays(10.0)) ==
                Status::Ready);
        REQUIRE(rules.ObserveRegion(a, std::nullopt, GameTime::FromDays(10.05)) ==
                Status::Ready);
        const AnonymityMemory* memory = rules.Find(a);
        REQUIRE(memory->localAnonymous);
        REQUIRE(memory->recognitionDays == 7.0);
        REQUIRE(rules.State().transitionsConsumed == 2);
    }

    SECTION("leaving after the grace subtracts exactly 0.125 days") {
        REQUIRE(rules.ObserveRegion(std::nullopt, a, GameTime::FromDays(10.0)) ==
                Status::Ready);
        REQUIRE(rules.ObserveRegion(a, std::nullopt, GameTime::FromDays(10.2)) ==
                Status::Ready);
        const AnonymityMemory* memory = rules.Find(a);
        REQUIRE_FALSE(memory->localAnonymous);
        REQUIRE_FALSE(memory->startedAnonymous);
        REQUIRE(memory->recognitionDays > 7.07);
        REQUIRE(memory->recognitionDays < 7.08);
    }

    SECTION("a region change consumes one leave and one enter") {
        REQUIRE(rules.ObserveRegion(std::nullopt, a, GameTime::FromDays(10.0)) ==
                Status::Ready);
        REQUIRE(rules.ObserveRegion(a, b, GameTime::FromDays(11.0)) == Status::Ready);
        REQUIRE(rules.State().transitionsConsumed == 3);
        REQUIRE(rules.Find(a)->lastExit == GameTime::FromDays(11.0));
        REQUIRE(rules.Find(b)->lastEnter == GameTime::FromDays(11.0));
    }

    SECTION("hourly sampling expires local anonymity at the three hour boundary") {
        REQUIRE(rules.ObserveRegion(std::nullopt, a, GameTime::FromDays(10.0)) ==
                Status::Ready);
        REQUIRE(rules.AdvanceHourly(a, GameTime::FromDays(10.124)) == Status::Ready);
        REQUIRE(rules.Find(a)->localAnonymous);
        REQUIRE(rules.AdvanceHourly(a, GameTime::FromDays(10.125)) == Status::Ready);
        REQUIRE_FALSE(rules.Find(a)->localAnonymous);
        REQUIRE(rules.AdvanceHourly(b, GameTime::FromDays(10.125)) == Status::Missing);
    }

    SECTION("non-monotonic samples are stale and do not consume a transition") {
        REQUIRE(rules.ObserveRegion(std::nullopt, a, GameTime::FromDays(10.0)) ==
                Status::Ready);
        REQUIRE(rules.ObserveRegion(a, b, GameTime::FromDays(9.0)) == Status::Stale);
        REQUIRE(rules.State().transitionsConsumed == 1);
    }

    SECTION("leaving a region that was never entered creates no memory") {
        REQUIRE(rules.ObserveRegion(std::nullopt, a, GameTime::FromDays(10.0)) ==
                Status::Ready);
        REQUIRE(rules.ObserveRegion(b, std::nullopt, GameTime::FromDays(11.0)) ==
                Status::Ready);
        REQUIRE(rules.Find(b) == nullptr);
        REQUIRE(rules.State().transitionsConsumed == 1);
        REQUIRE(rules.ObserveRegion(std::nullopt, b, GameTime::FromDays(15.0)) ==
                Status::Ready);
        REQUIRE(rules.Find(b) != nullptr);
        REQUIRE(rules.Find(b)->localAnonymous);
        REQUIRE(rules.Find(b)->recognitionDays == 7.0);
        REQUIRE(rules.State().transitionsConsumed == 2);
    }

    SECTION("recognition growth uses max(7, 2R - away)") {
        REQUIRE(rules.ObserveRegion(std::nullopt, a, GameTime::FromDays(10.0)) ==
                Status::Ready);
        REQUIRE(rules.ObserveRegion(a, std::nullopt, GameTime::FromDays(20.0)) ==
                Status::Ready);
        REQUIRE(rules.Find(a)->recognitionDays == 16.875);
        REQUIRE(rules.ObserveRegion(std::nullopt, a, GameTime::FromDays(40.0)) ==
                Status::Ready);
        REQUIRE(rules.Find(a)->localAnonymous);
        REQUIRE(rules.Find(a)->recognitionDays == 13.75);
        REQUIRE(rules.ObserveRegion(a, std::nullopt, GameTime::FromDays(40.1)) ==
                Status::Ready);
        REQUIRE(rules.Find(a)->localAnonymous);
        REQUIRE(rules.ObserveRegion(std::nullopt, a, GameTime::FromDays(41.0)) ==
                Status::Ready);
        REQUIRE(rules.Find(a)->recognitionDays > 26.5);
        REQUIRE(rules.Find(a)->recognitionDays < 26.7);
    }
}
TEST_CASE("anonymity evaluation follows the static and dynamic legacy branches",
          "[gameplay][anonymity]") {
    AnonymityRules rules;
    const RegionId a = R(1);
    REQUIRE(rules.ObserveRegion(std::nullopt, a, GameTime::FromDays(10.0)) ==
            Status::Ready);

    AnonymityConditions conditions;
    conditions.anonymityEnabled = B(true);
    conditions.dynamicAnonymity = B(false);
    conditions.lowFameRegion = B(true);
    conditions.thanesCantBeAnonymous = B(false);
    conditions.isThane = B(false);

    SECTION("disabled switch overrides every gear fact") {
        conditions.anonymityEnabled = B(false);
        conditions.hidesIdentity = B(true);
        const auto decision = rules.Evaluate(a, conditions);
        REQUIRE(decision.IsReady());
        REQUIRE_FALSE(decision.value);
    }

    SECTION("static chain is an OR of identity, hood and head slot") {
        conditions.hidesIdentity = B(true);
        REQUIRE(rules.Evaluate(a, conditions).value);
        conditions.hidesIdentity = B(false);
        conditions.deviousDevicesInstalled = B(true);
        conditions.deviousHoodWorn = B(true);
        REQUIRE(rules.Evaluate(a, conditions).value);
        conditions.deviousHoodWorn = B(false);
        conditions.headSlotOccupied = B(true);
        conditions.headSlotBikiniArmor = B(false);
        conditions.headSlotDoesNotHideIdentity = B(false);
        REQUIRE(rules.Evaluate(a, conditions).value);
        conditions.headSlotBikiniArmor = B(true);
        REQUIRE_FALSE(rules.Evaluate(a, conditions).value);
    }

    SECTION("dynamic branch uses local state, fame and the thane block") {
        conditions.dynamicAnonymity = B(true);
        REQUIRE(rules.Evaluate(a, conditions).value);
        conditions.lowFameRegion = B(false);
        REQUIRE_FALSE(rules.Evaluate(a, conditions).value);
        conditions.lowFameRegion = B(true);
        conditions.thanesCantBeAnonymous = B(true);
        conditions.isThane = B(true);
        REQUIRE_FALSE(rules.Evaluate(a, conditions).value);
        conditions.isThane = B(false);
        REQUIRE(rules.Evaluate(a, conditions).value);
    }

    SECTION("unknown thane switch with a known thane stays not ready") {
        conditions.dynamicAnonymity = B(true);
        conditions.thanesCantBeAnonymous = U();
        conditions.isThane = B(true);
        const auto decision = rules.Evaluate(a, conditions);
        REQUIRE(decision.status == Status::NotReady);
    }

    SECTION("dynamic local flag needs a live region memory") {
        conditions.dynamicAnonymity = B(true);
        REQUIRE_FALSE(rules.Evaluate(R(99), conditions).value);
    }
}
TEST_CASE("body tattoo visibility mirrors the four legacy branches", "[gameplay][visibility]") {
    VisibilityRules rules;
    VisibilityFacts facts = VisibilityFixture();

    SECTION("branch A none uses the AND ranks or a bikini body slot") {
        SetTattoo(facts, TattooSubcategory::None);
        SetAndRank(facts, AndLocation::Chest);
        PresentAt(facts, ArmorSlot::Body);
        REQUIRE(rules.BodyTattooVisible(facts, 0).value);
        facts.andRanks.rankIsOne[static_cast<std::size_t>(AndLocation::Chest)] = B(false);
        REQUIRE_FALSE(rules.BodyTattooVisible(facts, 0).value);
        Slot(facts, ArmorSlot::Body).bikiniArmor = B(true);
        REQUIRE(rules.BodyTattooVisible(facts, 0).value);
        SetTattoo(facts, TattooSubcategory::Chest);
        REQUIRE_FALSE(rules.BodyTattooVisible(facts, 0).value);
        facts.andRanks.rankIsOne[static_cast<std::size_t>(AndLocation::Chest)] = B(true);
        REQUIRE(rules.BodyTattooVisible(facts, 0).value);
    }

    SECTION("branch A back checks covers-back, upper and lower slots") {
        SetTattoo(facts, TattooSubcategory::Back);
        PresentAt(facts, ArmorSlot::Body);
        SetAndRank(facts, AndLocation::Chest);
        PresentAt(facts, ArmorSlot::BackUpper);
        Slot(facts, ArmorSlot::BackUpper).bikiniArmor = B(true);
        Slot(facts, ArmorSlot::BackUpper).doesNotCoverBack = B(true);
        REQUIRE(rules.BodyTattooVisible(facts, 0).value);
        facts.worn.coversBack = B(true);
        REQUIRE_FALSE(rules.BodyTattooVisible(facts, 0).value);
        facts.worn.coversBack = B(false);
        Slot(facts, ArmorSlot::BackUpper).bikiniArmor = B(false);
        Slot(facts, ArmorSlot::BackUpper).doesNotCoverBack = B(false);
        REQUIRE_FALSE(rules.BodyTattooVisible(facts, 0).value);
    }

    SECTION("branch A back still checks a covering lower slot when upper is absent") {
        SetTattoo(facts, TattooSubcategory::Back);
        PresentAt(facts, ArmorSlot::Body);
        SetAndRank(facts, AndLocation::Chest);
        PresentAt(facts, ArmorSlot::BackLower);
        Slot(facts, ArmorSlot::BackLower).doesNotCoverBack = B(false);
        REQUIRE_FALSE(rules.BodyTattooVisible(facts, 0).value);
        Slot(facts, ArmorSlot::BackLower).doesNotCoverBack = B(true);
        REQUIRE(rules.BodyTattooVisible(facts, 0).value);
    }

    SECTION("branch A thigh and calves use their blocking slots") {
        SetAndRank(facts, AndLocation::Chest);
        SetTattoo(facts, TattooSubcategory::Thigh);
        PresentAt(facts, ArmorSlot::Body);
        REQUIRE_FALSE(rules.BodyTattooVisible(facts, 0).value);
        Slot(facts, ArmorSlot::Body).bikiniArmor = B(true);
        REQUIRE(rules.BodyTattooVisible(facts, 0).value);
        Slot(facts, ArmorSlot::Body).present = B(false);
        REQUIRE(rules.BodyTattooVisible(facts, 0).value);

        SetTattoo(facts, TattooSubcategory::Calves);
        PresentAt(facts, ArmorSlot::Feet);
        REQUIRE_FALSE(rules.BodyTattooVisible(facts, 0).value);
        Slot(facts, ArmorSlot::Feet).present = B(false);
        PresentAt(facts, ArmorSlot::Calves);
        REQUIRE_FALSE(rules.BodyTattooVisible(facts, 0).value);
        Slot(facts, ArmorSlot::Calves).present = B(false);
        REQUIRE(rules.BodyTattooVisible(facts, 0).value);
    }

    SECTION("branch B ignores the body armor for non-back subcategories") {
        facts.mods.sls = B(false);
        SetAndRank(facts, AndLocation::Chest);
        SetTattoo(facts, TattooSubcategory::None);
        PresentAt(facts, ArmorSlot::Body);
        REQUIRE(rules.BodyTattooVisible(facts, 0).value);
        SetTattoo(facts, TattooSubcategory::Back);
        PresentAt(facts, ArmorSlot::BackUpper);
        Slot(facts, ArmorSlot::BackUpper).doesNotCoverBack = B(false);
        REQUIRE_FALSE(rules.BodyTattooVisible(facts, 0).value);
    }

    SECTION("branch B back also checks a covering lower slot when upper is absent") {
        facts.mods.sls = B(false);
        SetAndRank(facts, AndLocation::Chest);
        SetTattoo(facts, TattooSubcategory::Back);
        PresentAt(facts, ArmorSlot::BackLower);
        Slot(facts, ArmorSlot::BackLower).doesNotCoverBack = B(false);
        REQUIRE_FALSE(rules.BodyTattooVisible(facts, 0).value);
        Slot(facts, ArmorSlot::BackLower).doesNotCoverBack = B(true);
        REQUIRE(rules.BodyTattooVisible(facts, 0).value);
    }

    SECTION("branch C needs open body, upper and lower back") {
        facts.mods.sls = B(true);
        facts.andRanks.installed = B(false);
        SetTattoo(facts, TattooSubcategory::Back);
        PresentAt(facts, ArmorSlot::Body);
        Slot(facts, ArmorSlot::Body).bikiniArmor = B(true);
        REQUIRE(rules.BodyTattooVisible(facts, 0).value);
        Slot(facts, ArmorSlot::Body).bikiniArmor = B(false);
        REQUIRE_FALSE(rules.BodyTattooVisible(facts, 0).value);
    }

    SECTION("branch D only requires an absent body slot") {
        facts.mods.sls = B(false);
        facts.andRanks.installed = B(false);
        SetTattoo(facts, TattooSubcategory::None);
        REQUIRE(rules.BodyTattooVisible(facts, 0).value);
        PresentAt(facts, ArmorSlot::Body);
        REQUIRE_FALSE(rules.BodyTattooVisible(facts, 0).value);
    }
}
TEST_CASE("face, hand and foot tattoo visibility preserve keyword precedence",
          "[gameplay][visibility]") {
    VisibilityRules rules;
    VisibilityFacts facts = VisibilityFixture();

    SetTattoo(facts, TattooSubcategory::None);
    facts.tattoos.face = facts.tattoos.body;
    facts.tattoos.hands = facts.tattoos.body;
    facts.tattoos.feet = facts.tattoos.body;

    SECTION("static anonymity only consults IsPlayerAnonymous") {
        facts.settings.anonymityEnabled = B(true);
        facts.settings.dynamicAnonymity = B(false);
        facts.anonymous = B(false);
        facts.worn.hidesIdentity = B(true);
        REQUIRE(rules.FaceTattooVisible(facts, 0).value);
        facts.anonymous = B(true);
        REQUIRE_FALSE(rules.FaceTattooVisible(facts, 0).value);
    }

    SECTION("dynamic anonymity uses the identity chain") {
        facts.settings.dynamicAnonymity = B(true);
        facts.worn.hidesIdentity = B(true);
        REQUIRE_FALSE(rules.FaceTattooVisible(facts, 0).value);
        facts.worn.hidesIdentity = B(false);
        PresentAt(facts, ArmorSlot::Head);
        REQUIRE_FALSE(rules.FaceTattooVisible(facts, 0).value);
        Slot(facts, ArmorSlot::Head).doesNotHideIdentity = B(true);
        REQUIRE(rules.FaceTattooVisible(facts, 0).value);
    }

    SECTION("disabled anonymity ignores an unknown dynamic switch") {
        facts.settings.anonymityEnabled = B(false);
        facts.settings.dynamicAnonymity = U();
        facts.anonymous = U();
        REQUIRE(rules.FaceTattooVisible(facts, 0).value);
    }

    SECTION("stale snapshots cannot produce known bondage or cum results") {
        facts.status = Status::Stale;
        REQUIRE(rules.EvaluateBondage(facts).lightlyBound.status == Status::NotReady);
        facts.cum.anal.value = 1;
        REQUIRE(rules.AssCumVisible(facts).status == Status::NotReady);
        REQUIRE(rules.VisibleAssCum(facts).status == Status::NotReady);
    }

    SECTION("an unapplied or out-of-range tattoo slot pauses") {
        SetTattoo(facts, TattooSubcategory::Chest);
        facts.tattoos.body.appliedCountKnown = B(false);
        REQUIRE(rules.BodyTattooVisible(facts, 0).status == Status::NotReady);

        facts.tattoos.body.appliedCountKnown = B(true);
        REQUIRE(rules.BodyTattooVisible(facts, 1).status == Status::NotReady);
    }

    SECTION("non-ready snapshots cannot produce tattoo decisions") {
        SetTattoo(facts, TattooSubcategory::Chest);
        facts.status = Status::Stale;
        REQUIRE(rules.BodyTattooVisible(facts, 0).status == Status::NotReady);
        facts.status = Status::Incompatible;
        REQUIRE(rules.FaceTattooVisible(facts, 0).status == Status::NotReady);
        facts.status = Status::Invalid;
        REQUIRE(rules.BodyTattooVisible(facts, 0).status == Status::Invalid);
    }

    SECTION("a devious hood blocks face tattoos in the identity chain") {
        SetTattoo(facts, TattooSubcategory::Chest);
        facts.tattoos.face = facts.tattoos.body;
        facts.settings.dynamicAnonymity = B(true);
        facts.worn.deviousHood = B(true);
        REQUIRE_FALSE(rules.FaceTattooVisible(facts, 0).value);
        facts.worn.deviousHood = B(false);
        REQUIRE(rules.FaceTattooVisible(facts, 0).value);
    }

    SECTION("hands and feet require both keyword and slot checks to pass") {
        facts.worn.coversHands = B(true);
        REQUIRE_FALSE(rules.HandTattooVisible(facts, 0).value);
        facts.worn.coversHands = B(false);
        PresentAt(facts, ArmorSlot::Hands);
        REQUIRE_FALSE(rules.HandTattooVisible(facts, 0).value);
        Slot(facts, ArmorSlot::Hands).doesNotCoverHands = B(true);
        REQUIRE(rules.HandTattooVisible(facts, 0).value);

        facts.worn.coversFeet = B(true);
        REQUIRE_FALSE(rules.FootTattooVisible(facts, 0).value);
        facts.worn.coversFeet = B(false);
        PresentAt(facts, ArmorSlot::Feet);
        REQUIRE_FALSE(rules.FootTattooVisible(facts, 0).value);
        Slot(facts, ArmorSlot::Feet).doesNotCoverFeet = B(true);
        REQUIRE(rules.FootTattooVisible(facts, 0).value);
    }
}
TEST_CASE("cum visibility separates the count gate from the visibility gate",
          "[gameplay][visibility]") {
    VisibilityRules rules;
    VisibilityFacts facts = VisibilityFixture();

    SECTION("anal cum needs a positive count, while the visible count does not") {
        facts.cum.anal.value = 0;
        REQUIRE_FALSE(rules.AssCumVisible(facts).value);
        REQUIRE(rules.VisibleAssCum(facts).value == 0);
        PresentAt(facts, ArmorSlot::Body);
        facts.cum.anal.value = 3;
        REQUIRE_FALSE(rules.AssCumVisible(facts).value);
        REQUIRE(rules.VisibleAssCum(facts).value == 0);
        Slot(facts, ArmorSlot::Body).bikiniArmor = B(true);
        REQUIRE(rules.AssCumVisible(facts).value);
        REQUIRE(rules.VisibleAssCum(facts).value == 3);
        facts.andRanks.installed = B(true);
        facts.andRanks.rankIsOne[static_cast<std::size_t>(AndLocation::Ass)] = B(true);
        REQUIRE(rules.AssCumVisible(facts).value);
        facts.andRanks.rankIsOne[static_cast<std::size_t>(AndLocation::Ass)] = B(false);
        REQUIRE_FALSE(rules.AssCumVisible(facts).value);
    }

    SECTION("an AND rank only matters when AND is installed") {
        facts.cum.anal.value = 1;
        PresentAt(facts, ArmorSlot::Body);
        facts.andRanks.installed = B(false);
        facts.andRanks.rankIsOne[static_cast<std::size_t>(AndLocation::Ass)] = B(true);
        REQUIRE_FALSE(rules.AssCumVisible(facts).value);
        REQUIRE(rules.VisibleAssCum(facts).value == 0);
        facts.andRanks.installed = B(true);
        REQUIRE(rules.AssCumVisible(facts).value);
        REQUIRE(rules.VisibleAssCum(facts).value == 1);

        facts.cum.vaginal.value = 1;
        facts.playerIsMale = B(false);
        facts.andRanks.installed = B(false);
        facts.andRanks.rankIsOne[static_cast<std::size_t>(AndLocation::Genitals)] = B(true);
        REQUIRE_FALSE(rules.VaginalCumVisible(facts).value);
        REQUIRE(rules.VisibleVaginalCum(facts).value == 0);
        facts.andRanks.installed = B(true);
        REQUIRE(rules.VaginalCumVisible(facts).value);
    }

    SECTION("vaginal cum excludes males and uses genitals or an open body slot") {
        facts.cum.vaginal.value = 2;
        facts.playerIsMale = B(true);
        REQUIRE_FALSE(rules.VaginalCumVisible(facts).value);
        REQUIRE(rules.VisibleVaginalCum(facts).value == 0);
        facts.status = Status::Stale;
        REQUIRE(rules.VaginalCumVisible(facts).status == Status::NotReady);
        facts.status = Status::Ready;
        facts.playerIsMale = B(false);
        PresentAt(facts, ArmorSlot::Body);
        REQUIRE_FALSE(rules.VaginalCumVisible(facts).value);
        SetAndRank(facts, AndLocation::Genitals);
        REQUIRE(rules.VaginalCumVisible(facts).value);
        Slot(facts, ArmorSlot::Body).present = B(false);
        REQUIRE(rules.VaginalCumVisible(facts).value);
    }

    SECTION("oral cum is blocked by identity and by a closed head slot") {
        facts.cum.oral.value = 2;
        facts.worn.hidesIdentity = B(true);
        REQUIRE_FALSE(rules.OralCumVisible(facts).value);
        REQUIRE(rules.VisibleOralCum(facts).value == 0);
        facts.worn.hidesIdentity = B(false);
        PresentAt(facts, ArmorSlot::Head);
        REQUIRE_FALSE(rules.OralCumVisible(facts).value);
        Slot(facts, ArmorSlot::Head).bikiniArmor = B(true);
        REQUIRE(rules.OralCumVisible(facts).value);
        REQUIRE(rules.VisibleOralCum(facts).value == 2);
    }
}
TEST_CASE("bondage visibility follows heavy, lockable and anonymous legacy paths",
          "[gameplay][visibility]") {
    VisibilityRules rules;
    VisibilityFacts facts = VisibilityFixture();

    SECTION("heavy bondage reports the full legacy reset") {
        facts.worn.deviousHeavyBondage = B(true);
        facts.worn.deviousLockable = B(false);
        facts.worn.deviousBeltWorn = B(true);
        facts.worn.deviousCollarWorn = B(true);
        const BondageResult result = rules.EvaluateBondage(facts);
        REQUIRE(result.heavilyBound.value);
        REQUIRE_FALSE(result.lightlyBound.value);
        REQUIRE(result.visiblyBound.value);
        REQUIRE_FALSE(result.collarVisible.value);
        REQUIRE_FALSE(result.braVisible.value);
        REQUIRE_FALSE(result.beltVisible.value);
        REQUIRE_FALSE(result.harnessVisible.value);
        REQUIRE_FALSE(result.wearingUnhideable.value);
        REQUIRE(result.beltWorn.value);
        REQUIRE(result.collarWorn.value);
        facts.anonymous = B(true);
        REQUIRE_FALSE(rules.EvaluateBondage(facts).visiblyBound.value);
    }

    SECTION("lockable devices use slots 45, 49, 56 and 58 plus the belt fallback") {
        facts.worn.deviousLockable = B(true);
        PresentAt(facts, ArmorSlot::Collar);
        Slot(facts, ArmorSlot::Collar).deviousCollar = B(true);
        PresentAt(facts, ArmorSlot::Bra);
        Slot(facts, ArmorSlot::Bra).deviousBra = B(true);
        PresentAt(facts, ArmorSlot::Harness);
        Slot(facts, ArmorSlot::Harness).deviousBelt = B(true);
        Slot(facts, ArmorSlot::Harness).deviousHarness = B(true);
        const BondageResult result = rules.EvaluateBondage(facts);
        REQUIRE(result.lightlyBound.value);
        REQUIRE_FALSE(result.heavilyBound.value);
        REQUIRE(result.collarVisible.value);
        REQUIRE(result.braVisible.value);
        REQUIRE(result.beltVisible.value);
        REQUIRE(result.harnessVisible.value);
        REQUIRE(result.visiblyBound.value);
        facts.anonymous = B(true);
        REQUIRE_FALSE(rules.EvaluateBondage(facts).visiblyBound.value);
    }

    SECTION("a present belt slot wins before the harness fallback") {
        facts.worn.deviousLockable = B(true);
        PresentAt(facts, ArmorSlot::Belt);
        Slot(facts, ArmorSlot::Belt).deviousBelt = B(true);
        PresentAt(facts, ArmorSlot::Harness);
        Slot(facts, ArmorSlot::Harness).deviousBelt = B(false);
        REQUIRE(rules.EvaluateBondage(facts).beltVisible.value);

        Slot(facts, ArmorSlot::Belt).present = B(false);
        Slot(facts, ArmorSlot::Harness).deviousBelt = B(true);
        REQUIRE(rules.EvaluateBondage(facts).beltVisible.value);
    }

    SECTION("an accepted unhideable device is visibly bound") {
        facts.worn.deviousLockable = B(true);
        facts.worn.devious.gag = B(true);
        const BondageResult result = rules.EvaluateBondage(facts);
        REQUIRE(result.wearingUnhideable.value);
        REQUIRE(result.visiblyBound.value);
        REQUIRE(result.lightlyBound.value);
    }

    SECTION("a forbidden or cursed collar does not count") {
        facts.worn.deviousLockable = B(true);
        PresentAt(facts, ArmorSlot::Collar);
        Slot(facts, ArmorSlot::Collar).deviousCollar = B(true);
        facts.settings.allowCollarBoundFame = B(false);
        REQUIRE_FALSE(rules.EvaluateBondage(facts).collarVisible.value);
        facts.settings.allowCollarBoundFame = B(true);
        facts.settings.allowSlscCursedCollarBoundFame = B(false);
        facts.worn.slsCollarCurse = B(true);
        REQUIRE_FALSE(rules.EvaluateBondage(facts).collarVisible.value);
        facts.settings.allowSlscCursedCollarBoundFame = B(true);
        REQUIRE(rules.EvaluateBondage(facts).collarVisible.value);
    }
}
TEST_CASE("NPC scan continues after skips, rolls once per survivor and scores once",
          "[gameplay][witness]") {
    WitnessSettings settings = WitnessFixture();
    settings.npcNeedsLineOfSight = false;

    SECTION("a skipped or invalid NPC no longer aborts the remaining scan") {
        ScriptedRandom rng({1, 1});
        NpcScanner scanner(settings, rng);
        auto invalid = NpcFixture();
        invalid.validLocation = B(false);
        auto dead = NpcFixture();
        dead.dead = B(true);
        auto alive = NpcFixture();
        alive.relationshipRank = -1;
        const NpcScanResult result = scanner.Run({invalid, dead, alive});
        REQUIRE(result.status == Status::Ready);
        REQUIRE(result.entries.size() == 3);
        REQUIRE_FALSE(result.entries[0].rolled);
        REQUIRE(result.entries[0].reason == "invalid location");
        REQUIRE_FALSE(result.entries[1].rolled);
        REQUIRE(result.entries[1].reason == "dead");
        REQUIRE(result.entries[2].rolled);
        REQUIRE(result.success);
        REQUIRE(rng.calls == 1);
    }

    SECTION("an unknown fact pauses the scan without drawing for later NPCs") {
        ScriptedRandom rng({1});
        NpcScanner scanner(settings, rng);
        auto unknown = NpcFixture();
        unknown.relationshipKnown = U();
        auto later = NpcFixture();
        const NpcScanResult result = scanner.Run({unknown, later});
        REQUIRE(result.status == Status::NotReady);
        REQUIRE(result.entries.size() == 1);
        REQUIRE(rng.calls == 0);
        REQUIRE_FALSE(result.entries[0].rolled);
    }

    SECTION("a paused batch resumes without rerolling or scoring twice") {
        ScriptedRandom rng({1, 1, 1});
        NpcScanner scanner(settings, rng);
        auto first = NpcFixture();
        auto unknown = NpcFixture();
        unknown.relationshipKnown = U();
        auto later = NpcFixture();

        const NpcScanResult paused = scanner.Run({first, unknown, later});
        REQUIRE(paused.status == Status::NotReady);
        REQUIRE(paused.resumeIndex == 1);
        REQUIRE(paused.success);
        REQUIRE(paused.entries.size() == 2);
        REQUIRE(paused.entries[0].scored);
        REQUIRE(rng.calls == 1);

        unknown.relationshipKnown = B(true);
        const NpcScanResult resumed = scanner.Run({first, unknown, later});
        REQUIRE(resumed.status == Status::Ready);
        REQUIRE(resumed.success);
        REQUIRE(resumed.entries.size() == 3);
        REQUIRE(resumed.entries[0].scored);
        REQUIRE_FALSE(resumed.entries[1].scored);
        REQUIRE_FALSE(resumed.entries[2].scored);
        REQUIRE(resumed.entries[1].reason == "scan already succeeded");
        REQUIRE(resumed.entries[2].reason == "scan already succeeded");
        REQUIRE(rng.calls == 3);
        REQUIRE(rng.ranges[0] == std::pair<std::int32_t, std::int32_t>{1, 100});
        REQUIRE(rng.ranges[1] == std::pair<std::int32_t, std::int32_t>{1, 100});
        REQUIRE(rng.ranges[2] == std::pair<std::int32_t, std::int32_t>{1, 100});
    }

    SECTION("one success per scan while every survivor still rolls") {
        ScriptedRandom rng({1, 1, 1});
        NpcScanner scanner(settings, rng);
        auto first = NpcFixture();
        auto second = NpcFixture();
        auto third = NpcFixture();
        const NpcScanResult result = scanner.Run({first, second, third});
        REQUIRE(result.status == Status::Ready);
        REQUIRE(result.success);
        REQUIRE(rng.calls == 3);
        REQUIRE(result.entries[0].scored);
        REQUIRE_FALSE(result.entries[1].scored);
        REQUIRE_FALSE(result.entries[2].scored);
        REQUIRE(result.SuccessEntry() == &result.entries[0]);
    }

    SECTION("a reused scanner starts a fresh scan without a stale success flag") {
        ScriptedRandom rng({1, 100});
        NpcScanner scanner(settings, rng);
        const NpcScanResult first = scanner.Run({NpcFixture()});
        REQUIRE(first.success);

        const NpcScanResult second = scanner.Run({NpcFixture()});
        REQUIRE(second.status == Status::Ready);
        REQUIRE_FALSE(second.success);
        REQUIRE_FALSE(second.entries[0].scored);
        REQUIRE(rng.calls == 2);
    }

    SECTION("relationship ranks map to enemy, neutral, friend and lover") {
        ScriptedRandom rng({100, 100, 100, 100});
        NpcScanner scanner(settings, rng);
        std::vector<NpcScanTargetFacts> targets;
        for (const int rank : {-4, 0, 3, 4}) {
            auto target = NpcFixture();
            target.relationshipRank = rank;
            targets.push_back(target);
        }
        const NpcScanResult result = scanner.Run(targets);
        REQUIRE(result.entries[0].relationship == RelationshipGroup::Enemy);
        REQUIRE(result.entries[1].relationship == RelationshipGroup::Neutral);
        REQUIRE(result.entries[2].relationship == RelationshipGroup::Friend);
        REQUIRE(result.entries[2].friendOfPlayer);
        REQUIRE(result.entries[3].relationship == RelationshipGroup::Lover);
        REQUIRE(result.entries[3].loverOfPlayer);
    }
}
TEST_CASE("NPC scan applies the exact line-of-sight and detection gates",
          "[gameplay][witness]") {
    WitnessSettings settings = WitnessFixture();
    ScriptedRandom rng({1, 1, 1, 1});
    NpcScanner scanner(settings, rng);

    auto farNoLos = NpcFixture();
    farNoLos.lineOfSight = B(false);
    farNoLos.distance = ObservedDouble::Known(60.0);
    auto nearNoLos = NpcFixture();
    nearNoLos.lineOfSight = B(false);
    nearNoLos.distance = ObservedDouble::Known(10.0);
    auto detected = NpcFixture();
    detected.lineOfSight = B(true);
    detected.playerDetected = B(true);
    auto unseen = NpcFixture();
    unseen.lineOfSight = B(true);
    unseen.playerDetected = B(false);

    const NpcScanResult result = scanner.Run({farNoLos, nearNoLos, detected, unseen});
    REQUIRE(result.status == Status::Ready);
    REQUIRE(result.entries[0].reason == "no line of sight beyond minimum distance");
    REQUIRE_FALSE(result.entries[0].rolled);
    REQUIRE(result.entries[1].rolled);
    REQUIRE(result.entries[2].rolled);
    REQUIRE(result.entries[3].reason == "not detected");
    REQUIRE_FALSE(result.entries[3].rolled);
    REQUIRE(rng.calls == 2);

    ScriptedRandom unknownDistanceRng({1});
    NpcScanner unknownDistance(settings, unknownDistanceRng);
    auto noDistance = NpcFixture();
    noDistance.lineOfSight = B(false);
    noDistance.distance = ObservedDouble{};
    REQUIRE(unknownDistance.Run({noDistance}).status == Status::NotReady);
    REQUIRE(unknownDistanceRng.calls == 0);
}
TEST_CASE("fame gain roll always pays the betray draw and preserves canonical order",
          "[gameplay][witness]") {
    SECTION("invalid location and anonymity skip before any RNG") {
        FameGainRollFacts facts;
        facts.validLocation = B(false);
        facts.anonymous = B(false);
        ScriptedRandom rng({1});
        FameGainRoller roller(rng);
        const auto skipped = roller.Run(facts);
        REQUIRE(skipped.skipped);
        REQUIRE(skipped.status == Status::Ready);
        REQUIRE(rng.calls == 0);
    }

    SECTION("the betray roll is consumed even with zero candidates") {
        FameGainRollFacts facts;
        facts.validLocation = B(true);
        facts.anonymous = B(false);
        SetAllEligible(facts, false);
        ScriptedRandom rng({37});
        FameGainRoller roller(rng);
        const auto result = roller.Run(facts);
        REQUIRE(result.status == Status::Ready);
        REQUIRE(result.candidates.empty());
        REQUIRE(result.betrayRoll == 37);
        REQUIRE(rng.calls == 1);
    }

    SECTION("candidates follow canonical enum order and a single candidate skips selection") {
        FameGainRollFacts facts;
        facts.validLocation = B(true);
        facts.anonymous = B(false);
        SetAllEligible(facts, false);
        facts.eligible[static_cast<std::size_t>(FameCategory::Anal)] = B(true);
        facts.eligible[static_cast<std::size_t>(FameCategory::Whore)] = B(true);
        facts.eligible[static_cast<std::size_t>(FameCategory::Oral)] = B(true);
        ScriptedRandom rng({1, 1, 0});
        FameGainRoller roller(rng);
        const auto result = roller.Run(facts);
        REQUIRE(result.candidates.size() == 3);
        REQUIRE(result.candidates[0] == FameCategory::Whore);
        REQUIRE(result.candidates[1] == FameCategory::Oral);
        REQUIRE(result.candidates[2] == FameCategory::Anal);
        REQUIRE(result.candidateMask.Count() == 3);
        REQUIRE(result.candidateMask.Test(FameCategory::Whore));
        REQUIRE(result.candidateMask.Test(FameCategory::Oral));
        REQUIRE(result.candidateMask.Test(FameCategory::Anal));
        REQUIRE(rng.ranges[0] == std::pair<std::int32_t, std::int32_t>{1, 100});
        REQUIRE(rng.ranges[1] == std::pair<std::int32_t, std::int32_t>{1, 3});
        REQUIRE(rng.ranges[2] == std::pair<std::int32_t, std::int32_t>{0, 2});
        REQUIRE(result.selection.selected.size() == 1);
        REQUIRE(result.selection.selected[0] == FameCategory::Whore);
        REQUIRE(rng.calls == 3);

        FameGainRollFacts single;
        single.validLocation = B(true);
        single.anonymous = B(false);
        SetAllEligible(single, false);
        single.eligible[static_cast<std::size_t>(FameCategory::Anal)] = B(true);
        ScriptedRandom singleRng({5});
        FameGainRoller singleRoller(singleRng);
        const auto singleResult = singleRoller.Run(single);
        REQUIRE(singleResult.selection.selected.size() == 1);
        REQUIRE(singleResult.selection.selected[0] == FameCategory::Anal);
        REQUIRE(singleRng.calls == 1);
    }

    SECTION("an unknown candidate pauses selection after the betray draw") {
        FameGainRollFacts facts;
        facts.validLocation = B(true);
        facts.anonymous = B(false);
        SetAllEligible(facts, false);
        facts.eligible[static_cast<std::size_t>(FameCategory::Whore)] = U();
        facts.eligible[static_cast<std::size_t>(FameCategory::Anal)] = B(true);
        ScriptedRandom rng({1});
        FameGainRoller roller(rng);
        const auto result = roller.Run(facts);
        REQUIRE(result.status == Status::NotReady);
        REQUIRE(result.candidates.size() == 1);
        REQUIRE(result.candidates[0] == FameCategory::Anal);
        REQUIRE(rng.calls == 1);
    }

    SECTION("an ineligible category does not surface its forbidden high-fame unknown") {
        FameGainRollFacts facts;
        facts.validLocation = B(true);
        facts.anonymous = B(false);
        SetAllEligible(facts, false);
        facts.relationship = RelationshipGroup::Friend;
        facts.settings.friendBetrayChance = 10;
        facts.settings.friendsBetrayFromHighFame = true;
        facts.settings.friendHighFameThreshold = 100;
        facts.settings.forbiddenByFriend.Set(FameCategory::Whore);
        // currentFame[Whore] stays Unknown, but Whore is not eligible, so the
        // candidate set does not depend on it.
        ScriptedRandom rng({80});
        FameGainRoller roller(rng);
        const auto result = roller.Run(facts);
        REQUIRE(result.status == Status::Ready);
        REQUIRE(result.candidates.empty());
        REQUIRE(rng.calls == 1);
    }

    SECTION("friend betrayal applies the high-fame modifier before the chance test") {
        FameGainRollFacts facts;
        facts.validLocation = B(true);
        facts.anonymous = B(false);
        SetAllEligible(facts, false);
        facts.relationship = RelationshipGroup::Friend;
        facts.settings.friendBetrayChance = 10;
        facts.settings.friendsBetrayFromHighFame = true;
        facts.settings.friendHighFameThreshold = 100;
        facts.settings.forbiddenByFriend.Set(FameCategory::Whore);
        facts.currentFame[static_cast<std::size_t>(FameCategory::Whore)] =
            FameValue::Known(100);
        facts.eligible[static_cast<std::size_t>(FameCategory::Whore)] = B(true);
        // 55 - 50 == 5 passes only when the high-fame discount is applied.
        ScriptedRandom rng({55});
        FameGainRoller roller(rng);
        const auto result = roller.Run(facts);
        REQUIRE(result.status == Status::Ready);
        REQUIRE(result.candidates.size() == 1);
        REQUIRE(result.candidates[0] == FameCategory::Whore);
        REQUIRE(result.selection.selectedMask.Test(FameCategory::Whore));
        REQUIRE(rng.ranges == std::vector<std::pair<std::int32_t, std::int32_t>>{{1, 100}});
        REQUIRE(rng.calls == 1);
    }

    SECTION("the high-fame modifier persists across later prohibited categories") {
        FameGainRollFacts facts;
        facts.validLocation = B(true);
        facts.anonymous = B(false);
        SetAllEligible(facts, false);
        facts.relationship = RelationshipGroup::Friend;
        facts.settings.friendBetrayChance = 25;
        facts.settings.friendsBetrayFromHighFame = true;
        facts.settings.friendHighFameThreshold = 100;
        facts.settings.forbiddenByFriend.Set(FameCategory::Oral);
        facts.currentFame[static_cast<std::size_t>(FameCategory::Whore)] =
            FameValue::Known(100);
        facts.currentFame[static_cast<std::size_t>(FameCategory::Oral)] =
            FameValue::Known(0);
        facts.eligible[static_cast<std::size_t>(FameCategory::Whore)] = B(true);
        facts.eligible[static_cast<std::size_t>(FameCategory::Oral)] = B(true);
        // Whore is not prohibited but raises the shared modifier to 50; Oral is
        // prohibited and 60 - 50 == 10 <= 25, so legacy selects both categories.
        ScriptedRandom rng({60, 2});
        FameGainRoller roller(rng);
        const auto result = roller.Run(facts);
        REQUIRE(result.status == Status::Ready);
        REQUIRE(result.candidates.size() == 2);
        REQUIRE(result.candidates[0] == FameCategory::Whore);
        REQUIRE(result.candidates[1] == FameCategory::Oral);
        REQUIRE(result.candidateMask.Count() == 2);
        REQUIRE(result.selection.wanted == 2);
        REQUIRE(result.selection.selected.size() == 2);
        REQUIRE(result.selection.selected[0] == FameCategory::Whore);
        REQUIRE(result.selection.selected[1] == FameCategory::Oral);
        REQUIRE(result.selection.selectedMask.Count() == 2);
        REQUIRE(rng.ranges == std::vector<std::pair<std::int32_t, std::int32_t>>{{1, 100}, {1, 2}});
        REQUIRE(rng.calls == 2);
    }

    SECTION("a negative betray chance still respects an accumulated modifier") {
        FameGainRollFacts facts;
        facts.validLocation = B(true);
        facts.anonymous = B(false);
        SetAllEligible(facts, false);
        facts.relationship = RelationshipGroup::Friend;
        facts.settings.friendBetrayChance = -1;
        facts.settings.friendsBetrayFromHighFame = true;
        facts.settings.friendHighFameThreshold = 100;
        facts.settings.forbiddenByFriend.Set(FameCategory::Whore);
        facts.currentFame[static_cast<std::size_t>(FameCategory::Whore)] =
            FameValue::Known(100);
        facts.eligible[static_cast<std::size_t>(FameCategory::Whore)] = B(true);
        ScriptedRandom rng({40});
        FameGainRoller roller(rng);
        const auto result = roller.Run(facts);
        REQUIRE(result.candidates.size() == 1);
        REQUIRE(result.candidates[0] == FameCategory::Whore);
    }
}
TEST_CASE("candidate selection deduplicates and reports a 50-attempt shortfall",
          "[gameplay][witness]") {
    std::vector<std::int32_t> samples{2};
    for (int i = 0; i < 50; ++i) {
        samples.push_back(0);
    }
    ScriptedRandom rng(samples);
    CandidateSelector selector(rng);
    const FameSelectionResult result =
        selector.Select({FameCategory::Oral, FameCategory::Oral, FameCategory::Anal});
    REQUIRE(result.wanted == 2);
    REQUIRE(result.selected.size() == 1);
    REQUIRE(result.selected[0] == FameCategory::Oral);
    REQUIRE(result.attempts == 50);
    REQUIRE(result.duplicatesSkipped == 49);
    REQUIRE(result.shortfall);
    REQUIRE(rng.calls == 51);
    REQUIRE(rng.ranges[0] == std::pair<std::int32_t, std::int32_t>{1, 3});
    REQUIRE(rng.ranges[1] == std::pair<std::int32_t, std::int32_t>{0, 2});
    REQUIRE(result.selectedMask.Test(FameCategory::Oral));
}
TEST_CASE("candidate selection preserves order and masks every selected category",
          "[gameplay][witness]") {
    ScriptedRandom rng({2, 0, 2});
    CandidateSelector selector(rng);
    const FameSelectionResult result =
        selector.Select({FameCategory::Whore, FameCategory::Oral, FameCategory::Anal});
    REQUIRE(result.wanted == 2);
    REQUIRE(result.selected.size() == 2);
    REQUIRE(result.selected[0] == FameCategory::Whore);
    REQUIRE(result.selected[1] == FameCategory::Anal);
    REQUIRE(result.selectedMask.Count() == 2);
    REQUIRE_FALSE(result.shortfall);
    REQUIRE(rng.ranges[0] == std::pair<std::int32_t, std::int32_t>{1, 3});
    REQUIRE(rng.ranges[1] == std::pair<std::int32_t, std::int32_t>{0, 2});
}
TEST_CASE("direct SubAnalyze preserves explicit flags without grace or relationship rolls", "[game-ready][animation]") {
    auto facts = AnimationFixture();
    facts.startGraceElapsed = B(false);
    facts.actors.push_back(Partner(0, AnimationRace::Orc));
    facts.actors.push_back(Partner(0, AnimationRace::Orc));
    facts.actors[1].relationshipKnown = U();
    facts.actors[2].relationshipKnown = U();
    facts.sexWorker = B(true);
    facts.settings.forbiddenByFriend.Set(FameCategory::Whore);
    facts.settings.forbiddenByLover.Set(FameCategory::Whore);
    facts.settings.forbiddenByLover.Set(FameCategory::Slut);
    const auto result = AnimationClassifier{}.ClassifySubAnalyze(facts, true, true, true);
    REQUIRE(result.kind == AnimationResultKind::Analyzed);
    REQUIRE(result.foreplay);
    REQUIRE(result.kissAndTellRoll == -1);
    REQUIRE(std::find(result.categories.begin(), result.categories.end(), FameCategory::Whore) == result.categories.end());
    REQUIRE(std::find(result.categories.begin(), result.categories.end(), FameCategory::Slut) != result.categories.end());
    REQUIRE(std::count(result.categories.begin(), result.categories.end(), FameCategory::LikesOrc) == 2);
}

TEST_CASE("animation classification keeps the analyzer gate order and SubAnalyze order",
          "[gameplay][animation]") {
    AnimationClassifier classifier;

    SECTION("anonymous, invalid location and ended scenes skip without RNG") {
        AnimationFacts facts = AnimationFixture();
        facts.anonymous = B(true);
        ScriptedRandom rng({1});
        const auto result = classifier.Classify(facts, rng);
        REQUIRE(result.kind == AnimationResultKind::Skipped);
        REQUIRE(rng.calls == 0);

        facts = AnimationFixture();
        facts.validLocation = B(false);
        REQUIRE(classifier.Classify(facts, rng).kind == AnimationResultKind::Skipped);
        facts = AnimationFixture();
        facts.sceneEnded = B(true);
        REQUIRE(classifier.Classify(facts, rng).kind == AnimationResultKind::Skipped);
    }

    SECTION("an invalid snapshot is invalid rather than merely deferred") {
        AnimationFacts facts = AnimationFixture();
        facts.status = Status::Invalid;
        ScriptedRandom rng({1});
        const auto result = classifier.Classify(facts, rng);
        REQUIRE(result.status == Status::Invalid);
        REQUIRE(result.kind == AnimationResultKind::Invalid);
        REQUIRE(rng.calls == 0);
    }

    SECTION("the ten second start grace defers before any scoring roll") {
        AnimationFacts facts = AnimationFixture();
        facts.startGraceElapsed = B(false);
        ScriptedRandom rng({1});
        const auto early = classifier.Classify(facts, rng);
        REQUIRE(early.status == Status::Ready);
        REQUIRE(early.kind == AnimationResultKind::Deferred);
        REQUIRE(rng.calls == 0);

        facts.startGraceElapsed = U();
        const auto unknown = classifier.Classify(facts, rng);
        REQUIRE(unknown.status == Status::NotReady);
        REQUIRE(unknown.kind == AnimationResultKind::Deferred);
        REQUIRE(rng.calls == 0);
        REQUIRE(kAnimationStartGraceSeconds == 10.0);
    }

    SECTION("lead-in without foreplay fails, with foreplay continues") {
        AnimationFacts facts = AnimationFixture();
        facts.leadIn = B(true);
        ScriptedRandom rng({1});
        REQUIRE(classifier.Classify(facts, rng).kind == AnimationResultKind::Skipped);
        facts.allowForeplayFame = B(true);
        REQUIRE(classifier.Classify(facts, rng).foreplay);
        REQUIRE(rng.calls == 0);
    }

    SECTION("a single actor triggers the NPC scan with no kiss roll") {
        AnimationFacts facts = AnimationFixture();
        ScriptedRandom rng({1});
        const auto result = classifier.Classify(facts, rng);
        REQUIRE(result.kind == AnimationResultKind::SingleActorScan);
        REQUIRE(result.triggerNpcScan);
        REQUIRE(rng.calls == 0);
    }

    SECTION("friends and lovers pay one kiss-and-tell roll before partner checks") {
        AnimationFacts friends = AnimationFixture();
        friends.actors.push_back(Partner(2));
        friends.friendKissAndTellChance = 0;
        ScriptedRandom friendRng({1});
        const auto friendResult = classifier.Classify(friends, friendRng);
        REQUIRE(friendResult.kind == AnimationResultKind::NoScoring);
        REQUIRE(friendResult.allFriends);
        REQUIRE(friendRng.calls == 1);
        REQUIRE(friendRng.ranges[0] == std::pair<std::int32_t, std::int32_t>{1, 100});

        AnimationFacts lovers = AnimationFixture();
        lovers.actors.push_back(Partner(4));
        ScriptedRandom loverRng({1});
        const auto loverResult = classifier.Classify(lovers, loverRng);
        REQUIRE(loverResult.kind == AnimationResultKind::Analyzed);
        REQUIRE(loverResult.allLovers);
        REQUIRE(loverResult.loverScoring);
        REQUIRE(loverRng.calls == 1);
        REQUIRE(loverRng.ranges[0] == std::pair<std::int32_t, std::int32_t>{1, 100});
        REQUIRE(!loverResult.categories.empty());
    }

    SECTION("friend and lover masks gate categories without changing append order") {
        AnimationFacts facts = AnimationFixture();
        facts.actors.push_back(Partner(2));
        facts.sexWorker = B(true);
        facts.settings.forbiddenByFriend.Set(FameCategory::Whore);
        ScriptedRandom rng({1});
        const auto result = classifier.Classify(facts, rng);
        REQUIRE(result.kind == AnimationResultKind::Analyzed);
        REQUIRE(std::find(result.categories.begin(), result.categories.end(),
                          FameCategory::Whore) == result.categories.end());
        REQUIRE(std::find(result.categories.begin(), result.categories.end(),
                          FameCategory::Slut) != result.categories.end());

        facts.settings.forbiddenByFriend.Set(FameCategory::Slut);
        const auto blocked = classifier.Classify(facts, rng);
        REQUIRE(std::find(blocked.categories.begin(), blocked.categories.end(),
                          FameCategory::Slut) == blocked.categories.end());
    }

    SECTION("victim-only categories honour the allow flag and Papyrus precedence") {
        AnimationFacts facts = AnimationFixture();
        facts.actors.push_back(Partner(0, AnimationRace::Orc));
        facts.victim = B(true);
        facts.allowLikeFameWhenRaped = B(false);
        ScriptedRandom rng({1});
        const auto blocked = classifier.Classify(facts, rng);
        REQUIRE(std::find(blocked.categories.begin(), blocked.categories.end(),
                          FameCategory::LikesOrc) == blocked.categories.end());

        facts.allowLikeFameWhenRaped = B(true);
        const auto allowed = classifier.Classify(facts, rng);
        REQUIRE(std::find(allowed.categories.begin(), allowed.categories.end(),
                          FameCategory::LikesOrc) != allowed.categories.end());

        facts.victim = B(false);
        facts.allowLikeFameWhenRaped = B(false);
        const auto nonVictim = classifier.Classify(facts, rng);
        REQUIRE(std::find(nonVictim.categories.begin(), nonVictim.categories.end(),
                          FameCategory::LikesOrc) != nonVictim.categories.end());
    }

    SECTION("a lockable devious actor adds Sadist only after the relationship gate") {
        AnimationFacts facts = AnimationFixture();
        AnimationActorFacts actor = Partner(2);
        actor.devious.lockable = B(true);
        actor.devious.gag = B(true);
        facts.actors.push_back(actor);
        facts.deviousDevicesInstalled = B(true);
        ScriptedRandom rng({1});
        const auto allowed = classifier.Classify(facts, rng);
        REQUIRE(std::find(allowed.categories.begin(), allowed.categories.end(),
                          FameCategory::Sadist) != allowed.categories.end());

        facts.settings.forbiddenByFriend.Set(FameCategory::Sadist);
        const auto blocked = classifier.Classify(facts, rng);
        REQUIRE(std::find(blocked.categories.begin(), blocked.categories.end(),
                          FameCategory::Sadist) == blocked.categories.end());
    }

    SECTION("SubAnalyze appends in the legacy order and keeps repeated races") {
        AnimationFacts facts = AnimationFixture();
        facts.tags = {"Oral", "Anal", "Orgy"};
        facts.actors.push_back(Partner(0, AnimationRace::Orc));
        facts.actors.push_back(Partner(0, AnimationRace::Orc));
        ScriptedRandom rng({1});
        const auto result = classifier.Classify(facts, rng);
        REQUIRE(result.kind == AnimationResultKind::Analyzed);
        std::vector<FameCategory> expected = {
            FameCategory::Oral, FameCategory::Anal, FameCategory::Group,
            FameCategory::Slut, FameCategory::Dominant, FameCategory::LikesOrc,
            FameCategory::LikesOrc};
        REQUIRE(result.categories == expected);
    }

    SECTION("an unknown relationship pauses after the kiss roll and clears categories") {
        AnimationFacts facts = AnimationFixture();
        auto actor = Partner(2);
        actor.relationshipKnown = U();
        facts.actors.push_back(actor);
        ScriptedRandom rng({1});
        const auto result = classifier.Classify(facts, rng);
        REQUIRE(result.status == Status::NotReady);
        REQUIRE(result.kind == AnimationResultKind::Deferred);
        REQUIRE(rng.calls == 1);
        REQUIRE(result.categories.empty());
    }

    SECTION("an unknown SubAnalyze fact defers and clears known categories") {
        AnimationFacts facts = AnimationFixture();
        facts.actors.push_back(Partner(0));
        facts.tags = {"Oral"};
        facts.canGainBoundFame = U();
        ScriptedRandom rng({1});
        const auto result = classifier.Classify(facts, rng);
        REQUIRE(result.status == Status::NotReady);
        REQUIRE(result.categories.empty());
    }
}
TEST_CASE("comments schedule and roll exactly like CommentManager", "[gameplay][comments]") {
    CommentRules rules;

    SECTION("an invalid snapshot is invalid and never draws") {
        CommentFacts facts = CommentFixture();
        facts.status = Status::Invalid;
        ScriptedRandom rng({1});
        const auto result = rules.Evaluate(facts, rng);
        REQUIRE(result.status == Status::Invalid);
        REQUIRE_FALSE(result.rolled);
        REQUIRE(rng.calls == 0);
    }

    SECTION("anonymous ticks do not roll and reschedule after ten seconds") {
        CommentFacts facts = CommentFixture();
        facts.anonymous = B(true);
        ScriptedRandom rng({1});
        const auto result = rules.Evaluate(facts, rng);
        REQUIRE(result.status == Status::Ready);
        REQUIRE_FALSE(result.allowed);
        REQUIRE_FALSE(result.rolled);
        REQUIRE(result.nextDelaySeconds == kCommentAnonymousDelaySeconds);
        REQUIRE(rng.calls == 0);
    }

    SECTION("combat ticks do not roll and reschedule after five seconds") {
        CommentFacts facts = CommentFixture();
        facts.combatState = ObservedNumber::Known(1);
        ScriptedRandom rng({1});
        const auto result = rules.Evaluate(facts, rng);
        REQUIRE_FALSE(result.allowed);
        REQUIRE_FALSE(result.rolled);
        REQUIRE(result.nextDelaySeconds == 5.0);
        REQUIRE(rng.calls == 0);
    }

    SECTION("normal ticks roll 1..100 against the integer frequency") {
        CommentFacts facts = CommentFixture();
        facts.commentFrequency = ObservedNumber::Known(50);
        ScriptedRandom pass({50});
        REQUIRE(rules.Evaluate(facts, pass).allowed);
        REQUIRE(pass.calls == 1);
        REQUIRE(pass.ranges[0] == std::pair<std::int32_t, std::int32_t>{1, 100});
        ScriptedRandom fail({51});
        REQUIRE_FALSE(rules.Evaluate(facts, fail).allowed);
    }

    SECTION("naked comment chance follows the PublicWhore and AND branches") {
        CommentFacts facts = CommentFixture();
        facts.publicWhore = B(true);
        facts.disableNakedCommentsWhilePW = B(true);
        ScriptedRandom rng({1});
        const auto suppressed = rules.Evaluate(facts, rng);
        REQUIRE(suppressed.naked.changes);
        REQUIRE(suppressed.naked.chance == -1);
        facts.publicWhore = B(false);
        facts.andInstalled = B(false);
        const auto forced = rules.Evaluate(facts, rng);
        REQUIRE(forced.naked.changes);
        REQUIRE(forced.naked.chance == 100);
    }
}
