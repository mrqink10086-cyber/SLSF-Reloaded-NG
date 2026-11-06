#pragma once

#include "slsf/core/Commands.h"
#include "slsf/core/Snapshots.h"
#include "slsf/core/Types.h"
#include "slsf/core/reputation/ReputationState.h"
#include "slsf/core/reputation/Settlement.h"
#include "slsf/core/time/Scheduler.h"
#include "slsf/runtime/ClockService.h"
#include "slsf/runtime/SessionGate.h"

#include <atomic>
#include <coroutine>
#include <cstdint>
#include <deque>
#include <exception>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace slsf::runtime {

using time::ClockDomain;
using time::TimerToken;

struct RuntimeBindings {
    const DiffusionGraph* graph{};
    const Parameters* parameters{};
    const Configuration* configuration{};
    const RegionSnapshot* regions{};
    IRandom* rng{};
    std::shared_ptr<const ConfigurationData> settings;
};

class Dispatcher {
private:
    // Defined in the private section below; the early private declaration
    // keeps elaborated look-ups inside promise_type from creating a phantom
    // namespace-scope type, and the private placement keeps C4240 quiet.
    struct WaitEntry;
    using WaitList = std::list<std::shared_ptr<WaitEntry>>;

public:
    using DirtySink = std::function<void(RegionId)>;
    using EventBody = std::function<void()>;

    static constexpr double kDirtyWindowSeconds = 1.0;

    struct Stats {
        std::uint64_t steps{};
        std::uint64_t published{};
        std::uint64_t eventsPosted{};
        std::uint64_t eventsConsumed{};
        std::uint64_t eventsDroppedStale{};
        std::uint64_t changesScheduled{};
        std::uint64_t dirtyRequests{};
        std::uint64_t dirtyMerged{};
        std::uint64_t dirtyFired{};
        std::uint64_t advanceErrors{};
        std::uint64_t timerErrors{};
    };

    struct Flow {
        struct promise_type {
            Dispatcher* owner{nullptr};
            bool started{false};
            std::shared_ptr<WaitEntry> entry;
            bool stopping{false};
            std::function<Flow()>* keepBody{nullptr};
            std::exception_ptr failure;

            ~promise_type() { delete keepBody; }

            Flow get_return_object() noexcept;
            std::suspend_always initial_suspend() noexcept { return {}; }
            std::suspend_always final_suspend() noexcept { return {}; }
            void return_void() noexcept {}
            void unhandled_exception() noexcept { failure = std::current_exception(); }
        };

        Flow() noexcept = default;
        explicit Flow(std::coroutine_handle<promise_type> handle) noexcept;
        ~Flow();
        Flow(Flow&& other) noexcept;
        Flow& operator=(Flow&& other) noexcept;
        Flow(const Flow&) = delete;
        Flow& operator=(const Flow&) = delete;

        void Start();
        bool Done() const noexcept;
        std::exception_ptr Failure() const noexcept;
        bool CancelWait();
        std::coroutine_handle<promise_type> Handle() const noexcept { return handle_; }

    private:
        friend class Dispatcher;
        std::coroutine_handle<promise_type> handle_{};
    };

    struct Delay {
        Delay(Dispatcher& dispatcher, double seconds, ClockDomain domain)
            : dispatcher_(&dispatcher), seconds_(seconds), domain_(domain) {}
        bool await_ready() const noexcept { return false; }
        bool await_suspend(std::coroutine_handle<Flow::promise_type> frame);
        bool await_resume() const noexcept { return !aborted_; }

    private:
        friend class Dispatcher;
        [[maybe_unused]] Dispatcher* dispatcher_;
        double seconds_;
        ClockDomain domain_;
        bool aborted_{false};
    };

    Flow SpawnFlow(std::function<Flow()> body);

    explicit Dispatcher(std::shared_ptr<SessionGate> hostGate = {})
        : hostGate_(std::move(hostGate)) {}
    ~Dispatcher();
    Dispatcher(const Dispatcher&) = delete;
    Dispatcher& operator=(const Dispatcher&) = delete;

    bool Attach(Epoch epoch, const RuntimeBindings& bindings, State state);
    Result<void> Reconfigure(const RuntimeBindings& bindings,
                             const ConfigurationSnapshot& configuration);
    void Detach();
    bool Attached() const;
    Epoch CurrentEpoch() const;

    void Suspend();
    void Resume();
    bool Suspended() const;

    CommandResult Execute(const ExplicitCommand& command);
    CommandResult EditState(const std::function<Result<void>(State&)>& edit);
    CommandResult DecayBatch(int multiplier);

    struct GainOutcome {
        Status status{Status::NotReady};
        Revision revision{};
        int gained{};
        std::string message;
    };
    struct ApplyOutcome {
        Status status{Status::NotReady};
        Revision revision{};
        double value{};
        std::string message;
    };
    GainOutcome GainFame(GameTime now, RegionId account, FameCategory category,
                        const GainFameInputs& ruleInputs);
    ApplyOutcome ApplyManualSpread(RegionId source, RegionId target,
                                   FameCategory category, int percent);
    ApplyOutcome ApplyManualDecay(RegionId target, FameCategory category,
                                  int minimum, int maximum);
    ApplyOutcome ApplyManualGain(RegionId target, FameCategory category,
                                 int minimum, int maximum);
    CommandResult ApplySpreadPulse(GameTime now, std::span<const RegionId> sources);

    PublishedStatePtr Snapshot() const noexcept
    {
        return published_.load(std::memory_order_acquire);
    }
    Result<PersistentState> Capture() const;
    std::shared_ptr<const ConfigurationData> Settings() const;
    Result<PublicView> Query(const QueryRequest& request) const;

    void ScheduleChange(ScheduledChange change);
    std::size_t PendingChanges() const;
    Result<AdvanceResult> AdvanceTime(GameTime now);

    void Post(EventBody body);
    void Post(Epoch epoch, EventBody body);
    std::size_t PendingEvents() const;

    void SetDirtySink(DirtySink sink);
    void RequestDirtyRefresh(RegionId target);
    std::size_t PendingDirty() const;

    void Step();
    Stats stats() const;

    void WakeTick(GameTime game);

    time::Scheduler& Timers() noexcept { return scheduler_; }
    ClockService& Clocks() noexcept { return clocks_; }

    TimerToken Schedule(double seconds, ClockDomain domain, time::TaskBody body);

private:
    struct EngineEvent {
        Epoch epoch{};
        EventBody body;
    };

    struct WaitEntry {
        std::coroutine_handle<Flow::promise_type> continuation{};
        TimerToken token{};
        Delay* awaiter{nullptr};
        bool live{false};
        WaitList::iterator position;
    };

    struct DirtyRequest {
        TimerToken token{};
        double deadline{0.0};
    };
    bool RegisterWait(Delay& awaiter, std::coroutine_handle<Flow::promise_type> frame,
                      double seconds, ClockDomain domain);
    std::optional<std::coroutine_handle<Flow::promise_type>>
    RetireWait(const std::shared_ptr<WaitEntry>& entry, bool aborted, bool stopping);
    void FireWait(const std::shared_ptr<WaitEntry>& entry);
    void FireDirty(RegionId target, std::uint64_t generation);

    void PublishLocked();
    Result<AdvanceResult> AdvanceLocked(GameTime now);
    double DomainNowLocked(ClockDomain domain);
    double DeadlineLocked(ClockDomain domain, double seconds);
    bool HasDueChangeLocked(GameTime now) const;
    bool Blocked() const { return suspended_ || (hostGate_ && hostGate_->Closed()); }

    mutable std::mutex mutex_;
    Epoch epoch_{};
    std::uint64_t lifecycleGeneration_{0};
    std::optional<State> state_;
    RuntimeBindings bindings_{};
    std::shared_ptr<const PreparedSettlement> prepared_{};
    std::shared_ptr<SettlementWorkspace> workspace_{};
    bool suspended_{false};
    std::shared_ptr<SessionGate> hostGate_;
    std::optional<GameTime> lastAdvance_{};

    std::atomic<PublishedStatePtr> published_{};

    std::deque<EngineEvent> events_;
    std::vector<ScheduledChange> changes_;
    std::map<RegionId, DirtyRequest> dirty_;
    DirtySink dirtySink_{};

    time::Scheduler scheduler_{};
    ClockService clocks_{};
    WaitList waits_{};

    Stats stats_{};
};

}
