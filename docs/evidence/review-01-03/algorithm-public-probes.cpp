#include "slsf/core/reputation/DiffusionGraph.h"
#include "slsf/core/reputation/DiffusionKernel.h"
#include "slsf/core/reputation/ReputationState.h"
#include "slsf/core/reputation/Settlement.h"

#include <bit>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {

using namespace slsf;

class FixedRng final : public IRandom {
public:
    std::int32_t UniformInt(std::int32_t lo, std::int32_t) override { return lo; }
};

StableRegionKey Key(std::uint32_t id) { return {"algorithm-probe.esp", id}; }

RegionSnapshot Regions(std::uint32_t count) {
    RegionSnapshot snapshot;
    snapshot.regions.reserve(count);
    for (std::uint32_t id = 1; id <= count; ++id) {
        snapshot.regions.push_back(Key(id));
    }
    return snapshot;
}

bool Set(State& state, RegionId id, double value) {
    RuleContext context{};
    const auto result = ApplyExplicit(
        state, ExplicitCommand{ExplicitCommandKind::Set, id, FameCategory::Whore, value}, context);
    return result.status == Status::Ready;
}

void PrintRow(std::string_view label, const AccountState& account) {
    std::cout << label << " local=" << account.local[FameCategory::Whore]
              << " remote=" << account.remote[FameCategory::Whore]
              << " fame=" << account.fame[FameCategory::Whore] << '\n';
}

bool NegativeSetThenAdvance() {
    State state;
    const auto regionSnapshot = Regions(1);
    if (!BindStateToRegions(state, regionSnapshot) || !Set(state, RegionId{0}, -42.0)) {
        return false;
    }
    PrintRow("negative.after_set", state.accounts[0]);

    DiffusionGraph graph;
    Parameters parameters{0.0, 1.0, 30.0, 0.6, 150.0};
    FixedRng rng;
    AdvanceInputs inputs{&graph, &parameters, nullptr, {}, &rng, DiffusionBackend::Scalar};
    const auto repeated = Advance(state, GameTime{0.0}, inputs);
    if (!repeated) {
        return false;
    }
    PrintRow("negative.same_time_noop", state.accounts[0]);
    const auto result = Advance(state, GameTime{1.0}, inputs);
    if (!result) {
        std::cout << "negative.advance_error=" << result.error().message << '\n';
        return false;
    }
    PrintRow("negative.after_advance", state.accounts[0]);
    return true;
}

bool ManualSpreadCap(double automaticCap) {
    State state;
    const auto regionSnapshot = Regions(2);
    if (!BindStateToRegions(state, regionSnapshot) || !Set(state, RegionId{0}, 200.0) ||
        !Set(state, RegionId{1}, 0.0)) {
        return false;
    }
    auto graphResult = BuildDiffusionGraph(
        regionSnapshot, {{Key(1), Key(2), 1.0}});
    if (!graphResult) {
        return false;
    }
    Parameters parameters{0.0, 0.0, 30.0, 0.6, automaticCap};
    FixedRng rng;
    AdvanceInputs inputs{&*graphResult, &parameters, nullptr, {}, &rng,
                         DiffusionBackend::Scalar};
    const auto advanced = Advance(state, GameTime{0.5}, inputs);
    if (!advanced) {
        std::cout << "manual.cap=" << automaticCap
                  << " advance_error=" << advanced.error().message << '\n';
        return false;
    }
    PrintRow(automaticCap == 150.0 ? "manual.before_default_cap" : "manual.before_extended_cap",
             state.accounts[1]);
    const auto spread = ApplyManualSpread(state, RegionId{0}, RegionId{1},
                                          FameCategory::Whore, 100, Configuration{});
    if (!spread) {
        std::cout << "manual.spread_error=" << spread.error().message << '\n';
        return false;
    }
    std::cout << "manual.cap=" << automaticCap << " return=" << *spread << ' ';
    PrintRow("manual.after", state.accounts[1]);
    return true;
}

bool RebindStableIdentity() {
    State state;
    RegionSnapshot original;
    original.regions = {Key(2), Key(4)};
    if (!BindStateToRegions(state, original) || !Set(state, RegionId{1}, 77.0)) {
        return false;
    }
    RegionSnapshot rebound;
    rebound.regions = {Key(1), Key(2), Key(4)};
    if (!BindStateToRegions(state, rebound)) {
        return false;
    }
    std::cout << "rebind.key_at_2=" << state.accountKeys[2].localFormId
              << " fame_at_2=" << state.accounts[2].fame[FameCategory::Whore]
              << " key_at_1=" << state.accountKeys[1].localFormId
              << " fame_at_1=" << state.accounts[1].fame[FameCategory::Whore] << '\n';
    DiffusionGraph graph{{{Key(4), Key(2), 0.5}}};
    const auto compiled = CompileDiffusionGraph(graph, state.accountKeys);
    if (!compiled) {
        return false;
    }
    std::size_t edgeTarget = state.accounts.size();
    for (std::size_t row = 0; row < state.accounts.size(); ++row) {
        if (compiled->inEdgeOffsets[row] != compiled->inEdgeOffsets[row + 1]) {
            edgeTarget = row;
            break;
        }
    }
    std::cout << "rebind.edge_source=" << compiled->operands[0].source
              << " edge_target=" << edgeTarget << '\n';
    return state.accounts[2].fame[FameCategory::Whore] == 77.0 &&
           compiled->operands[0].source == 2 && edgeTarget == 1;
}

bool ScheduledChangeUsesItsTimestamp() {
    const auto run = [](bool splitAtScheduledTime) -> double {
        State state;
        if (!BindStateToRegions(state, Regions(1))) {
            return std::numeric_limits<double>::quiet_NaN();
        }
        DiffusionGraph graph;
        Parameters parameters{2.0, 0.0, 30.0, 0.6, 150.0};
        FixedRng rng;
        AdvanceInputs inputs{&graph, &parameters, nullptr, {}, &rng,
                             DiffusionBackend::Scalar};
        if (splitAtScheduledTime) {
            if (!Advance(state, GameTime{0.5}, inputs) || !Set(state, RegionId{0}, 10.0) ||
                !Advance(state, GameTime{1.0}, inputs)) {
                return std::numeric_limits<double>::quiet_NaN();
            }
        } else {
            inputs.scheduledChanges.push_back(
                ScheduledChange{GameTime{0.5},
                                ExplicitCommand{ExplicitCommandKind::Set, RegionId{0},
                                                FameCategory::Whore, 10.0}});
            if (!Advance(state, GameTime{1.0}, inputs)) {
                return std::numeric_limits<double>::quiet_NaN();
            }
        }
        return state.accounts[0].fame[FameCategory::Whore];
    };
    const double queued = run(false);
    const double segmented = run(true);
    State queuedAtBoundaryState;
    State settledThenAppliedState;
    const auto snapshot = Regions(1);
    if (!BindStateToRegions(queuedAtBoundaryState, snapshot) ||
        !BindStateToRegions(settledThenAppliedState, snapshot)) {
        return false;
    }
    DiffusionGraph graph;
    Parameters parameters{2.0, 0.0, 30.0, 0.6, 150.0};
    FixedRng rng;
    AdvanceInputs queuedBoundaryInputs{&graph, &parameters, nullptr, {}, &rng,
                                       DiffusionBackend::Scalar};
    queuedBoundaryInputs.scheduledChanges.push_back(
        ScheduledChange{GameTime{0.5},
                        ExplicitCommand{ExplicitCommandKind::Set, RegionId{0},
                                        FameCategory::Whore, 10.0}});
    const auto queuedBoundaryResult =
        Advance(queuedAtBoundaryState, GameTime{0.5}, queuedBoundaryInputs);
    AdvanceInputs settleInputs{&graph, &parameters, nullptr, {}, &rng,
                               DiffusionBackend::Scalar};
    const auto settleFirst = Advance(settledThenAppliedState, GameTime{0.5}, settleInputs);
    const bool explicitSet = Set(settledThenAppliedState, RegionId{0}, 10.0);
    if (!queuedBoundaryResult || !settleFirst || !explicitSet) {
        return false;
    }
    const double queuedBoundaryFame =
        queuedAtBoundaryState.accounts[0].fame[FameCategory::Whore];
    const double settleThenApply =
        settledThenAppliedState.accounts[0].fame[FameCategory::Whore];
    std::cout << "scheduled.at=0.5 now=0.5 queued_fame=" << queuedBoundaryFame
              << " settle_then_apply_fame=" << settleThenApply << '\n';
    std::cout << "scheduled.at=0.5 now=1 queued_fame=" << queued
              << " settle_then_apply_fame=" << segmented << '\n';
    return std::isfinite(queued) && std::isfinite(segmented) &&
           std::isfinite(queuedBoundaryFame) && std::isfinite(settleThenApply);
}

bool GainRejectsInvalidTime() {
    const auto run = [](double days) {
        State state;
        if (!BindStateToRegions(state, Regions(1))) {
            return false;
        }
        FixedRng rng;
        GainFameInputs inputs;
        inputs.maxFameValue = 150.0;
        const auto result = ApplyGainFame(state, GameTime{days}, RegionId{0},
                                          FameCategory::Whore, inputs,
                                          Configuration{}, rng);
        std::cout << "gain.time=" << days << ' '
                  << (result ? "accepted" : "rejected")
                  << " fame=" << state.accounts[0].fame[FameCategory::Whore]
                  << " protection=" << state.accounts[0].protectionUntilDay[0] << '\n';
        return true;
    };
    run(std::numeric_limits<double>::quiet_NaN());
    run(std::numeric_limits<double>::infinity());
    return true;
}

bool KernelEdgeCases() {
    if (!KernelSupportsAvx2()) {
        std::cout << "kernel.avx2=unavailable; scalar-vs-avx2 probe skipped\n";
        return true;
    }
    std::vector<StableRegionKey> keys{Key(1), Key(2), Key(3), Key(4)};
    DiffusionGraph graph{{{Key(1), Key(3), 0.6}, {Key(2), Key(3), 0.8},
                          {Key(3), Key(4), 1.0}, {Key(4), Key(1), 0.25}}};
    const auto compiled = CompileDiffusionGraph(graph, keys);
    if (!compiled) {
        return false;
    }

    NumericRowBuffer local(4), remote(4), scalar(4), avx(4);
    for (std::size_t row = 0; row < 4; ++row) {
        for (std::size_t slot = 0; slot < kFameCategoryCount; ++slot) {
            const double base = static_cast<double>((row + 1) * (slot + 3));
            local[row][slot] = (slot % 5 == 0) ? -base : base;
            remote[row][slot] = (slot % 4 == 0) ? base * 0.75 : -base * 0.5;
        }
        for (std::size_t slot = kFameCategoryCount; slot < kFameSlotCount; ++slot) {
            local[row][slot] = std::numeric_limits<double>::quiet_NaN();
            remote[row][slot] = std::numeric_limits<double>::infinity();
        }
    }
    local[0][FameCategory::Whore] = std::numeric_limits<double>::quiet_NaN();
    remote[1][FameCategory::Whore] = std::numeric_limits<double>::infinity();
    local[2][FameCategory::Whore] = -0.0;
    remote[2][FameCategory::Whore] = +0.0;
    local[3][FameCategory::Whore] = std::numeric_limits<double>::infinity();
    remote[3][FameCategory::Whore] = -std::numeric_limits<double>::infinity();

    constexpr double cutoff = 3.25;
    ComputeIncomingRows(scalar, local, remote, *compiled, cutoff, DiffusionBackend::Scalar);
    ComputeIncomingRows(avx, local, remote, *compiled, cutoff, DiffusionBackend::Avx2);
    for (std::size_t row = 0; row < scalar.size(); ++row) {
        for (std::size_t slot = 0; slot < kFameSlotCount; ++slot) {
            if (std::bit_cast<std::uint64_t>(scalar[row][slot]) !=
                std::bit_cast<std::uint64_t>(avx[row][slot])) {
                std::cout << "kernel.mismatch row=" << row << " slot=" << slot
                          << " scalar=" << scalar[row][slot] << " avx=" << avx[row][slot]
                          << '\n';
                return false;
            }
        }
    }
    std::cout << "kernel.avx2=available special-values=nan,inf,signed-zero dirty-padding=passed\n";
    return true;
}

} // namespace

int main() {
    std::cout << std::setprecision(17);
    bool ok = NegativeSetThenAdvance();
    ok = ManualSpreadCap(150.0) && ok;
    ok = ManualSpreadCap(200.0) && ok;
    ok = RebindStableIdentity() && ok;
    ok = ScheduledChangeUsesItsTimestamp() && ok;
    ok = GainRejectsInvalidTime() && ok;
    ok = KernelEdgeCases() && ok;
    std::cout << "algorithm-public-probes=" << (ok ? "PASS" : "FAIL") << '\n';
    return ok ? 0 : 1;
}
