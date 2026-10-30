#pragma once

#include "slsf/core/Commands.h"
#include "slsf/core/Snapshots.h"
#include "slsf/core/Types.h"
#include "slsf/core/reputation/DiffusionGraph.h"
#include "slsf/core/reputation/DiffusionKernel.h"
#include "slsf/core/reputation/GainRules.h"

#include <optional>
#include <span>

namespace slsf {

// Stores copied account/region bindings, graph, custom-region keys, and compiled rules.
// Rebuild after any of those or configuration changes; numeric writes can reuse it.
// Stable keys are copied, so inputs need not outlive the snapshot.
Result<std::shared_ptr<const PreparedSettlement>> PrepareSettlement(
    const State& state, const DiffusionGraph& graph, const Configuration& configuration,
    const RegionSnapshot* regions = nullptr);

std::shared_ptr<SettlementWorkspace> MakeSettlementWorkspace();

// Legacy-effective default diffusion parameters (recorded in
// docs/superpowers/specs/2026-10-26-weighted-reputation-diffusion-design.md):
// decay periods are the legacy DecayTimeNeeded=24 half-hour batches -> 0.5
// day; the spread threshold follows MinimumFameToSpread=30; uncalibrated
// links use retention 0.6 and remote rows retreat at one value per day.
Parameters LegacyDefaultParameters() noexcept;

// Gain application (legacy GainFame entry).  Resolves multipliers, tier caps,
// the night window and the protection duration from configuration, adds the
// rule's points to the local row capped at maxFame, and renews the account's
// decay protection window (all categories): legacy GainFame sets a location
// wide CanDecay pause of int(DecayTimeNeeded) half-hour batches.
Result<int> ApplyGainFame(State& state,
                          GameTime now,
                          RegionId account,
                          FameCategory category,
                          const GainFameInputs& ruleInputs,
                          const Configuration& configuration,
                          IRandom& rng);

// Protection window length in days: DecayTimeNeeded half-hour batches.
double LegacyDecayProtectionDays(const Configuration& configuration);

// Manual spread application: validates through the spread rule, writes the
// target local row and refreshes published state and region counts.  Legacy
// spread does not refresh decay protection.
Result<double> ApplyManualSpread(State& state,
                                 RegionId source,
                                 RegionId target,
                                 FameCategory category,
                                 int percent,
                                 const Configuration& configuration);

// Manual decay application (legacy OnExternalManualFameDecay): samples or
// takes the clamped amount, subtracts it from the published fame, floors at
// zero and mirrors the result into the local row so the very next query sees
// it.  The legacy event fails without applying when both bounds are zero.
Result<double> ApplyManualDecay(State& state,
                                RegionId target,
                                FameCategory category,
                                int minimum,
                                int maximum,
                                const Configuration& configuration,
                                IRandom& rng);

// Manual gain application (legacy OnExternalManualFameGain): samples the
// increase, adds it to the published fame capped at maxFame, and mirrors the
// result into the local row so the very next query sees it.
Result<double> ApplyManualGain(State& state,
                               RegionId target,
                               FameCategory category,
                               int minimum,
                               int maximum,
                               const Configuration& configuration,
                               IRandom& rng);

struct SpreadPulseResult {
    std::size_t appliedTargets{};
};

// One-hop spread pulse (legacy Spread/SpreadRoll): ONE deferred hop
// applies against the frozen rows without decay or cascade.  Target slots
// receiving a candidate at or above the spread threshold take
// max(remote, candidate) and refresh their protection window exactly like a
// gain (legacy SpreadFame leaves the target CanDecay=false with a pause of
// int(DecayTimeNeeded) half-hour batches).
Result<SpreadPulseResult> ApplySpreadPulse(State& state,
                                           GameTime now,
                                           std::span<const RegionId> sources,
                                           const DiffusionGraph& graph,
                                           const Parameters& parameters,
                                           const Configuration& configuration,
                                           DiffusionBackend backend = DiffusionBackend::Auto);

// Call after Advance settles state through now; sources select rows in the prepared graph.
Result<SpreadPulseResult> ApplySpreadPulse(State& state, GameTime now,
    std::span<const RegionId> sources, const PreparedSettlement& prepared,
    const Parameters& parameters, SettlementWorkspace& workspace,
    DiffusionBackend backend = DiffusionBackend::Auto);

}
