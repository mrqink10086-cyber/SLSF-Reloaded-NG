#include "slsf/core/gameplay/Witness.h"

#include <algorithm>
#include <optional>
#include <utility>

namespace slsf::gameplay {
namespace {

} // namespace

const NpcScanEntry* NpcScanResult::SuccessEntry() const {
    for (const NpcScanEntry& entry : entries) {
        if (entry.scored) {
            return &entry;
        }
    }
    return nullptr;
}

NpcScanner::NpcScanner(WitnessSettings settings, IRandom& rng)
    : settings_(std::move(settings)), rng_(rng) {}

void NpcScanner::Reset() noexcept {
    success_ = false;
    resumeArmed_ = false;
    resumeIndex_ = 0;
    batchActors_.clear();
    previousEntries_.clear();
}

NpcScanResult NpcScanner::Run(const std::vector<NpcScanTargetFacts>& targets) {
    // A paused batch may be retried with the same actor list.  Any other call is
    // a fresh cast, which must not inherit the previous scan's success flag.
    bool resuming = resumeArmed_ && batchActors_.size() == targets.size();
    if (resuming) {
        for (std::size_t i = 0; i < targets.size(); ++i) {
            if (!(batchActors_[i] == targets[i].actor)) {
                resuming = false;
                break;
            }
        }
    }

    NpcScanResult result;
    if (resuming) {
        result.entries = previousEntries_;
    } else {
        success_ = false;
        resumeArmed_ = false;
        resumeIndex_ = 0;
        previousEntries_.clear();
        batchActors_.clear();
        batchActors_.reserve(targets.size());
        for (const NpcScanTargetFacts& target : targets) {
            batchActors_.push_back(target.actor);
        }
    }

    const std::size_t start = resuming ? resumeIndex_ : 0;
    for (std::size_t index = start; index < targets.size(); ++index) {
        const NpcScanTargetFacts& target = targets[index];
        NpcScanEntry entry;
        entry.actor = target.actor;

        bool skipped = false;
        bool paused = false;
        const auto gate = [&](const Fact& fact, bool skipValue, const char* skipWhy,
                              const char* unknownWhy) {
            const Tri value = TriOf(fact);
            if (value == Tri::Unknown) {
                entry.status = Status::NotReady;
                entry.reason = unknownWhy;
                result.status = Status::NotReady;
                paused = true;
                return;
            }
            if (value == (skipValue ? Tri::True : Tri::False)) {
                entry.status = Status::Ready;
                entry.reason = skipWhy;
                skipped = true;
            }
        };

        gate(target.validLocation, false, "invalid location",
             "location validity unknown");
        if (!skipped && !paused) {
            gate(target.dead, true, "dead", "death state unknown");
        }
        if (!skipped && !paused) {
            gate(target.inCombat, true, "in combat", "combat state unknown");
        }
        if (!skipped && !paused) {
            gate(target.sexLabActive, true, "sexlab active",
                 "sexlab state unknown");
        }

        if (!skipped && !paused && settings_.npcNeedsLineOfSight) {
            const Tri los = TriOf(target.lineOfSight);
            if (los == Tri::Unknown) {
                entry.status = Status::NotReady;
                entry.reason = "line of sight unknown";
                result.status = Status::NotReady;
                paused = true;
            } else if (los == Tri::False) {
                if (!target.distance.IsKnown()) {
                    entry.status = Status::NotReady;
                    entry.reason = "distance unknown";
                    result.status = Status::NotReady;
                    paused = true;
                } else if (target.distance.value >
                           settings_.minimumNpcLosDistance) {
                    entry.status = Status::Ready;
                    entry.reason = "no line of sight beyond minimum distance";
                    skipped = true;
                }
            } else {
                const Tri detected = TriOf(target.playerDetected);
                if (detected == Tri::Unknown) {
                    entry.status = Status::NotReady;
                    entry.reason = "detection unknown";
                    result.status = Status::NotReady;
                    paused = true;
                } else if (detected == Tri::False) {
                    entry.status = Status::Ready;
                    entry.reason = "not detected";
                    skipped = true;
                }
            }
        }

        if (!skipped && !paused && !target.relationshipKnown.IsKnownTrue()) {
            entry.status = Status::NotReady;
            entry.reason = "relationship unknown";
            result.status = Status::NotReady;
            paused = true;
        }

        if (paused) {
            result.entries.push_back(std::move(entry));
            result.resumeIndex = index;
            result.success = success_;
            previousEntries_ = result.entries;
            previousEntries_.pop_back(); // the paused target is re-evaluated on resume
            resumeArmed_ = true;
            resumeIndex_ = index;
            return result;
        }
        if (skipped) {
            result.entries.push_back(std::move(entry));
            continue;
        }

        entry.relationship = ClassifyRelationshipRank(target.relationshipRank);
        entry.friendOfPlayer = entry.relationship == RelationshipGroup::Friend;
        entry.loverOfPlayer = entry.relationship == RelationshipGroup::Lover;

        int chance = 0;
        switch (entry.relationship) {
        case RelationshipGroup::Enemy: chance = settings_.enemyChance; break;
        case RelationshipGroup::Neutral: chance = settings_.neutralChance; break;
        case RelationshipGroup::Friend: chance = settings_.friendChance; break;
        case RelationshipGroup::Lover: chance = settings_.loverChance; break;
        }

        entry.roll = rng_.UniformInt(1, 100);
        entry.rolled = true;
        if (entry.roll <= chance && !success_) {
            success_ = true;
            entry.scored = true;
            entry.reason = "scan success";
        } else if (success_) {
            entry.reason = "scan already succeeded";
        } else {
            entry.reason = "scan roll failed";
        }
        result.entries.push_back(std::move(entry));
    }

    result.status = Status::Ready;
    result.success = success_;
    result.resumeIndex = targets.size();
    resumeArmed_ = false;
    resumeIndex_ = 0;
    previousEntries_.clear();
    batchActors_.clear();
    return result;
}

CandidateSelector::CandidateSelector(IRandom& rng) : rng_(rng) {}

FameSelectionResult CandidateSelector::Select(
    const std::vector<FameCategory>& orderedCandidates) {
    FameSelectionResult result;
    for (const FameCategory category : orderedCandidates) {
        if (!IsValidFameCategory(category)) {
            result.status = Status::Invalid;
            result.selected.clear();
            result.selectedMask.Clear();
            return result;
        }
    }

    const int count = static_cast<int>(orderedCandidates.size());
    if (count == 0) {
        result.status = Status::Ready;
        return result;
    }
    if (count == 1) {
        result.wanted = 1;
        result.selected.push_back(orderedCandidates.front());
        result.selectedMask.Set(orderedCandidates.front());
        return result;
    }

    result.wanted = rng_.UniformInt(1, count);
    if (result.wanted == count) {
        for (const FameCategory category : orderedCandidates) {
            if (result.selectedMask.Test(category)) {
                ++result.duplicatesSkipped;
                continue;
            }
            result.selectedMask.Set(category);
            result.selected.push_back(category);
        }
        return result;
    }

    int applied = 0;
    while (applied < result.wanted && result.attempts < 50) {
        const int index = rng_.UniformInt(0, count - 1);
        ++result.attempts;
        result.selectionRolls.push_back(index);
        const FameCategory category = orderedCandidates[static_cast<std::size_t>(index)];
        if (result.selectedMask.Test(category)) {
            ++result.duplicatesSkipped;
            continue;
        }
        result.selectedMask.Set(category);
        result.selected.push_back(category);
        ++applied;
    }
    result.shortfall = applied < result.wanted;
    return result;
}

FameGainRoller::FameGainRoller(IRandom& rng) : rng_(rng) {}

namespace {

// Legacy FameManager keeps one BetrayRollModifier for the whole roll.  Every
// eligible category may raise it before that category's relationship gate, and
// a lower-fame category neither lowers nor clears it.  The value is therefore
// carried between categories instead of being recomputed per gate.
std::optional<int> UpdateBetrayModifier(const FameGainRollFacts& facts,
                                        std::size_t index, Tri eligible,
                                        std::optional<int> modifier) {
    const bool friendRelationship =
        facts.relationship == RelationshipGroup::Friend;
    const bool loverRelationship = facts.relationship == RelationshipGroup::Lover;
    if (!friendRelationship && !loverRelationship) {
        return modifier;
    }

    const WitnessSettings& settings = facts.settings;
    const bool highFameEnabled = friendRelationship
                                     ? settings.friendsBetrayFromHighFame
                                     : settings.loversBetrayFromHighFame;
    if (!highFameEnabled) {
        return modifier;
    }

    const FameValue& fame = facts.currentFame[index];
    if (!fame.known.IsKnownTrue()) {
        return std::nullopt;
    }
    const int threshold = friendRelationship
                              ? settings.friendHighFameThreshold
                              : settings.loverHighFameThreshold;
    if (fame.value < threshold) {
        return modifier;
    }
    if (eligible == Tri::Unknown) {
        return std::nullopt;
    }
    return fame.value / (friendRelationship ? 2 : 4);
}

Tri BetrayGate(const FameGainRollFacts& facts, std::size_t index, int roll,
               std::optional<int> modifier) {
    const WitnessSettings& settings = facts.settings;
    const FameCategory category = static_cast<FameCategory>(index);
    const bool friendRelationship =
        facts.relationship == RelationshipGroup::Friend;
    const bool loverRelationship = facts.relationship == RelationshipGroup::Lover;

    if (!friendRelationship && !loverRelationship) {
        return Tri::True;
    }

    const bool forbidden = friendRelationship
                               ? settings.forbiddenByFriend.Test(category)
                               : settings.forbiddenByLover.Test(category);
    if (!forbidden) {
        return Tri::True;
    }

    const int chance = friendRelationship ? settings.friendBetrayChance
                                          : settings.loverBetrayChance;
    if (chance >= 100) {
        return Tri::True;
    }
    if (!modifier.has_value()) {
        return Tri::Unknown;
    }
    return (roll - *modifier <= chance) ? Tri::True : Tri::False;
}

} // namespace

FameGainRollResult FameGainRoller::Run(const FameGainRollFacts& facts) {
    FameGainRollResult result;
    if (facts.status != Status::Ready) {
        result.status = facts.status == Status::Invalid ? Status::Invalid
                                                       : Status::NotReady;
        return result;
    }
    const Tri location = TriOf(facts.validLocation);
    if (location == Tri::False) {
        result.skipped = true;
        result.status = Status::Ready;
        return result;
    }
    if (location == Tri::Unknown) {
        result.status = Status::NotReady;
        return result;
    }
    const Tri anonymous = TriOf(facts.anonymous);
    if (anonymous == Tri::True) {
        result.skipped = true;
        result.status = Status::Ready;
        return result;
    }
    if (anonymous == Tri::Unknown) {
        result.status = Status::NotReady;
        return result;
    }

    // Legacy contracts: this draw is unconditional and precedes every candidate
    // condition, including the zero-candidate case.
    result.betrayRoll = rng_.UniformInt(1, 100);

    bool unknownCandidate = false;
    std::vector<FameCategory> candidates;
    CategoryMask candidateMask;
    std::optional<int> betrayModifier = 0;
    for (std::size_t i = 0; i < kFameCategoryCount; ++i) {
        // Eligibility is independent of the relationship/betray gate.  Check it
        // first so an ineligible category does not surface an unrelated unknown
        // high-fame value and pause the whole scan.
        const Tri eligible = TriOf(facts.eligible[i]);
        if (eligible == Tri::False) {
            continue;
        }
        betrayModifier = UpdateBetrayModifier(facts, i, eligible, betrayModifier);
        const Tri gate = BetrayGate(facts, i, result.betrayRoll, betrayModifier);
        if (gate == Tri::False) {
            continue;
        }
        const Tri candidate = TriAnd(gate, eligible);
        if (candidate == Tri::Unknown) {
            unknownCandidate = true;
            continue;
        }
        const FameCategory category = static_cast<FameCategory>(i);
        candidates.push_back(category);
        candidateMask.Set(category);
    }

    result.candidates = std::move(candidates);
    result.candidateMask = candidateMask;
    if (unknownCandidate) {
        result.status = Status::NotReady;
        result.selection.status = Status::NotReady;
        return result;
    }

    result.selection = CandidateSelector(rng_).Select(result.candidates);
    result.status = result.selection.status;
    return result;
}

} // namespace slsf::gameplay