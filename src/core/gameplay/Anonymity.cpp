#include "slsf/core/gameplay/Anonymity.h"

#include <algorithm>

namespace slsf::gameplay {
namespace {

double ElapsedDays(GameTime later, GameTime earlier) {
    return std::max(0.0, later.AsDays() - earlier.AsDays());
}
}
Status AnonymityRules::ObserveRegion(std::optional<RegionId> previous,
                                     std::optional<RegionId> current,
                                     GameTime now) {
    if (previous.has_value() && current.has_value() && *previous == *current) {
        return Status::Ready;
    }
    if (!previous.has_value() && !current.has_value()) {
        return Status::Ready;
    }
    if (state_.hasLastTransition && now < state_.lastTransition) {
        return Status::Stale;
    }

    if (previous.has_value()) {
        // A leave only exists for a region that was actually entered.  Creating
        // default memory here would treat the absolute game clock as time spent
        // in that region and permanently poison its recognition value.
        if (state_.regions.contains(*previous)) {
            Leave(*previous, now);
            ++state_.transitionsConsumed;
        }
    }
    if (current.has_value()) {
        Enter(*current, now);
        ++state_.transitionsConsumed;
    }
    state_.lastTransition = now;
    state_.hasLastTransition = true;
    return Status::Ready;
}
void AnonymityRules::Leave(RegionId region, GameTime now) {
    AnonymityMemory& memory = state_.regions[region];
    memory.lastExit = now;
    const double timeSpent = ElapsedDays(now, memory.lastEnter);

    // Legacy: a short visit while still anonymous is ignored entirely.
    if (timeSpent < kStayThresholdDays && memory.localAnonymous) {
        return;
    }

    double added = timeSpent;
    if (memory.localAnonymous || memory.startedAnonymous) {
        added -= kStayThresholdDays;
        memory.localAnonymous = false;
        memory.startedAnonymous = false;
    }
    // The legacy script only caps the upper bound.  A short visit that had
    // already lost local anonymity therefore subtracts the 0.125-day grace even
    // when that makes recognition dip below the seven-day floor.
    memory.recognitionDays += added;
    if (memory.recognitionDays > kMaxRecognitionDays) {
        memory.recognitionDays = kMaxRecognitionDays;
    }
}
void AnonymityRules::Enter(RegionId region, GameTime now) {
    AnonymityMemory& memory = state_.regions[region];
    memory.lastEnter = now;
    const double away = ElapsedDays(now, memory.lastExit);

    if (!memory.localAnonymous) {
        if (away > memory.recognitionDays) {
            memory.localAnonymous = true;
            memory.startedAnonymous = true;
            memory.recognitionDays =
                std::max(kMinRecognitionDays, 2.0 * memory.recognitionDays - away);
        }
        return;
    }

    memory.recognitionDays =
        std::max(kMinRecognitionDays, 2.0 * memory.recognitionDays - away);
}
Status AnonymityRules::AdvanceHourly(RegionId current, GameTime now) {
    auto it = state_.regions.find(current);
    if (it == state_.regions.end()) {
        return Status::Missing;
    }
    if (now < it->second.lastEnter) {
        return Status::Stale;
    }
    if (ElapsedDays(now, it->second.lastEnter) >= kStayThresholdDays) {
        it->second.localAnonymous = false;
    }
    return Status::Ready;
}
RuleDecision AnonymityRules::Evaluate(RegionId region,
                                      const AnonymityConditions& conditions) const {
    const Tri enabled = TriOf(conditions.anonymityEnabled);
    if (enabled == Tri::False) {
        return RuleDecision::Known(false, "anonymity disabled");
    }
    if (enabled == Tri::Unknown) {
        return RuleDecision::NotReady("anonymity switch unknown");
    }

    const Tri dynamic = TriOf(conditions.dynamicAnonymity);
    if (dynamic == Tri::Unknown) {
        return RuleDecision::NotReady("dynamic anonymity switch unknown");
    }

    const auto memoryIt = state_.regions.find(region);
    const AnonymityMemory memory =
        memoryIt == state_.regions.end() ? AnonymityMemory{} : memoryIt->second;

    if (dynamic == Tri::True) {
        if (!memory.localAnonymous) {
            return RuleDecision::Known(false, "location anonymity expired");
        }

        Tri thaneBlock = Tri::False;
        switch (TriOf(conditions.thanesCantBeAnonymous)) {
        case Tri::True: thaneBlock = TriOf(conditions.isThane); break;
        case Tri::False: thaneBlock = Tri::False; break;
        case Tri::Unknown:
            // Unknown switch and a known thane is still only a possible block:
            // the switch may be disabled, so preserve the uncertainty.
            thaneBlock = TriAnd(Tri::Unknown, TriOf(conditions.isThane));
            break;
        }

        const Tri result = TriAnd(TriNot(thaneBlock), TriOf(conditions.lowFameRegion));
        return RuleDecision::FromTri(result, "dynamic anonymity");
    }

    const Tri headHides =
        TriAnd(TriOf(conditions.headSlotOccupied),
               TriNot(TriOr(TriOf(conditions.headSlotBikiniArmor),
                            TriOf(conditions.headSlotDoesNotHideIdentity))));
    const Tri hidden = TriAny({
        TriOf(conditions.hidesIdentity),
        TriAnd(TriOf(conditions.deviousDevicesInstalled),
               TriOf(conditions.deviousHoodWorn)),
        headHides,
    });
    return RuleDecision::FromTri(hidden, "static anonymity");
}
const AnonymityMemory* AnonymityRules::Find(RegionId region) const {
    const auto it = state_.regions.find(region);
    return it == state_.regions.end() ? nullptr : &it->second;
}
}