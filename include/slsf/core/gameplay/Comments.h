#pragma once

#include "slsf/core/gameplay/Common.h"
#include "slsf/core/gameplay/Witness.h"

namespace slsf::gameplay {

// SLSF_Reloaded_CommentManager.psc.  The initial OnInit delay is 10 seconds;
// anonymous ticks reschedule after 10 seconds and all other completed ticks
// reschedule after 5 seconds.
inline constexpr double kCommentInitialDelaySeconds = 10.0;
inline constexpr double kCommentAnonymousDelaySeconds = 10.0;
inline constexpr double kCommentNormalDelaySeconds = 5.0;

struct CommentFacts {
    Status status{Status::Ready};
    Fact anonymous{FactState::Unknown};
    ObservedNumber combatState;
    ObservedNumber commentFrequency;

    // NakedCommentPublicWhoreCheck inputs.
    Fact publicWhore{FactState::Unknown};
    Fact disableNakedCommentsWhilePW{FactState::Unknown};
    Fact andInstalled{FactState::Unknown};
};

struct CommentNakedResult {
    Status status{Status::Ready};
    bool changes{};
    int chance{};
};

struct CommentDecision {
    Status status{Status::Ready};
    bool allowed{};
    bool rolled{};
    int roll{};
    double nextDelaySeconds{kCommentNormalDelaySeconds};
    CommentNakedResult naked;
};

class CommentRules {
public:
    CommentDecision Evaluate(const CommentFacts& facts, IRandom& rng) const;
};

} // namespace slsf::gameplay
