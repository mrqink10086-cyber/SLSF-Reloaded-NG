#include "slsf/core/Commands.h"
#include "slsf/core/reputation/Settlement.h"
#include "slsf/core/time/Scheduler.h"
#include "slsf/runtime/Dispatcher.h"

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cstdint>
#include <deque>
#include <functional>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <numeric>
#include <random>
#include <string_view>
#include <vector>

namespace {

using slsf::Configuration;
using slsf::DiffusionGraph;
using slsf::Epoch;
using slsf::ExplicitCommand;
using slsf::ExplicitCommandKind;
using slsf::FameCategory;
using slsf::Parameters;
using slsf::RegionId;
using slsf::StableRegionKey;
using slsf::State;
using slsf::runtime::Dispatcher;
using slsf::runtime::RuntimeBindings;
using slsf::time::ClockDomain;
using slsf::time::Scheduler;

constexpr FameCategory kCategory = FameCategory::Whore;
using SteadyClock = std::chrono::steady_clock;

StableRegionKey Key(std::uint32_t index) {
    return StableRegionKey{"algorithm-probe.esp", index + 1};
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

RuntimeBindings Bindings(const DiffusionGraph& graph, const Parameters& parameters,
                         const Configuration& configuration, DeterministicQueue& rng) {
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
    const auto value = dispatcher.Query(slsf::QueryRequest{
        slsf::QueryRequest::Kind::Fame, slsf::LocationId{0}, kCategory});
    return value && !value->fame.empty() ? value->fame.front() : -999.0;
}

void ProbeEqualTimeChanges() {
    Dispatcher dispatcher;
    const DiffusionGraph graph{};
    const Parameters parameters = slsf::LegacyDefaultParameters();
    const Configuration configuration{};
    DeterministicQueue rng;
    dispatcher.Attach(Epoch{101, 0, 0}, Bindings(graph, parameters, configuration, rng),
                      MakeState());
    const auto at = slsf::GameTime::FromDays(1.0);
    dispatcher.ScheduleChange({at, SetCommand(10.0)});
    dispatcher.ScheduleChange({at, SetCommand(20.0)});
    const auto result = dispatcher.AdvanceTime(at);
    std::cout << "PROBE equal_time_changes status=" << (result ? "ok" : "error")
              << " set10_then_set20_final=" << FameAt(dispatcher)
              << " fifo_expected=20\n";
}

void ProbeExecuteClockOrdering(bool settleFirst) {
    Dispatcher dispatcher;
    const DiffusionGraph graph{};
    const Parameters parameters = slsf::LegacyDefaultParameters();
    const Configuration configuration{};
    DeterministicQueue rng;
    dispatcher.Attach(Epoch{settleFirst ? 103U : 102U, 0, 0},
                      Bindings(graph, parameters, configuration, rng), MakeState());
    const auto now = slsf::GameTime::FromDays(1.0);
    dispatcher.WakeTick(now); // public wake path records game time and runs Step
    const auto settled = settleFirst ? dispatcher.AdvanceTime(now)
                                     : slsf::Result<slsf::AdvanceResult>{slsf::AdvanceResult{}};
    const auto command = dispatcher.Execute(SetCommand(20.0));
    const auto later = dispatcher.AdvanceTime(now);
    std::cout << "PROBE execute_clock_order path=" << (settleFirst ? "settle-before-execute" : "execute-before-settle")
              << " pre_settle=" << (settleFirst ? (settled ? "ok" : "error") : "skipped")
              << " execute_status=" << static_cast<int>(command.status)
              << " followup_advance=" << (later ? "ok" : "error")
              << " final=" << FameAt(dispatcher) << "\n";
}

void ProbeDetachDuringTimerBatch() {
    Dispatcher dispatcher;
    const DiffusionGraph graph{};
    const Parameters parameters = slsf::LegacyDefaultParameters();
    const Configuration configuration{};
    DeterministicQueue rng;
    dispatcher.Attach(Epoch{104, 0, 0}, Bindings(graph, parameters, configuration, rng),
                      MakeState());
    int first = 0;
    int second = 0;
    dispatcher.Schedule(0.0, ClockDomain::Game, [&] {
        ++first;
        dispatcher.Detach();
    });
    dispatcher.Schedule(0.0, ClockDomain::Game, [&] { ++second; });
    dispatcher.Step();
    std::cout << "PROBE detach_during_due_batch attached=" << dispatcher.Attached()
              << " first=" << first << " second_old_timer=" << second << "\n";
}

void ProbeCancelledFarFutureCompaction(std::size_t count) {
    Scheduler scheduler;
    for (std::size_t i = 0; i < count; ++i) {
        const auto token = scheduler.Schedule(ClockDomain::Game, 1000.0,
                                              [] {});
        scheduler.Cancel(token);
    }
    const auto heapAfterCancel = scheduler.HeapSize(ClockDomain::Game);
    const auto discardedAfterCancel = scheduler.DiscardedCount(ClockDomain::Game);
    scheduler.NewSession();
    const auto heapAfterNewSession = scheduler.HeapSize(ClockDomain::Game);
    const auto discardedAfterNewSession = scheduler.DiscardedCount(ClockDomain::Game);
    scheduler.AcquireDue(ClockDomain::Game, 999.0);
    const auto heapBeforeDeadline = scheduler.HeapSize(ClockDomain::Game);
    scheduler.AcquireDue(ClockDomain::Game, 1000.0);
    std::cout << "PROBE cancelled_far_future count=" << count
              << " live=" << scheduler.PendingCount(ClockDomain::Game)
              << " heap_after_cancel=" << heapAfterCancel
              << " discarded_after_cancel=" << discardedAfterCancel
              << " heap_after_new_session=" << heapAfterNewSession
              << " discarded_after_new_session=" << discardedAfterNewSession
              << " heap_at_999=" << heapBeforeDeadline
              << " heap_at_1000=" << scheduler.HeapSize(ClockDomain::Game)
              << " discarded_at_1000=" << scheduler.DiscardedCount(ClockDomain::Game)
              << "\n";
}

struct BenchEvent {
    std::uint64_t epoch{};
    std::uint64_t id{};
    std::function<void()> body;
};

using EventVector = std::vector<BenchEvent>;
using EventDeque = std::deque<BenchEvent>;

template <class Container>
Container MakeEvents(const std::vector<std::uint64_t>& ids, std::uint64_t& checksum) {
    Container events;
    for (const std::uint64_t id : ids) {
        events.push_back(BenchEvent{1, id, [&checksum, id] {
            checksum = std::rotl(checksum, 7) ^ id;
        }});
    }
    return events;
}

void DrainEraseFront(EventVector& events) {
    while (!events.empty()) {
        BenchEvent event = std::move(events.front());
        events.erase(events.begin());
        if (event.epoch == 1 && event.body) {
            event.body();
        }
    }
}

void DrainVectorHead(EventVector& events) {
    std::size_t head = 0;
    while (head < events.size()) {
        BenchEvent event = std::move(events[head++]);
        if (event.epoch == 1 && event.body) {
            event.body();
        }
    }
    events.clear();
}

void DrainDeque(EventDeque& events) {
    while (!events.empty()) {
        BenchEvent event = std::move(events.front());
        events.pop_front();
        if (event.epoch == 1 && event.body) {
            event.body();
        }
    }
}

template <class Container, class Drain>
std::pair<std::int64_t, std::uint64_t> TimeEventDrain(
    const std::vector<std::uint64_t>& ids, Drain drain) {
    std::uint64_t checksum = 0;
    Container events = MakeEvents<Container>(ids, checksum);
    const auto start = SteadyClock::now();
    drain(events);
    const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
        SteadyClock::now() - start).count();
    return {elapsed, checksum};
}

struct HeapKey {
    double deadline{};
    std::uint64_t sequence{};
};

bool Later(const HeapKey& lhs, const HeapKey& rhs) {
    if (lhs.deadline != rhs.deadline) {
        return lhs.deadline > rhs.deadline;
    }
    return lhs.sequence > rhs.sequence;
}

[[gnu::noinline]] bool RootHasDue(const std::vector<HeapKey>& heap,
                                  std::mutex& mutex,
                                  const volatile double* now) {
    const std::lock_guard<std::mutex> guard(mutex);
    const double current = *now;
    return !heap.empty() && heap.front().deadline <= current;
}

template <class F>
std::int64_t MedianNanos(F&& sample, std::size_t repetitions) {
    std::vector<std::int64_t> times;
    times.reserve(7);
    for (int repeat = 0; repeat < 7; ++repeat) {
        const auto start = SteadyClock::now();
        sample(repetitions);
        times.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(
            SteadyClock::now() - start).count());
    }
    std::sort(times.begin(), times.end());
    return times[times.size() / 2];
}

void BenchmarkEventDrains() {
    std::mt19937_64 random(1170);
    for (const std::size_t count : {100U, 1000U, 5000U}) {
        std::vector<std::uint64_t> ids(count);
        for (auto& id : ids) {
            id = random();
        }
        std::uint64_t eraseChecksum = 0;
        std::uint64_t headChecksum = 0;
        std::uint64_t dequeChecksum = 0;
        std::vector<std::int64_t> eraseTimes;
        std::vector<std::int64_t> headTimes;
        std::vector<std::int64_t> dequeTimes;
        for (int repeat = 0; repeat < 7; ++repeat) {
            auto [eraseNs, ec] = TimeEventDrain<EventVector>(ids, DrainEraseFront);
            auto [headNs, hc] = TimeEventDrain<EventVector>(ids, DrainVectorHead);
            auto [dequeNs, dc] = TimeEventDrain<EventDeque>(ids, DrainDeque);
            eraseTimes.push_back(eraseNs);
            headTimes.push_back(headNs);
            dequeTimes.push_back(dequeNs);
            if (repeat == 6) {
                eraseChecksum = ec;
                headChecksum = hc;
                dequeChecksum = dc;
            }
        }
        const auto median = [](std::vector<std::int64_t>& values) {
            std::sort(values.begin(), values.end());
            return values[values.size() / 2];
        };
        std::cout << "BENCH event_drain n=" << count
                  << " erase_begin_median_ns=" << median(eraseTimes)
                  << " vector_head_median_ns=" << median(headTimes)
                  << " deque_median_ns=" << median(dequeTimes)
                  << " checksum_erase=" << eraseChecksum
                  << " checksum_head=" << headChecksum
                  << " checksum_deque=" << dequeChecksum << "\n";
    }
}

void BenchmarkDueCount() {
    constexpr std::size_t calls = 500;
    for (const std::size_t count : {128U, 1024U, 8192U}) {
        Scheduler scheduler;
        std::vector<HeapKey> keys;
        keys.reserve(count);
        for (std::size_t i = 0; i < count; ++i) {
            const double deadline = 1000.0 + static_cast<double>(i);
            scheduler.Schedule(ClockDomain::Game, deadline, [] {});
            keys.push_back(HeapKey{deadline, static_cast<std::uint64_t>(i)});
        }
        std::make_heap(keys.begin(), keys.end(), Later);
        const auto actual = scheduler.DueCount(ClockDomain::Game, 0.0);
        std::mutex rootMutex;
        volatile double now = 0.0;
        const bool candidate = RootHasDue(keys, rootMutex, &now);
        if (actual != 0 || candidate) {
            std::cerr << "BENCH due_count equivalence failed n=" << count << "\n";
            std::exit(2);
        }
        const auto scanNs = MedianNanos([&](std::size_t repetitions) {
            std::size_t checksum = 0;
            for (std::size_t i = 0; i < repetitions; ++i) {
                checksum += scheduler.DueCount(ClockDomain::Game, 0.0);
            }
            if (checksum != 0) {
                std::exit(3);
            }
        }, calls);
        const auto rootNs = MedianNanos([&](std::size_t repetitions) {
            std::size_t checksum = 0;
            for (std::size_t i = 0; i < repetitions; ++i) {
                checksum += RootHasDue(keys, rootMutex, &now) ? 1U : 0U;
            }
            if (checksum != 0) {
                std::exit(4);
            }
        }, calls);
        std::cout << "BENCH due_count n=" << count << " calls=" << calls
                  << " actual_scan_median_total_ns=" << scanNs
                  << " root_key_model_median_total_ns=" << rootNs
                  << " actual_scan_median_ns_per_call="
                  << static_cast<double>(scanNs) / static_cast<double>(calls)
                  << " root_key_model_median_ns_per_call="
                  << static_cast<double>(rootNs) / static_cast<double>(calls)
                  << " checksum=0\n";
    }
}

} // namespace

int main() {
    std::cout << std::setprecision(17);
    ProbeEqualTimeChanges();
    ProbeExecuteClockOrdering(false);
    ProbeExecuteClockOrdering(true);
    ProbeDetachDuringTimerBatch();
    ProbeCancelledFarFutureCompaction(1000);
    BenchmarkEventDrains();
    BenchmarkDueCount();
    return 0;
}
