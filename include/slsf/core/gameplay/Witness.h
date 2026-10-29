#pragma once

#include "slsf/core/CategoryMask.h"
#include "slsf/core/gameplay/Common.h"
#include "slsf/interfaces/EnginePort.h"

#include <array>
#include <cstddef>
#include <string>
#include <vector>

namespace slsf::gameplay {

// IRandom is declared by the public engine port (UniformInt(min,max)).
using ::slsf::IRandom;

struct WitnessSettings {
    CategoryMask forbiddenByFriend{};
    CategoryMask forbiddenByLover{};
    bool friendsBetrayFromHighFame{true};
    bool loversBetrayFromHighFame{true};
    int friendHighFameThreshold{100};
    int loverHighFameThreshold{100};
    int friendBetrayChance{};
    int loverBetrayChance{};

    // NPCScan.psc chances, evaluated after one roll per surviving NPC.
    int enemyChance{};
    int neutralChance{};
    int friendChance{};
    int loverChance{};
    bool npcNeedsLineOfSight{true};
    double minimumNpcLosDistance{};
};

struct ObservedNumber {
    Fact known{FactState::Unknown};
    int value{};

    static ObservedNumber Known(int result) {
        return ObservedNumber{Fact::Known(true), result};
    }

    [[nodiscard]] bool IsKnown() const { return known.IsKnownTrue(); }
};

struct ObservedDouble {
    Fact known{FactState::Unknown};
    double value{};

    static ObservedDouble Known(double result) {
        return ObservedDouble{Fact::Known(true), result};
    }

    [[nodiscard]] bool IsKnown() const { return known.IsKnownTrue(); }
};

// One row of NPCScan.psc's candidate list.  Facts that are not needed because an
// earlier gate already skipped the NPC may stay Unknown.
struct NpcScanTargetFacts {
    StableFormKey actor;
    Fact validLocation{FactState::Unknown};
    Fact dead{FactState::Unknown};
    Fact inCombat{FactState::Unknown};
    Fact sexLabActive{FactState::Unknown};

    Fact lineOfSight{FactState::Unknown};
    ObservedDouble distance;
    Fact playerDetected{FactState::Unknown};

    Fact relationshipKnown{FactState::Unknown};
    int relationshipRank{};
};

struct NpcScanEntry {
    StableFormKey actor;
    Status status{Status::Ready};
    bool rolled{};
    bool scored{};
    int roll{};
    RelationshipGroup relationship{RelationshipGroup::Neutral};
    bool friendOfPlayer{};
    bool loverOfPlayer{};
    std::string reason;
};

struct NpcScanResult {
    Status status{Status::Ready};
    bool success{};
    std::vector<NpcScanEntry> entries;
    // Index of the first target not evaluated when status is NotReady.  Calling
    // Run again with the same actors resumes there, preserving success_ and the
    // entries above; a completed scan reports entries.size().
    std::size_t resumeIndex{};

    [[nodiscard]] const NpcScanEntry* SuccessEntry() const;
};

// Port of SLSF_Reloaded_NPCScan.psc.  One RNG draw is consumed per surviving
// NPC, even after a success; at most one success is reported per scan.
//
// If Run pauses on an unknown fact, call Run again with the same actor keys
// after the fact is refreshed.  It resumes at the paused target instead of
// re-rolling earlier NPCs, so a NotReady batch cannot score twice.  A different
// actor list starts a fresh scan.  Reset() discards any resumable batch.
class NpcScanner {
public:
    NpcScanner(WitnessSettings settings, IRandom& rng);

    NpcScanResult Run(const std::vector<NpcScanTargetFacts>& targets);
    void Reset() noexcept;

private:
    WitnessSettings settings_;
    IRandom& rng_;
    bool success_{};
    bool resumeArmed_{};
    std::size_t resumeIndex_{};
    std::vector<StableFormKey> batchActors_;
    std::vector<NpcScanEntry> previousEntries_;
};

// Ordered selection stage used by FameGainRoller.  Animation analyses report a
// single-actor scan instead, which the executor feeds to NpcScanner.
// The input order is preserved; duplicates are kept for the count and for RNG
// indices, while selectedMask prevents applying the same category twice.
struct FameSelectionResult {
    Status status{Status::Ready};
    std::vector<FameCategory> selected;
    CategoryMask selectedMask{};
    int wanted{};
    int attempts{};
    int duplicatesSkipped{};
    bool shortfall{};
    std::vector<int> selectionRolls;
};

class CandidateSelector {
public:
    explicit CandidateSelector(IRandom& rng);

    FameSelectionResult Select(const std::vector<FameCategory>& orderedCandidates);

private:
    IRandom& rng_;
};

struct FameValue {
    Fact known{FactState::Unknown};
    int value{};

    static FameValue Known(int result) {
        return FameValue{Fact::Known(true), result};
    }
};

struct FameGainRollFacts {
    Status status{Status::Ready};
    Fact validLocation{FactState::Unknown};
    Fact anonymous{FactState::Unknown};

    RelationshipGroup relationship{RelationshipGroup::Neutral};
    std::array<Fact, kFameCategoryCount> eligible{};
    std::array<FameValue, kFameCategoryCount> currentFame{};
    WitnessSettings settings;
};

struct FameGainRollResult {
    Status status{Status::Ready};
    bool skipped{};
    int betrayRoll{};
    std::vector<FameCategory> candidates;
    CategoryMask candidateMask{};
    FameSelectionResult selection;

    [[nodiscard]] bool IsReady() const noexcept { return status == Status::Ready; }
};

// Port of the candidate-building and selection half of FameManager.GainFameRoll.
// The mandatory RandomInt(1,100) betray draw occurs before candidate inspection.
class FameGainRoller {
public:
    explicit FameGainRoller(IRandom& rng);

    FameGainRollResult Run(const FameGainRollFacts& facts);

private:
    IRandom& rng_;
};

} // namespace slsf::gameplay