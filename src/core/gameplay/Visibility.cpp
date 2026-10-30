#include "slsf/core/gameplay/Visibility.h"

#include <algorithm>
#include <initializer_list>
#include <string>

namespace slsf::gameplay {
namespace {

const ItemFacts& Item(const VisibilityFacts& facts, ArmorSlot slot) {
    return facts.items[static_cast<std::size_t>(slot)];
}
Tri Present(const ItemFacts& item) {
    return TriOf(item.present);
}
Tri Property(const ItemFacts& item, const Fact& property) {
    const Tri present = Present(item);
    if (present == Tri::False) {
        return Tri::False;
    }
    if (present == Tri::Unknown) {
        return Tri::Unknown;
    }
    return TriOf(property);
}
Tri Bikini(const ItemFacts& item) { return Property(item, item.bikiniArmor); }
Tri DoesNotCoverBack(const ItemFacts& item) {
    return Property(item, item.doesNotCoverBack);
}Tri DoesNotHideIdentity(const ItemFacts& item) {
    return Property(item, item.doesNotHideIdentity);
}
Tri AnyRank(const VisibilityFacts& facts,
            std::initializer_list<AndLocation> locations) {
    Tri result = Tri::False;
    for (const AndLocation location : locations) {
        result = TriOr(result,
                       TriOf(facts.andRanks.rankIsOne[static_cast<std::size_t>(location)]));
    }
    return result;
}
Tri AndCondition(const VisibilityFacts& facts,
                 std::initializer_list<AndLocation> locations) {
    return TriAnd(TriOf(facts.andRanks.installed), AnyRank(facts, locations));
}
Tri BackSlots(const VisibilityFacts& facts, bool requireBikiniUpper) {
    const ItemFacts& upper = Item(facts, ArmorSlot::BackUpper);
    const ItemFacts& lower = Item(facts, ArmorSlot::BackLower);
    // Slot 46 is optional, but a present slot 47 that covers the back blocks the
    // tattoo even when slot 46 is empty.  The legacy script evaluates the lower
    // slot on both sides of the upper-slot branch.
    const Tri lowerOpen =
        TriOr(TriNot(Present(lower)), DoesNotCoverBack(lower));
    const Tri upperOpen = requireBikiniUpper ? Bikini(upper)
                                             : DoesNotCoverBack(upper);
    return TriAnd(lowerOpen, TriOr(TriNot(Present(upper)), upperOpen));
}
Tri HeadGate(const VisibilityFacts& facts) {
    const ItemFacts& head = Item(facts, ArmorSlot::Head);
    return TriOr(TriNot(Present(head)),
                 TriOr(Bikini(head), DoesNotHideIdentity(head)));
}
Tri IdentityBlocked(const VisibilityFacts& facts) {
    return TriOr(TriOf(facts.worn.hidesIdentity),
                 TriAnd(TriOf(facts.mods.deviousDevices),
                        TriOf(facts.worn.deviousHood)));
}
Tri TattooEarly(const VisibilityFacts& facts, const TattooAreaFacts& area,
                std::size_t slot, std::string_view name) {
    if (facts.status != Status::Ready) {
        return Tri::Unknown;
    }
    const Tri installed = TriOf(area.installed);
    if (installed == Tri::False) {
        return Tri::False;
    }
    if (installed == Tri::Unknown) {
        return Tri::Unknown;
    }
    if (!area.appliedCountKnown.IsKnownTrue()) {
        // A known-false "count known" fact means the count itself is unavailable;
        // the numeric field must not be treated as an observed zero.
        return Tri::Unknown;
    }
    if (area.appliedCount <= 0) {
        return Tri::False;
    }
    if (slot >= area.slots.size()) {
        return Tri::Unknown;
    }
    const TattooSlotFacts& entry = area.slots[slot];
    if (entry.applied.IsKnownFalse()) {
        return Tri::False;
    }
    if (!entry.applied.IsKnownTrue()) {
        return Tri::Unknown;
    }
    (void)name;
    return Tri::True;
}
RuleDecision FromTriChecked(const VisibilityFacts& facts, Tri value,
                            std::string reason) {
    if (facts.status == Status::Invalid) {
        return RuleDecision::Invalid("visibility snapshot invalid");
    }
    if (facts.status != Status::Ready) {
        // Stale and incompatible snapshots are just as unusable as missing
        // ones: they must not turn a computed Tri into a Known decision.
        return RuleDecision::NotReady("visibility snapshot not ready");
    }
    return RuleDecision::FromTri(value, std::move(reason));
}
NumericDecision FromTriCount(const VisibilityFacts& facts, Tri gate,
                             const CountFacts& count, std::string reason) {
    if (facts.status == Status::Invalid) {
        return NumericDecision::Invalid("visibility snapshot invalid");
    }
    if (facts.status != Status::Ready) {
        return NumericDecision::NotReady("visibility snapshot not ready");
    }
    if (gate == Tri::False) {
        return NumericDecision::Known(0, std::move(reason));
    }
    if (gate == Tri::Unknown) {
        return NumericDecision::NotReady(std::move(reason));
    }
    if (!count.known.IsKnownTrue()) {
        return NumericDecision::NotReady("cum count unknown");
    }
    return NumericDecision::Known(count.value, std::move(reason));
}
struct SelectedCount {
    Tri positive{Tri::Unknown};
    int value{};
};

SelectedCount SelectCount(const VisibilityFacts& facts, const CountFacts& count) {
    const Tri source = TriOf(facts.mods.slacs);
    if (source == Tri::Unknown) {
        return {};
    }
    if (!count.known.IsKnownTrue()) {
        return {};
    }
    return SelectedCount{count.value > 0 ? Tri::True : Tri::False, count.value};
}
Tri AssGate(const VisibilityFacts& facts) {
    const Tri andInstalled = TriOf(facts.andRanks.installed);
    const Tri bodyPresent = Present(Item(facts, ArmorSlot::Body));
    const Tri fallback =
        TriOr(TriNot(bodyPresent),
              TriAnd(TriOf(facts.mods.sls),
                     TriAnd(bodyPresent, Bikini(Item(facts, ArmorSlot::Body)))));
    return TriOr(AndCondition(facts, {AndLocation::Ass}),
                 TriAnd(TriNot(andInstalled), fallback));
}
Tri VaginalGate(const VisibilityFacts& facts) {
    const Tri andInstalled = TriOf(facts.andRanks.installed);
    return TriOr(AndCondition(facts, {AndLocation::Genitals}),
                 TriAnd(TriNot(andInstalled),
                        TriNot(Present(Item(facts, ArmorSlot::Body)))));
}
Tri OralGate(const VisibilityFacts& facts) {
    return TriAnd(TriNot(IdentityBlocked(facts)), HeadGate(facts));
}
RuleDecision Decision(const VisibilityFacts& facts, Tri value,
                      std::string reason) {
    return FromTriChecked(facts, value, std::move(reason));
}
}
std::vector<Tri> DeviousWornFacts::AnyUnhideable() const {
    return {
        TriOf(hood), TriOf(gag), TriOf(gagPanel), TriOf(armCuffs),
        TriOf(armCuffsFront), TriOf(armbinder), TriOf(armbinderElbow),
        TriOf(gloves), TriOf(legCuffs), TriOf(boots), TriOf(suit),
        TriOf(corset), TriOf(blindfold),
    };
}
RuleDecision VisibilityRules::VisibilityReady(const VisibilityFacts& facts,
                                              std::string_view what) const {
    switch (facts.status) {
    case Status::Ready:
        return RuleDecision::Known(true, std::string(what));
    case Status::Invalid:
        return RuleDecision::Invalid(std::string(what) + ": snapshot invalid");
    case Status::Missing:
        return RuleDecision::NotReady(std::string(what) + ": snapshot missing");
    case Status::NotReady:
    case Status::Stale:
    case Status::Incompatible:
        return RuleDecision::NotReady(std::string(what) + ": snapshot not ready");
    }
    return RuleDecision::NotReady(std::string(what));
}
RuleDecision VisibilityRules::BodyTattooVisible(const VisibilityFacts& facts,
                                                std::size_t slot) const {
    const TattooAreaFacts& area = facts.tattoos.body;
    const Tri early = TattooEarly(facts, area, slot, "body");
    if (early != Tri::True) {
        return Decision(facts, early, "body tattoo early gate");
    }

    const Tri sls = TriOf(facts.mods.sls);
    const Tri andInstalled = TriOf(facts.andRanks.installed);
    if (sls == Tri::Unknown || andInstalled == Tri::Unknown) {
        return Decision(facts, Tri::Unknown, "tattoo mod branch unknown");
    }

    const TattooSubcategory subcategory = area.slots[slot].subcategory;
    if (subcategory == TattooSubcategory::Unknown) {
        return RuleDecision::Invalid("unknown body tattoo subcategory");
    }

    const ItemFacts& body = Item(facts, ArmorSlot::Body);
    const ItemFacts& upper = Item(facts, ArmorSlot::BackUpper);
    const ItemFacts& lower = Item(facts, ArmorSlot::BackLower);

    if (sls == Tri::True && andInstalled == Tri::True) {
        switch (subcategory) {
        case TattooSubcategory::None:
            return Decision(
                facts,
                TriOr(AnyRank(facts, {AndLocation::Chest, AndLocation::Genitals,
                                      AndLocation::Bra, AndLocation::Underwear}),
                      TriAnd(Present(body), Bikini(body))),
                "branch A none");
        case TattooSubcategory::Chest:
            return Decision(facts,
                            AnyRank(facts, {AndLocation::Chest, AndLocation::Bra}),
                            "branch A chest");
        case TattooSubcategory::Pelvis:
            return Decision(facts,
                            AnyRank(facts,
                                    {AndLocation::Genitals, AndLocation::Underwear}),
                            "branch A pelvis");
        case TattooSubcategory::Ass:
            return Decision(facts, AnyRank(facts, {AndLocation::Ass}),
                            "branch A ass");
        case TattooSubcategory::Back: {
            const Tri bodyGate = TriOr(
                TriNot(Present(body)),
                TriOr(Bikini(body),
                      AnyRank(facts, {AndLocation::Chest, AndLocation::Bra})));
            return Decision(
                facts,
                TriAnd(TriNot(TriOf(facts.worn.coversBack)),
                       TriAnd(bodyGate, BackSlots(facts, true))),
                "branch A back");
        }
        case TattooSubcategory::Thigh: {
            const Tri blocked =
                TriOr(Present(Item(facts, ArmorSlot::Thigh)),
                      TriAnd(Present(body), TriNot(Bikini(body))));
            return Decision(facts, TriNot(blocked), "branch A thigh");
        }
        case TattooSubcategory::Calves:
            return Decision(
                facts,
                TriNot(TriOr(Present(Item(facts, ArmorSlot::Feet)),
                             Present(Item(facts, ArmorSlot::Calves)))),
                "branch A calves");
        case TattooSubcategory::Unknown:
            break;
        }
        return RuleDecision::Invalid("unknown body tattoo subcategory");
    }

    if (sls == Tri::False && andInstalled == Tri::True) {
        switch (subcategory) {
        case TattooSubcategory::None:
            return Decision(facts,
                            AnyRank(facts,
                                    {AndLocation::Chest, AndLocation::Bra,
                                     AndLocation::Underwear}),
                            "branch B none");
        case TattooSubcategory::Chest:
            return Decision(facts,
                            AnyRank(facts, {AndLocation::Chest, AndLocation::Bra}),
                            "branch B chest");
        case TattooSubcategory::Pelvis:
            return Decision(facts,
                            AnyRank(facts,
                                    {AndLocation::Genitals, AndLocation::Underwear}),
                            "branch B pelvis");
        case TattooSubcategory::Ass:
            return Decision(facts, AnyRank(facts, {AndLocation::Ass}),
                            "branch B ass");
        case TattooSubcategory::Back:
            return Decision(
                facts,
                TriAnd(TriNot(TriOf(facts.worn.coversBack)),
                       TriAnd(AnyRank(facts,
                                      {AndLocation::Chest, AndLocation::Bra}),
                              BackSlots(facts, false))),
                "branch B back");
        case TattooSubcategory::Thigh:
            return Decision(
                facts,
                TriNot(TriOr(Present(Item(facts, ArmorSlot::Thigh)),
                             Present(body))),
                "branch B thigh");
        case TattooSubcategory::Calves:
            return Decision(
                facts,
                TriNot(TriOr(Present(Item(facts, ArmorSlot::Feet)),
                             Present(Item(facts, ArmorSlot::Calves)))),
                "branch B calves");
        case TattooSubcategory::Unknown:
            break;
        }
        return RuleDecision::Invalid("unknown body tattoo subcategory");
    }

    if (sls == Tri::True && andInstalled == Tri::False) {
        const Tri bodyGate = TriOr(TriNot(Present(body)), Bikini(body));
        const Tri upperGate =
            TriOr(TriNot(Present(upper)),
                  TriOr(Bikini(upper), DoesNotCoverBack(upper)));
        const Tri lowerGate =
            TriOr(TriNot(Present(lower)), DoesNotCoverBack(lower));
        return Decision(facts, TriAll({bodyGate, upperGate, lowerGate}),
                        "branch C");
    }

    return Decision(facts, TriNot(Present(body)), "branch D body absent");
}
RuleDecision VisibilityRules::FaceTattooVisible(const VisibilityFacts& facts,
                                                std::size_t slot) const {
    const TattooAreaFacts& area = facts.tattoos.face;
    const Tri early = TattooEarly(facts, area, slot, "face");
    if (early != Tri::True) {
        return Decision(facts, early, "face tattoo early gate");
    }

    const Tri anonymityEnabled = TriOf(facts.settings.anonymityEnabled);
    if (anonymityEnabled == Tri::Unknown) {
        return Decision(facts, Tri::Unknown, "face anonymity branch unknown");
    }

    if (anonymityEnabled == Tri::True) {
        const Tri dynamic = TriOf(facts.settings.dynamicAnonymity);
        if (dynamic == Tri::Unknown) {
            return Decision(facts, Tri::Unknown, "face anonymity branch unknown");
        }
        if (dynamic == Tri::False) {
            const Tri anonymous = TriOf(facts.anonymous);
            if (anonymous == Tri::True) {
                return RuleDecision::Known(false, "face hidden by anonymity");
            }
            if (anonymous == Tri::Unknown) {
                return RuleDecision::NotReady("anonymous state unknown");
            }
            return RuleDecision::Known(true,
                                       "face visible outside identity chain");
        }
    }

    // Dynamic anonymity and disabled anonymity both use the worn-identity
    // chain; only the static branch consults IsPlayerAnonymous.
    return Decision(facts,
                    TriAnd(TriNot(IdentityBlocked(facts)), HeadGate(facts)),
                    "face identity chain");
}
RuleDecision VisibilityRules::HandTattooVisible(const VisibilityFacts& facts,
                                                std::size_t slot) const {
    const TattooAreaFacts& area = facts.tattoos.hands;
    const Tri early = TattooEarly(facts, area, slot, "hands");
    if (early != Tri::True) {
        return Decision(facts, early, "hand tattoo early gate");
    }
    const ItemFacts& hands = Item(facts, ArmorSlot::Hands);
    // The legacy hand rule uses DoesNotCoverHands, not DoesNotHideIdentity.
    const Tri coveredHands =
        TriAnd(Present(hands), TriNot(Property(hands, hands.doesNotCoverHands)));
    return Decision(facts,
                    TriAnd(TriNot(TriOf(facts.worn.coversHands)),
                           TriNot(coveredHands)),
                    "hand tattoo visibility");
}
RuleDecision VisibilityRules::FootTattooVisible(const VisibilityFacts& facts,
                                                std::size_t slot) const {
    const TattooAreaFacts& area = facts.tattoos.feet;
    const Tri early = TattooEarly(facts, area, slot, "feet");
    if (early != Tri::True) {
        return Decision(facts, early, "foot tattoo early gate");
    }
    const ItemFacts& feet = Item(facts, ArmorSlot::Feet);
    const Tri coveredFeet =
        TriAnd(Present(feet), TriNot(Property(feet, feet.doesNotCoverFeet)));
    return Decision(facts,
                    TriAnd(TriNot(TriOf(facts.worn.coversFeet)),
                           TriNot(coveredFeet)),
                    "foot tattoo visibility");
}
RuleDecision VisibilityRules::AssCumVisible(const VisibilityFacts& facts) const {
    const SelectedCount count = SelectCount(facts, facts.cum.anal);
    if (count.positive == Tri::False) {
        return Decision(facts, Tri::False, "no anal cum");
    }
    const Tri gate = AssGate(facts);
    return Decision(facts, TriAnd(count.positive, gate), "anal cum visibility");
}
NumericDecision VisibilityRules::VisibleAssCum(const VisibilityFacts& facts) const {
    const Tri gate = AssGate(facts);
    return FromTriCount(facts, gate, facts.cum.anal, "visible anal cum");
}
RuleDecision VisibilityRules::VaginalCumVisible(const VisibilityFacts& facts) const {
    const Tri male = TriOf(facts.playerIsMale);
    if (male == Tri::True) {
        // Route the early exclusion through the snapshot gate too: a stale or
        // invalid snapshot must not produce a known zero observation.
        return Decision(facts, Tri::False, "males excluded");
    }
    if (male == Tri::Unknown) {
        return Decision(facts, Tri::Unknown, "player sex unknown");
    }
    const SelectedCount count = SelectCount(facts, facts.cum.vaginal);
    if (count.positive == Tri::False) {
        return Decision(facts, Tri::False, "no vaginal cum");
    }
    return Decision(facts, TriAnd(count.positive, VaginalGate(facts)),
                    "vaginal cum visibility");
}
NumericDecision VisibilityRules::VisibleVaginalCum(const VisibilityFacts& facts) const {
    const Tri male = TriOf(facts.playerIsMale);
    // Both early exits still honour the snapshot status through FromTriCount.
    if (male == Tri::True) {
        return FromTriCount(facts, Tri::False, facts.cum.vaginal, "males excluded");
    }
    if (male == Tri::Unknown) {
        return FromTriCount(facts, Tri::Unknown, facts.cum.vaginal,
                            "player sex unknown");
    }
    return FromTriCount(facts, VaginalGate(facts), facts.cum.vaginal,
                        "visible vaginal cum");
}
RuleDecision VisibilityRules::OralCumVisible(const VisibilityFacts& facts) const {
    const SelectedCount count = SelectCount(facts, facts.cum.oral);
    if (count.positive == Tri::False) {
        return Decision(facts, Tri::False, "no oral cum");
    }
    return Decision(facts, TriAnd(count.positive, OralGate(facts)),
                    "oral cum visibility");
}
NumericDecision VisibilityRules::VisibleOralCum(const VisibilityFacts& facts) const {
    return FromTriCount(facts, OralGate(facts), facts.cum.oral,
                        "visible oral cum");
}
BondageResult VisibilityRules::EvaluateBondage(const VisibilityFacts& facts) const {
    BondageResult result{};
    const auto make = [&](Tri value, std::string reason) {
        return Decision(facts, value, std::move(reason));
    };

    const Tri ddInstalled = TriOf(facts.mods.deviousDevices);
    const Tri beltWorn = TriOf(facts.worn.deviousBeltWorn);
    const Tri collarWorn = TriOf(facts.worn.deviousCollarWorn);

    if (ddInstalled == Tri::False) {
        result.lightlyBound = make(Tri::False, "DD not installed");
        result.heavilyBound = make(Tri::False, "DD not installed");
        result.visiblyBound = make(Tri::False, "DD not installed");
        result.wearingUnhideable = make(Tri::False, "DD not installed");
        result.beltWorn = make(beltWorn, "belt keyword");
        result.collarWorn = make(collarWorn, "collar keyword");
        result.collarVisible = make(Tri::False, "DD not installed");
        result.braVisible = make(Tri::False, "DD not installed");
        result.beltVisible = make(Tri::False, "DD not installed");
        result.harnessVisible = make(Tri::False, "DD not installed");
        return result;
    }
    if (ddInstalled == Tri::Unknown) {
        result.lightlyBound = make(Tri::Unknown, "DD install unknown");
        result.heavilyBound = make(Tri::Unknown, "DD install unknown");
        result.visiblyBound = make(Tri::Unknown, "DD install unknown");
        result.wearingUnhideable = make(Tri::Unknown, "DD install unknown");
        result.beltWorn = make(beltWorn, "belt keyword");
        result.collarWorn = make(collarWorn, "collar keyword");
        result.collarVisible = make(Tri::Unknown, "DD install unknown");
        result.braVisible = make(Tri::Unknown, "DD install unknown");
        result.beltVisible = make(Tri::Unknown, "DD install unknown");
        result.harnessVisible = make(Tri::Unknown, "DD install unknown");
        return result;
    }

    const Tri heavy = TriOf(facts.worn.deviousHeavyBondage);
    const Tri lockable = TriOf(facts.worn.deviousLockable);
    if (heavy == Tri::True) {
        result.lightlyBound = make(Tri::False, "heavy bondage");
        result.heavilyBound = make(Tri::True, "heavy bondage");
        result.visiblyBound =
            make(TriAnd(Tri::True, TriNot(TriOf(facts.anonymous))),
                 "heavy bondage visible");
        // Legacy resets all per-item visibility flags before the heavy-bondage
        // branch, so they are observably false rather than unexamined.
        result.wearingUnhideable = make(Tri::False, "not examined under heavy bondage");
        result.beltWorn = make(beltWorn, "belt keyword");
        result.collarWorn = make(collarWorn, "collar keyword");
        result.collarVisible = make(Tri::False, "not examined under heavy bondage");
        result.braVisible = make(Tri::False, "not examined under heavy bondage");
        result.beltVisible = make(Tri::False, "not examined under heavy bondage");
        result.harnessVisible = make(Tri::False, "not examined under heavy bondage");
        return result;
    }
    if (heavy == Tri::Unknown) {
        result.lightlyBound = make(Tri::Unknown, "heavy bondage keyword unknown");
        result.heavilyBound = make(Tri::Unknown, "heavy bondage keyword unknown");
        result.visiblyBound = make(Tri::Unknown, "heavy bondage keyword unknown");
        result.wearingUnhideable = make(Tri::Unknown, "heavy bondage keyword unknown");
        result.beltWorn = make(beltWorn, "belt keyword");
        result.collarWorn = make(collarWorn, "collar keyword");
        result.collarVisible = make(Tri::Unknown, "heavy bondage keyword unknown");
        result.braVisible = make(Tri::Unknown, "heavy bondage keyword unknown");
        result.beltVisible = make(Tri::Unknown, "heavy bondage keyword unknown");
        result.harnessVisible = make(Tri::Unknown, "heavy bondage keyword unknown");
        return result;
    }
    if (lockable == Tri::False) {
        result.lightlyBound = make(Tri::False, "not lockable");
        result.heavilyBound = make(Tri::False, "not lockable");
        result.visiblyBound = make(Tri::False, "not lockable");
        result.wearingUnhideable = make(Tri::False, "not lockable");
        result.beltWorn = make(beltWorn, "belt keyword");
        result.collarWorn = make(collarWorn, "collar keyword");
        result.collarVisible = make(Tri::False, "not lockable");
        result.braVisible = make(Tri::False, "not lockable");
        result.beltVisible = make(Tri::False, "not lockable");
        result.harnessVisible = make(Tri::False, "not lockable");
        return result;
    }
    if (lockable == Tri::Unknown) {
        result.lightlyBound = make(Tri::Unknown, "lockable keyword unknown");
        result.heavilyBound = make(Tri::Unknown, "lockable keyword unknown");
        result.visiblyBound = make(Tri::Unknown, "lockable keyword unknown");
        result.wearingUnhideable = make(Tri::Unknown, "lockable keyword unknown");
        result.beltWorn = make(beltWorn, "belt keyword");
        result.collarWorn = make(collarWorn, "collar keyword");
        result.collarVisible = make(Tri::Unknown, "lockable keyword unknown");
        result.braVisible = make(Tri::Unknown, "lockable keyword unknown");
        result.beltVisible = make(Tri::Unknown, "lockable keyword unknown");
        result.harnessVisible = make(Tri::Unknown, "lockable keyword unknown");
        return result;
    }

    const ItemFacts& body = Item(facts, ArmorSlot::Body);
    const ItemFacts& collar = Item(facts, ArmorSlot::Collar);
    const ItemFacts& bra = Item(facts, ArmorSlot::Bra);
    const ItemFacts& belt = Item(facts, ArmorSlot::Belt);
    const ItemFacts& harness = Item(facts, ArmorSlot::Harness);

    const Tri collarGate = TriOr(
        TriNot(TriOf(facts.mods.sls)),
        TriOr(TriNot(TriOf(facts.worn.slsCollarCurse)),
              TriOf(facts.settings.allowSlscCursedCollarBoundFame)));
    const Tri collarVisible = TriAll({
        Present(collar),
        Property(collar, collar.deviousCollar),
        TriOf(facts.settings.allowCollarBoundFame),
        collarGate,
    });

    const Tri braVisible = TriAnd(
        Present(bra),
        TriAnd(Property(bra, bra.deviousBra),
               TriOr(AndCondition(facts,
                                  {AndLocation::Bra, AndLocation::Chest}),
                     TriNot(Present(body)))));

    const Tri beltCondition =
        TriOr(AndCondition(facts, {AndLocation::Underwear, AndLocation::Ass,
                                   AndLocation::Genitals}),
              TriNot(Present(body)));
    const Tri belt49 = TriAnd(
        Present(belt), TriAnd(Property(belt, belt.deviousBelt), beltCondition));
    const Tri belt58 = TriAnd(
        TriNot(Present(belt)),
        TriAnd(Present(harness),
               TriAnd(Property(harness, harness.deviousBelt), beltCondition)));
    const Tri beltVisible = TriOr(belt49, belt58);

    const Tri harnessVisible =
        TriAnd(Present(harness),
               TriAnd(Property(harness, harness.deviousHarness),
                      TriOr(AndCondition(facts,
                                         {AndLocation::Bra, AndLocation::Chest}),
                            TriNot(Present(body)))));

    const Tri unhideable = TriAny(facts.worn.devious.AnyUnhideable());
    const Tri anyVisible = TriAny({collarVisible, braVisible, beltVisible,
                                   harnessVisible, unhideable});
    const Tri anonymous = TriOf(facts.anonymous);
    const Tri visible = TriAnd(anyVisible, TriNot(anonymous));

    result.collarVisible = make(collarVisible, "collar visible");
    result.braVisible = make(braVisible, "bra visible");
    result.beltVisible = make(beltVisible, "belt visible");
    result.harnessVisible = make(harnessVisible, "harness visible");
    result.wearingUnhideable = make(unhideable, "unhideable device");
    result.visiblyBound = make(visible, "lockable visible");
    result.lightlyBound = make(Tri::True, "lockable");
    result.heavilyBound = make(Tri::False, "lockable");
    result.beltWorn = make(beltWorn, "belt keyword");
    result.collarWorn = make(collarWorn, "collar keyword");
    return result;
}
}