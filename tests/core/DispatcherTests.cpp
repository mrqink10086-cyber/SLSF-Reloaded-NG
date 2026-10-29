#include "slsf/runtime/Dispatcher.h"
#include "slsf/runtime/ClockService.h"

#include "slsf/core/Commands.h"
#include "slsf/core/Types.h"
#include "slsf/core/reputation/Settlement.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_message.hpp>

#include <atomic>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <algorithm>
#include <vector>

namespace {

using slsf::AdvanceResult;
using slsf::CommandResult;
using slsf::Configuration;
using slsf::DiffusionGraph;
using slsf::EdgeDefinition;
using slsf::ExplicitCommand;
using slsf::LocationId;
using slsf::ExplicitCommandKind;
using slsf::Epoch;
using slsf::FameCategory;
using slsf::Parameters;
using slsf::PublicView;
using slsf::QueryRequest;
using slsf::RegionId;
using slsf::Result;
using slsf::ScheduledChange;
using slsf::StableRegionKey;
using slsf::State;
using slsf::Status;
using slsf::runtime::ClockDomain;
using slsf::runtime::Dispatcher;
using slsf::runtime::RuntimeBindings;

using Catch::Approx;

constexpr std::size_t kAccounts = 3;
// SettlementTests uses this ordering for the derived category sets.
constexpr FameCategory kCat0 = FameCategory::Whore;

StableRegionKey Key(std::uint32_t accountIndex) {
    return StableRegionKey{"location.esp", accountIndex + 1};
}

// Locations use the raw identifier space; MakeState maps location i to
// account i, so query results read straight from account i's row.
LocationId Loc(std::uint32_t index) { return LocationId{index}; }

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

class DeterministicQueue final : public slsf::IRandom {
public:
    std::int32_t UniformInt(std::int32_t lo, std::int32_t) override { return lo; }
};

RuntimeBindings MakeBindings(const DiffusionGraph& graph, const Parameters& parameters,
                             const Configuration& configuration) {
    static DeterministicQueue queue; // unchanged across calls; safe for concurrent readers
    RuntimeBindings bindings;
    bindings.graph = &graph;
    bindings.parameters = &parameters;
    bindings.configuration = &configuration;
    bindings.regions = nullptr;
    bindings.rng = &queue;
    return bindings;
}

} // namespace

TEST_CASE("dispatcher attach prepares and publishes the settlement snapshot") {
    Dispatcher dispatcher;
    const auto graph = TwoSourceGraph();
    const auto parameters = slsf::LegacyDefaultParameters();
    const Configuration configuration{};

    REQUIRE_FALSE(dispatcher.Attached());
    auto result = dispatcher.Query(QueryRequest{});
    REQUIRE_FALSE(result.has_value());

    const Epoch epoch{1, 0, 0};
    REQUIRE(dispatcher.Attach(epoch, MakeBindings(graph, parameters, configuration),
                              MakeState()));
    REQUIRE(dispatcher.Attached());
    REQUIRE(dispatcher.CurrentEpoch() == epoch);

    const auto advanceOk = dispatcher.AdvanceTime(slsf::GameTime::FromDays(4.0));
    REQUIRE(advanceOk.has_value());
    const auto snapshot = dispatcher.Snapshot();
    REQUIRE(snapshot != nullptr);
    CHECK(snapshot->epoch == epoch);
    CHECK(snapshot->revision >= slsf::Revision{});

    // Detach keeps the publication readable until the next session.
    dispatcher.Detach();
    REQUIRE_FALSE(dispatcher.Attached());
    REQUIRE(dispatcher.Snapshot() != nullptr);
    CHECK(dispatcher.Execute(ExplicitCommand{}).status == Status::Stale);
}

TEST_CASE("dispatcher sync command commits and publishes before returning") {
    Dispatcher dispatcher;
    const auto graph = TwoSourceGraph();
    const auto parameters = slsf::LegacyDefaultParameters();
    const Configuration configuration{};
    REQUIRE(dispatcher.Attach(Epoch{2, 0, 0}, MakeBindings(graph, parameters, configuration),
                              MakeState()));

    ExplicitCommand setCommand;
    setCommand.kind = ExplicitCommandKind::Set;
    setCommand.target = RegionId{0};
    setCommand.category = kCat0;
    setCommand.value = 30.0;

    const auto result = dispatcher.Execute(setCommand);
    REQUIRE(result.status == Status::Ready);

    // The publication must be visible without any further executor round:
    // the revision matches the command result exactly (commit-before-return).
    const auto snapshot = dispatcher.Snapshot();
    REQUIRE(snapshot != nullptr);
    CHECK(snapshot->revision.value == result.revision.value);

    const auto viewed = dispatcher.Query(
        QueryRequest{QueryRequest::Kind::Fame, Loc(0), kCat0});
    REQUIRE(viewed.has_value());
    REQUIRE_FALSE(viewed->fame.empty());
    CHECK(viewed->fame.front() == Approx(30.0));
    CHECK(viewed->revision.value == result.revision.value);

    // Rejection keeps the revision (apply is atomic).
    ExplicitCommand bogus = setCommand;
    bogus.category = static_cast<FameCategory>(99);
    const auto rejected = dispatcher.Execute(bogus);
    CHECK(rejected.status == Status::Invalid);
    CHECK(rejected.revision.value == result.revision.value);
    CHECK(dispatcher.Snapshot()->revision.value == result.revision.value);
}

TEST_CASE("dispatcher scheduled changes settle once at their game day") {
    Dispatcher dispatcher;
    const auto graph = TwoSourceGraph();
    const auto parameters = slsf::LegacyDefaultParameters();
    const Configuration configuration{};
    REQUIRE(dispatcher.Attach(Epoch{3, 0, 0}, MakeBindings(graph, parameters, configuration),
                              MakeState()));

    ExplicitCommand setCommand;
    setCommand.kind = ExplicitCommandKind::Set;
    setCommand.target = RegionId{0};
    setCommand.category = kCat0;
    setCommand.value = 20.0;

    dispatcher.ScheduleChange(ScheduledChange{slsf::GameTime::FromDays(3.0), setCommand});
    REQUIRE(dispatcher.PendingChanges() == 1);
    auto advanceEarly = dispatcher.AdvanceTime(slsf::GameTime::FromDays(2.0));
    if (!advanceEarly) {
        INFO(advanceEarly.error().message);
    }
    REQUIRE(advanceEarly.has_value());
    CHECK(dispatcher.PendingChanges() == 1); // tomorrow's business stays queued

    auto before = dispatcher.Snapshot();
    REQUIRE(before != nullptr);

    auto advanced = dispatcher.AdvanceTime(slsf::GameTime::FromDays(3.0));
    REQUIRE(advanced.has_value());
    CHECK(advanced->status == Status::Ready);
    CHECK(advanced->revision.value > before->revision.value);
    CHECK(dispatcher.PendingChanges() == 0); // consumed exactly once

    const auto viewed = dispatcher.Query(
        QueryRequest{QueryRequest::Kind::Fame, Loc(0), kCat0});
    CHECK(viewed.value().fame.front() == Approx(20.0));

    // Game-clock rewind: the value is rejected as client error (same
    // day already advanced).  A real session change happens via Detach.
    auto rewound = dispatcher.AdvanceTime(slsf::GameTime::FromDays(1.0));
    REQUIRE_FALSE(rewound.has_value());
    CHECK(rewound.error().code == slsf::Error::Code::InvalidArgument);
    CHECK(dispatcher.Snapshot()->revision.value > 0); // rejection keeps state
}

TEST_CASE("dispatcher executor rounds run due timers in stable domain order") {
    Dispatcher dispatcher;
    const auto graph = TwoSourceGraph();
    const auto parameters = slsf::LegacyDefaultParameters();
    const Configuration configuration{};
    REQUIRE(dispatcher.Attach(Epoch{4, 0, 0}, MakeBindings(graph, parameters, configuration),
                              MakeState()));

    // Deterministic wall seconds on top of the domain accumulators.
    double fakeWall = 100.0;
    dispatcher.Clocks().SetWallSource([&fakeWall] { return fakeWall; });

    std::vector<char> order;
    std::ignore = dispatcher.Schedule(2.0, ClockDomain::Real, [&] { order.push_back('A'); });
    std::ignore = dispatcher.Schedule(1.0, ClockDomain::Real, [&] { order.push_back('B'); });
    std::ignore = dispatcher.Schedule(2.0, ClockDomain::Active, [&] { order.push_back('C'); });

    auto token = dispatcher.Schedule(1.5, ClockDomain::Real, [&] { order.push_back('X'); });
    std::ignore = token;
    REQUIRE(dispatcher.Timers().Cancel(token));

    // Nothing fires while the clocks stand still.
    dispatcher.Step();
    CHECK(order.empty());
    CHECK(dispatcher.stats().steps == 1);

    // One second of active time: only the earliest real timer is due.
    fakeWall += 1.0;
    dispatcher.Step();
    CHECK(order == std::vector<char>{'B'});

    // The remaining pair ties on deadline: game -> real -> active, and both
    // real timers keep their stable enqueue order.
    fakeWall += 1.0;
    dispatcher.Step();
    CHECK(order == std::vector<char>{'B', 'A', 'C'});
}

TEST_CASE("dispatcher resumes parked timer flows exactly once") {
    Dispatcher dispatcher;
    const auto graph = TwoSourceGraph();
    const auto parameters = slsf::LegacyDefaultParameters();
    const Configuration configuration{};
    REQUIRE(dispatcher.Attach(Epoch{5, 0, 0}, MakeBindings(graph, parameters, configuration),
                              MakeState()));
    double fakeWall = 500.0;
    dispatcher.Clocks().SetWallSource([&fakeWall] { return fakeWall; });

    int elapsedNormally = 0;
    int aborted = 0;
    auto flow = dispatcher.SpawnFlow(
        [&]() -> Dispatcher::Flow {
            Dispatcher::Delay wait{dispatcher, 3.0, ClockDomain::Real};
            if (co_await wait) {
                ++elapsedNormally;
            } else {
                ++aborted;
            }
            co_return;
        });
    REQUIRE(flow.Handle() != nullptr);
    REQUIRE(dispatcher.Timers().PendingCount(ClockDomain::Real) == 1);

    fakeWall += 1.0;
    dispatcher.Step();
    CHECK(elapsedNormally == 0); // 2 s remain
    fakeWall += 2.0;
    dispatcher.Step();
    CHECK(elapsedNormally == 1);
    CHECK(aborted == 0);
    CHECK(flow.Done());
    dispatcher.Step();
    CHECK(elapsedNormally == 1); // the frame is gone; no second resume

    // The finished flow destroys exactly once (Flow Destructor RAII).
    flow.Handle() = nullptr; // nothing left for the RAII guard
}

TEST_CASE("dispatcher session replacement aborts waits and releases once") {
    Dispatcher dispatcher;
    const auto graph = TwoSourceGraph();
    const auto parameters = slsf::LegacyDefaultParameters();
    const Configuration configuration{};
    REQUIRE(dispatcher.Attach(Epoch{6, 0, 0}, MakeBindings(graph, parameters, configuration),
                              MakeState()));
    double fakeWall = 900.0;
    dispatcher.Clocks().SetWallSource([&fakeWall] { return fakeWall; });

    int elapsedNormally = 0;
    int aborted = 0;
    auto flowA = dispatcher.SpawnFlow(
        [&]() -> Dispatcher::Flow {
            Dispatcher::Delay wait{dispatcher, 60.0, ClockDomain::Real};
            if (co_await wait) {
                ++elapsedNormally;
            } else {
                ++aborted;
            }
            co_return;
        });
    auto flowB = dispatcher.SpawnFlow(
        [&]() -> Dispatcher::Flow {
            Dispatcher::Delay wait{dispatcher, 60.0, ClockDomain::Active};
            if (co_await wait) {
                ++elapsedNormally;
            } else {
                ++aborted;
            }
            co_return;
        });
    REQUIRE(dispatcher.Timers().PendingCount() == 2);

    dispatcher.Detach(); // kills both waits: one release per resource
    CHECK(aborted == 2);
    CHECK(elapsedNormally == 0);
    CHECK(dispatcher.Timers().PendingCount() == 0);
    REQUIRE(flowA.Done());
    REQUIRE(flowB.Done());

    // Step and re-attach with fresh epoch: no zombie resumes, and events
    // from the old epoch never leak into the new session.
    dispatcher.Step();
    CHECK(elapsedNormally == 0);
    Epoch staleEpoch{6, 0, 0};
    dispatcher.Post(staleEpoch, [] {});
    dispatcher.Post(Epoch{7, 0, 0}, [] {});
    dispatcher.Detach();
    REQUIRE(dispatcher.Attach(Epoch{7, 0, 0}, MakeBindings(graph, parameters, configuration),
                              MakeState()));
    CHECK(dispatcher.Timers().PendingCount() == 0);
}

TEST_CASE("dispatcher consumes queued events individually and drops stale epochs") {
    Dispatcher dispatcher;
    const auto graph = TwoSourceGraph();
    const auto parameters = slsf::LegacyDefaultParameters();
    const Configuration configuration{};
    REQUIRE(dispatcher.Attach(Epoch{8, 0, 0}, MakeBindings(graph, parameters, configuration),
                              MakeState()));

    std::vector<int> consumed;
    dispatcher.Post([&] { consumed.push_back(1); });
    dispatcher.Post([&] { consumed.push_back(2); });
    dispatcher.Post(Epoch{999, 0, 0}, [] {}); // stale at post-time stamp
    REQUIRE(dispatcher.PendingEvents() == 3);

    dispatcher.Step();
    CHECK(consumed == std::vector<int>{1, 2}); // one by one, in post order
    auto stats = dispatcher.stats();
    CHECK(stats.eventsConsumed == 2);
    CHECK(stats.eventsDroppedStale == 1);
    CHECK(dispatcher.PendingEvents() == 0);
}

TEST_CASE("dispatcher merges same-target dirty requests inside the window") {
    Dispatcher dispatcher;
    const auto graph = TwoSourceGraph();
    const auto parameters = slsf::LegacyDefaultParameters();
    const Configuration configuration{};
    REQUIRE(dispatcher.Attach(Epoch{9, 0, 0}, MakeBindings(graph, parameters, configuration),
                              MakeState()));
    double fakeWall = 0.0;
    dispatcher.Clocks().SetWallSource([&fakeWall] { return fakeWall; });

    std::vector<RegionId> fired;
    dispatcher.SetDirtySink([&](RegionId target) { fired.push_back(target); });

    const RegionId target{5};
    dispatcher.RequestDirtyRefresh(target);
    dispatcher.RequestDirtyRefresh(target);
    dispatcher.RequestDirtyRefresh(target); // same window: all three are one batch
    CHECK(dispatcher.PendingDirty() == 1);
    auto stats = dispatcher.stats();
    CHECK(stats.dirtyRequests == 3);
    CHECK(stats.dirtyMerged == 2);

    const RegionId other{6};
    dispatcher.RequestDirtyRefresh(other); // parallel window, separate timer
    CHECK(dispatcher.PendingDirty() == 2);

    fakeWall += 1.0; // just over Executor::kDirtyWindowSeconds
    dispatcher.Step();
    std::sort(fired.begin(), fired.end());
    CHECK(fired == std::vector<RegionId>{target, other});
    CHECK(dispatcher.PendingDirty() == 0);
    CHECK(dispatcher.stats().dirtyFired == 2);

    // After a fire the next request arms a fresh window, not a merge.
    fired.clear();
    dispatcher.RequestDirtyRefresh(target);
    CHECK(dispatcher.PendingDirty() == 1);
    fakeWall += 1.0;
    dispatcher.Step();
    CHECK(fired == std::vector<RegionId>{target});
    CHECK(dispatcher.stats().dirtyFired == 3);
}

TEST_CASE("dispatcher suspension gates commands, timers, and events") {
    Dispatcher dispatcher;
    const auto graph = TwoSourceGraph();
    const auto parameters = slsf::LegacyDefaultParameters();
    const Configuration configuration{};
    REQUIRE(dispatcher.Attach(Epoch{10, 0, 0},
                              MakeBindings(graph, parameters, configuration), MakeState()));
    double fakeWall = 0.0;
    dispatcher.Clocks().SetWallSource([&fakeWall] { return fakeWall; });

    int fired = 0;
    std::ignore = dispatcher.Schedule(1.0, ClockDomain::Real, [&] { ++fired; });
    dispatcher.Post([] {});
    dispatcher.RequestDirtyRefresh(RegionId{1});

    dispatcher.Suspend();
    REQUIRE(dispatcher.Suspended());

    fakeWall += 5.0;
    dispatcher.Step(); // accumulators move, nothing else does
    CHECK(fired == 0);
    CHECK(dispatcher.PendingEvents() == 1);
    CHECK(dispatcher.PendingDirty() == 1);
    CHECK(dispatcher.Execute(ExplicitCommand{}).status == Status::NotReady);

    dispatcher.Resume();
    CHECK_FALSE(dispatcher.Suspended());
    dispatcher.Step(); // the accumulated time makes the timer due
    CHECK(fired == 1);
    CHECK(dispatcher.PendingEvents() == 0);
    CHECK(dispatcher.PendingDirty() == 0);
}

TEST_CASE("dispatcher survives concurrent commands while the executor steps") {
    Dispatcher dispatcher;
    const auto graph = TwoSourceGraph();
    const auto parameters = slsf::LegacyDefaultParameters();
    const Configuration configuration{};
    REQUIRE(dispatcher.Attach(Epoch{11, 0, 0},
                              MakeBindings(graph, parameters, configuration), MakeState()));

    ExplicitCommand setCommand;
    setCommand.kind = ExplicitCommandKind::Set;
    setCommand.target = RegionId{0};
    setCommand.category = kCat0;
    setCommand.value = 1.0;

    std::atomic<std::uint64_t> observedMaxRevision{0};
    std::atomic<bool> stop{false};
    std::atomic<int> commits{0};
    std::atomic<int> rejects{0};

    std::vector<std::thread> workers;
    for (int worker = 0; worker < 3; ++worker) {
        workers.emplace_back([&, worker] {
            auto local = setCommand;
            local.target = RegionId{static_cast<std::uint32_t>(worker)};
            for (int i = 0; i < 64; ++i) {
                const auto result = dispatcher.Execute(local);
                if (result.status == Status::Ready) {
                    ++commits;
                } else {
                    ++rejects;
                    REQUIRE(dispatcher.Suspended() == false);
                }
            }
        });
    }
    workers.emplace_back([&] {
        while (!stop.load()) {
            const auto snapshot = dispatcher.Snapshot();
            if (snapshot != nullptr) {
                // Reaching here guarantees at least one published revision
                // entered the reader; the main thread waits for this before
                // it stops, so the join is race-free.
                std::uint64_t previous = observedMaxRevision.load();
                while (snapshot->revision.value > previous &&
                       !observedMaxRevision.compare_exchange_weak(
                           previous, snapshot->revision.value)) {
                }
            }
        }
    });

    // Barrier: the reader must observe a committed revision before the
    // round ends; otherwise the whole commit burst could finish while the
    // reader is still being scheduled (the same wait also keeps the
    // executor round after at least one concurrent publication).
    while (observedMaxRevision.load() == 0) {
        std::this_thread::yield();
    }

    dispatcher.Step();
    stop.store(true);
    for (auto& worker : workers) {
        worker.join();
    }

    CHECK(observedMaxRevision.load() > 0);
    CHECK(commits.load() >= 192);
    CHECK(dispatcher.Snapshot() != nullptr);
    CHECK(dispatcher.Snapshot()->revision.value >= observedMaxRevision.load());
}

TEST_CASE("dispatcher flow move assignment destroys the replaced frame once") {
    Dispatcher dispatcher;
    const auto graph = TwoSourceGraph();
    const auto parameters = slsf::LegacyDefaultParameters();
    const Configuration configuration{};
    REQUIRE(dispatcher.Attach(Epoch{20, 0, 0}, MakeBindings(graph, parameters, configuration),
                              MakeState()));
    double fakeWall = 0.0;
    dispatcher.Clocks().SetWallSource([&fakeWall] { return fakeWall; });

    int abortedA = 0;
    int resumedB = 0;
    int abortedB = 0;
    auto flowA = dispatcher.SpawnFlow([&]() -> Dispatcher::Flow {
        Dispatcher::Delay wait{dispatcher, 100.0, ClockDomain::Real};
        if (co_await wait) {
            ++resumedB; // never: A is replaced before its deadline
        } else {
            ++abortedA;
        }
        co_return;
    });
    auto flowB = dispatcher.SpawnFlow([&]() -> Dispatcher::Flow {
        Dispatcher::Delay wait{dispatcher, 200.0, ClockDomain::Real};
        if (co_await wait) {
            ++resumedB;
        } else {
            ++abortedB;
        }
        co_return;
    });
    REQUIRE(dispatcher.Timers().PendingCount() == 2);

    flowA = std::move(flowB); // must destroy A's frame exactly once
    CHECK(abortedA == 1);
    CHECK_FALSE(flowA.Done()); // now owns B
    CHECK(dispatcher.Timers().PendingCount() == 1);

    fakeWall += 200.0;
    dispatcher.Step();
    CHECK(resumedB == 1);
    CHECK(abortedB == 0);
    CHECK(flowA.Done());
}

TEST_CASE("dispatcher cancel wait disarms the pending timer") {
    Dispatcher dispatcher;
    const auto graph = TwoSourceGraph();
    const auto parameters = slsf::LegacyDefaultParameters();
    const Configuration configuration{};
    REQUIRE(dispatcher.Attach(Epoch{21, 0, 0}, MakeBindings(graph, parameters, configuration),
                              MakeState()));
    double fakeWall = 10.0;
    dispatcher.Clocks().SetWallSource([&fakeWall] { return fakeWall; });

    int resumed = 0;
    int aborted = 0;
    auto flow = dispatcher.SpawnFlow([&]() -> Dispatcher::Flow {
        Dispatcher::Delay wait{dispatcher, 100.0, ClockDomain::Real};
        if (co_await wait) {
            ++resumed;
        } else {
            ++aborted;
        }
        co_return;
    });
    REQUIRE(dispatcher.Timers().PendingCount(ClockDomain::Real) == 1);

    REQUIRE(flow.CancelWait()); // the armed timer must die too
    CHECK(aborted == 1);
    CHECK(flow.Done());
    CHECK(dispatcher.Timers().PendingCount(ClockDomain::Real) == 0);

    fakeWall += 1000.0;
    dispatcher.Step();
    CHECK(resumed == 0);
    CHECK(aborted == 1);
}

TEST_CASE("dispatcher destroying a parked flow disarms its timer") {
    Dispatcher dispatcher;
    const auto graph = TwoSourceGraph();
    const auto parameters = slsf::LegacyDefaultParameters();
    const Configuration configuration{};
    REQUIRE(dispatcher.Attach(Epoch{22, 0, 0}, MakeBindings(graph, parameters, configuration),
                              MakeState()));
    double fakeWall = 0.0;
    dispatcher.Clocks().SetWallSource([&fakeWall] { return fakeWall; });

    int aborted = 0;
    {
        auto flow = dispatcher.SpawnFlow([&]() -> Dispatcher::Flow {
            Dispatcher::Delay wait{dispatcher, 30.0, ClockDomain::Active};
            if (!(co_await wait)) {
                ++aborted;
            }
            co_return;
        });
        REQUIRE(dispatcher.Timers().PendingCount(ClockDomain::Active) == 1);
    } // ~Flow cancels the wait and settles the frame

    CHECK(aborted == 1);
    CHECK(dispatcher.Timers().PendingCount(ClockDomain::Active) == 0);
    fakeWall += 100.0;
    dispatcher.Step();
    CHECK(aborted == 1); // no zombie resume
}

TEST_CASE("dispatcher isolates throwing timer bodies") {
    Dispatcher dispatcher;
    const auto graph = TwoSourceGraph();
    const auto parameters = slsf::LegacyDefaultParameters();
    const Configuration configuration{};
    REQUIRE(dispatcher.Attach(Epoch{23, 0, 0}, MakeBindings(graph, parameters, configuration),
                              MakeState()));
    double fakeWall = 0.0;
    dispatcher.Clocks().SetWallSource([&fakeWall] { return fakeWall; });

    std::size_t ran = 0;
    dispatcher.Schedule(1.0, ClockDomain::Real, [] { throw std::runtime_error("boom"); });
    dispatcher.Schedule(1.0, ClockDomain::Real, [&ran] { ++ran; });

    fakeWall += 1.0;
    CHECK_NOTHROW(dispatcher.Step());
    CHECK(ran == 1);
    CHECK(dispatcher.stats().timerErrors == 1);
    CHECK(dispatcher.Timers().PendingCount(ClockDomain::Real) == 0);
}

TEST_CASE("dispatcher rejects non-finite game time without moving the floor") {
    Dispatcher dispatcher;
    const auto graph = TwoSourceGraph();
    const auto parameters = slsf::LegacyDefaultParameters();
    const Configuration configuration{};
    REQUIRE(dispatcher.Attach(Epoch{24, 0, 0}, MakeBindings(graph, parameters, configuration),
                              MakeState()));

    REQUIRE(dispatcher.AdvanceTime(slsf::GameTime::FromDays(10.0)).has_value());
    const auto infinite =
        dispatcher.AdvanceTime(slsf::GameTime::FromDays(std::numeric_limits<double>::infinity()));
    REQUIRE_FALSE(infinite.has_value());
    CHECK(infinite.error().code == slsf::Error::Code::InvalidArgument);
    const auto nanAdvance =
        dispatcher.AdvanceTime(slsf::GameTime::FromDays(std::numeric_limits<double>::quiet_NaN()));
    REQUIRE_FALSE(nanAdvance.has_value());

    // Floor is still 10: a "rewind" relative to the accepted advance fails,
    // and a fresh forward advance still works.
    CHECK_FALSE(dispatcher.AdvanceTime(slsf::GameTime::FromDays(5.0)).has_value());
    const auto ahead = dispatcher.AdvanceTime(slsf::GameTime::FromDays(11.0));
    REQUIRE(ahead.has_value());
    CHECK(ahead->advancedTo.AsDays() == Approx(11.0));
}

TEST_CASE("dispatcher detach clears changes and dirty requests") {
    Dispatcher dispatcher;
    const auto graph = TwoSourceGraph();
    const auto parameters = slsf::LegacyDefaultParameters();
    const Configuration configuration{};
    REQUIRE(dispatcher.Attach(Epoch{25, 0, 0}, MakeBindings(graph, parameters, configuration),
                              MakeState()));

    ExplicitCommand setCommand;
    setCommand.kind = ExplicitCommandKind::Set;
    setCommand.target = RegionId{0};
    setCommand.category = kCat0;
    setCommand.value = 12.0;
    dispatcher.ScheduleChange(ScheduledChange{slsf::GameTime::FromDays(2.0), setCommand});
    dispatcher.RequestDirtyRefresh(RegionId{1});
    CHECK(dispatcher.PendingChanges() == 1);
    CHECK(dispatcher.PendingDirty() == 1);
    CHECK(dispatcher.Timers().PendingCount(ClockDomain::Active) == 1);

    dispatcher.Detach(); // no cross-session leftovers, timer dies
    CHECK(dispatcher.PendingChanges() == 0);
    CHECK(dispatcher.PendingDirty() == 0);
    CHECK(dispatcher.Timers().PendingCount() == 0);
    dispatcher.Step(); // detached rounds are quiet no-ops
    CHECK(dispatcher.PendingChanges() == 0);
}

TEST_CASE("dispatcher reattach publishes a clean session snapshot") {
    Dispatcher dispatcher;
    const auto graph = TwoSourceGraph();
    const auto parameters = slsf::LegacyDefaultParameters();
    const Configuration configuration{};

    REQUIRE(dispatcher.Attach(Epoch{26, 0, 0}, MakeBindings(graph, parameters, configuration),
                              MakeState()));
    ExplicitCommand setCommand;
    setCommand.kind = ExplicitCommandKind::Set;
    setCommand.target = RegionId{0};
    setCommand.category = kCat0;
    setCommand.value = 41.0;
    REQUIRE(dispatcher.Execute(setCommand).status == Status::Ready);
    REQUIRE(dispatcher.Snapshot()->fame.at(0) == Approx(41.0));

    dispatcher.Detach(); // old publication stays readable
    REQUIRE(dispatcher.Snapshot() != nullptr);

    REQUIRE(dispatcher.Attach(Epoch{27, 0, 0}, MakeBindings(graph, parameters, configuration),
                              MakeState()));
    const auto fresh = dispatcher.Snapshot();
    REQUIRE(fresh != nullptr);
    CHECK(fresh->epoch == (Epoch{27, 0, 0}));
    CHECK(fresh->fame.at(0) == Approx(0.0)); // no leakage from the old session
    const auto viewed =
        dispatcher.Query(QueryRequest{QueryRequest::Kind::Fame, Loc(0), kCat0});
    REQUIRE(viewed.has_value());
    CHECK(viewed->fame.front() == Approx(0.0));
}

TEST_CASE("dispatcher detach during a timer batch cannot double resume") {
    Dispatcher dispatcher;
    const auto graph = TwoSourceGraph();
    const auto parameters = slsf::LegacyDefaultParameters();
    const Configuration configuration{};
    REQUIRE(dispatcher.Attach(Epoch{28, 0, 0}, MakeBindings(graph, parameters, configuration),
                              MakeState()));
    double fakeWall = 0.0;
    dispatcher.Clocks().SetWallSource([&fakeWall] { return fakeWall; });
    dispatcher.Step(); // anchor clocks

    int resumedB = 0;
    int abortedA = 0;
    int abortedB = 0;
    auto flowA = dispatcher.SpawnFlow([&]() -> Dispatcher::Flow {
        Dispatcher::Delay wait{dispatcher, 1.0, ClockDomain::Real};
        if (co_await wait) {
            dispatcher.Detach(); // detach mid-batch
        } else {
            ++abortedA;
        }
        co_return;
    });
    auto flowB = dispatcher.SpawnFlow([&]() -> Dispatcher::Flow {
        Dispatcher::Delay wait{dispatcher, 1.0, ClockDomain::Real};
        if (co_await wait) {
            ++resumedB;
        } else {
            ++abortedB;
        }
        co_return;
    });
    REQUIRE(dispatcher.Timers().PendingCount(ClockDomain::Real) == 2);

    fakeWall += 1.0;
    CHECK_NOTHROW(dispatcher.Step()); // second body sees a dead entry
    CHECK_FALSE(dispatcher.Attached());
    CHECK(resumedB == 0);
    CHECK(abortedB == 1);
    CHECK(abortedA == 0);
    CHECK(flowA.Done());
    CHECK(flowB.Done());
}

TEST_CASE("dispatcher preserves FIFO changes at the same business time", "[fix05]") {
    Dispatcher dispatcher;
    const DiffusionGraph graph{};
    const auto parameters = slsf::LegacyDefaultParameters();
    const Configuration configuration{};
    REQUIRE(dispatcher.Attach(Epoch{30, 0, 0}, MakeBindings(graph, parameters, configuration),
                              MakeState()));
    ExplicitCommand command{ExplicitCommandKind::Set, RegionId{0}, kCat0, 10.0};
    dispatcher.ScheduleChange({slsf::GameTime::FromDays(1), command});
    command.value = 20;
    dispatcher.ScheduleChange({slsf::GameTime::FromDays(1), command});
    REQUIRE(dispatcher.AdvanceTime(slsf::GameTime::FromDays(1)));
    CHECK(dispatcher.Snapshot()->fame[0] == 20);
}

TEST_CASE("dispatcher settles the current clock before a synchronous write", "[fix05]") {
    Dispatcher dispatcher;
    const DiffusionGraph graph{};
    const auto parameters = slsf::LegacyDefaultParameters();
    const Configuration configuration{};
    REQUIRE(dispatcher.Attach(Epoch{31, 0, 0}, MakeBindings(graph, parameters, configuration),
                              MakeState()));
    ExplicitCommand command{ExplicitCommandKind::Set, RegionId{0}, kCat0, 20.0};
    SECTION("idle wake") {
        dispatcher.WakeTick(slsf::GameTime::FromDays(1));
    }
    SECTION("older queued change") {
        auto older = command;
        older.value = 10;
        dispatcher.ScheduleChange({slsf::GameTime::FromDays(1), older});
        dispatcher.Clocks().RecordGameTime(slsf::GameTime::FromDays(2));
    }
    const auto written = dispatcher.Execute(command);
    REQUIRE(written.status == Status::Ready);
    CHECK(dispatcher.Snapshot()->revision == written.revision);
    CHECK(dispatcher.PendingChanges() == 0);
    REQUIRE(dispatcher.AdvanceTime(dispatcher.Clocks().Game()));
    CHECK(dispatcher.Snapshot()->fame[0] == 20);
}

TEST_CASE("dispatcher drops extracted work after session replacement", "[fix05]") {
    Dispatcher dispatcher;
    const DiffusionGraph graph{};
    const auto parameters = slsf::LegacyDefaultParameters();
    const Configuration configuration{};
    const auto bindings = MakeBindings(graph, parameters, configuration);
    REQUIRE(dispatcher.Attach(Epoch{32, 0, 0}, bindings, MakeState()));
    double wall = 0;
    dispatcher.Clocks().SetWallSource([&] { return wall; });
    int staleRuns = 0;
    dispatcher.SetDirtySink([&](RegionId) { ++staleRuns; });
    dispatcher.Schedule(1, ClockDomain::Active, [&] {
        dispatcher.Detach();
        REQUIRE(dispatcher.Attach(Epoch{32, 0, 0}, bindings, MakeState()));
        dispatcher.RequestDirtyRefresh(RegionId{0});
    });
    dispatcher.Schedule(1, ClockDomain::Active, [&] { ++staleRuns; });
    dispatcher.RequestDirtyRefresh(RegionId{0});
    wall = 1;
    dispatcher.Step();
    CHECK(staleRuns == 0);
    CHECK(dispatcher.PendingDirty() == 1);
    CHECK(dispatcher.Timers().PendingCount() == 1);
    wall = 2;
    dispatcher.Step();
    CHECK(staleRuns == 1);
    CHECK(dispatcher.PendingDirty() == 0);
}

TEST_CASE("dispatcher releases callbacks and samples clocks outside its lock", "[fix05]") {
    Dispatcher dispatcher;
    const DiffusionGraph graph{};
    const auto parameters = slsf::LegacyDefaultParameters();
    const Configuration configuration{};
    REQUIRE(dispatcher.Attach(Epoch{34, 0, 0}, MakeBindings(graph, parameters, configuration), MakeState()));
    dispatcher.Clocks().SetWallSource([&] { return dispatcher.Attached() ? 0.0 : 1.0; });
    int released = 0;
    auto capture = std::shared_ptr<int>(new int(0), [&](int* value) {
        CHECK_FALSE(dispatcher.Attached());
        ++released;
        delete value;
    });
    dispatcher.Schedule(10, ClockDomain::Real, [capture] {});
    dispatcher.Post([capture] {});
    capture.reset();
    dispatcher.Detach();
    CHECK(released == 1);
}

TEST_CASE("published query supports dense and reordered region rows", "[fix05]") {
    auto view = slsf::MakePublishedState(MakeState());
    view.regionIds = {RegionId{7}, RegionId{2}};
    view.locationAccounts[0] = RegionId{2};
    view.fame[slsf::kFameSlotCount + static_cast<std::size_t>(FameCategory::Anal)] = 22;
    QueryRequest request{};
    request.location = LocationId{0};
    request.category = FameCategory::Anal;
    request.kind = QueryRequest::Kind::Fame;
    const auto result = slsf::Query(view, request);
    REQUIRE(result.has_value());
    CHECK(result->fame.front() == 22);
}

TEST_CASE("uncaught flow exceptions remain observable without stopping the executor", "[fix05-06]") {
    Dispatcher dispatcher;
    const DiffusionGraph graph{};
    const auto parameters = slsf::LegacyDefaultParameters();
    const Configuration configuration{};
    REQUIRE(dispatcher.Attach(Epoch{40, 0, 0}, MakeBindings(graph, parameters, configuration), MakeState()));
    auto eager = dispatcher.SpawnFlow([]() -> Dispatcher::Flow {
        throw std::runtime_error("eager failure");
        co_return;
    });
    REQUIRE(eager.Done());
    REQUIRE(eager.Failure());
    CHECK_THROWS_AS(std::rethrow_exception(eager.Failure()), std::runtime_error);

    int ran = 0;
    auto delayed = dispatcher.SpawnFlow([&]() -> Dispatcher::Flow {
        if (co_await Dispatcher::Delay{dispatcher, 0, ClockDomain::Active}) {
            throw std::runtime_error("delayed failure");
        }
    });
    dispatcher.Schedule(0, ClockDomain::Active, [&] { ++ran; });
    CHECK_NOTHROW(dispatcher.Step());
    REQUIRE(delayed.Done());
    REQUIRE(delayed.Failure());
    CHECK_THROWS_AS(std::rethrow_exception(delayed.Failure()), std::runtime_error);
    CHECK(ran == 1);
    CHECK(dispatcher.Timers().PendingCount() == 0);
    Dispatcher::Flow empty;
    CHECK_FALSE(empty.Failure());
}

TEST_CASE("flow exceptions during abort finish only the failing frame", "[fix05-06]") {
    Dispatcher dispatcher;
    const DiffusionGraph graph{};
    const auto parameters = slsf::LegacyDefaultParameters();
    const Configuration configuration{};
    REQUIRE(dispatcher.Attach(Epoch{41, 0, 0}, MakeBindings(graph, parameters, configuration), MakeState()));
    auto failing = dispatcher.SpawnFlow([&]() -> Dispatcher::Flow {
        if (!(co_await Dispatcher::Delay{dispatcher, 100, ClockDomain::Real})) {
            throw std::runtime_error("abort failure");
        }
    });
    SECTION("explicit cancellation") { CHECK(failing.CancelWait()); }
    SECTION("session detach") { CHECK_NOTHROW(dispatcher.Detach()); }
    REQUIRE(failing.Done());
    REQUIRE(failing.Failure());
    CHECK_THROWS_AS(std::rethrow_exception(failing.Failure()), std::runtime_error);
    CHECK(dispatcher.Timers().PendingCount() == 0);
}

TEST_CASE("mixed wait retirement preserves expiry and detach order", "[fix05-06]") {
    Dispatcher dispatcher;
    const DiffusionGraph graph{};
    const auto parameters = slsf::LegacyDefaultParameters();
    const Configuration configuration{};
    REQUIRE(dispatcher.Attach(Epoch{42, 0, 0}, MakeBindings(graph, parameters, configuration), MakeState()));
    double wall = 0;
    dispatcher.Clocks().SetWallSource([&] { return wall; });
    std::vector<int> cancelled, elapsed, detached, expectedCancelled, expectedElapsed;
    std::vector<Dispatcher::Flow> flows;
    for (int id = 0; id < 512; ++id) {
        flows.push_back(dispatcher.SpawnFlow([&, id]() -> Dispatcher::Flow {
            if (!(co_await Dispatcher::Delay{dispatcher, 1, ClockDomain::Real})) {
                cancelled.push_back(id);
                co_return;
            }
            elapsed.push_back(id);
            if (!(co_await Dispatcher::Delay{dispatcher, 100, ClockDomain::Active})) {
                detached.push_back(id);
            }
        }));
        if (id % 3 == 0) { expectedCancelled.push_back(id); }
        else { expectedElapsed.push_back(id); }
    }
    for (const auto id : expectedCancelled) { REQUIRE(flows[id].CancelWait()); }
    wall = 1;
    dispatcher.Step();
    CHECK(cancelled == expectedCancelled);
    CHECK(elapsed == expectedElapsed);
    dispatcher.Detach();
    CHECK(detached == expectedElapsed);
    CHECK(dispatcher.Timers().PendingCount() == 0);
    for (auto& flow : flows) {
        CHECK(flow.Done());
        CHECK_FALSE(flow.Failure());
        CHECK_FALSE(flow.CancelWait());
    }
}
