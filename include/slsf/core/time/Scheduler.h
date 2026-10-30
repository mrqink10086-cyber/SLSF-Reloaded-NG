#pragma once

#include "slsf/core/Types.h"

#include <array>
#include <cstdint>
#include <functional>
#include <mutex>
#include <utility>
#include <vector>

namespace slsf::runtime { class Dispatcher; }

namespace slsf::time {

// Independent clock domains.  Game follows the engine game-day clock; Real
// accumulates wall time (including menus); Active accumulates wall time
// only while the session is not menu-blocked (menus/main menu/loading).
enum class ClockDomain : std::uint8_t {
    Game = 0,
    Real = 1,
    Active = 2,
};

inline constexpr std::size_t kClockDomainCount = 3;

// A task token: owning domain, slot identity, and the cancellation
// generation.  Tokens from recycled slots never match a live task, and a
// token whose session was replaced is dead even if the slot still exists.
struct TimerToken {
    ClockDomain domain{ClockDomain::Game};
    TaskId id{}; // 0 is invalid; id-1 indexes the slot table
    std::uint64_t generation{};

    friend constexpr bool operator==(const TimerToken& lhs, const TimerToken& rhs) = default;
};

using TaskBody = std::function<void()>;

// A due task in stable order: (deadline, enqueue sequence).  The body has
// already been moved out of the slot table; running it must happen outside
// the scheduler lock.  The slot is retired on extraction, so a body runs
// at most once even if the caller keeps the Due record around.
struct DueTask {
    TimerToken token{};
    TaskBody body{};
    double deadline{0.0};
    std::uint64_t sequence{0};
    std::uint64_t session{0};
};

class Scheduler {
public:
    Scheduler() = default;

    // Enqueue `body` on `domain` to fire at `deadline` (domain-native
    // units).  O(log n); never runs the body inline.  An invalid domain or
    // a non-finite deadline is rejected with an invalid (id 0) token.
    TimerToken Schedule(ClockDomain domain, double deadline, TaskBody body);

    // Cancels a pending task: bumps the slot generation and releases the
    // stored body exactly once.  Returns true if the token matched a live,
    // session-current task. Captures are destroyed outside the scheduler lock.
    bool Cancel(TimerToken token);

    // Extracts every live entry on `domain` with deadline <= `now`, ordered
    // by (deadline, sequence); the stable enqueue order breaks ties.  Dead
    // entries (cancelled / superseded session) are skipped and recycled.
    // An invalid domain or a non-finite `now` yields an empty batch.
    std::vector<DueTask> AcquireDue(ClockDomain domain, double now);

    // Number of live tasks on `domain` that are currently due.  O(n).  An
    // invalid domain or a non-finite `now` yields 0.
    std::size_t DueCount(ClockDomain domain, double now) const;

    bool HasDue(ClockDomain domain, double now);

    std::size_t PendingCount(ClockDomain domain) const;
    std::size_t PendingCount() const;

    // Session reset: invalidates every token scheduled before this call.  Slot
    // bodies are released outside the lock; all heap entries are removed.
    // Returns the new session generation.
    std::uint64_t NewSession();

    std::uint64_t Session() const;

    // Test/ops visibility: live entries still queued in the heap on a
    // domain, including dead ones that have not been recycled yet.
    std::size_t HeapSize(ClockDomain domain) const;

    // Test/ops visibility: dead entries dropped since creation (cumulative;
    // both lazy pops and compaction passes count; the per-compaction window
    // stays internal).
    std::size_t DiscardedCount(ClockDomain domain) const;

private:
    friend class slsf::runtime::Dispatcher;
    // Keeps capture destruction outside a caller's outer transaction lock.
    std::uint64_t NewSession(std::vector<TaskBody>& retired);
    struct Slot {
        TaskBody body;
        bool armed{false};           // set while a live token may cancel;
                                     // empty-marker bodies are still armed
        std::uint64_t generation{0}; // bumped on schedule/cancel/extract/reuse
        std::uint64_t session{1};
    };

    struct HeapEntry {
        double deadline{0.0};
        std::uint64_t sequence{0};
        std::size_t slot{0};
        std::uint64_t generation{0};
    };

    struct Domain {
        std::vector<HeapEntry> heap;
        std::vector<Slot> slots;
        std::vector<std::size_t> freeSlots;
        std::uint64_t nextSequence{0};
        std::uint64_t discarded{0};
        std::uint64_t discardedTotal{0};
    };

    static bool LaterThan(const HeapEntry& lhs, const HeapEntry& rhs) noexcept;

    Domain& PickerDomain(ClockDomain domain) noexcept;
    const Domain& PickerDomain(ClockDomain domain) const noexcept;

    // Releases a slot (body once + generation bump + free list).  Caller
    // holds the scheduler lock; used by cancel paths and session changes.
    TaskBody RetireSlotLocked(Domain& domain, std::size_t slotIndex);
    bool IsLiveLocked(const Domain& domain, const HeapEntry& entry) const;

    std::size_t CompactLocked(Domain& domain);

    struct State {
        std::array<Domain, kClockDomainCount> domains{};
        std::uint64_t session{1};
        mutable std::mutex mutex;
    };

    State state_;
};

}