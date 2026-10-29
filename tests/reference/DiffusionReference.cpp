#include "DiffusionReference.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace diffref {
namespace {

double Trunc(double value) {
    return std::trunc(value);
}

double Clamp(double value, double low, double high) {
    return std::max(low, std::min(high, value));
}

bool InNightWindow(double hour, double nightStart, double nightEnd) {
    if (nightStart == nightEnd) {
        return false;
    }
    if (nightStart < nightEnd) {
        return hour > nightStart && hour < nightEnd;
    }
    return hour > nightStart || hour < nightEnd;
}

int DecayTierCap(double previous) {
    if (previous >= 100.0) {
        return 1;
    }
    if (previous >= 75.0) {
        return 2;
    }
    if (previous >= 50.0) {
        return 3;
    }
    if (previous >= 25.0) {
        return 4;
    }
    return 5;
}

}

GainOutcome Gain(const GainInputs& inputs) {
    double halveFactor = 1.0;
    bool halved = false;
    if (inputs.nightEnabled && inputs.foreplay &&
        InNightWindow(inputs.hour, inputs.nightStart, inputs.nightEnd)) {
        halveFactor = 0.5;
        halved = true;
    }
    const int firstProduct =
        static_cast<int>(Trunc(static_cast<double>(inputs.sample) * inputs.multiplier * halveFactor));
    const double scaled = static_cast<double>(firstProduct) * inputs.iterationMultiplier;
    bool floored = false;
    double effectiveGain = scaled;
    if (scaled < 1.0) {
        effectiveGain = 1.0;
        floored = true;
    }
    const int gained = static_cast<int>(Trunc(effectiveGain));
    const double next = Clamp(inputs.previousFame + static_cast<double>(gained), 0.0, inputs.maxFame);
    return GainOutcome{next, gained, halved, floored};
}

SpreadOutcome ManualSpread(const SpreadInputs& inputs) {
    const int percent = static_cast<int>(Clamp(inputs.percent, 10.0, 100.0));
    const double raw = inputs.target + inputs.source * (percent / 100.0);
    const double newValue = Clamp(Trunc(raw), 0.0, inputs.maxFame);
    return SpreadOutcome{newValue, percent};
}

int ManualDecayAmount(int legacyMin, int legacyMax, IDie& die, double maxFame) {
    const int lo = static_cast<int>(Clamp(legacyMin, 0.0, maxFame));
    const int hi = static_cast<int>(Clamp(legacyMax, 0.0, maxFame));
    const int amount = lo >= hi ? lo : die.UniformInt(lo, hi);
    return static_cast<int>(Clamp(amount, 0.0, maxFame));
}

DecayOutcome DecayRoll(const DecayInputs& inputs, IDie& die, double maxFame) {
    const int cap = DecayTierCap(inputs.previous);
    int amount = 0;
    if (cap > 0) {
        const int sample = die.UniformInt(1, cap);
        amount = static_cast<int>(Trunc(static_cast<double>(sample) * inputs.multiplier));
    }
    const double total = static_cast<double>(amount) * static_cast<double>(inputs.owedPeriods);
    const double floored = total < 1.0 ? 1.0 : total;
    const double next = inputs.previous - floored;
    return DecayOutcome{Clamp(Trunc(next), 0.0, maxFame), amount};
}

void ReferenceIncoming(std::vector<Row>& incoming,
                       const std::vector<Row>& local,
                       const std::vector<Row>& remote,
                       const ReferenceGraph& graph,
                       double cutoff) {
    incoming.assign(graph.accountCount, Row{});
    std::array<double, kCategories> best{};
    for (std::uint32_t target = 0; target < graph.accountCount; ++target) {
        const auto begin = graph.offsets[target];
        const auto end = graph.offsets[target + 1];
        if (begin == end) {
            continue;
        }
        best.fill(0.0);
        for (auto edge = begin; edge < end; ++edge) {
            const Edge& operand = graph.operands[edge];
            const Row& source = local[operand.source];
            const Row& sourceRemote = remote[operand.source];
            for (std::size_t slot = 0; slot < kCategories; ++slot) {
                const double candidate = operand.coefficient * std::max(source[slot], sourceRemote[slot]);
                if (candidate > best[slot]) {
                    best[slot] = candidate;
                }
            }
        }
        for (std::size_t slot = 0; slot < kCategories; ++slot) {
            if (best[slot] >= cutoff) {
                incoming[target][slot] = best[slot];
            }
        }
    }
}

void ReferenceRelax(Row& remote, const Row& incoming, double elapsedDays,
                    double relaxationPerDay, double maxFame) {
    const double cap = std::isfinite(maxFame) && maxFame >= 0.0 ? maxFame : 150.0;
    if (!(elapsedDays > 0.0)) {
        for (std::size_t slot = 0; slot < kSlots; ++slot) {
            if (incoming[slot] >= remote[slot]) {
                remote[slot] = std::min(cap, incoming[slot]);
            }
        }
        return;
    }
    const double retreat = std::max(0.0, relaxationPerDay * elapsedDays);
    for (std::size_t slot = 0; slot < kSlots; ++slot) {
        const double goal = std::min(cap, incoming[slot]);
        if (goal >= remote[slot]) {
            remote[slot] = goal;
            continue;
        }
        remote[slot] = std::max(goal, remote[slot] - retreat);
    }
}

namespace {

// Scalar business-time segment used by the reference harness. It deliberately
// keeps its own row traversal, decay arithmetic and one-hop implementation.
void AdvanceSegment(std::vector<Row>& local,
                    std::vector<Row>& remote,
                    std::vector<double>& settledAt,
                    std::vector<double>& remoteSettledAt,
                    const std::vector<Row>& protection,
                    const ReferenceGraph& graph,
                    const Parameters& params,
                    double nowDays,
                    IDie& decayDie,
                    double decayMultiplier) {
    const auto count = local.size();
    std::vector<double> relaxElapsed(count, 0.0);
    bool consumed = false;

    for (std::size_t index = 0; index < count; ++index) {
        if (settledAt[index] > nowDays) {
            settledAt[index] = nowDays;
        }
        if (!(remoteSettledAt[index] > 0.0)) {
            remoteSettledAt[index] = settledAt[index];
        }
        if (remoteSettledAt[index] > nowDays) {
            remoteSettledAt[index] = nowDays;
        }

        relaxElapsed[index] = nowDays - remoteSettledAt[index];
        remoteSettledAt[index] = nowDays;
        const double elapsed = nowDays - settledAt[index];
        if (relaxElapsed[index] > 0.0) {
            consumed = true;
        }
        if (!(elapsed > 0.0)) {
            continue;
        }

        const double periods = params.decayPerDay > 0.0
            ? std::floor(elapsed * params.decayPerDay + 1e-12)
            : 0.0;
        if (!(periods > 0.0)) {
            continue;
        }

        consumed = true;
        const double oldAnchor = settledAt[index];
        settledAt[index] = std::min(nowDays, oldAnchor + periods / params.decayPerDay);
        const int wholePeriods = static_cast<int>(std::min(
            periods, static_cast<double>(std::numeric_limits<int>::max())));
        for (std::size_t slot = 0; slot < kCategories; ++slot) {
            const double previous = local[index][slot];
            if (!(previous > 0.0)) {
                continue;
            }

            int owed = wholePeriods;
            if (protection[index][slot] > 0.0) {
                const double covered = std::floor(
                    (protection[index][slot] - oldAnchor) * params.decayPerDay + 1e-12);
                if (covered > 0.0) {
                    owed = std::max(0, owed - static_cast<int>(std::min(
                        static_cast<double>(owed), covered)));
                }
            }
            if (owed <= 0) {
                continue;
            }
            const DecayOutcome outcome = DecayRoll(
                DecayInputs{previous, decayMultiplier, owed}, decayDie, params.maxFame);
            local[index][slot] = outcome.newLocal;
        }
    }

    if (!consumed) {
        return;
    }

    std::vector<Row> incoming;
    ReferenceIncoming(incoming, local, remote, graph, params.spreadThreshold);
    for (std::size_t index = 0; index < count; ++index) {
        if (relaxElapsed[index] <= 0.0) {
            for (std::size_t slot = 0; slot < kSlots; ++slot) {
                if (incoming[index][slot] > remote[index][slot]) {
                    remote[index][slot] = std::min(
                        params.maxFame, std::max(0.0, incoming[index][slot]));
                }
            }
        } else {
            ReferenceRelax(remote[index], incoming[index], relaxElapsed[index],
                           params.relaxationPerDay, params.maxFame);
        }
    }
}

}

AdvanceResult AdvanceReference(std::vector<Row> local,
                               std::vector<Row> remote,
                               const std::vector<double>& settledAt,
                               const std::vector<double>& remoteSettledAt,
                               const std::vector<Row>& protection,
                               const ReferenceGraph& graph,
                               const Parameters& params,
                               double nowDays,
                               IDie& decayDie,
                               double decayMultiplier,
                               const std::vector<ScheduledChange>& scheduled) {
    const auto count = local.size();
    if (remote.size() != count || settledAt.size() != count ||
        remoteSettledAt.size() != count || protection.size() != count) {
        throw std::invalid_argument("row sizes disagree");
    }

    std::vector<ScheduledChange> due;
    due.reserve(scheduled.size());
    for (const auto& change : scheduled) {
        if (!std::isfinite(change.at)) {
            throw std::invalid_argument("non-finite scheduled change time");
        }
        if (change.at <= nowDays) {
            due.push_back(change);
        }
    }
    std::stable_sort(due.begin(), due.end(),
                     [](const ScheduledChange& a, const ScheduledChange& b) {
                         return a.at < b.at;
                     });
    std::vector<double> settled = settledAt;
    std::vector<double> remoteClock = remoteSettledAt;
    std::vector<Row> currentProtection = protection;

    double boundaryFloor = 0.0;
    if (count > 0) {
        boundaryFloor = remoteClock.front();
        for (const double clock : remoteClock) {
            boundaryFloor = std::max(boundaryFloor, clock);
        }
        boundaryFloor = std::min(boundaryFloor, nowDays);
    }

    for (const auto& change : due) {
        if (change.account >= count || change.category >= kCategories) {
            throw std::invalid_argument("scheduled change out of range");
        }
        const double boundary = std::max(boundaryFloor, change.at);
        AdvanceSegment(local, remote, settled, remoteClock, currentProtection,
                       graph, params, boundary, decayDie, decayMultiplier);
        boundaryFloor = boundary;

        const std::size_t account = change.account;
        const std::size_t category = change.category;
        if (change.kind == ScheduledKind::Set) {
            if (!std::isfinite(change.value)) {
                throw std::invalid_argument("scheduled set value is non-finite");
            }
            local[account][category] = change.value;
            remote[account][category] = std::min(remote[account][category], change.value);
        } else if (change.kind == ScheduledKind::Clear) {
            local[account][category] = 0.0;
            remote[account][category] = 0.0;
            currentProtection[account][category] = 0.0;
        } else {
            throw std::invalid_argument("unsupported scheduled command kind");
        }
        settled[account] = boundary;
    }

    AdvanceSegment(local, remote, settled, remoteClock, currentProtection,
                   graph, params, nowDays, decayDie, decayMultiplier);

    AdvanceResult result;
    result.advancedTo = nowDays;
    result.accounts.resize(count);
    for (std::size_t index = 0; index < count; ++index) {
        AccountResult& state = result.accounts[index];
        state.local = local[index];
        state.remote = remote[index];
        state.protection = currentProtection[index];
        state.settledAt = settled[index];
        state.remoteSettledAt = remoteClock[index];
    }
    return result;
}

bool RowEqual(const Row& a, const Row& b) {
    return std::equal(a.begin(), a.end(), b.begin());
}

double PublishedValue(const Row& local, const Row& remote, std::size_t slot) {
    return std::max(local[slot], remote[slot]);
}

std::string FormatRow(const Row& row) {
    std::ostringstream out;
    out << std::setprecision(3);
    out << '[' << row[0];
    for (std::size_t slot = 1; slot < kSlots; ++slot) {
        out << ',' << row[slot];
    }
    out << ']';
    return out.str();
}

}
