#include "slsf/core/gameplay/Comments.h"

namespace slsf::gameplay {
namespace {

CommentNakedResult EvaluateNaked(const CommentFacts& facts) {
    CommentNakedResult result;
    const Tri publicWhore = TriOf(facts.publicWhore);
    const Tri disable = TriOf(facts.disableNakedCommentsWhilePW);

    // If (PublicWhore && DisableNakedCommentsWhilePW) -> -1
    // ElseIf !ANDInstalled -> 100
    // Else leave the global untouched.
    const Tri suppress = TriAnd(publicWhore, disable);
    if (suppress == Tri::True) {
        result.changes = true;
        result.chance = -1;
        return result;
    }
    if (suppress == Tri::Unknown) {
        result.status = Status::NotReady;
        return result;
    }
    const Tri andInstalled = TriOf(facts.andInstalled);
    if (andInstalled == Tri::False) {
        result.changes = true;
        result.chance = 100;
    } else if (andInstalled == Tri::Unknown) {
        result.status = Status::NotReady;
    }
    return result;
}

} // namespace

CommentDecision CommentRules::Evaluate(const CommentFacts& facts, IRandom& rng) const {
    CommentDecision result;
    if (facts.status != Status::Ready) {
        result.status = facts.status == Status::Invalid ? Status::Invalid
                                                       : Status::NotReady;
        return result;
    }

    result.naked = EvaluateNaked(facts);
    if (result.naked.status != Status::Ready) {
        // The naked-chance side effect is part of every tick and cannot be
        // applied from a partially known snapshot.
        result.status = result.naked.status;
        return result;
    }

    const Tri anonymous = TriOf(facts.anonymous);
    if (anonymous == Tri::Unknown) {
        result.status = Status::NotReady;
        return result;
    }
    if (anonymous == Tri::True) {
        result.allowed = false;
        result.rolled = false;
        result.nextDelaySeconds = kCommentAnonymousDelaySeconds;
        return result;
    }

    if (!facts.combatState.IsKnown()) {
        result.status = Status::NotReady;
        return result;
    }
    if (facts.combatState.value != 0) {
        result.allowed = false;
        result.rolled = false;
        result.nextDelaySeconds = kCommentNormalDelaySeconds;
        return result;
    }

    if (!facts.commentFrequency.IsKnown()) {
        result.status = Status::NotReady;
        return result;
    }

    result.rolled = true;
    result.roll = rng.UniformInt(1, 100);
    result.allowed = result.roll <= facts.commentFrequency.value;
    result.nextDelaySeconds = kCommentNormalDelaySeconds;
    return result;
}

} // namespace slsf::gameplay