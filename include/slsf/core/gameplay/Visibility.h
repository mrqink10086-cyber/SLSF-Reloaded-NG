#pragma once

#include "slsf/core/gameplay/Common.h"

#include <array>
#include <cstddef>
#include <vector>

namespace slsf::gameplay {

// Armor slots used by SLSF_Reloaded_VisibilityManager.psc.  Values are the
// Skyrim biped slot numbers so an engine snapshot can fill the array directly.
enum class ArmorSlot : std::size_t {
    Head = 31,
    Body = 32,
    Hands = 33,
    Feet = 37,
    Collar = 45,
    BackUpper = 46,
    BackLower = 47,
    Belt = 49,
    Thigh = 53,
    Calves = 54,
    Bra = 56,
    Harness = 58,
    Count = 61,
};

constexpr std::size_t kArmorSlotCount = static_cast<std::size_t>(ArmorSlot::Count);

// Properties of one equipped armor item.  Each property is a Fact so a partial
// engine snapshot pauses only the rule that needs the missing keyword.
struct ItemFacts {
    Fact present{FactState::Unknown};
    Fact bikiniArmor{FactState::Unknown};
    Fact doesNotHideIdentity{FactState::Unknown};
    Fact doesNotCoverBack{FactState::Unknown};
    Fact doesNotCoverHands{FactState::Unknown};
    Fact doesNotCoverFeet{FactState::Unknown};
    Fact deviousCollar{FactState::Unknown};
    Fact deviousBra{FactState::Unknown};
    Fact deviousBelt{FactState::Unknown};
    Fact deviousHarness{FactState::Unknown};
};

struct DeviousWornFacts {
    Fact hood{FactState::Unknown};
    Fact gag{FactState::Unknown};
    Fact gagPanel{FactState::Unknown};
    Fact armCuffs{FactState::Unknown};
    Fact armCuffsFront{FactState::Unknown};
    Fact armbinder{FactState::Unknown};
    Fact armbinderElbow{FactState::Unknown};
    Fact gloves{FactState::Unknown};
    Fact legCuffs{FactState::Unknown};
    Fact boots{FactState::Unknown};
    Fact suit{FactState::Unknown};
    Fact corset{FactState::Unknown};
    Fact blindfold{FactState::Unknown};

    [[nodiscard]] std::vector<Tri> AnyUnhideable() const;
};

struct WornFacts {
    Fact hidesIdentity{FactState::Unknown};
    Fact coversBack{FactState::Unknown};
    Fact coversHands{FactState::Unknown};
    Fact coversFeet{FactState::Unknown};
    Fact deviousHood{FactState::Unknown};
    Fact deviousHeavyBondage{FactState::Unknown};
    Fact deviousLockable{FactState::Unknown};
    Fact deviousBeltWorn{FactState::Unknown};
    Fact deviousCollarWorn{FactState::Unknown};
    Fact slsCollarCurse{FactState::Unknown};
    DeviousWornFacts devious;
};

enum class AndLocation : std::size_t {
    Chest = 0,
    Genitals,
    Bra,
    Underwear,
    Ass,
    Count,
};

constexpr std::size_t kAndLocationCount = static_cast<std::size_t>(AndLocation::Count);

struct AndFacts {
    Fact installed{FactState::Unknown};
    std::array<Fact, kAndLocationCount> rankIsOne{
        Fact{FactState::Unknown}, Fact{FactState::Unknown}, Fact{FactState::Unknown},
        Fact{FactState::Unknown}, Fact{FactState::Unknown}};
};

struct ModInstallFacts {
    Fact deviousDevices{FactState::Unknown};
    Fact armorKeywords{FactState::Unknown};
    Fact sls{FactState::Unknown};
    Fact slacs{FactState::Unknown};
    Fact slaveTats{FactState::Unknown};
};

struct VisibilitySettings {
    Fact anonymityEnabled{FactState::Unknown};
    Fact dynamicAnonymity{FactState::Unknown};
    Fact allowCollarBoundFame{FactState::Unknown};
    Fact allowSlscCursedCollarBoundFame{FactState::Unknown};
};

enum class TattooArea : std::size_t { Body = 0, Face, Hands, Feet, Count };
constexpr std::size_t kTattooAreaCount = static_cast<std::size_t>(TattooArea::Count);

// Subcategories are the literal SlaveTats subcategory strings checked by the
// legacy body-tattoo visibility function.
enum class TattooSubcategory : unsigned char {
    None = 0,
    Chest,
    Pelvis,
    Ass,
    Back,
    Thigh,
    Calves,
    Unknown,
};

struct TattooSlotFacts {
    Fact applied{FactState::Unknown};
    TattooSubcategory subcategory{TattooSubcategory::Unknown};
};

struct TattooAreaFacts {
    Fact installed{FactState::Unknown};
    Fact appliedCountKnown{FactState::Unknown};
    int appliedCount{};
    std::vector<TattooSlotFacts> slots;
};

struct TattooFacts {
    TattooAreaFacts body;
    TattooAreaFacts face;
    TattooAreaFacts hands;
    TattooAreaFacts feet;
};

struct CountFacts {
    Fact known{FactState::Unknown};
    int value{};
};

struct CumCountFacts {
    CountFacts anal;
    CountFacts vaginal;
    CountFacts oral;
};

struct BondageResult {
    RuleDecision lightlyBound;
    RuleDecision heavilyBound;
    RuleDecision visiblyBound;
    RuleDecision wearingUnhideable;
    RuleDecision beltWorn;
    RuleDecision collarWorn;
    RuleDecision collarVisible;
    RuleDecision braVisible;
    RuleDecision beltVisible;
    RuleDecision harnessVisible;
};

struct VisibilityFacts {
    Status status{Status::Ready};
    Fact anonymous{FactState::Unknown};
    Fact playerIsMale{FactState::Unknown};
    ModInstallFacts mods;
    VisibilitySettings settings;
    std::array<ItemFacts, kArmorSlotCount> items{};
    WornFacts worn;
    AndFacts andRanks;
    TattooFacts tattoos;
    CumCountFacts cum;
};

// Pure port of SLSF_Reloaded_VisibilityManager.psc.  The class owns no engine
// state; all inputs arrive in VisibilityFacts and all results are decisions.
class VisibilityRules {
public:
    RuleDecision BodyTattooVisible(const VisibilityFacts& facts, std::size_t slot) const;
    RuleDecision FaceTattooVisible(const VisibilityFacts& facts, std::size_t slot) const;
    RuleDecision HandTattooVisible(const VisibilityFacts& facts, std::size_t slot) const;
    RuleDecision FootTattooVisible(const VisibilityFacts& facts, std::size_t slot) const;

    RuleDecision AssCumVisible(const VisibilityFacts& facts) const;
    NumericDecision VisibleAssCum(const VisibilityFacts& facts) const;
    RuleDecision VaginalCumVisible(const VisibilityFacts& facts) const;
    NumericDecision VisibleVaginalCum(const VisibilityFacts& facts) const;
    RuleDecision OralCumVisible(const VisibilityFacts& facts) const;
    NumericDecision VisibleOralCum(const VisibilityFacts& facts) const;

    BondageResult EvaluateBondage(const VisibilityFacts& facts) const;

private:
    RuleDecision VisibilityReady(const VisibilityFacts& facts, std::string_view what) const;
};

} // namespace slsf::gameplay
