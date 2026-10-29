#pragma once

// Thread model: one executor thread drives Step()/WakeTick()/AdvanceTime()
// (SKSE task callbacks in the target runtime).  Readers take the published
// snapshot atomically; writers hold one short mutex.  A Flow object must be
// started, awaited on, and destroyed without racing another thread that
// runs that flow's executor callbacks.  Flows may outlive the dispatcher:
// destroying the dispatcher detaches every parked frame (aborted, owner
// cleared) and the later Flow destruction settles and frees that frame
// without touching the dispatcher.
//
// Flow contract: a flow must `co_return` promptly when Delay() reports
// false; keep waiting loops conditioned on the reported result.  A flow
// that ignores an abort spins synchronously inside Delay (detectable hang)
// - dispatcher waits are the only parking points; flows never suspend on
// foreign awaiters.

#include "slsf/core/Commands.h"
#include "slsf/core/Snapshots.h"
#include "slsf/core/Types.h"
#include "slsf/core/reputation/ReputationState.h"
#include "slsf/core/reputation/Settlement.h"
#include "slsf/core/time/Scheduler.h"
#include "slsf/runtime/ClockService.h"

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
#include <string>
#include <utility>
#include <vector>

namespace slsf::runtime {

using time::ClockDomain;
using time::TimerToken;

// Inputs relayed into Advance(); they must outlive the dispatcher's Attach.
struct RuntimeBindings {
    const DiffusionGraph* graph{};
    const Parameters* parameters{};
    const Configuration* configuration{};
    const RegionSnapshot* regions{};
    IRandom* rng{};
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

    // Equipment dirty requests merge into one timer per target over ~1 s of
    // active (menu-excluded) time.
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

    // Timed flow: the body runs eagerly, parks at its first Delay, and resumes
    // from the executor round. Move-own the returned Flow; destroying it
    // cancels a pending wait and destroys the frame exactly once.
    //   auto script = [](Dispatcher& d, Args... captured) -> Dispatcher::Flow {
    //       if (!co_await d.Delay(3.0, ClockDomain::Real)) co_return;
    //       ... business work ...
    //   };
    //   auto flow = d.SpawnFlow(std::bind_front(script, std::ref(d), args...));
    struct Flow {
        struct promise_type {
            Dispatcher* owner{nullptr};
            bool started{false};
            // Active wait.  Shared ownership: the timer callback locks a
            // weak_ptr, so a cancel/detach that drops the table entry can
            // never leave the callback with a dangling pointer.
            std::shared_ptr<WaitEntry> entry;
            // Set when the owning Flow dies: every subsequent Delay awaits
            // reports false and cannot park the frame again.
            bool stopping{false};
            // Heap-kept copy of the body lambda handed to SpawnFlow.  The
            // compiler copies only the lambda's `this` pointer into the
            // coroutine frame; that pointer must outlive the call site, so
            // the capture container lives here until the frame is destroyed.
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

        // Starts the parked body; a no-op once started or completed.
        void Start();
        bool Done() const noexcept;
        // An uncaught body exception completes only this flow; the owner can
        // inspect or rethrow it without interrupting other executor work.
        std::exception_ptr Failure() const noexcept;
        // Cancels the pending wait without destroying the flow; Delay
        // reports false when the frame resumes.  No-op without a wait.
        bool CancelWait();
        std::coroutine_handle<promise_type> Handle() const noexcept { return handle_; }

    private:
        friend class Dispatcher;
        std::coroutine_handle<promise_type> handle_{};
    };

    // co_await d.Delay(seconds, domain) -> bool (true = elapsed normally).
    struct Delay {
        Delay(Dispatcher& dispatcher, double seconds, ClockDomain domain)
            : dispatcher_(&dispatcher), seconds_(seconds), domain_(domain) {}
        bool await_ready() const noexcept { return false; }
        // Registers a one-shot timer on the domain clock and parks the
        // frame.  Returns true (stays parked) or false because the flow is
        // stopping, so await_resume reports false immediately.
        bool await_suspend(std::coroutine_handle<Flow::promise_type> frame);
        bool await_resume() const noexcept { return !aborted_; }

    private:
        friend class Dispatcher;
        Dispatcher* dispatcher_;
        double seconds_;
        ClockDomain domain_;
        bool aborted_{false}; // set under the dispatcher lock before abort resume
    };

    // Wraps a coroutine body and runs it to its first wait (or past it if
    // it waits immediately).  Invalid bodies yield an empty Flow.
    Flow SpawnFlow(std::function<Flow()> body);

    Dispatcher() = default;
    ~Dispatcher();
    Dispatcher(const Dispatcher&) = delete;
    Dispatcher& operator=(const Dispatcher&) = delete;

    // Adopts the state with the engine epoch, binds settlement inputs,
    // prepares the settlement snapshot, and publishes the new
    // session immediately so readers never observe the previous session's
    // publication under the new epoch.  Returns false when a session is
    // already attached.
    bool Attach(Epoch epoch, const RuntimeBindings& bindings, State state);
    // Session reset: aborts every pending wait (each released exactly once),
    // drops queued work, rolls the scheduler session generation, and keeps
    // the last publication readable until the next Attach publishes.
    void Detach();
    bool Attached() const;
    Epoch CurrentEpoch() const;

    // Session pause: executor rounds still accumulate wall clocks but skip
    // settlements, timers, event consumption and commands.
    void Suspend();
    void Resume();
    bool Suspended() const;

    // Settles the game-time state before applying the command. A settlement
    // failure leaves the command unapplied; committed changes are published.
    CommandResult Execute(const ExplicitCommand& command);

    // Latest publication; lock-free snapshot read, never null after the
    // first successful write.
    PublishedStatePtr Snapshot() const noexcept
    {
        return published_.load(std::memory_order_acquire);
    }
    // Copies the mutable state under the transaction lock.  The caller may
    // encode the returned value after this call, outside the lock.
    Result<PersistentState> Capture() const;
    Result<PublicView> Query(const QueryRequest& request) const;

    // One-shot queue: due entries settle once on the next settled Advance.
    // Detached sessions reject the entry (it would otherwise leak into the
    // next session) and non-finite timestamps are refused.
    void ScheduleChange(ScheduledChange change);
    std::size_t PendingChanges() const;
    // Settles the due change queue and propagates state to `now`; publishes
    // on success.  Rejects detachment, suspension, non-finite values, and
    // rewinding clocks (unexpected); rewinds require Detach + Attach (new
    // session).  The monotonic floor only advances when the settle succeeds.
    Result<AdvanceResult> AdvanceTime(GameTime now);

    // Business semantics arrive as closures; the executor validates the
    // stamped epoch against the current one on consumption.  Stale events
    // drop and count; valid ones consume one by one in post order, each
    // exactly once.
    void Post(EventBody body); // stamps the current epoch at post time
    void Post(Epoch epoch, EventBody body);
    std::size_t PendingEvents() const;

    void SetDirtySink(DirtySink sink);
    // The first request per target arms one Active-domain timer
    // (~kDirtyWindowSeconds); further requests inside the window merge into
    // it.  On expiry the sink fires once per target.  Requests while no
    // session is attached are dropped: an armed timer belongs to the
    // session that requested it.
    void RequestDirtyRefresh(RegionId target);
    std::size_t PendingDirty() const;

    // One round: tick the clock accumulators, settle due scheduled changes,
    // run due timers in game -> real -> active order (stable
    // (deadline, enqueue) order per domain), then consume queued events one
    // by one.  While suspended only the accumulators move.
    void Step();
    Stats stats() const;

    void WakeTick(GameTime game);

    time::Scheduler& Timers() noexcept { return scheduler_; }
    ClockService& Clocks() noexcept { return clocks_; }

    // Convenience timer in seconds on a domain; see Scheduler for token
    // semantics (cancel via Timers()).
    TimerToken Schedule(double seconds, ClockDomain domain, time::TaskBody body);

private:
    struct EngineEvent {
        Epoch epoch{};
        EventBody body;
    };

    struct WaitEntry {
        std::coroutine_handle<Flow::promise_type> continuation{};
        TimerToken token{};
        Delay* awaiter{nullptr}; // abort flag target; null after detach
        bool live{false};
        WaitList::iterator position;
    };

    struct DirtyRequest {
        TimerToken token{};
        double deadline{0.0};
    };
    // Wait machinery: resume calls run outside the lock.
    // Registers the wait and parks the frame; returns false (with the
    // awaiter flagged aborted) when the flow is already stopping or when
    // the delay is not a finite number of seconds.
    bool RegisterWait(Delay& awaiter, std::coroutine_handle<Flow::promise_type> frame,
                      double seconds, ClockDomain domain);
    // Returns the entry's continuation when this call won the single flip;
    // callers resume that frame outside the lock.  Marks the awaiter when
    // `aborted`, marks the promise when `stopping`, cancels the scheduler
    // token (so a cancelled wait's timer can never fire), and drops the
    // table reference.  The entry object itself lives on through the
    // caller's shared_ptr.
    std::optional<std::coroutine_handle<Flow::promise_type>>
    RetireWait(const std::shared_ptr<WaitEntry>& entry, bool aborted, bool stopping);
    void FireWait(const std::shared_ptr<WaitEntry>& entry);
    void FireDirty(RegionId target, std::uint64_t generation);

    // Helpers below require mutex_ held.
    void PublishLocked();
    Result<AdvanceResult> AdvanceLocked(GameTime now);
    double DomainNowLocked(ClockDomain domain);
    // Converts a delay in seconds to the domain's heap units (days for the
    // game domain) and adds the domain's current time.
    double DeadlineLocked(ClockDomain domain, double seconds);
    bool HasDueChangeLocked(GameTime now) const;

    mutable std::mutex mutex_;
    Epoch epoch_{};
    std::uint64_t lifecycleGeneration_{0};
    std::optional<State> state_;
    RuntimeBindings bindings_{};
    std::shared_ptr<const PreparedSettlement> prepared_{};
    std::shared_ptr<SettlementWorkspace> workspace_{};
    bool suspended_{false};
    std::optional<GameTime> lastAdvance_{};

    std::atomic<PublishedStatePtr> published_{};

    std::deque<EngineEvent> events_;
    std::vector<ScheduledChange> changes_; // sorted by .at for due-settle
    std::map<RegionId, DirtyRequest> dirty_;
    DirtySink dirtySink_{};

    time::Scheduler scheduler_{};
    ClockService clocks_{};
    WaitList waits_{};

    Stats stats_{};
};

} // namespace slsf::runtime
