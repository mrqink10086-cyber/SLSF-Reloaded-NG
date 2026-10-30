#include "slsf/core/gameplay/AnimationClassifier.h"

#include <string_view>
#include <utility>
#include <vector>

namespace slsf::gameplay {
namespace {

[[nodiscard]] Tri BoolTri(bool value) noexcept {
    return value ? Tri::True : Tri::False;
}
[[nodiscard]] bool HasTag(const std::vector<std::string>& tags, std::string_view tag) noexcept {
    for (const std::string& value : tags) {
        if (value == tag) {
            return true;
        }
    }
    return false;
}
[[nodiscard]] Tri TagTri(const std::vector<std::string>& tags, std::string_view tag) noexcept {
    return BoolTri(HasTag(tags, tag));
}
[[nodiscard]] Tri CountGreater(const ObservedNumber& number, int threshold) noexcept {
    if (!number.IsKnown()) {
        return Tri::Unknown;
    }
    return BoolTri(number.value > threshold);
}
// (Friend == false && Lover == false) || (Friend == true && !forbiddenFriend)
// || (Lover == true && !forbiddenLover).  The masks are configuration, never
// runtime facts, so this gate is always two-valued.
[[nodiscard]] Tri RelationshipGate(const AnimationFacts& facts, bool friendOfPlayer,
                                   bool loverOfPlayer, FameCategory category) noexcept {
    if (!friendOfPlayer && !loverOfPlayer) {
        return Tri::True;
    }
    if (friendOfPlayer && !facts.settings.forbiddenByFriend.Test(category)) {
        return Tri::True;
    }
    if (loverOfPlayer && !facts.settings.forbiddenByLover.Test(category)) {
        return Tri::True;
    }
    return Tri::False;
}
// Papyrus expression:
//   IsVictim == False || (IsVictim == True && AllowX == True) && relGate
// Papyrus && binds tighter than ||, so relGate is only consulted for victims.
// Non-victims therefore receive the category regardless of Friend/Lover masks.
[[nodiscard]] Tri VictimGate(const AnimationFacts& facts, Tri victim, bool friendOfPlayer,
                             bool loverOfPlayer, const Fact& allowWhenVictim,
                             FameCategory category) noexcept {
    const Tri gate = RelationshipGate(facts, friendOfPlayer, loverOfPlayer, category);
    const Tri victimBranch =
        TriAnd(TriAnd(victim, TriOf(allowWhenVictim)), gate);
    return TriOr(TriNot(victim), victimBranch);
}
class SubAnalyzer {
public:
    SubAnalyzer(const AnimationFacts& facts, bool friendOfPlayer, bool loverOfPlayer,
                std::vector<FameCategory>& categories)
        : facts_(facts), friend_(friendOfPlayer), lover_(loverOfPlayer), categories_(categories) {}

    bool Run() {
        const Tri victim = TriOf(facts_.victim);
        const Tri aggressor = TriOf(facts_.aggressor);
        const Tri aggressiveTag = TagTri(facts_.tags, "Aggressive");
        const Tri strapon = TriOf(facts_.usingStrapon);
        const Tri female = TriOf(facts_.playerIsFemale);

        Add(TagTri(facts_.tags, "Oral"), FameCategory::Oral);
        Add(TagTri(facts_.tags, "Anal"), FameCategory::Anal);
        Add(TriOr(TagTri(facts_.tags, "Orgy"), BoolTri(facts_.actors.size() > 2)),
            FameCategory::Group);

        // The Whore/Slut block is the only SubAnalyze block guarded by the
        // not-aggressive/not-victim/not-aggressor condition.
        const Tri whoreOuter = TriAnd(
            TriNot(aggressiveTag), TriAnd(TriNot(victim), TriNot(aggressor)));
        const Tri sexWork = TriOr(TriOf(facts_.publicWhore), TriOf(facts_.sexWorker));
        const bool whoreGate =
            RelationshipGate(facts_, friend_, lover_, FameCategory::Whore) == Tri::True;
        const bool slutGate =
            RelationshipGate(facts_, friend_, lover_, FameCategory::Slut) == Tri::True;

        if (whoreOuter == Tri::True) {
            if (whoreGate) {
                if (sexWork == Tri::True) {
                    categories_.push_back(FameCategory::Whore);
                } else if (sexWork == Tri::Unknown) {
                    unknown_ = true;
                } else if (slutGate) {
                    categories_.push_back(FameCategory::Slut);
                }
            } else if (slutGate) {
                categories_.push_back(FameCategory::Slut);
            }
        } else if (whoreOuter == Tri::Unknown) {
            const bool canGainWhore = whoreGate && sexWork != Tri::False;
            const bool canGainSlut =
                slutGate && (!whoreGate || sexWork != Tri::True);
            if (canGainWhore || canGainSlut) {
                unknown_ = true;
            }
        }

        const Tri submissiveCond = TriAnd(
            TriOr(TriOr(TriOf(facts_.submissiveDefault), TriOf(facts_.canGainBoundFame)),
                  TriAnd(female, TriNot(strapon))),
            TriNot(TriOf(facts_.dominantDefault)));

        const Tri dominantCond = TriAnd(
            TriOr(TriOr(TriOf(facts_.dominantDefault), TriAnd(female, strapon)),
                  TriNot(female)),
            TriNot(TriOf(facts_.submissiveDefault)));

        if (submissiveCond == Tri::True) {
            submissive_ = Tri::True;
            Add(Tri::True, FameCategory::Submissive);
        } else if (submissiveCond == Tri::False) {
            submissive_ = Tri::False;
            if (dominantCond == Tri::True) {
                dominant_ = Tri::True;
                Add(Tri::True, FameCategory::Dominant);
            } else if (dominantCond == Tri::Unknown) {
                dominant_ = Tri::Unknown;
                if (RelationshipGate(facts_, friend_, lover_, FameCategory::Dominant) != Tri::False) {
                    unknown_ = true;
                }
            } else {
                dominant_ = Tri::False;
            }
        } else {
            submissive_ = Tri::Unknown;
            if (RelationshipGate(facts_, friend_, lover_, FameCategory::Submissive) != Tri::False) {
                unknown_ = true;
            }
            if (dominantCond == Tri::False) {
                dominant_ = Tri::False;
            } else {
                dominant_ = Tri::Unknown;
                if (RelationshipGate(facts_, friend_, lover_, FameCategory::Dominant) != Tri::False) {
                    unknown_ = true;
                }
            }
        }

        Add(TriOf(facts_.canGainBoundFame), FameCategory::Bound);

        const Tri masochistCond = TriOr(
            TriAnd(victim, TriOf(facts_.victimsAreMasochist)),
            TriOr(TriAnd(TriOf(facts_.canGainBoundFame), TriNot(dominant_)),
                  TriAnd(aggressiveTag, TriAnd(TriNot(victim), submissive_))));
        if (masochistCond == Tri::True) {
            Add(Tri::True, FameCategory::Masochist);
            gainedMasochist_ = Tri::True;
        } else if (masochistCond == Tri::Unknown) {
            gainedMasochist_ = Tri::Unknown;
            if (RelationshipGate(facts_, friend_, lover_, FameCategory::Masochist) != Tri::False) {
                unknown_ = true;
            }
        } else {
            gainedMasochist_ = Tri::False;
        }

        const Tri sadistCond = TriAnd(
            TriOr(aggressor, TriAnd(aggressiveTag, dominant_)),
            TriNot(gainedMasochist_));
        if (sadistCond == Tri::True) {
            Add(Tri::True, FameCategory::Sadist);
            gainedSadist_ = Tri::True;
        } else if (sadistCond == Tri::Unknown) {
            gainedSadist_ = Tri::Unknown;
            if (RelationshipGate(facts_, friend_, lover_, FameCategory::Sadist) != Tri::False) {
                unknown_ = true;
            }
        } else {
            gainedSadist_ = Tri::False;
        }

        const Tri likesWomenCond = TriOr(
            TriAnd(female, CountGreater(facts_.femaleCount, 1)),
            TriAnd(TriNot(female), CountGreater(facts_.femaleCount, 0)));
        AddVictimOnly(likesWomenCond, facts_.allowLikeFameWhenRaped, FameCategory::LikesWomen);

        const Tri likesMenCond = TriOr(
            TriAnd(female, CountGreater(facts_.maleCount, 0)),
            TriAnd(TriNot(female), CountGreater(facts_.maleCount, 1)));
        AddVictimOnly(likesMenCond, facts_.allowLikeFameWhenRaped, FameCategory::LikesMen);

        AddVictimOnly(TriOr(TagTri(facts_.tags, "Creature"), TagTri(facts_.tags, "Bestiality")),
                      facts_.allowBestialityWhenRaped, FameCategory::Bestiality);

        const Tri gentleTags = TriAny({TagTri(facts_.tags, "Loving"),
                                       TagTri(facts_.tags, "Hugging"),
                                       TagTri(facts_.tags, "Cuddling"),
                                       TagTri(facts_.tags, "Kissing")});
        Add(TriAnd(TriAnd(TriNot(victim), TriNot(aggressor)), gentleTags),
            FameCategory::Gentle);

        for (const AnimationActorFacts& actor : facts_.actors) {
            if (actor.isPlayer) {
                continue;
            }

            switch (actor.race) {
            case AnimationRace::Orc:
                AddVictimOnly(Tri::True, facts_.allowLikeFameWhenRaped, FameCategory::LikesOrc);
                break;
            case AnimationRace::Khajiit:
                AddVictimOnly(Tri::True, facts_.allowLikeFameWhenRaped,
                              FameCategory::LikesKhajiit);
                break;
            case AnimationRace::Argonian:
                AddVictimOnly(Tri::True, facts_.allowLikeFameWhenRaped,
                              FameCategory::LikesArgonian);
                break;
            case AnimationRace::Unknown:
                AddVictimOnly(Tri::Unknown, facts_.allowLikeFameWhenRaped,
                              FameCategory::LikesOrc);
                AddVictimOnly(Tri::Unknown, facts_.allowLikeFameWhenRaped,
                              FameCategory::LikesKhajiit);
                AddVictimOnly(Tri::Unknown, facts_.allowLikeFameWhenRaped,
                              FameCategory::LikesArgonian);
                break;
            case AnimationRace::Other:
                break;
            }

            const Tri accepted = TriAny(actor.devious.AcceptedLockables());
            const Tri deviousCond = TriAnd(
                TriOf(facts_.deviousDevicesInstalled),
                TriAnd(TriNot(gainedMasochist_),
                       TriAnd(TriNot(gainedSadist_),
                              TriAnd(TriOf(actor.devious.lockable), accepted))));
            if (deviousCond == Tri::True) {
                // Unlike the earlier Sadist block, this one sets the flag only
                // when the relationship gate allows the category.
                if (RelationshipGate(facts_, friend_, lover_, FameCategory::Sadist) ==
                    Tri::True) {
                    categories_.push_back(FameCategory::Sadist);
                    gainedSadist_ = Tri::True;
                }
            } else if (deviousCond == Tri::Unknown) {
                if (RelationshipGate(facts_, friend_, lover_, FameCategory::Sadist) ==
                    Tri::True) {
                    unknown_ = true;
                    gainedSadist_ = Tri::Unknown;
                }
            }
        }

        return !unknown_;
    }

    [[nodiscard]] bool Unknown() const noexcept { return unknown_; }

private:
    [[nodiscard]] Tri Gate(FameCategory category) const noexcept {
        return RelationshipGate(facts_, friend_, lover_, category);
    }

    void Add(Tri condition, FameCategory category) {
        if (Gate(category) == Tri::False) {
            return;
        }
        if (condition == Tri::True) {
            categories_.push_back(category);
        } else if (condition == Tri::Unknown) {
            unknown_ = true;
        }
    }

    // Condition already contains the victim-only relationship gate.
    void AddVictimOnly(Tri condition, const Fact& allowWhenVictim, FameCategory category) {
        const Tri gated =
            TriAnd(condition, VictimGate(facts_, TriOf(facts_.victim), friend_, lover_,
                                         allowWhenVictim, category));
        if (gated == Tri::True) {
            categories_.push_back(category);
        } else if (gated == Tri::Unknown) {
            unknown_ = true;
        }
    }

    const AnimationFacts& facts_;
    bool friend_{};
    bool lover_{};
    std::vector<FameCategory>& categories_;
    bool unknown_{};
    Tri submissive_{Tri::False};
    Tri dominant_{Tri::False};
    Tri gainedMasochist_{Tri::False};
    Tri gainedSadist_{Tri::False};
};

}
std::vector<Tri> AnimationDeviousFacts::AcceptedLockables() const {
    return {TriOf(hood),       TriOf(harness),      TriOf(belt),
            TriOf(bra),        TriOf(heavyBondage), TriOf(armCuffs),
            TriOf(armCuffsFront), TriOf(armbinder), TriOf(armbinderElbow),
            TriOf(gloves),     TriOf(legCuffs),     TriOf(boots),
            TriOf(gag),        TriOf(gagPanel),     TriOf(suit),
            TriOf(corset),     TriOf(blindfold)};
}
AnimationResult AnimationClassifier::Classify(const AnimationFacts& facts, IRandom& rng) const {
    AnimationResult result;
    if (facts.status != Status::Ready) {
        result.status = facts.status == Status::Invalid ? Status::Invalid
                                                       : Status::NotReady;
        result.kind = facts.status == Status::Invalid
                          ? AnimationResultKind::Invalid
                          : AnimationResultKind::Deferred;
        return result;
    }

    const auto stopEarly = [&result](AnimationResultKind kind) {
        result.status = Status::Ready;
        result.kind = kind;
        return result;
    };

    const Tri anonymous = TriOf(facts.anonymous);
    if (anonymous == Tri::True) {
        return stopEarly(AnimationResultKind::Skipped);
    }
    if (anonymous == Tri::Unknown) {
        result.status = Status::NotReady;
        result.kind = AnimationResultKind::Deferred;
        return result;
    }

    const Tri validLocation = TriOf(facts.validLocation);
    if (validLocation == Tri::False) {
        return stopEarly(AnimationResultKind::Skipped);
    }
    if (validLocation == Tri::Unknown) {
        result.status = Status::NotReady;
        result.kind = AnimationResultKind::Deferred;
        return result;
    }

    const Tri startGrace = TriOf(facts.startGraceElapsed);
    if (startGrace == Tri::False) {
        return stopEarly(AnimationResultKind::Deferred);
    }
    if (startGrace == Tri::Unknown) {
        result.status = Status::NotReady;
        result.kind = AnimationResultKind::Deferred;
        return result;
    }

    const Tri sceneEnded = TriOf(facts.sceneEnded);
    if (sceneEnded == Tri::True) {
        return stopEarly(AnimationResultKind::Skipped);
    }
    if (sceneEnded == Tri::Unknown) {
        result.status = Status::NotReady;
        result.kind = AnimationResultKind::Deferred;
        return result;
    }

    const Tri bodySearch =
        TriOr(TriOf(facts.bodySearch), TagTri(facts.tags, "BodySearch"));
    if (bodySearch == Tri::True) {
        return stopEarly(AnimationResultKind::Skipped);
    }
    if (bodySearch == Tri::Unknown) {
        result.status = Status::NotReady;
        result.kind = AnimationResultKind::Deferred;
        return result;
    }

    const Tri leadIn = TriOr(TriOf(facts.leadIn), TagTri(facts.tags, "LeadIn"));
    if (leadIn == Tri::Unknown) {
        result.status = Status::NotReady;
        result.kind = AnimationResultKind::Deferred;
        return result;
    }
    if (leadIn == Tri::True) {
        const Tri allowForeplay = TriOf(facts.allowForeplayFame);
        if (allowForeplay == Tri::Unknown) {
            result.status = Status::NotReady;
            result.kind = AnimationResultKind::Deferred;
            return result;
        }
        if (allowForeplay == Tri::False) {
            return stopEarly(AnimationResultKind::Skipped);
        }
        result.foreplay = true;
    }

    int playerCount = 0;
    for (const AnimationActorFacts& actor : facts.actors) {
        if (actor.isPlayer) {
            ++playerCount;
        }
    }
    if (playerCount != 1) {
        result.status = Status::Invalid;
        result.kind = AnimationResultKind::Invalid;
        return result;
    }

    if (facts.actors.size() <= 1) {
        // The legacy analyzer casts the NPC scan for a single-actor scene and
        // never enters AnimationSubAnalyze, so no Kiss-and-Tell roll is drawn.
        result.status = Status::Ready;
        result.kind = AnimationResultKind::SingleActorScan;
        result.triggerNpcScan = true;
        return result;
    }

    // One draw per multi-actor scene, always before the relationship walk.
    result.kissAndTellRoll = rng.UniformInt(1, 100);

    bool stopPartnerCheck = false;
    bool allFriends = false;
    bool allLovers = false;
    for (const AnimationActorFacts& actor : facts.actors) {
        if (actor.isPlayer) {
            continue;
        }
        if (!actor.relationshipKnown.IsKnownTrue()) {
            result.status = Status::NotReady;
            result.kind = AnimationResultKind::Deferred;
            result.allFriends = allFriends;
            result.allLovers = allLovers;
            return result;
        }
        const int rank = actor.relationshipRank;
        if (rank < 1) {
            stopPartnerCheck = true;
            allFriends = false;
            allLovers = false;
        } else if (rank > 0 && rank < 4) {
            allFriends = true;
            allLovers = false;
        } else if (rank > 3 && !allFriends) {
            allLovers = true;
        }
        if (stopPartnerCheck) {
            break;
        }
    }

    result.allFriends = allFriends;
    result.allLovers = allLovers;

    if (!allFriends && !allLovers) {
        // Neutral/enemy partner: SubAnalyze runs without friend/lover flags.
    } else if (allFriends) {
        if (result.kissAndTellRoll <= facts.friendKissAndTellChance) {
            result.friendScoring = true;
        } else {
            result.status = Status::Ready;
            result.kind = AnimationResultKind::NoScoring;
            return result;
        }
    } else if (allLovers) {
        if (result.kissAndTellRoll <= facts.loverKissAndTellChance) {
            result.loverScoring = true;
        } else {
            result.status = Status::Ready;
            result.kind = AnimationResultKind::NoScoring;
            return result;
        }
    }

    std::vector<FameCategory> categories;
    SubAnalyzer analyzer(facts, result.friendScoring, result.loverScoring, categories);
    const bool complete = analyzer.Run();
    result.categories = std::move(categories);
    if (!complete) {
        result.status = Status::NotReady;
        result.kind = AnimationResultKind::Deferred;
        result.categories.clear();
    } else {
        result.status = Status::Ready;
        result.kind = AnimationResultKind::Analyzed;
    }
    return result;
}
}