#pragma once

#include "slsf/core/gameplay/Common.h"

#include <cstdint>
#include <map>
#include <optional>

namespace slsf::gameplay {

// One per-location recognition record.  RecognitionTime is stored in game days,
// matching Utility.GetCurrentGameTime(); 0.125 days is three game hours.
struct AnonymityMemory {
    bool localAnonymous{};
    bool startedAnonymous{};
    double recognitionDays{};
    GameTime lastEnter{};
    GameTime lastExit{};
};

struct AnonymityState {
    std::map<RegionId, AnonymityMemory> regions;
    std::uint64_t transitionsConsumed{};
    GameTime lastTransition{};
    bool hasLastTransition{};
};

// Readiness of the facts that can change IsPlayerAnonymous.  Dynamic anonymity
// ignores worn identity gear in the legacy script, while static anonymity uses
// the keyword/head-slot chain.
struct AnonymityConditions {
    Fact anonymityEnabled{FactState::Unknown};
    Fact dynamicAnonymity{FactState::Unknown};
    Fact lowFameRegion{FactState::Unknown};

    Fact thanesCantBeAnonymous{FactState::Unknown};
    Fact isThane{FactState::Unknown};

    Fact hidesIdentity{FactState::Unknown};
    Fact deviousDevicesInstalled{FactState::Unknown};
    Fact deviousHoodWorn{FactState::Unknown};
    Fact headSlotOccupied{FactState::Unknown};
    Fact headSlotBikiniArmor{FactState::Unknown};
    Fact headSlotDoesNotHideIdentity{FactState::Unknown};
};

// Pure state machine for DynamicAnonymity.  Each actual fame-region leave or
// enter is consumed exactly once; sampling AdvanceHourly only expires a stay.
class AnonymityRules {
public:
    static constexpr double kStayThresholdDays = 0.125;
    static constexpr double kMaxRecognitionDays = 365.0;
    static constexpr double kMinRecognitionDays = 7.0;

    Status ObserveRegion(std::optional<RegionId> previous,
                         std::optional<RegionId> current,
                         GameTime now);
    Status AdvanceHourly(RegionId current, GameTime now);

    RuleDecision Evaluate(RegionId region, const AnonymityConditions& conditions) const;

    const AnonymityMemory* Find(RegionId region) const;
    const AnonymityState& State() const noexcept { return state_; }
    void Clear() noexcept { state_ = {}; }

private:
    void Leave(RegionId region, GameTime now);
    void Enter(RegionId region, GameTime now);

    AnonymityState state_;
};

} // namespace slsf::gameplay
