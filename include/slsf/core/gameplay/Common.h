#pragma once

#include "slsf/core/Types.h"

#include <string>
#include <string_view>
#include <vector>

namespace slsf::gameplay {

// Tri-state logic used by the pure gameplay rules.  A rule never turns missing
// engine data into a false observation: it reports NotReady instead.
enum class Tri : unsigned char {
    False = 0,
    True,
    Unknown,
};

inline Tri TriOf(const Fact& fact) noexcept {
    if (fact.IsKnownTrue()) {
        return Tri::True;
    }
    if (fact.IsKnownFalse()) {
        return Tri::False;
    }
    return Tri::Unknown;
}

inline Tri TriNot(Tri value) noexcept {
    switch (value) {
    case Tri::False: return Tri::True;
    case Tri::True: return Tri::False;
    default: return Tri::Unknown;
    }
}

inline Tri TriAnd(Tri lhs, Tri rhs) noexcept {
    if (lhs == Tri::False || rhs == Tri::False) {
        return Tri::False;
    }
    if (lhs == Tri::True && rhs == Tri::True) {
        return Tri::True;
    }
    return Tri::Unknown;
}

inline Tri TriOr(Tri lhs, Tri rhs) noexcept {
    if (lhs == Tri::True || rhs == Tri::True) {
        return Tri::True;
    }
    if (lhs == Tri::False && rhs == Tri::False) {
        return Tri::False;
    }
    return Tri::Unknown;
}

inline Tri TriAll(const std::vector<Tri>& values) noexcept {
    Tri result = Tri::True;
    for (const Tri value : values) {
        result = TriAnd(result, value);
        if (result == Tri::False) {
            break;
        }
    }
    return result;
}

inline Tri TriAny(const std::vector<Tri>& values) noexcept {
    Tri result = Tri::False;
    for (const Tri value : values) {
        result = TriOr(result, value);
        if (result == Tri::True) {
            break;
        }
    }
    return result;
}

struct RuleDecision {
    Status status{Status::NotReady};
    bool value{};
    std::string reason;

    static RuleDecision Known(bool result, std::string why = {}) {
        return RuleDecision{Status::Ready, result, std::move(why)};
    }

    static RuleDecision NotReady(std::string why) {
        return RuleDecision{Status::NotReady, false, std::move(why)};
    }

    static RuleDecision Invalid(std::string why) {
        return RuleDecision{Status::Invalid, false, std::move(why)};
    }

    static RuleDecision FromTri(Tri value, std::string why = {}) {
        switch (value) {
        case Tri::True: return Known(true, std::move(why));
        case Tri::False: return Known(false, std::move(why));
        default: return NotReady(std::move(why));
        }
    }

    [[nodiscard]] bool IsReady() const noexcept { return status == Status::Ready; }
};

struct NumericDecision {
    Status status{Status::NotReady};
    int value{};
    std::string reason;

    static NumericDecision Known(int result, std::string why = {}) {
        return NumericDecision{Status::Ready, result, std::move(why)};
    }

    static NumericDecision NotReady(std::string why) {
        return NumericDecision{Status::NotReady, 0, std::move(why)};
    }

    static NumericDecision Invalid(std::string why) {
        return NumericDecision{Status::Invalid, 0, std::move(why)};
    }

    [[nodiscard]] bool IsReady() const noexcept { return status == Status::Ready; }
};

// BGSRelationship stores 0..8 (lover..archnemesis).  The project assumption is
// that the runtime rank returned to Papyrus is 4 - level; this must be confirmed
// on the target 1.6.1170 runtime before the engine adapter relies on it.  The
// gameplay rules only consume the already-translated runtime value and the four
// legacy groups.
enum class RelationshipGroup : unsigned char {
    Enemy = 0,   // rank < 0
    Neutral,     // rank == 0
    Friend,      // 1..3
    Lover,       // >= 4
};

inline RelationshipGroup ClassifyRelationshipRank(int rank) noexcept {
    if (rank < 0) {
        return RelationshipGroup::Enemy;
    }
    if (rank == 0) {
        return RelationshipGroup::Neutral;
    }
    if (rank < 4) {
        return RelationshipGroup::Friend;
    }
    return RelationshipGroup::Lover;
}

} // namespace slsf::gameplay
