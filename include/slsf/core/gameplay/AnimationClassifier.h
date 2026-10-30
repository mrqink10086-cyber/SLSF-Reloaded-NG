#pragma once

#include "slsf/core/gameplay/Common.h"
#include "slsf/core/gameplay/Witness.h"

#include <string>
#include <vector>

namespace slsf::gameplay {

// Unknown means the adapter could not resolve the race; it pauses the three
// race-preference categories rather than silently omitting them.  Use Other for
// a race that was resolved and is not Orc, Khajiit or Argonian.
enum class AnimationRace : unsigned char {
    Unknown = 0,
    Other,
    Orc,
    Khajiit,
    Argonian,
};

struct AnimationDeviousFacts {
    Fact lockable{FactState::Unknown};
    Fact hood{FactState::Unknown};
    Fact harness{FactState::Unknown};
    Fact belt{FactState::Unknown};
    Fact bra{FactState::Unknown};
    Fact heavyBondage{FactState::Unknown};
    Fact armCuffs{FactState::Unknown};
    Fact armCuffsFront{FactState::Unknown};
    Fact armbinder{FactState::Unknown};
    Fact armbinderElbow{FactState::Unknown};
    Fact gloves{FactState::Unknown};
    Fact legCuffs{FactState::Unknown};
    Fact boots{FactState::Unknown};
    Fact gag{FactState::Unknown};
    Fact gagPanel{FactState::Unknown};
    Fact suit{FactState::Unknown};
    Fact corset{FactState::Unknown};
    Fact blindfold{FactState::Unknown};

    [[nodiscard]] std::vector<Tri> AcceptedLockables() const;
};

struct AnimationActorFacts {
    bool isPlayer{};
    Fact relationshipKnown{FactState::Unknown};
    int relationshipRank{};
    AnimationRace race{AnimationRace::Unknown};
    AnimationDeviousFacts devious;
};

// SLSF_Reloaded_SexAnimationAnalyzer.psc waits ten real seconds after the
// scene starts before it reads the scene.  The executor supplies this fact;
// the pure classifier must not score before it is known true.
inline constexpr double kAnimationStartGraceSeconds = 10.0;

struct AnimationFacts {
    Status status{Status::Ready};
    Fact anonymous{FactState::Unknown};
    Fact validLocation{FactState::Unknown};
    Fact startGraceElapsed{FactState::Unknown};
    Fact sceneEnded{FactState::Unknown};
    Fact bodySearch{FactState::Unknown};

    Fact leadIn{FactState::Unknown};
    Fact allowForeplayFame{FactState::Unknown};

    std::vector<std::string> tags;
    std::vector<AnimationActorFacts> actors;

    Fact playerIsFemale{FactState::Unknown};
    ObservedNumber maleCount;
    ObservedNumber femaleCount;

    Fact aggressor{FactState::Unknown};
    Fact victim{FactState::Unknown};
    Fact usingStrapon{FactState::Unknown};

    Fact sexWorker{FactState::Unknown};
    Fact publicWhore{FactState::Unknown};
    Fact submissiveDefault{FactState::Unknown};
    Fact dominantDefault{FactState::Unknown};
    Fact canGainBoundFame{FactState::Unknown};
    Fact victimsAreMasochist{FactState::Unknown};
    Fact allowLikeFameWhenRaped{FactState::Unknown};
    Fact allowBestialityWhenRaped{FactState::Unknown};
    Fact deviousDevicesInstalled{FactState::Unknown};

    WitnessSettings settings;
    int friendKissAndTellChance{};
    int loverKissAndTellChance{};
};

enum class AnimationResultKind : unsigned char {
    Skipped = 0,
    Deferred,
    NoScoring,
    SingleActorScan,
    Analyzed,
    Invalid,
};

struct AnimationResult {
    Status status{Status::Ready};
    AnimationResultKind kind{AnimationResultKind::Skipped};
    bool foreplay{};
    bool allFriends{};
    bool allLovers{};
    bool friendScoring{};
    bool loverScoring{};
    bool triggerNpcScan{};
    int kissAndTellRoll{-1};
    std::vector<FameCategory> categories;

    [[nodiscard]] bool IsReady() const noexcept { return status == Status::Ready; }
};

// Port of SLSF_Reloaded_SexAnimationAnalyzer.psc.  The ten real-second start
// grace is represented by AnimationFacts::startGraceElapsed; before it elapses
// the classifier returns Deferred without drawing.  The classifier evaluates
// the already-settled scene and preserves SubAnalyze's append order, including
// duplicate race categories for repeated actors.
class AnimationClassifier {
public:
    AnimationResult Classify(const AnimationFacts& facts, IRandom& rng) const;
};

}