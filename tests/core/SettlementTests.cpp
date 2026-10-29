#include <catch2/catch_test_macros.hpp>

#include "slsf/core/Commands.h"
#include "slsf/core/Types.h"
#include "slsf/core/reputation/DiffusionGraph.h"
#include "slsf/core/reputation/ReputationState.h"
#include "slsf/core/reputation/Settlement.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace {

using slsf::AdvanceInputs;
using slsf::AdvanceResult;
using slsf::Configuration;
using slsf::CustomRegionState;
using slsf::DiffusionBackend;
using slsf::DiffusionGraph;
using slsf::EdgeDefinition;
using slsf::ExplicitCommand;
using slsf::ExplicitCommandKind;
using slsf::Fact;
using slsf::FameCategory;
using slsf::GameTime;
using slsf::GainFameInputs;
using slsf::IRandom;
using slsf::kGainConfigKeyDecayVeryHigh;
using slsf::kGainConfigKeyNightEnd;
using slsf::kGainConfigKeyNightReduction;
using slsf::kGainConfigKeyNightStart;
using slsf::Parameters;
using slsf::RegionId;
using slsf::ScheduledChange;
using slsf::State;
using slsf::Status;
using slsf::StableRegionKey;

constexpr std::size_t kAccounts = 3;
constexpr FameCategory kCat0 = FameCategory::Whore;

StableRegionKey Key(std::uint32_t accountIndex) {
    return StableRegionKey{"location.esp", accountIndex + 1};
}

State MakeState(std::size_t count = kAccounts) {
    State state;
    state.accounts.resize(count);
    state.accountKeys.resize(count);
    state.locationAccounts.resize(count);
    for (std::size_t i = 0; i < count; ++i) {
        state.accountKeys[i] = Key(static_cast<std::uint32_t>(i));
        state.locationAccounts[i] = RegionId(static_cast<std::uint32_t>(i));
    }
    return state;
}

DiffusionGraph TwoSourceGraph() {
    DiffusionGraph graph;
    graph.edges = {
        EdgeDefinition{Key(0), Key(2), 1.0},
        EdgeDefinition{Key(1), Key(2), 1.0},
    };
    return graph;
}

DiffusionGraph WeakLinkGraph() {
    DiffusionGraph graph;
    graph.edges = {EdgeDefinition{Key(0), Key(2), 0.6}};
    return graph;
}

class SampleQueue final : public IRandom {
public:
    explicit SampleQueue(std::vector<int> samples) : samples_(std::move(samples)) {}

    std::int32_t UniformInt(std::int32_t lo, std::int32_t hi) override {
        lastLow = lo;
        lastHigh = hi;
        ++calls;
        if (cursor_ >= samples_.size()) {
            FAIL("unexpected random draw");
        }
        const int sample = samples_[cursor_++];
        if (sample < lo) {
            return lo;
        }
        if (sample > hi) {
            return hi;
        }
        return sample;
    }

    int calls{};
    std::int32_t lastLow{};
    std::int32_t lastHigh{};

private:
    std::vector<int> samples_;
    std::size_t cursor_{};
};

Parameters TestParameters(double decayPerDay, double relaxationPerDay) {
    return Parameters{decayPerDay, relaxationPerDay, 10.0, 0.6, 150.0};
}

GameTime At(double days) {
    return GameTime{days};
}

double PublishedAt(const State& state, RegionId account, FameCategory category) {
    return state.accounts[account.value].fame[category];
}

double LocalAt(const State& state, RegionId account, FameCategory category) {
    return state.accounts[account.value].local[category];
}

double RemoteAt(const State& state, RegionId account, FameCategory category) {
    return state.accounts[account.value].remote[category];
}

slsf::CommandResult ApplySet(State& state, RegionId target, FameCategory category, double value) {
    ExplicitCommand command;
    command.kind = ExplicitCommandKind::Set;
    command.target = target;
    command.category = category;
    command.value = value;
    slsf::RuleContext context;
    return slsf::ApplyExplicit(state, command, context);
}

AdvanceInputs AdvanceWith(const DiffusionGraph& graph, const Parameters& parameters) {
    AdvanceInputs inputs;
    inputs.graph = &graph;
    inputs.parameters = &parameters;
    return inputs;
}

}

TEST_CASE("propagation takes the strongest source and falls back when it leaves",
          "[settlement][advance]") {
    auto state = MakeState();
    const auto graph = TwoSourceGraph();
    const auto parameters = TestParameters(0.0, 5.0);
    SampleQueue die({});
    auto inputs = AdvanceWith(graph, parameters);
    inputs.rng = &die;

    REQUIRE(ApplySet(state, RegionId(0), kCat0, 25.0).status == Status::Ready);
    REQUIRE(ApplySet(state, RegionId(1), kCat0, 20.0).status == Status::Ready);
    REQUIRE(ApplySet(state, RegionId(2), kCat0, 0.0).status == Status::Ready);

    const auto first = slsf::Advance(state, At(1.0), inputs);
    REQUIRE(first.has_value());
    REQUIRE(first->advancedTo.days == 1.0);
    REQUIRE(PublishedAt(state, RegionId(2), kCat0) == 25.0);
    REQUIRE(RemoteAt(state, RegionId(2), kCat0) == 25.0);

    REQUIRE(ApplySet(state, RegionId(0), kCat0, 0.0).status == Status::Ready);
    const auto second = slsf::Advance(state, At(2.0), inputs);
    REQUIRE(second.has_value());
    REQUIRE(RemoteAt(state, RegionId(2), kCat0) == 20.0);
    REQUIRE(PublishedAt(state, RegionId(2), kCat0) == 20.0);

    const auto third = slsf::Advance(state, At(3.0), inputs);
    REQUIRE(third.has_value());
    REQUIRE(RemoteAt(state, RegionId(2), kCat0) == 20.0);
    REQUIRE(PublishedAt(state, RegionId(2), kCat0) == 20.0);
}

TEST_CASE("a batch delivery keeps the transmitted value without an instant fade",
          "[settlement][advance]") {
    auto state = MakeState();
    const auto graph = TwoSourceGraph();
    const auto parameters = TestParameters(0.0, 1.0);
    SampleQueue die({});
    auto inputs = AdvanceWith(graph, parameters);
    inputs.rng = &die;

    REQUIRE(ApplySet(state, RegionId(0), kCat0, 10.0).status == Status::Ready);
    const auto result = slsf::Advance(state, At(1.0), inputs);
    REQUIRE(result.has_value());
    REQUIRE(RemoteAt(state, RegionId(2), kCat0) == 10.0);
    REQUIRE(PublishedAt(state, RegionId(2), kCat0) == 10.0);
}

TEST_CASE("the same interval settles exactly once", "[settlement][advance]") {
    auto state = MakeState();
    const auto graph = TwoSourceGraph();
    const auto parameters = TestParameters(0.0, 5.0);
    SampleQueue die({});
    auto inputs = AdvanceWith(graph, parameters);
    inputs.rng = &die;

    REQUIRE(ApplySet(state, RegionId(0), kCat0, 25.0).status == Status::Ready);
    const auto first = slsf::Advance(state, At(1.0), inputs);
    REQUIRE(first.has_value());
    const auto revisionAfterFirst = state.revision;
    REQUIRE(RemoteAt(state, RegionId(2), kCat0) == 25.0);

    const auto repeat = slsf::Advance(state, At(1.0), inputs);
    REQUIRE(repeat.has_value());
    REQUIRE(state.revision.value == revisionAfterFirst.value);
    REQUIRE(RemoteAt(state, RegionId(2), kCat0) == 25.0);

    const auto later = slsf::Advance(state, At(1.5), inputs);
    REQUIRE(later.has_value());
    REQUIRE(std::abs(RemoteAt(state, RegionId(2), kCat0) - 25.0) < 1e-9);
    REQUIRE(LocalAt(state, RegionId(0), kCat0) == 25.0);
}

TEST_CASE("scheduled changes join the settlement batch in time order",
          "[settlement][advance]") {
    auto state = MakeState();
    const auto graph = TwoSourceGraph();
    const auto parameters = TestParameters(0.0, 0.0);
    SampleQueue die({});
    auto inputs = AdvanceWith(graph, parameters);
    inputs.rng = &die;

    ScheduledChange past;
    past.at = At(0.5);
    past.command.kind = ExplicitCommandKind::Set;
    past.command.target = RegionId(0);
    past.command.category = kCat0;
    past.command.value = 99.0;

    ScheduledChange future = past;
    future.at = At(5.0);
    future.command.value = 1.0;

    inputs.scheduledChanges = {future, past};
    const auto result = slsf::Advance(state, At(1.0), inputs);
    REQUIRE(result.has_value());
    REQUIRE(LocalAt(state, RegionId(0), kCat0) == 99.0);
    REQUIRE(PublishedAt(state, RegionId(0), kCat0) == 99.0);
    REQUIRE(LocalAt(state, RegionId(1), kCat0) == 0.0);

    const auto revisionAfterBatch = state.revision;
    REQUIRE(slsf::Advance(state, At(1.0), inputs).has_value());
    REQUIRE(state.revision.value == revisionAfterBatch.value);
    REQUIRE(inputs.scheduledChanges.size() == 1);

    ScheduledChange invalid = past;
    invalid.at = At(2.0);
    invalid.command.target = RegionId(77);
    auto failing = inputs;
    failing.scheduledChanges = {invalid};
    const auto revisionBeforeFailure = state.revision;
    const auto rejected = slsf::Advance(state, At(2.0), failing);
    REQUIRE_FALSE(rejected.has_value());
    REQUIRE(state.revision.value == revisionBeforeFailure.value);

    ScheduledChange validBeforeInvalid = past;
    validBeforeInvalid.at = At(1.5);
    validBeforeInvalid.command.value = 42.0;
    auto mixed = inputs;
    mixed.scheduledChanges = {validBeforeInvalid, invalid};
    const auto rejectedMixed = slsf::Advance(state, At(2.0), mixed);
    REQUIRE_FALSE(rejectedMixed.has_value());
    REQUIRE(LocalAt(state, RegionId(0), kCat0) == 99.0);

    ScheduledChange nonFinite = past;
    nonFinite.at = At(std::numeric_limits<double>::quiet_NaN());
    auto broken = inputs;
    broken.scheduledChanges = {nonFinite};
    REQUIRE_FALSE(slsf::Advance(state, At(3.0), broken).has_value());
    REQUIRE(LocalAt(state, RegionId(0), kCat0) == 99.0);
}

TEST_CASE("a scheduled set starts decay at its business time", "[settlement][advance][boundary]") {
    auto state = MakeState(1);
    DiffusionGraph graph;
    const auto parameters = TestParameters(2.0, 0.0);
    SampleQueue die({1});
    auto inputs = AdvanceWith(graph, parameters);
    inputs.rng = &die;
    inputs.scheduledChanges = {{At(0.5), {ExplicitCommandKind::Set, RegionId(0), kCat0, 10.0}}};

    REQUIRE(slsf::Advance(state, At(0.5), inputs).has_value());
    REQUIRE(LocalAt(state, RegionId(0), kCat0) == 10.0);
    REQUIRE(die.calls == 0);
    REQUIRE(slsf::Advance(state, At(1.0), inputs).has_value());
    REQUIRE(LocalAt(state, RegionId(0), kCat0) == 9.0);
    REQUIRE(die.calls == 1);
}

TEST_CASE("one advance settles each scheduled boundary before replacing fame",
          "[settlement][advance][boundary]") {
    auto state = MakeState(1);
    DiffusionGraph graph;
    const auto parameters = TestParameters(2.0, 0.0);
    SampleQueue die({1, 1});
    auto inputs = AdvanceWith(graph, parameters);
    inputs.rng = &die;
    inputs.scheduledChanges = {{At(0.5), {ExplicitCommandKind::Set, RegionId(0), kCat0, 10.0}}};
    REQUIRE(slsf::Advance(state, At(1.0), inputs).has_value());
    REQUIRE(LocalAt(state, RegionId(0), kCat0) == 9.0);
    REQUIRE(die.calls == 1);

    auto fractional = MakeState(1);
    auto partial = AdvanceWith(graph, parameters);
    partial.rng = &die;
    partial.scheduledChanges = {{At(0.25), {ExplicitCommandKind::Set, RegionId(0), kCat0, 10.0}}};
    REQUIRE(slsf::Advance(fractional, At(0.5), partial).has_value());
    REQUIRE(LocalAt(fractional, RegionId(0), kCat0) == 10.0);
    REQUIRE(die.calls == 1);
    REQUIRE(slsf::Advance(fractional, At(0.75), partial).has_value());
    REQUIRE(LocalAt(fractional, RegionId(0), kCat0) == 9.0);
    REQUIRE(die.calls == 2);
}

TEST_CASE("backward clock settlement re-anchors instead of owing debt",
          "[settlement][advance]") {
    auto state = MakeState();
    const auto graph = TwoSourceGraph();
    const auto settleParameters = TestParameters(2.0, 0.0);
    const auto parameters = TestParameters(0.0, 0.0);
    SampleQueue die({5, 5, 5, 5});
    auto settleInputs = AdvanceWith(graph, settleParameters);
    settleInputs.rng = &die;
    auto inputs = AdvanceWith(graph, parameters);
    inputs.rng = &die;

    REQUIRE(ApplySet(state, RegionId(0), kCat0, 40.0).status == Status::Ready);
    REQUIRE(slsf::Advance(state, At(2.0), settleInputs).has_value());
    const auto publishedBefore = PublishedAt(state, RegionId(2), kCat0);
    const auto localBefore = LocalAt(state, RegionId(0), kCat0);

    const auto rewound = slsf::Advance(state, At(1.0), inputs);
    REQUIRE(rewound.has_value());
    REQUIRE(PublishedAt(state, RegionId(2), kCat0) == publishedBefore);
    REQUIRE(LocalAt(state, RegionId(0), kCat0) == localBefore);
    for (const auto& account : state.accounts) {
        REQUIRE(account.settledAt == 1.0);
        REQUIRE(account.remoteSettledAt == 1.0);
    }
}

TEST_CASE("a pure clock rewind re-anchors without replaying a hop",
          "[settlement][advance]") {
    auto state = MakeState(4);
    DiffusionGraph chain;
    chain.edges = {EdgeDefinition{Key(0), Key(2), 1.0}, EdgeDefinition{Key(2), Key(3), 1.0}};
    const auto parameters = TestParameters(0.0, 0.0);
    SampleQueue die({});
    auto inputs = AdvanceWith(chain, parameters);
    inputs.rng = &die;

    REQUIRE(ApplySet(state, RegionId(0), kCat0, 40.0).status == Status::Ready);
    REQUIRE(slsf::Advance(state, At(2.0), inputs).has_value());
    REQUIRE(RemoteAt(state, RegionId(2), kCat0) == 40.0);
    REQUIRE(slsf::Advance(state, At(3.0), inputs).has_value());
    REQUIRE(RemoteAt(state, RegionId(3), kCat0) == 40.0);
    const auto revisionBefore = state.revision;

    state.accounts[2].remote[kCat0] = 5.0;
    REQUIRE(slsf::Advance(state, At(1.0), inputs).has_value());
    REQUIRE(RemoteAt(state, RegionId(3), kCat0) == 40.0);
    REQUIRE(state.accounts[3].settledAt == 0.0);
    REQUIRE(state.accounts[3].remoteSettledAt == 1.0);
    REQUIRE(state.revision.value == revisionBefore.value);
}

TEST_CASE("decay consumes whole periods once and skips protected batches",
          "[settlement][advance]") {
    auto state = MakeState();
    const auto graph = TwoSourceGraph();
    const auto parameters = TestParameters(2.0, 0.0);
    SampleQueue die({});
    auto inputs = AdvanceWith(graph, parameters);
    inputs.rng = &die;

    REQUIRE(ApplySet(state, RegionId(2), kCat0, 0.0).status == Status::Ready);

    SECTION("unprotected decay rolls per period with the batch floor") {
        REQUIRE(ApplySet(state, RegionId(1), kCat0, 7.0).status == Status::Ready);
        SampleQueue batchDie({5, 5});
        AdvanceInputs now = inputs;
        now.rng = &batchDie;
        const auto first = slsf::Advance(state, At(0.5), now);
        REQUIRE(first.has_value());
        REQUIRE(LocalAt(state, RegionId(1), kCat0) == 2.0);
        REQUIRE(batchDie.calls == 1);

        const auto second = slsf::Advance(state, At(1.0), now);
        REQUIRE(second.has_value());
        REQUIRE(LocalAt(state, RegionId(1), kCat0) == 0.0);
        REQUIRE(batchDie.calls == 2);
    }

    SECTION("protection skips the covered periods without back pay") {
        GainFameInputs rule;
        rule.iterationMultiplier = 1.0;
        rule.maxFameValue = 150.0;
        rule.resolvedMultiplier = 1.0;
        rule.nightStart = 22.0;
        rule.nightEnd = 6.0;
        rule.hourOfDay = 12.0;
        SampleQueue gainDie({5});
        const auto gained = slsf::ApplyGainFame(state, At(0.5), RegionId(1), kCat0, rule,
                                                slsf::Configuration{}, gainDie);
        REQUIRE(gained.has_value());
        REQUIRE(*gained == 5);
        REQUIRE(state.accounts[1].protectionUntilDay[static_cast<std::size_t>(kCat0)] == 1.0);

        SampleQueue protectedDie({5, 5});
        AdvanceInputs now = inputs;
        now.rng = &protectedDie;
        const auto covered = slsf::Advance(state, At(1.0), now);
        REQUIRE(covered.has_value());
        REQUIRE(LocalAt(state, RegionId(1), kCat0) == 5.0);
        REQUIRE(protectedDie.calls == 0);

        const auto afterWindow = slsf::Advance(state, At(1.5), now);
        REQUIRE(afterWindow.has_value());
        REQUIRE(LocalAt(state, RegionId(1), kCat0) == 0.0);
        REQUIRE(protectedDie.calls == 1);
    }

    SECTION("a deadline inside a batch delays only the ticks it covers") {
        REQUIRE(ApplySet(state, RegionId(1), kCat0, 7.0).status == Status::Ready);
        GainFameInputs rule;
        rule.iterationMultiplier = 1.0;
        rule.maxFameValue = 150.0;
        rule.resolvedMultiplier = 1.0;
        rule.nightStart = 22.0;
        rule.nightEnd = 6.0;
        rule.hourOfDay = 12.0;
        SampleQueue gainDie({5});
        const auto gained = slsf::ApplyGainFame(state, At(0.75), RegionId(1), kCat0, rule,
                                                slsf::Configuration{}, gainDie);
        REQUIRE(gained.has_value());
        REQUIRE(*gained == 5);
        REQUIRE(LocalAt(state, RegionId(1), kCat0) == 12.0);

        SampleQueue protectedDie({5, 5});
        AdvanceInputs now = inputs;
        now.rng = &protectedDie;
        const auto result = slsf::Advance(state, At(1.0), now);
        REQUIRE(result.has_value());
        REQUIRE(LocalAt(state, RegionId(1), kCat0) == 12.0);
        REQUIRE(protectedDie.calls == 0);

        const auto afterDeadline = slsf::Advance(state, At(2.0), now);
        REQUIRE(afterDeadline.has_value());
        REQUIRE(LocalAt(state, RegionId(1), kCat0) == 2.0);
        REQUIRE(protectedDie.calls == 1);
    }

    SECTION("the batch floor keeps at least one decay point") {
        REQUIRE(ApplySet(state, RegionId(1), kCat0, 4.0).status == Status::Ready);
        Configuration configuration;
        configuration.values.emplace(std::string{slsf::kGainConfigKeyMultiplier}, 0.4);
        SampleQueue tiny({2});
        AdvanceInputs configured = inputs;
        configured.rng = &tiny;
        configured.configuration = &configuration;
        const auto result = slsf::Advance(state, At(0.5), configured);
        REQUIRE(result.has_value());
        REQUIRE(LocalAt(state, RegionId(1), kCat0) == 3.0);
        REQUIRE(tiny.calls == 1);
    }
}

TEST_CASE("gain renews the whole-row decay protection", "[settlement]") {
    auto state = MakeState();
    GainFameInputs rule;
    rule.iterationMultiplier = 1.0;
    rule.maxFameValue = 150.0;
    rule.resolvedMultiplier = 1.0;
    rule.nightStart = 22.0;
    rule.nightEnd = 6.0;
    rule.hourOfDay = 12.0;
    SampleQueue die({5});
    const auto gained = slsf::ApplyGainFame(state, At(1.0), RegionId(1), kCat0, rule,
                                            slsf::Configuration{}, die);
    REQUIRE(gained.has_value());
    REQUIRE(*gained == 5);
    REQUIRE(LocalAt(state, RegionId(1), kCat0) == 5.0);
    REQUIRE(PublishedAt(state, RegionId(1), kCat0) == 5.0);
    for (const auto deadline : state.accounts[1].protectionUntilDay) {
        REQUIRE(deadline == 1.5);
    }

    REQUIRE(ApplySet(state, RegionId(0), kCat0, 100.0).status == Status::Ready);
    REQUIRE(ApplySet(state, RegionId(2), kCat0, 20.0).status == Status::Ready);
    const auto spread = slsf::ApplyManualSpread(state, RegionId(0), RegionId(2), kCat0, 30,
                                                slsf::Configuration{});
    REQUIRE(spread.has_value());
    REQUIRE(*spread == 50.0);
    REQUIRE(PublishedAt(state, RegionId(2), kCat0) == 50.0);
    REQUIRE(LocalAt(state, RegionId(2), kCat0) == 50.0);

    REQUIRE(ApplySet(state, RegionId(1), kCat0, 5.0).status == Status::Ready);
    const auto rejected =
        slsf::ApplyManualSpread(state, RegionId(1), RegionId(2), kCat0, 30, slsf::Configuration{});
    REQUIRE_FALSE(rejected.has_value());
}

TEST_CASE("gain reads the published fame and stays visible over a remote arrival",
          "[settlement][gain]") {
    auto state = MakeState();
    state.accounts[1].local[kCat0] = 10.0;
    state.accounts[1].remote[kCat0] = 80.0;
    slsf::RecomputeAccount(state.accounts[1]);

    GainFameInputs rule;
    rule.iterationMultiplier = 1.0;
    rule.maxFameValue = 150.0;
    rule.resolvedMultiplier = 1.0;
    rule.nightStart = 22.0;
    rule.nightEnd = 6.0;
    rule.hourOfDay = 12.0;
    SampleQueue die({10});
    const auto gained = slsf::ApplyGainFame(state, At(1.0), RegionId(1), kCat0, rule,
                                            slsf::Configuration{}, die);
    REQUIRE(gained.has_value());
    REQUIRE(die.lastHigh == 4);
    REQUIRE(*gained == 4);
    REQUIRE(LocalAt(state, RegionId(1), kCat0) == 84.0);
    REQUIRE(PublishedAt(state, RegionId(1), kCat0) == 84.0);
}

TEST_CASE("the spread pulse transmits exactly one hop and is idempotent",
          "[settlement][pulse]") {
    auto state = MakeState();
    const auto graph = WeakLinkGraph();
    const auto parameters = TestParameters(0.0, 0.0);
    SampleQueue die({});
    auto inputs = AdvanceWith(graph, parameters);
    inputs.rng = &die;

    REQUIRE(ApplySet(state, RegionId(0), kCat0, 40.0).status == Status::Ready);

    std::vector<RegionId> sources{RegionId(0)};
    const auto pulse = slsf::ApplySpreadPulse(state, At(1.0), sources, graph, parameters,
                                              slsf::Configuration{}, DiffusionBackend::Auto);
    REQUIRE(pulse.has_value());
    REQUIRE(pulse->appliedTargets == 1);
    REQUIRE(RemoteAt(state, RegionId(2), kCat0) == 24.0);
    REQUIRE(PublishedAt(state, RegionId(2), kCat0) == 24.0);
    for (const auto deadline : state.accounts[2].protectionUntilDay) {
        REQUIRE(deadline == 1.5);
    }

    const auto revisionBefore = state.revision;
    const auto repeat = slsf::ApplySpreadPulse(state, At(1.0), sources, graph, parameters,
                                               slsf::Configuration{}, DiffusionBackend::Auto);
    REQUIRE(repeat.has_value());
    REQUIRE(repeat->appliedTargets == 1);
    REQUIRE(state.revision.value == revisionBefore.value);

    auto chainState = MakeState(4);
    DiffusionGraph chain;
    chain.edges = {EdgeDefinition{Key(0), Key(2), 1.0}, EdgeDefinition{Key(2), Key(3), 1.0}};
    REQUIRE(ApplySet(chainState, RegionId(0), kCat0, 40.0).status == Status::Ready);
    const auto chainPulse =
        slsf::ApplySpreadPulse(chainState, At(1.0), sources, chain, parameters,
                               slsf::Configuration{}, DiffusionBackend::Auto);
    REQUIRE(chainPulse.has_value());
    REQUIRE(chainPulse->appliedTargets == 1);
    REQUIRE(RemoteAt(chainState, RegionId(2), kCat0) == 40.0);
    REQUIRE(RemoteAt(chainState, RegionId(3), kCat0) == 0.0);

    std::vector<RegionId> middleAsSource{RegionId(2)};
    const auto fromMiddle =
        slsf::ApplySpreadPulse(chainState, At(1.0), middleAsSource, chain, parameters,
                               slsf::Configuration{}, DiffusionBackend::Auto);
    REQUIRE(fromMiddle.has_value());
    REQUIRE(RemoteAt(chainState, RegionId(3), kCat0) == 40.0);

    State mismatched = MakeState();
    mismatched.accountKeys.push_back(Key(9));
    const auto corrupt = slsf::ApplySpreadPulse(mismatched, At(1.0), sources, graph, parameters,
                                                slsf::Configuration{}, DiffusionBackend::Auto);
    REQUIRE_FALSE(corrupt.has_value());
    REQUIRE(corrupt.error().code == slsf::Error::Code::InvalidState);
}

TEST_CASE("a spread pulse anchors the remote clock at its own time",
          "[settlement][pulse]") {
    auto state = MakeState(3);
    DiffusionGraph graph;
    graph.edges = {EdgeDefinition{Key(0), Key(2), 1.0}};
    const auto parameters = TestParameters(0.0, 1.0);
    SampleQueue die({});
    auto inputs = AdvanceWith(graph, parameters);
    inputs.rng = &die;

    REQUIRE(ApplySet(state, RegionId(0), kCat0, 40.0).status == Status::Ready);
    REQUIRE(slsf::Advance(state, At(1.0), inputs).has_value());
    REQUIRE(RemoteAt(state, RegionId(2), kCat0) == 40.0);
    REQUIRE(state.accounts[2].remoteSettledAt == 1.0);

    std::vector<RegionId> sources{RegionId(0)};
    REQUIRE(slsf::ApplySpreadPulse(state, At(2.0), sources, graph, parameters,
                                   slsf::Configuration{}, DiffusionBackend::Auto)
                .has_value());
    REQUIRE(state.accounts[2].remoteSettledAt == 2.0);

    REQUIRE(ApplySet(state, RegionId(0), kCat0, 0.0).status == Status::Ready);
    REQUIRE(slsf::Advance(state, At(2.5), inputs).has_value());
    REQUIRE(RemoteAt(state, RegionId(2), kCat0) == 39.5);
}

TEST_CASE("custom locations use the hard-coded decay tiers", "[settlement][decay]") {
    auto state = MakeState();
    state.customRegions.push_back(CustomRegionState{"Custom Camp", Key(2)});
    const auto graph = TwoSourceGraph();
    const auto parameters = TestParameters(2.0, 0.0);
    Configuration configuration;
    configuration.values.emplace(std::string{kGainConfigKeyDecayVeryHigh}, 10.0);

    REQUIRE(ApplySet(state, RegionId(1), kCat0, 120.0).status == Status::Ready);
    REQUIRE(ApplySet(state, RegionId(2), kCat0, 120.0).status == Status::Ready);

    SampleQueue die({7});
    auto inputs = AdvanceWith(graph, parameters);
    inputs.rng = &die;
    inputs.configuration = &configuration;
    REQUIRE(slsf::Advance(state, At(0.5), inputs).has_value());

    REQUIRE(LocalAt(state, RegionId(1), kCat0) == 113.0);
    REQUIRE(LocalAt(state, RegionId(2), kCat0) == 119.0);
    REQUIRE(die.calls == 1);
}

TEST_CASE("gain reads the night switch from configuration", "[settlement][gain]") {
    auto state = MakeState();
    Configuration configuration;
    configuration.values.emplace(std::string{kGainConfigKeyNightReduction}, 1.0);
    configuration.values.emplace(std::string{kGainConfigKeyNightStart}, 22.0);
    configuration.values.emplace(std::string{kGainConfigKeyNightEnd}, 6.0);

    GainFameInputs rule;
    rule.foreplay = Fact::Known(true);
    SampleQueue die({6});
    const auto gained = slsf::ApplyGainFame(state, At(23.0 / 24.0), RegionId(1), kCat0, rule,
                                            configuration, die);
    REQUIRE(gained.has_value());
    REQUIRE(*gained == 3);
    REQUIRE(LocalAt(state, RegionId(1), kCat0) == 3.0);
}

TEST_CASE("manual decay and manual spread keep their legacy edges",
          "[settlement][rules]") {
    auto state = MakeState();
    SampleQueue die({});
    REQUIRE(ApplySet(state, RegionId(1), kCat0, 30.0).status == Status::Ready);

    SECTION("manual decay subtracts and floors at zero") {
        const auto amount = slsf::ApplyManualDecay(state, RegionId(1), kCat0, 10, 10,
                                                   Configuration{}, die);
        REQUIRE(amount.has_value());
        REQUIRE(*amount == 10.0);
        REQUIRE(PublishedAt(state, RegionId(1), kCat0) == 20.0);

        const auto zeroRange = slsf::ApplyManualDecay(state, RegionId(1), kCat0, 0, 0,
                                                      Configuration{}, die);
        REQUIRE_FALSE(zeroRange.has_value());

        REQUIRE(ApplySet(state, RegionId(1), kCat0, 4.0).status == Status::Ready);
        REQUIRE(slsf::ApplyManualDecay(state, RegionId(1), kCat0, 10, 10,
                                       Configuration{}, die)
                    .has_value());
        REQUIRE(PublishedAt(state, RegionId(1), kCat0) == 0.0);
    }

    SECTION("a source may spread to itself, as the legacy event allows") {
        REQUIRE(ApplySet(state, RegionId(1), kCat0, 100.0).status == Status::Ready);
        const auto spread = slsf::ApplyManualSpread(state, RegionId(1), RegionId(1), kCat0,
                                                    30, Configuration{});
        REQUIRE(spread.has_value());
        REQUIRE(*spread == 130.0);
        REQUIRE(PublishedAt(state, RegionId(1), kCat0) == 130.0);
    }
}

TEST_CASE("the passive path honours the public fame cap", "[settlement][advance]") {
    auto state = MakeState(3);
    DiffusionGraph graph;
    graph.edges = {EdgeDefinition{Key(0), Key(2), 1.0}};
    auto parameters = TestParameters(0.0, 1.0);
    parameters.maxFame = 50.0;
    SampleQueue die({});
    auto inputs = AdvanceWith(graph, parameters);
    inputs.rng = &die;

    REQUIRE(ApplySet(state, RegionId(0), kCat0, 200.0).status == Status::Ready);
    REQUIRE(slsf::Advance(state, At(1.0), inputs).has_value());
    REQUIRE(RemoteAt(state, RegionId(2), kCat0) == 50.0);

    REQUIRE(slsf::Advance(state, At(1.5), inputs).has_value());
    REQUIRE(RemoteAt(state, RegionId(2), kCat0) == 50.0);
}

TEST_CASE("a long jump protects only the deadline ticks", "[settlement][decay]") {
    auto state = MakeState(1);
    GainFameInputs rule;
    rule.iterationMultiplier = 1.0;
    rule.maxFameValue = 150.0;
    rule.resolvedMultiplier = 1.0;
    rule.nightStart = 22.0;
    rule.nightEnd = 6.0;
    rule.hourOfDay = 12.0;
    SampleQueue gainDie({5});
    REQUIRE(slsf::ApplyGainFame(state, At(0.0), RegionId(0), kCat0, rule,
                                slsf::Configuration{}, gainDie).has_value());
    REQUIRE(state.accounts[0].protectionUntilDay[static_cast<std::size_t>(kCat0)] == 0.5);

    DiffusionGraph noEdges;
    const auto parameters = TestParameters(2.0, 0.0);
    SampleQueue die({3});
    auto inputs = AdvanceWith(noEdges, parameters);
    inputs.rng = &die;
    REQUIRE(slsf::Advance(state, At(10.0), inputs).has_value());
    REQUIRE(LocalAt(state, RegionId(0), kCat0) == 0.0);
    REQUIRE(die.calls == 1);
}

TEST_CASE("an equal-candidate pulse still renews protection", "[settlement][pulse]") {
    auto state = MakeState();
    state.accounts[2].remote[kCat0] = 24.0;
    slsf::RecomputeAccount(state.accounts[2]);
    REQUIRE(ApplySet(state, RegionId(0), kCat0, 40.0).status == Status::Ready);
    const auto graph = WeakLinkGraph();
    const auto parameters = TestParameters(0.0, 0.0);
    const slsf::Configuration configuration{};

    const auto first = slsf::ApplySpreadPulse(state, At(2.0), std::array{RegionId(0)}, graph,
                                              parameters, configuration,
                                              slsf::DiffusionBackend::Scalar);
    REQUIRE(first.has_value());
    REQUIRE(first->appliedTargets == 1);
    REQUIRE(RemoteAt(state, RegionId(2), kCat0) == 24.0);
    for (const auto deadline : state.accounts[2].protectionUntilDay) {
        REQUIRE(deadline == 2.5);
    }
    const auto revision = state.revision.value;

    const auto second = slsf::ApplySpreadPulse(state, At(2.2), std::array{RegionId(0)}, graph,
                                               parameters, configuration,
                                               slsf::DiffusionBackend::Scalar);
    REQUIRE(second.has_value());
    REQUIRE(RemoteAt(state, RegionId(2), kCat0) == 24.0);
    REQUIRE(state.accounts[2].protectionUntilDay[static_cast<std::size_t>(kCat0)] == 2.7);
    REQUIRE(state.revision.value >= revision);
}

TEST_CASE("manual spread keeps the legacy same-location transfer", "[settlement][rules]") {
    auto state = MakeState();
    REQUIRE(ApplySet(state, RegionId(0), kCat0, 100.0).status == Status::Ready);
    const auto spread = slsf::ApplyManualSpread(state, RegionId(0), RegionId(0), kCat0, 30,
                                                slsf::Configuration{});
    REQUIRE(spread.has_value());
    REQUIRE(*spread == 130.0);
    REQUIRE(LocalAt(state, RegionId(0), kCat0) == 130.0);
}

TEST_CASE("legacy default parameters match the research record", "[settlement]") {
    const auto parameters = slsf::LegacyDefaultParameters();
    REQUIRE(parameters.decayPerDay == 2.0);
    REQUIRE(parameters.relaxationPerDay == 1.0);
    REQUIRE(parameters.spreadThreshold == 30.0);
    REQUIRE(parameters.spreadRetention == 0.6);
    REQUIRE(parameters.maxFame == 150.0);
}

TEST_CASE("advance rejects missing or hostile inputs", "[settlement][advance]") {
    auto state = MakeState();
    const auto graph = TwoSourceGraph();
    const auto parameters = TestParameters(2.0, 1.0);
    SampleQueue die({});
    auto inputs = AdvanceWith(graph, parameters);
    inputs.rng = &die;

    SECTION("null graph") {
        auto broken = inputs;
        broken.graph = nullptr;
        const auto result = slsf::Advance(state, At(1.0), broken);
        REQUIRE_FALSE(result.has_value());
    }

    SECTION("null parameters") {
        auto broken = inputs;
        broken.parameters = nullptr;
        const auto result = slsf::Advance(state, At(1.0), broken);
        REQUIRE_FALSE(result.has_value());
    }

    SECTION("null rng with decay owed") {
        auto broken = inputs;
        broken.rng = nullptr;
        const auto result = slsf::Advance(state, At(1.0), broken);
        REQUIRE_FALSE(result.has_value());
    }

    SECTION("non-finite time") {
        const auto result = slsf::Advance(state, At(std::nan("")), inputs);
        REQUIRE_FALSE(result.has_value());
    }

    SECTION("retention outside [0, 1]") {
        auto broken = parameters;
        broken.spreadRetention = 1.5;
        auto bounded = inputs;
        bounded.parameters = &broken;
        const auto result = slsf::Advance(state, At(1.0), bounded);
        REQUIRE_FALSE(result.has_value());
    }

    SECTION("an unresolvable graph edge fails the batch") {
        DiffusionGraph dangling;
        dangling.edges = {EdgeDefinition{StableRegionKey{"other.esp", 9}, Key(2), 0.5}};
        auto broken = inputs;
        broken.graph = &dangling;
        const auto result = slsf::Advance(state, At(1.0), broken);
        REQUIRE_FALSE(result.has_value());
    }
}

TEST_CASE("late commands apply after time already consumed", "[settlement][boundary]") {
    auto state = MakeState(1);
    DiffusionGraph graph;
    const auto parameters = TestParameters(2, 0);
    SampleQueue die({1, 1});
    auto inputs = AdvanceWith(graph, parameters);
    inputs.rng = &die;
    REQUIRE(ApplySet(state, RegionId(0), kCat0, 20).status == Status::Ready);
    REQUIRE(slsf::Advance(state, At(1), inputs));
    REQUIRE(LocalAt(state, RegionId(0), kCat0) == 18);
    REQUIRE(die.calls == 1);

    inputs.scheduledChanges.push_back({At(0.5), {ExplicitCommandKind::Set, RegionId(0), kCat0, 10}});
    REQUIRE(slsf::Advance(state, At(1.5), inputs));
    REQUIRE(LocalAt(state, RegionId(0), kCat0) == 9);
    REQUIRE(state.accounts[0].settledAt == 1.5);
    REQUIRE(state.accounts[0].remoteSettledAt == 1.5);
    REQUIRE(die.calls == 2);
    REQUIRE(inputs.scheduledChanges.empty());
}

TEST_CASE("advance refreshes cached graph and rules after edits", "[settlement][prepared]") {
    auto state = MakeState();
    auto graph = WeakLinkGraph();
    const auto parameters = TestParameters(0.0, 0.0);
    Configuration configuration;
    SampleQueue die({});
    auto inputs = AdvanceWith(graph, parameters);
    inputs.configuration = &configuration;
    inputs.rng = &die;
    REQUIRE(ApplySet(state, RegionId(0), kCat0, 40).status == Status::Ready);
    REQUIRE(slsf::Advance(state, At(1), inputs));
    REQUIRE(RemoteAt(state, RegionId(2), kCat0) == 24);
    REQUIRE(ApplySet(state, RegionId(2), kCat0, 0).status == Status::Ready);
    graph.edges[0].retention = 0.5;
    REQUIRE(slsf::Advance(state, At(2), inputs));
    REQUIRE(RemoteAt(state, RegionId(2), kCat0) == 20);

    auto decaying = MakeState(1);
    DiffusionGraph empty;
    const auto decayParameters = TestParameters(2, 0);
    auto decay = AdvanceWith(empty, decayParameters);
    SampleQueue rolls({5, 5, 5});
    decay.rng = &rolls;
    decay.configuration = &configuration;
    REQUIRE(ApplySet(decaying, RegionId(0), kCat0, 20).status == Status::Ready);
    REQUIRE(slsf::Advance(decaying, At(0.5), decay));
    REQUIRE(LocalAt(decaying, RegionId(0), kCat0) == 15);
    configuration.values[std::string{slsf::kGainConfigKeyMultiplier}] = 2;
    REQUIRE(slsf::Advance(decaying, At(1), decay));
    REQUIRE(LocalAt(decaying, RegionId(0), kCat0) == 5);
    REQUIRE(rolls.calls == 2);
}

TEST_CASE("advance refreshes custom-region membership without changing random order",
          "[settlement][prepared]") {
    auto state = MakeState(1);
    DiffusionGraph graph;
    const auto parameters = TestParameters(2, 0);
    Configuration configuration;
    configuration.values[std::string{kGainConfigKeyDecayVeryHigh}] = 10;
    SampleQueue die({7, 7});
    auto inputs = AdvanceWith(graph, parameters);
    inputs.configuration = &configuration;
    inputs.rng = &die;
    REQUIRE(ApplySet(state, RegionId(0), kCat0, 120).status == Status::Ready);
    REQUIRE(slsf::Advance(state, At(0.5), inputs));
    REQUIRE(LocalAt(state, RegionId(0), kCat0) == 113);
    state.customRegions.push_back(CustomRegionState{"Camp", Key(0)});
    REQUIRE(slsf::Advance(state, At(1), inputs));
    REQUIRE(LocalAt(state, RegionId(0), kCat0) == 112);
    REQUIRE(die.calls == 1);
    state.customRegions.clear();
    REQUIRE(slsf::Advance(state, At(1.5), inputs));
    REQUIRE(LocalAt(state, RegionId(0), kCat0) == 105);
    REQUIRE(die.calls == 2);
}

TEST_CASE("prepared settlement owns inputs and matches the raw advance path",
          "[settlement][prepared]") {
    auto raw = MakeState();
    REQUIRE(ApplySet(raw, RegionId(0), kCat0, 90).status == Status::Ready);
    auto preparedState = raw;
    const auto parameters = TestParameters(2, 1);
    auto graph = WeakLinkGraph();
    Configuration configuration;
    const auto prepared = slsf::PrepareSettlement(raw, graph, configuration);
    REQUIRE(prepared);
    SampleQueue rawDie(std::vector<int>(20, 2));
    SampleQueue preparedDie(std::vector<int>(20, 2));
    auto rawInputs = AdvanceWith(graph, parameters);
    rawInputs.configuration = &configuration;
    rawInputs.rng = &rawDie;
    AdvanceInputs preparedInputs;
    preparedInputs.parameters = &parameters;
    preparedInputs.prepared = *prepared;
    preparedInputs.rng = &preparedDie;
    for (const double time : {0.5, 1.0, 1.0, 1.5}) {
        REQUIRE(slsf::Advance(raw, At(time), rawInputs));
        REQUIRE(slsf::Advance(preparedState, At(time), preparedInputs));
        for (std::size_t index = 0; index < raw.accounts.size(); ++index) {
            const auto& a = raw.accounts[index];
            const auto& b = preparedState.accounts[index];
            REQUIRE(a.local == b.local);
            REQUIRE(a.remote == b.remote);
            REQUIRE(a.fame == b.fame);
            REQUIRE(a.settledAt == b.settledAt);
            REQUIRE(a.remoteSettledAt == b.remoteSettledAt);
            REQUIRE(a.localMask == b.localMask);
            REQUIRE(a.positiveMask == b.positiveMask);
        }
        REQUIRE(raw.positiveRegionCounts == preparedState.positiveRegionCounts);
        REQUIRE(raw.localRegionCounts == preparedState.localRegionCounts);
        REQUIRE(raw.revision.value == preparedState.revision.value);
    }
    REQUIRE(rawDie.calls == preparedDie.calls);
    graph.edges.clear();
    configuration.values[std::string{slsf::kGainConfigKeyMultiplier}] = 100;
    REQUIRE(slsf::Advance(preparedState, At(2), preparedInputs));
    REQUIRE(LocalAt(preparedState, RegionId(0), kCat0) == 82);
    auto rebound = MakeState(3);
    rebound.accountKeys[0].localFormId += 100;
    REQUIRE_FALSE(slsf::Advance(rebound, At(2), preparedInputs));
}

TEST_CASE("advance uses owner-projected edges and refreshes owner bindings",
          "[settlement][prepared][locations]") {
    auto state = MakeState();
    slsf::RegionSnapshot regions;
    regions.regions = state.accountKeys;
    regions.ownership = {{Key(0), {Key(1)}}};
    DiffusionGraph graph;
    graph.edges = {{Key(1), Key(2), 0.6}, {Key(0), Key(1), 1}};
    const auto parameters = TestParameters(0, 0);
    SampleQueue die({});
    auto inputs = AdvanceWith(graph, parameters);
    inputs.rng = &die;
    inputs.regions = &regions;
    REQUIRE(ApplySet(state, RegionId(0), kCat0, 40).status == Status::Ready);
    REQUIRE(ApplySet(state, RegionId(1), kCat0, 100).status == Status::Ready);
    REQUIRE(slsf::Advance(state, At(1), inputs));
    REQUIRE(RemoteAt(state, RegionId(2), kCat0) == 24);
    REQUIRE(RemoteAt(state, RegionId(1), kCat0) == 0);
    regions.ownership.clear();
    REQUIRE(ApplySet(state, RegionId(2), kCat0, 0).status == Status::Ready);
    REQUIRE(slsf::Advance(state, At(2), inputs));
    REQUIRE(RemoteAt(state, RegionId(2), kCat0) == 60);
}

TEST_CASE("a rejected boundary batch preserves rows, metadata and queue order",
          "[settlement][boundary]") {
    auto state = MakeState(1);
    state.epoch = slsf::Epoch{7};
    state.configurationRevision = 12;
    state.derived.providerActive.Set(kCat0);
    state.derived.relationshipForbidden.Set(FameCategory::Slut);
    slsf::LocationPersistentState absent;
    absent.key = Key(9);
    absent.local[0] = 42;
    state.unresolvedAccounts.push_back(absent);
    REQUIRE(ApplySet(state, RegionId(0), kCat0, 20).status == Status::Ready);
    const auto revision = state.revision;
    DiffusionGraph graph;
    const auto parameters = TestParameters(2, 0);
    SampleQueue die(std::vector<int>(10, 1));
    auto inputs = AdvanceWith(graph, parameters);
    inputs.rng = &die;
    inputs.scheduledChanges = {
        {At(0.75), {ExplicitCommandKind::Set, RegionId(99), kCat0, 30}},
        {At(2), {ExplicitCommandKind::Set, RegionId(0), kCat0, 40}},
        {At(0.5), {ExplicitCommandKind::ClearAll, {}, FameCategory::Count, 0}}};
    REQUIRE_FALSE(slsf::Advance(state, At(1), inputs));
    REQUIRE(LocalAt(state, RegionId(0), kCat0) == 20);
    REQUIRE(state.accounts[0].settledAt == 0);
    REQUIRE(state.accounts[0].remoteSettledAt == 0);
    REQUIRE(state.revision.value == revision.value);
    REQUIRE(state.unresolvedAccounts[0].local[0] == 42);
    REQUIRE(inputs.scheduledChanges[0].at.days == 0.75);
    REQUIRE(inputs.scheduledChanges[1].at.days == 2);
    REQUIRE(inputs.scheduledChanges[2].at.days == 0.5);
    inputs.scheduledChanges.erase(inputs.scheduledChanges.begin());
    REQUIRE(slsf::Advance(state, At(1), inputs));
    REQUIRE(state.unresolvedAccounts[0].local[0] == 0);
    REQUIRE(state.epoch.session == 7);
    REQUIRE(state.configurationRevision == 12);
    REQUIRE(state.derived.providerActive.Test(kCat0));
    REQUIRE(state.derived.relationshipForbidden.Test(FameCategory::Slut));
    REQUIRE(state.localRegionCounts[0] == 0);
    REQUIRE(inputs.scheduledChanges.size() == 1);
}

TEST_CASE("prepared pulses reuse selected frozen sources and workspace",
          "[settlement][prepared][pulse]") {
    auto raw = MakeState();
    REQUIRE(ApplySet(raw, RegionId(0), kCat0, 40).status == Status::Ready);
    REQUIRE(ApplySet(raw, RegionId(1), kCat0, 80).status == Status::Ready);
    auto cached = raw;
    const auto graph = TwoSourceGraph();
    const auto parameters = TestParameters(0, 0);
    const auto prepared = slsf::PrepareSettlement(cached, graph, Configuration{});
    REQUIRE(prepared);
    const auto workspace = slsf::MakeSettlementWorkspace();
    for (const RegionId source : {RegionId(0), RegionId(1)}) {
        const std::array sources{source};
        REQUIRE(slsf::ApplySpreadPulse(raw, At(1), sources, graph, parameters, Configuration{}));
        const auto pulse = slsf::ApplySpreadPulse(cached, At(1), sources, **prepared,
                                                parameters, *workspace);
        REQUIRE(pulse);
        REQUIRE(pulse->appliedTargets == 1);
        REQUIRE(raw.accounts[2].remote == cached.accounts[2].remote);
        REQUIRE(raw.accounts[2].protectionUntilDay == cached.accounts[2].protectionUntilDay);
        REQUIRE(raw.positiveRegionCounts == cached.positiveRegionCounts);
    }
    REQUIRE(RemoteAt(cached, RegionId(2), kCat0) == 80);
}
