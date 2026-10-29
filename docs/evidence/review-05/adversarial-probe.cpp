#include "slsf/core/Commands.h"
#include "slsf/core/reputation/Settlement.h"
#include "slsf/core/time/Scheduler.h"
#include "slsf/runtime/Dispatcher.h"

#include <cstdio>
#include <memory>

using slsf::Epoch;
using slsf::ExplicitCommand;
using slsf::ExplicitCommandKind;
using slsf::FameCategory;
using slsf::RegionId;
using slsf::StableRegionKey;
using slsf::State;
using slsf::runtime::Dispatcher;
using slsf::runtime::RuntimeBindings;
using slsf::time::ClockDomain;
using slsf::time::Scheduler;

namespace {

constexpr FameCategory kCategory = FameCategory::Whore;

StableRegionKey Key(std::uint32_t index) {
    return StableRegionKey{"adversarial-probe.esp", index + 1};
}

State MakeState(std::size_t count = 3) {
    State state;
    state.accounts.resize(count);
    state.accountKeys.resize(count);
    state.locationAccounts.resize(count);
    for (std::size_t i = 0; i < count; ++i) {
        state.accountKeys[i] = Key(static_cast<std::uint32_t>(i));
        state.locationAccounts[i] = RegionId{static_cast<std::uint32_t>(i)};
    }
    return state;
}

class DeterministicQueue final : public slsf::IRandom {
public:
    std::int32_t UniformInt(std::int32_t lo, std::int32_t) override { return lo; }
};

RuntimeBindings Bindings(const slsf::DiffusionGraph& graph,
                         const slsf::Parameters& parameters,
                         const slsf::Configuration& configuration,
                         DeterministicQueue& rng) {
    RuntimeBindings bindings;
    bindings.graph = &graph;
    bindings.parameters = &parameters;
    bindings.configuration = &configuration;
    bindings.rng = &rng;
    return bindings;
}

ExplicitCommand SetCommand(double value) {
    return ExplicitCommand{ExplicitCommandKind::Set, RegionId{0}, kCategory, value};
}

double FameAt(Dispatcher& dispatcher) {
    const auto result = dispatcher.Query(
        slsf::QueryRequest{slsf::QueryRequest::Kind::Fame, slsf::LocationId{0}, kCategory});
    return result && !result->fame.empty() ? result->fame.front() : -999.0;
}

void ProbeExecuteSettlementOrder(bool settleBeforeExecute) {
    Dispatcher dispatcher;
    const slsf::DiffusionGraph graph{};
    const auto parameters = slsf::LegacyDefaultParameters();
    const slsf::Configuration configuration{};
    DeterministicQueue rng;
    if (!dispatcher.Attach(Epoch{10, 0, 0}, Bindings(graph, parameters, configuration, rng),
                           MakeState())) {
        std::puts("execute_order attach=failed");
        return;
    }
    const auto now = slsf::GameTime::FromDays(2.0);
    dispatcher.ScheduleChange({slsf::GameTime::FromDays(1.0), SetCommand(10.0)});
    if (settleBeforeExecute) {
        (void)dispatcher.AdvanceTime(now);
    } else {
        dispatcher.Clocks().RecordGameTime(now);
    }
    const auto result = dispatcher.Execute(SetCommand(20.0));
    if (!settleBeforeExecute) {
        (void)dispatcher.AdvanceTime(now);
    }
    std::printf("execute_order settle_before=%d status=%d final=%.0f expected=20\n",
                settleBeforeExecute, static_cast<int>(result.status), FameAt(dispatcher));
}

void ProbeExecuteAfterIdleWake() {
    Dispatcher dispatcher;
    const slsf::DiffusionGraph graph{};
    const auto parameters = slsf::LegacyDefaultParameters();
    const slsf::Configuration configuration{};
    DeterministicQueue rng;
    (void)dispatcher.Attach(Epoch{12, 0, 0}, Bindings(graph, parameters, configuration, rng),
                           MakeState());
    const auto now = slsf::GameTime::FromDays(1.0);
    dispatcher.WakeTick(now); // no game timer or due scheduled change
    const auto result = dispatcher.Execute(SetCommand(20.0));
    (void)dispatcher.AdvanceTime(now);
    std::printf("execute_after_idle_wake status=%d current_clock=1 final=%.0f expected_if_settled_first=20\n",
                static_cast<int>(result.status), FameAt(dispatcher));
}

void ProbeEqualTimeChangeOrder() {
    Dispatcher dispatcher;
    const slsf::DiffusionGraph graph{};
    const auto parameters = slsf::LegacyDefaultParameters();
    const slsf::Configuration configuration{};
    DeterministicQueue rng;
    (void)dispatcher.Attach(Epoch{11, 0, 0}, Bindings(graph, parameters, configuration, rng),
                           MakeState());
    const auto at = slsf::GameTime::FromDays(1.0);
    dispatcher.ScheduleChange({at, SetCommand(10.0)});
    dispatcher.ScheduleChange({at, SetCommand(20.0)});
    (void)dispatcher.AdvanceTime(at);
    std::printf("equal_time_change_order set10_then_set20_final=%.0f expected_fifo=20\n",
                FameAt(dispatcher));
}

} // namespace

void ProbeDetachBatch() {
    Dispatcher dispatcher;
    if (!dispatcher.Attach(Epoch{1, 1, 1}, RuntimeBindings{}, slsf::State{})) {
        std::puts("detach_batch attach=failed");
        return;
    }

    int first = 0;
    int staleInNewSession = 0;
    dispatcher.Schedule(0.0, ClockDomain::Real, [&] {
        ++first;
        dispatcher.Detach();
        (void)dispatcher.Attach(Epoch{2, 1, 1}, RuntimeBindings{}, slsf::State{});
    });
    dispatcher.Schedule(0.0, ClockDomain::Real, [&] {
        const auto epoch = dispatcher.CurrentEpoch();
        staleInNewSession += epoch.session == 2 ? 1 : 0;
    });
    dispatcher.Step();
    std::printf("detach_batch first=%d stale_callback_in_new_session=%d attached=%d\n",
                first, staleInNewSession, dispatcher.Attached());
}

void ProbeSuspendBatch() {
    Dispatcher dispatcher;
    (void)dispatcher.Attach(Epoch{3, 1, 1}, RuntimeBindings{}, slsf::State{});
    int first = 0;
    int second = 0;
    int event = 0;
    dispatcher.Schedule(0.0, ClockDomain::Real, [&] {
        ++first;
        dispatcher.Suspend();
    });
    dispatcher.Schedule(0.0, ClockDomain::Real, [&] { ++second; });
    dispatcher.Post([&] { ++event; });
    dispatcher.Step();
    std::printf("mid_round_suspend first=%d second=%d event=%d suspended=%d pending_events=%zu\n",
                first, second, event, dispatcher.Suspended(), dispatcher.PendingEvents());

    Dispatcher alreadySuspended;
    (void)alreadySuspended.Attach(Epoch{4, 1, 1}, RuntimeBindings{}, slsf::State{});
    int parkedTimer = 0;
    int parkedEvent = 0;
    alreadySuspended.Schedule(0.0, ClockDomain::Real, [&] { ++parkedTimer; });
    alreadySuspended.Post([&] { ++parkedEvent; });
    alreadySuspended.Suspend();
    alreadySuspended.Step();
    std::printf("suspended_round timer=%d event=%d pending_events=%zu\n",
                parkedTimer, parkedEvent, alreadySuspended.PendingEvents());
}

void ProbeStaleDirtyAcrossSession() {
    Dispatcher dispatcher;
    double wallNow = 0.0;
    dispatcher.Clocks().SetWallSource([&] { return wallNow; });
    (void)dispatcher.Attach(Epoch{5, 1, 1}, RuntimeBindings{}, slsf::State{});
    (void)dispatcher.Step(); // anchor the fake wall clock at zero
    int sinkCalls = 0;
    dispatcher.SetDirtySink([&](RegionId) { ++sinkCalls; });
    constexpr RegionId target{42};
    dispatcher.Schedule(1.0, ClockDomain::Active, [&] {
        dispatcher.Detach();
        (void)dispatcher.Attach(Epoch{6, 1, 1}, RuntimeBindings{}, slsf::State{});
        dispatcher.RequestDirtyRefresh(target);
    });
    dispatcher.RequestDirtyRefresh(target); // old dirty timer is second in the due batch
    wallNow = 2.0;
    (void)dispatcher.Step();
    std::printf("stale_dirty_callback sink_calls=%d pending_dirty=%zu active_timers=%zu\n",
                sinkCalls, dispatcher.PendingDirty(),
                dispatcher.Timers().PendingCount(ClockDomain::Active));
}

void ProbeEventException() {
    Dispatcher dispatcher;
    (void)dispatcher.Attach(Epoch{7, 1, 1}, RuntimeBindings{}, slsf::State{});
    int afterThrow = 0;
    dispatcher.Post([] { throw 17; });
    dispatcher.Post([&] { ++afterThrow; });
    int caught = 0;
    try {
        dispatcher.Step();
    } catch (int value) {
        caught = value;
    }
    std::printf("event_exception caught=%d later_event_first_round=%d pending=%zu\n",
                caught, afterThrow, dispatcher.PendingEvents());
    (void)dispatcher.Step();
    std::printf("event_exception_next_round later_event=%d pending=%zu\n",
                afterThrow, dispatcher.PendingEvents());
}

void ProbeHeapRetention() {
    constexpr int count = 1000;
    Scheduler cancelled;
    for (int i = 0; i < count; ++i) {
        const auto token = cancelled.Schedule(ClockDomain::Game, 1000.0, [] {});
        (void)cancelled.Cancel(token);
    }
    std::printf("cancel_far_future count=%d live=%zu heap=%zu discarded=%zu\n",
                count, cancelled.PendingCount(ClockDomain::Game),
                cancelled.HeapSize(ClockDomain::Game),
                cancelled.DiscardedCount(ClockDomain::Game));
    (void)cancelled.AcquireDue(ClockDomain::Game, 999.0);
    std::printf("cancel_before_deadline heap=%zu discarded=%zu\n",
                cancelled.HeapSize(ClockDomain::Game),
                cancelled.DiscardedCount(ClockDomain::Game));
    (void)cancelled.AcquireDue(ClockDomain::Game, 1000.0);
    std::printf("cancel_at_deadline heap=%zu discarded=%zu\n",
                cancelled.HeapSize(ClockDomain::Game),
                cancelled.DiscardedCount(ClockDomain::Game));

    Scheduler sessions;
    for (int i = 0; i < count; ++i) {
        (void)sessions.Schedule(ClockDomain::Game, 1000.0, [] {});
        (void)sessions.NewSession();
    }
    std::printf("session_far_future count=%d live=%zu heap=%zu discarded=%zu\n",
                count, sessions.PendingCount(ClockDomain::Game),
                sessions.HeapSize(ClockDomain::Game),
                sessions.DiscardedCount(ClockDomain::Game));
}

void ProbeReorderedPublishedState() {
    slsf::PublishedState published;
    published.regionIds = {RegionId{7}, RegionId{2}};
    published.locationAccounts = {RegionId{2}};
    published.fame.resize(2 * slsf::kFameSlotCount);
    published.flags.resize(2 * slsf::kFameSlotCount);
    const auto category = static_cast<std::size_t>(slsf::FameCategory::Whore);
    published.fame[category] = 71.0; // RegionId 7, first block.
    published.fame[slsf::kFameSlotCount + category] = 22.0; // RegionId 2, second block.
    const auto result = slsf::Query(
        published, slsf::QueryRequest{slsf::QueryRequest::Kind::Fame,
                                      slsf::LocationId{0}, slsf::FameCategory::Whore});
    std::printf("reordered_published_state status=%s value=%.0f expected_region_2=22\n",
                result ? "ok" : "error",
                result && !result->fame.empty() ? result->fame.front() : -1.0);
}

int main() {
    ProbeExecuteSettlementOrder(false);
    ProbeExecuteSettlementOrder(true);
    ProbeExecuteAfterIdleWake();
    ProbeEqualTimeChangeOrder();
    ProbeDetachBatch();
    ProbeSuspendBatch();
    ProbeStaleDirtyAcrossSession();
    ProbeEventException();
    ProbeHeapRetention();
    ProbeReorderedPublishedState();
    return 0;
}
