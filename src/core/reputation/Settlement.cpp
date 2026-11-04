#include "slsf/core/reputation/Settlement.h"

#include "slsf/core/Commands.h"
#include "slsf/core/NumericRow.h"
#include "slsf/core/Types.h"
#include "slsf/core/reputation/DiffusionGraph.h"
#include "slsf/core/reputation/DiffusionKernel.h"
#include "slsf/core/reputation/GainRules.h"
#include "slsf/core/reputation/ReputationState.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace slsf {

class PreparedSettlement {
public:
    std::vector<StableRegionKey> accountKeys;
    DiffusionGraph sourceGraph;
    Configuration sourceConfiguration;
    std::optional<RegionSnapshot> regions;
    std::vector<StableRegionKey> customKeys;
    std::shared_ptr<const CompiledDiffusionGraph> graph;
    CompiledFameRules rules;
    std::vector<std::uint8_t> customByRegion;
};

struct SettlementWorkspace {
    std::shared_ptr<const PreparedSettlement> cached;
    State staged;
    NumericRowBuffer local;
    NumericRowBuffer remote;
    NumericRowBuffer incoming;
    std::vector<double> elapsed;
    std::vector<std::uint8_t> changed;
    std::vector<ScheduledChange> orderedChanges;
};

namespace {

// Legacy half-hour batch expressed in game days (DecayTimeNeeded counts
// half-hour batches).
constexpr double kHalfHourAsDays = 0.5 / 24.0;

constexpr std::size_t kCategorySlotCount = kFameCategoryCount;

double OwedPeriods(double elapsedDays, double perDay) noexcept {
    if (!(perDay > 0.0) || !(elapsedDays > 0.0)) {
        return 0.0;
    }
    const double periods = std::floor(elapsedDays * perDay + 1e-12);
    return periods > 0.0 ? periods : 0.0;
}

bool ValidCategory(FameCategory category) noexcept {
    return IsValidFameCategory(category);
}

bool ValidAccount(const State& state, RegionId account) noexcept {
    return account && account.value < state.accounts.size();
}

void RefreshAccountCounts(State& state, const AccountState& account,
                          const CategoryMask& oldPositive,
                          const CategoryMask& oldLocal,
                          const CategoryMask& oldFlagged) noexcept {
    const auto delta = [](const CategoryMask& before, const CategoryMask& after,
                          std::array<std::uint32_t, kFameCategoryCount>& counts,
                          CategoryMask& combined) {
        (after & ~before).ForEach([&counts, &combined](FameCategory category) {
            ++counts[static_cast<std::size_t>(category)];
            combined.Set(category);
        });
        (before & ~after).ForEach([&counts, &combined](FameCategory category) {
            auto& count = counts[static_cast<std::size_t>(category)];
            if (count > 0) {
                --count;
            }
            if (count == 0) {
                combined.Reset(category);
            }
        });
    };
    delta(oldPositive, account.positiveMask, state.positiveRegionCounts,
          state.derived.numericPositive);
    delta(oldLocal, account.localMask, state.localRegionCounts,
          state.derived.numericLocal);
    delta(oldFlagged, account.flaggedMask, state.flaggedRegionCounts,
          state.derived.flagged);
}

void RepublishAccount(State& state, std::size_t accountIndex,
                      const CategoryMask& oldPositive,
                      const CategoryMask& oldLocal,
                      const CategoryMask& oldFlagged) {
    auto& account = state.accounts[accountIndex];
    RecomputeAccount(account);
    RefreshAccountCounts(state, account, oldPositive, oldLocal, oldFlagged);
}

void BumpRevision(State& state) noexcept {
    state.revision = Revision{state.revision.value + 1};
}

double DecayAccount(AccountState& account, double nowDays, double decayPerDay,
                    const CompiledFameRules& rules, IRandom& rng,
                    bool customLocation, bool& localChanged, bool& timeConsumed) {
    timeConsumed = false;
    if (account.settledAt > nowDays) {
        account.settledAt = nowDays;
    }
    // Uninitialized remote clocks (sentinel 0.0) start after this batch; a
    // row that never settled must not relax across the whole epoch.
    if (!(account.remoteSettledAt > 0.0)) {
        account.remoteSettledAt = account.settledAt;
    }
    if (account.remoteSettledAt > nowDays) {
        account.remoteSettledAt = nowDays;
    }
    const double relaxElapsed = nowDays - account.remoteSettledAt;
    account.remoteSettledAt = nowDays;
    const double elapsed = nowDays - account.settledAt;
    if (relaxElapsed > 0.0) {
        timeConsumed = true;
    }
    if (elapsed <= 0.0) {
        return relaxElapsed;
    }
    const double periods = OwedPeriods(elapsed, decayPerDay);
    if (periods > 0.0) {
        timeConsumed = true;
        const double oldAnchor = account.settledAt;
        account.settledAt = std::min(nowDays, oldAnchor + periods / decayPerDay);
        account.localMask.ForEach([&](FameCategory category) {
            const auto slot = static_cast<std::size_t>(category);
            auto& value = account.local[slot];
            // The legacy pause decrements once per half-hour batch, so only
            // ticks at or before the deadline are shielded; a long jump is
            // not exempt for its whole span.
            double owed = periods;
            const double deadline = account.protectionUntilDay[slot];
            if (deadline > 0.0) {
                const double covered =
                    std::floor((deadline - oldAnchor) * decayPerDay + 1e-12);
                if (covered > 0.0) {
                    owed = std::max(0.0, periods - std::min(periods, covered));
                }
            }
            if (owed <= 0.0) {
                return;
            }
            const double multiplier = rules.categoryMultipliers[slot];
            const auto roll = RuleDecayRoll(value, multiplier, rng, rules,
                                           customLocation);
            double total = static_cast<double>(roll.amount) * owed;
            if (total < 1.0) {
                // Legacy decay floors a whole batch at one point.
                total = 1.0;
            }
            const double decayed = std::max(0.0, std::trunc(value - total));
            if (decayed != value) {
                value = decayed;
                localChanged = true;
            }
        });
    }
    return relaxElapsed;
}

bool SameGraph(const DiffusionGraph& a, const DiffusionGraph& b) {
    return a.edges.size() == b.edges.size() &&
        std::equal(a.edges.begin(), a.edges.end(), b.edges.begin(),
            [](const EdgeDefinition& x, const EdgeDefinition& y) {
                return x.from == y.from && x.to == y.to && x.retention == y.retention;
            });
}

bool SameOwners(const std::optional<RegionSnapshot>& cached, const RegionSnapshot* current) {
    if (cached.has_value() != (current != nullptr)) {
        return false;
    }
    return !current || (cached->regions == current->regions &&
        cached->ownership.size() == current->ownership.size() &&
        std::equal(cached->ownership.begin(), cached->ownership.end(),
                   current->ownership.begin(), [](const OwnerRule& a, const OwnerRule& b) {
                       return a.owner == b.owner && a.children == b.children;
                   }));
}

bool SameCustomKeys(const PreparedSettlement& cached, const State& state) {
    return cached.customKeys.size() == state.customRegions.size() &&
        std::equal(cached.customKeys.begin(), cached.customKeys.end(),
                   state.customRegions.begin(), [](const StableRegionKey& key,
                                                  const CustomRegionState& custom) {
                       return key == custom.key;
                   });
}

Result<std::shared_ptr<const PreparedSettlement>> Prepare(
    const State& state, const DiffusionGraph& graph, const Configuration& configuration,
    const RegionSnapshot* regions, const std::shared_ptr<const PreparedSettlement>& previous) {
    if (state.accounts.size() != state.accountKeys.size()) {
        return Fail(Error::Code::InvalidState, "account keys out of sync with rows");
    }
    const bool reusableGraph = previous && previous->accountKeys == state.accountKeys &&
        SameGraph(previous->sourceGraph, graph) && SameOwners(previous->regions, regions);
    if (reusableGraph && SameCustomKeys(*previous, state) &&
        previous->sourceConfiguration.values == configuration.values) {
        return previous;
    }
    auto prepared = std::make_shared<PreparedSettlement>();
    prepared->accountKeys = state.accountKeys;
    prepared->sourceGraph = graph;
    prepared->sourceConfiguration = configuration;
    if (regions) {
        prepared->regions = *regions;
    }
    if (reusableGraph) {
        prepared->graph = previous->graph;
    } else {
        auto compiled = regions ? CompileDiffusionGraph(graph, *regions, state.accountKeys)
                                : CompileDiffusionGraph(graph, state.accountKeys);
        if (!compiled) {
            return std::unexpected(compiled.error());
        }
        prepared->graph = std::make_shared<const CompiledDiffusionGraph>(std::move(*compiled));
    }
    prepared->rules = CompileFameRules(configuration);
    if (!state.customRegions.empty()) {
        prepared->customByRegion.resize(state.accounts.size(), 0);
        for (const auto& custom : state.customRegions) {
            const auto found = std::lower_bound(state.accountKeys.begin(), state.accountKeys.end(),
                                               custom.key);
            if (found != state.accountKeys.end() && *found == custom.key) {
                prepared->customByRegion[static_cast<std::size_t>(found - state.accountKeys.begin())] = 1;
            }
            prepared->customKeys.push_back(custom.key);
        }
    }
    return std::shared_ptr<const PreparedSettlement>(std::move(prepared));
}

bool NeedsTime(const State& state, double now, double decayPerDay) {
    return std::any_of(state.accounts.begin(), state.accounts.end(),
        [=](const AccountState& account) {
            return account.remoteSettledAt < now || account.settledAt > now ||
                account.remoteSettledAt > now ||
                OwedPeriods(now - account.settledAt, decayPerDay) > 0.0;
        });
}

bool SettleSegment(State& work, double now, const Parameters& parameters,
                   const PreparedSettlement& prepared, IRandom& rng,
                   DiffusionBackend backend, SettlementWorkspace& workspace) {
    const auto count = work.accounts.size();
    workspace.local.resize(count);
    workspace.remote.resize(count);
    workspace.incoming.resize(count);
    workspace.elapsed.resize(count);
    workspace.changed.assign(count, 0);
    bool consumed = false;
    for (std::size_t index = 0; index < count; ++index) {
        bool localChanged = false;
        bool timeConsumed = false;
        workspace.elapsed[index] = DecayAccount(work.accounts[index], now,
            parameters.decayPerDay, prepared.rules, rng,
            !prepared.customByRegion.empty() && prepared.customByRegion[index] != 0,
            localChanged, timeConsumed);
        workspace.local[index] = work.accounts[index].local;
        workspace.remote[index] = work.accounts[index].remote;
        workspace.changed[index] = localChanged ? 1 : 0;
        consumed = consumed || timeConsumed;
    }
    if (!consumed) {
        return false;
    }
    ComputeIncomingRows(workspace.incoming, workspace.local, workspace.remote,
                        *prepared.graph, parameters.spreadThreshold, backend);
    bool numericChanged = false;
    for (std::size_t index = 0; index < count; ++index) {
        auto& remote = workspace.remote[index];
        if (workspace.elapsed[index] <= 0.0) {
            for (std::size_t slot = 0; slot < kCategorySlotCount; ++slot) {
                if (workspace.incoming[index][slot] > remote[slot]) {
                    remote[slot] = std::min(parameters.maxFame,
                                           std::max(0.0, workspace.incoming[index][slot]));
                }
            }
        } else {
            RelaxRemoteRows(NumericRowSpan{&remote, 1},
                            ConstNumericRowSpan{&workspace.incoming[index], 1},
                            workspace.elapsed[index], parameters.relaxationPerDay,
                            backend, parameters.maxFame);
        }
        auto& account = work.accounts[index];
        if (remote != account.remote) {
            account.remote = remote;
            workspace.changed[index] = 1;
        }
        if (workspace.changed[index] != 0) {
            const auto oldPositive = account.positiveMask;
            const auto oldLocal = account.localMask;
            const auto oldFlagged = account.flaggedMask;
            RepublishAccount(work, index, oldPositive, oldLocal, oldFlagged);
            numericChanged = true;
        }
    }
    return numericChanged;
}

}

Result<std::shared_ptr<const PreparedSettlement>> PrepareSettlement(
    const State& state, const DiffusionGraph& graph, const Configuration& configuration,
    const RegionSnapshot* regions) {
    return Prepare(state, graph, configuration, regions, {});
}

std::shared_ptr<SettlementWorkspace> MakeSettlementWorkspace() {
    return std::make_shared<SettlementWorkspace>();
}

Parameters LegacyDefaultParameters() noexcept {
    return Parameters{2.0, 1.0, 30.0, 0.6, 150.0};
}

double LegacyDecayProtectionDays(const Configuration& configuration) {
    const double batches = configuration.Get(kGainConfigKeyDecayTimeNeeded, 24.0);
    if (!std::isfinite(batches) || batches < 0.0) {
        return 0.0;
    }
    return std::floor(batches) * kHalfHourAsDays;
}

Result<int> ApplyGainFame(State& state, GameTime now, RegionId account,
                          FameCategory category, const GainFameInputs& ruleInputs,
                          const Configuration& configuration, IRandom& rng) {
    if (!ValidCategory(category)) {
        return Fail(Error::Code::InvalidArgument, "invalid fame category");
    }
    if (!ValidAccount(state, account)) {
        return Fail(Error::Code::InvalidArgument, "invalid reputation region");
    }

    if (!std::isfinite(ruleInputs.maxFameValue) || ruleInputs.maxFameValue < 0.0) {
        return Fail(Error::Code::InvalidArgument, "invalid fame cap");
    }

    auto& target = state.accounts[account.value];
    GainFameInputs inputs = ruleInputs;
    inputs.previousFame = target.fame[category];
    inputs.nightReduction = configuration.Get(kGainConfigKeyNightReduction, 1.0) != 0.0;
    inputs.nightStart = configuration.Get(kGainConfigKeyNightStart, 22.0);
    inputs.nightEnd = configuration.Get(kGainConfigKeyNightEnd, 6.0);
    double hourOfDay = std::fmod(now.days * 24.0, 24.0);
    if (hourOfDay < 0.0) {
        hourOfDay += 24.0;
    }
    inputs.hourOfDay = hourOfDay;
    const auto result = RuleGainFame(inputs, rng, configuration,
                                     static_cast<std::uint32_t>(category));

    const double cap = inputs.maxFameValue;
    target.local[category] =
        std::min(cap, inputs.previousFame + static_cast<double>(result.gained));

    const double protectionDays = LegacyDecayProtectionDays(configuration);
    if (protectionDays > 0.0) {
        const double until = now.days + protectionDays;
        for (auto& deadline : target.protectionUntilDay) {
            deadline = std::max(deadline, until);
        }
    }

    const auto oldPositive = target.positiveMask;
    const auto oldLocal = target.localMask;
    const auto oldFlagged = target.flaggedMask;
    RepublishAccount(state, account.value, oldPositive, oldLocal, oldFlagged);
    BumpRevision(state);
    return result.gained;
}

Result<double> ApplyManualSpread(State& state, RegionId source, RegionId target,
                                 FameCategory category, int percent,
                                 const Configuration& configuration) {
    if (!ValidCategory(category)) {
        return Fail(Error::Code::InvalidArgument, "invalid fame category");
    }
    static_cast<void>(configuration);
    if (!ValidAccount(state, source)) {
        return Fail(Error::Code::InvalidArgument, "invalid source region");
    }
    if (!ValidAccount(state, target)) {
        return Fail(Error::Code::InvalidArgument, "invalid target region");
    }

    const auto& sourceAccount = state.accounts[source.value];
    const auto& targetAccount = state.accounts[target.value];
    const auto rule =
        RuleManualSpreadFame(sourceAccount.fame[category], targetAccount.fame[category],
                             percent);
    if (!rule) {
        return std::unexpected(rule.error());
    }

    auto& targetRow = state.accounts[target.value];
    targetRow.local[category] = rule->newValue;

    const auto oldPositive = targetRow.positiveMask;
    const auto oldLocal = targetRow.localMask;
    const auto oldFlagged = targetRow.flaggedMask;
    RepublishAccount(state, target.value, oldPositive, oldLocal, oldFlagged);
    BumpRevision(state);
    return rule->newValue;
}

Result<void> ApplyDecayBatch(State& state, GameTime now, int multiplier,
    const Configuration& configuration, const RegionSnapshot*, IRandom& rng) {
    if (multiplier < 1) return Fail(Error::Code::InvalidArgument, "invalid decay multiplier");
    const auto rules = CompileFameRules(configuration);
    bool changed = false;
    for (std::size_t row = 0; row < state.accounts.size(); ++row) {
        auto& account = state.accounts[row];
        const auto oldPositive = account.positiveMask, oldLocal = account.localMask, oldFlagged = account.flaggedMask;
        const bool custom = row < state.accountKeys.size() && std::any_of(state.customRegions.begin(), state.customRegions.end(),
            [&](const auto& entry) { return entry.key == state.accountKeys[row]; });
        for (std::size_t slot = 0; slot < kFameCategoryCount; ++slot) {
            const auto category = static_cast<FameCategory>(slot);
            const double previous = account.fame[category];
            if (previous <= 0 || account.protectionUntilDay[slot] > now.days) continue;
            const auto roll = RuleDecayRoll(previous, rules.categoryMultipliers[slot], rng, rules, custom);
            const auto next = std::max(0.0, std::trunc(previous - std::max(1.0, static_cast<double>(roll.amount) * multiplier)));
            account.local[category] = next;
            account.remote[category] = std::min(account.remote[category], next);
            changed = changed || next != previous;
        }
        RepublishAccount(state, row, oldPositive, oldLocal, oldFlagged);
    }
    if (changed) BumpRevision(state);
    return {};
}

Result<double> ApplyManualDecay(State& state, RegionId target, FameCategory category,
                                int minimum, int maximum,
                                const Configuration& configuration,
                                IRandom& rng) {
    if (!ValidCategory(category)) {
        return Fail(Error::Code::InvalidArgument, "invalid fame category");
    }
    if (!ValidAccount(state, target)) {
        return Fail(Error::Code::InvalidArgument, "invalid target region");
    }
    if (minimum == 0 && maximum == 0) {
        return Fail(Error::Code::InvalidArgument, "manual decay has no range");
    }
    const auto amount =
        RuleManualDecayAmount(minimum, maximum, rng, kLegacyMaxFameValue);
    if (!amount) {
        return std::unexpected(amount.error());
    }
    static_cast<void>(configuration);
    if (*amount <= 0.0) {
        return *amount;
    }

    auto& account = state.accounts[target.value];
    const double previous = account.fame[category];
    const double next = std::max(0.0, previous - *amount);
    account.local[category] = next;
    account.remote[category] = std::min(account.remote[category], next);

    const auto oldPositive = account.positiveMask;
    const auto oldLocal = account.localMask;
    const auto oldFlagged = account.flaggedMask;
    RepublishAccount(state, target.value, oldPositive, oldLocal, oldFlagged);
    BumpRevision(state);
    return *amount;
}

Result<double> ApplyManualGain(State& state, RegionId target, FameCategory category,
                               int minimum, int maximum,
                               const Configuration& configuration,
                               IRandom& rng) {
    if (!ValidCategory(category)) {
        return Fail(Error::Code::InvalidArgument, "invalid fame category");
    }
    if (!ValidAccount(state, target)) {
        return Fail(Error::Code::InvalidArgument, "invalid target region");
    }
    const auto amount =
        RuleManualGainAmount(minimum, maximum, rng, kLegacyMaxFameValue);
    if (!amount) {
        return std::unexpected(amount.error());
    }
    static_cast<void>(configuration);
    if (*amount <= 0.0) {
        return *amount;
    }

    auto& account = state.accounts[target.value];
    const double previous = account.fame[category];
    const double next = std::min(kLegacyMaxFameValue, previous + *amount);
    account.local[category] = next;
    account.remote[category] = std::min(account.remote[category], next);

    const auto oldPositive = account.positiveMask;
    const auto oldLocal = account.localMask;
    const auto oldFlagged = account.flaggedMask;
    RepublishAccount(state, target.value, oldPositive, oldLocal, oldFlagged);
    BumpRevision(state);
    return next;
}

Result<SpreadPulseResult> ApplySpreadPulse(State& state, GameTime now,
                                           std::span<const RegionId> sources,
                                           const DiffusionGraph& graph,
                                           const Parameters& parameters,
                                           const Configuration& configuration,
                                           DiffusionBackend backend) {
    const auto prepared = PrepareSettlement(state, graph, configuration);
    if (!prepared) {
        return std::unexpected(prepared.error());
    }
    auto workspace = MakeSettlementWorkspace();
    return ApplySpreadPulse(state, now, sources, **prepared, parameters, *workspace, backend);
}

Result<SpreadPulseResult> ApplySpreadPulse(State& state, GameTime now,
    std::span<const RegionId> sources, const PreparedSettlement& prepared,
    const Parameters& parameters, SettlementWorkspace& workspace, DiffusionBackend backend) {
    if (sources.empty()) {
        return SpreadPulseResult{};
    }
    if (!std::isfinite(now.days)) {
        return Fail(Error::Code::InvalidArgument, "invalid pulse time");
    }
    if (!std::isfinite(parameters.spreadThreshold) || parameters.spreadThreshold < 0.0) {
        return Fail(Error::Code::InvalidArgument, "invalid spread threshold");
    }
    if (!std::isfinite(parameters.maxFame) || parameters.maxFame <= 0.0) {
        return Fail(Error::Code::InvalidArgument, "invalid fame cap");
    }
    if (state.accounts.size() != state.accountKeys.size() ||
        state.accountKeys != prepared.accountKeys) {
        return Fail(Error::Code::InvalidState, "prepared region binding differs from state");
    }
    const auto accountCount = static_cast<std::uint32_t>(state.accounts.size());
    for (const RegionId source : sources) {
        if (!source || source.value >= accountCount) {
            return Fail(Error::Code::InvalidArgument, "invalid spread source");
        }
    }

    workspace.local.assign(accountCount, NumericRow{});
    workspace.remote.assign(accountCount, NumericRow{});
    workspace.incoming.resize(accountCount);
    for (const RegionId source : sources) {
        workspace.local[source.value] = state.accounts[source.value].local;
        workspace.remote[source.value] = state.accounts[source.value].remote;
    }
    auto& incoming = workspace.incoming;
    ComputeIncomingRows(incoming, workspace.local, workspace.remote, *prepared.graph,
                        parameters.spreadThreshold, backend);

    const double protectionDays = prepared.rules.decayProtectionDays;
    const double protectionUntil =
        protectionDays > 0.0 ? now.days + protectionDays : 0.0;
    workspace.changed.assign(accountCount, 0);
    auto& changed = workspace.changed;
    std::size_t appliedTargets = 0;
    bool anyChanged = false;
    for (std::size_t index = 0; index < state.accounts.size(); ++index) {
        auto& account = state.accounts[index];
        auto& remoteRow = account.remote;
        bool received = false;
        bool increased = false;
        for (std::size_t slot = 0; slot < kCategorySlotCount; ++slot) {
            const double candidate = incoming[index][slot];
            if (!(candidate > 0.0)) {
                continue;
            }
            received = true;
            const double bounded =
                std::min(parameters.maxFame, std::max(0.0, candidate));
            if (bounded > remoteRow[slot]) {
                remoteRow[slot] = bounded;
                increased = true;
            }
        }
        bool renewal = false;
        if (received) {
            account.remoteSettledAt = std::max(account.remoteSettledAt, now.days);
            if (protectionUntil > 0.0) {
                for (auto& deadline : account.protectionUntilDay) {
                    if (protectionUntil > deadline) {
                        deadline = protectionUntil;
                        renewal = true;
                    }
                }
            }
        }
        changed[index] = (increased || renewal) ? 1 : 0;
        anyChanged = anyChanged || (changed[index] != 0);
        appliedTargets += received ? 1u : 0u;
    }

    for (std::size_t index = 0; index < state.accounts.size(); ++index) {
        if (changed[index] == 0) {
            continue;
        }
        const auto oldPositive = state.accounts[index].positiveMask;
        const auto oldLocal = state.accounts[index].localMask;
        const auto oldFlagged = state.accounts[index].flaggedMask;
        RepublishAccount(state, index, oldPositive, oldLocal, oldFlagged);
    }
    if (anyChanged) {
        BumpRevision(state);
    }
    return SpreadPulseResult{appliedTargets};
}

Result<AdvanceResult> Advance(State& state, GameTime now, AdvanceInputs& inputs) {
    if (!std::isfinite(now.days)) {
        return Fail(Error::Code::InvalidArgument, "non-finite settlement time");
    }
    if (inputs.graph == nullptr && !inputs.prepared) {
        return Fail(Error::Code::InvalidArgument, "diffusion graph missing");
    }
    if (inputs.parameters == nullptr) {
        return Fail(Error::Code::InvalidArgument, "missing settlement parameters");
    }
    if (inputs.rng == nullptr) {
        return Fail(Error::Code::InvalidArgument, "random source missing");
    }
    const auto& parameters = *inputs.parameters;
    if (!std::isfinite(parameters.decayPerDay) || parameters.decayPerDay < 0.0 ||
        !std::isfinite(parameters.relaxationPerDay) || parameters.relaxationPerDay < 0.0 ||
        !std::isfinite(parameters.spreadThreshold) || parameters.spreadThreshold < 0.0 ||
        !std::isfinite(parameters.maxFame) || parameters.maxFame <= 0.0 ||
        !std::isfinite(parameters.spreadRetention) ||
        parameters.spreadRetention < 0.0 || parameters.spreadRetention > 1.0) {
        return Fail(Error::Code::InvalidArgument, "invalid settlement parameters");
    }
    if (state.accounts.size() != state.accountKeys.size()) {
        return Fail(Error::Code::InvalidState, "account keys out of sync with rows");
    }
    auto& scheduled = inputs.scheduledChanges;
    for (const auto& change : scheduled) {
        if (!std::isfinite(change.at.days)) {
            return Fail(Error::Code::InvalidArgument, "non-finite scheduled change time");
        }
    }
    if (!inputs.workspace) {
        inputs.workspace = MakeSettlementWorkspace();
    }
    auto& workspace = *inputs.workspace;
    static const Configuration emptyConfiguration{};
    auto prepared = inputs.prepared;
    if (!prepared) {
        auto candidate = Prepare(state, *inputs.graph,
            inputs.configuration ? *inputs.configuration : emptyConfiguration,
            inputs.regions, workspace.cached);
        if (!candidate) {
            return std::unexpected(candidate.error());
        }
        workspace.cached = *candidate;
        prepared = *candidate;
    } else if (prepared->accountKeys != state.accountKeys) {
        return Fail(Error::Code::InvalidState, "prepared region binding differs from state");
    }

    workspace.orderedChanges = scheduled;
    auto& ordered = workspace.orderedChanges;
    std::stable_sort(ordered.begin(), ordered.end(),
                     [](const ScheduledChange& a, const ScheduledChange& b) {
                         return a.at.days < b.at.days;
                     });
    const bool hasDueChanges = !ordered.empty() && ordered.front().at.days <= now.days;
    std::size_t dueCount = 0;
    while (dueCount < ordered.size() && ordered[dueCount].at.days <= now.days) {
        ++dueCount;
    }
    const bool consumedTime = NeedsTime(state, now.days, parameters.decayPerDay);
    if (!hasDueChanges && !consumedTime) {
        return AdvanceResult{Status::Ready, state.revision, now};
    }

    auto& work = workspace.staged;
    work.accounts = state.accounts;
    work.derived = state.derived;
    work.positiveRegionCounts = state.positiveRegionCounts;
    work.localRegionCounts = state.localRegionCounts;
    work.flaggedRegionCounts = state.flaggedRegionCounts;
    work.revision = state.revision;
    const bool touchesUnresolved = std::any_of(ordered.begin(), ordered.begin() + dueCount,
        [](const ScheduledChange& change) {
            return change.command.kind == ExplicitCommandKind::ClearAll ||
                   change.command.kind == ExplicitCommandKind::Wipe;
        });
    if (touchesUnresolved) {
        work.unresolvedAccounts = state.unresolvedAccounts;
    } else {
        work.unresolvedAccounts.clear();
    }
    RuleContext context;
    context.configuration = inputs.configuration;
    context.rng = inputs.rng;
    double boundaryFloor = 0.0;
    if (!work.accounts.empty()) {
        boundaryFloor = work.accounts.front().remoteSettledAt;
        for (const auto& account : work.accounts) {
            boundaryFloor = std::max(boundaryFloor, account.remoteSettledAt);
        }
        boundaryFloor = std::min(boundaryFloor, now.days);
    }
    bool anyCommand = false;
    bool numericChanged = false;
    for (std::size_t index = 0; index < dueCount; ++index) {
        const auto& change = ordered[index];
        const double boundary = std::max(boundaryFloor, change.at.days);
        numericChanged = SettleSegment(work, boundary, parameters, *prepared,
            *inputs.rng, inputs.backend, workspace) || numericChanged;
        boundaryFloor = boundary;
        context.clock = GameTime{boundary};
        const auto result = ApplyExplicit(work, change.command, context);
        if (result.status != Status::Ready) {
            return Fail(Error::Code::InvalidState,
                        "scheduled change rejected: " + result.message);
        }
        anyCommand = true;
        if (change.command.kind == ExplicitCommandKind::Set ||
            change.command.kind == ExplicitCommandKind::Clear) {
            work.accounts[change.command.target.value].settledAt = boundary;
        }
    }
    numericChanged = SettleSegment(work, now.days, parameters, *prepared,
        *inputs.rng, inputs.backend, workspace) || numericChanged;
    state.accounts.swap(work.accounts);
    if (touchesUnresolved) {
        state.unresolvedAccounts.swap(work.unresolvedAccounts);
    }
    state.derived = work.derived;
    state.positiveRegionCounts = work.positiveRegionCounts;
    state.localRegionCounts = work.localRegionCounts;
    state.flaggedRegionCounts = work.flaggedRegionCounts;
    if (anyCommand || numericChanged) {
        BumpRevision(state);
    }
    std::erase_if(scheduled, [now](const ScheduledChange& change) {
        return change.at.days <= now.days;
    });
    return AdvanceResult{Status::Ready, state.revision, now};
}

}
