#include "slsf/core/time/Scheduler.h"

#include <algorithm>
#include <cmath>

namespace slsf::time {

namespace {

bool ValidDomain(ClockDomain domain) noexcept {
    return static_cast<std::size_t>(domain) < kClockDomainCount;
}
}
bool Scheduler::LaterThan(const HeapEntry& lhs, const HeapEntry& rhs) noexcept {
    // "Later" = pops after the other.  std::make_heap with this comparator
    // keeps the earliest (deadline, sequence) entry at heap.front().
    if (lhs.deadline != rhs.deadline) {
        return lhs.deadline > rhs.deadline;
    }
    return lhs.sequence > rhs.sequence;
}
Scheduler::Domain& Scheduler::PickerDomain(ClockDomain domain) noexcept {
    return state_.domains[static_cast<std::size_t>(domain)];
}
const Scheduler::Domain& Scheduler::PickerDomain(ClockDomain domain) const noexcept {
    return state_.domains[static_cast<std::size_t>(domain)];
}
TaskBody Scheduler::RetireSlotLocked(Domain& domain, std::size_t slotIndex) {
    Slot& slot = domain.slots[slotIndex];
    domain.freeSlots.push_back(slotIndex);
    TaskBody released = std::move(slot.body);
    slot.body = nullptr;
    slot.armed = false;
    slot.generation += 1;
    return released;
}
bool Scheduler::IsLiveLocked(const Domain& domain, const HeapEntry& entry) const {
    const Slot& slot = domain.slots[entry.slot];
    return slot.armed && slot.generation == entry.generation &&
           slot.session == state_.session;
}std::size_t Scheduler::CompactLocked(Domain& domain) {
    std::vector<HeapEntry> kept;
    kept.reserve(domain.heap.size());
    for (const HeapEntry& entry : domain.heap) {
        if (IsLiveLocked(domain, entry)) {
            kept.push_back(entry);
        }
    }
    const std::size_t dropped = domain.heap.size() - kept.size();
    domain.heap.swap(kept);
    std::make_heap(domain.heap.begin(), domain.heap.end(), LaterThan);
    domain.discarded = 0;
    domain.discardedTotal += dropped;
    return dropped;
}
TimerToken Scheduler::Schedule(ClockDomain domain, double deadline, TaskBody body) {
    if (!ValidDomain(domain) || !std::isfinite(deadline)) {
        return TimerToken{};
    }
    const std::lock_guard<std::mutex> guard(state_.mutex);
    Domain& target = PickerDomain(domain);

    std::size_t slotIndex = 0;
    if (!target.freeSlots.empty()) {
        slotIndex = target.freeSlots.back();
        target.freeSlots.pop_back();
    } else {
        slotIndex = target.slots.size();
        target.slots.emplace_back();
    }

    Slot& slot = target.slots[slotIndex];
    slot.generation += 1;
    slot.session = state_.session;
    slot.body = std::move(body);
    slot.armed = true;

    HeapEntry entry;
    entry.deadline = deadline;
    entry.sequence = target.nextSequence++;
    entry.slot = slotIndex;
    entry.generation = slot.generation;
    target.heap.push_back(entry);
    std::push_heap(target.heap.begin(), target.heap.end(), LaterThan);

    TimerToken token;
    token.domain = domain;
    token.id = TaskId(slotIndex + 1);
    token.generation = slot.generation;
    return token;
}
bool Scheduler::Cancel(TimerToken token) {
    TaskBody released;
    if (token.id.value == 0) {
        return false;
    }
    if (!ValidDomain(token.domain)) {
        return false;
    }
    const std::lock_guard<std::mutex> guard(state_.mutex);
    Domain& target = state_.domains[static_cast<std::size_t>(token.domain)];
    if (token.id.value > target.slots.size()) {
        return false;
    }
    Slot& slot = target.slots[static_cast<std::size_t>(token.id.value) - 1];
    if (slot.generation != token.generation || !slot.armed
        || slot.session != state_.session) {

        return false;
    }
    released = RetireSlotLocked(target, static_cast<std::size_t>(token.id.value) - 1);
    ++target.discarded;
    if (target.discarded > 16 && target.discarded * 2 > target.heap.size()) {
        CompactLocked(target);
    }
    return true;
}
std::vector<DueTask> Scheduler::AcquireDue(ClockDomain domain, double now) {
    std::vector<DueTask> due;
    if (!ValidDomain(domain) || !std::isfinite(now)) {
        return due;
    }
    const std::lock_guard<std::mutex> guard(state_.mutex);
    Domain& target = PickerDomain(domain);

    while (!target.heap.empty()) {
        const HeapEntry& top = target.heap.front();
        if (top.deadline > now) {
            break;
        }
        std::pop_heap(target.heap.begin(), target.heap.end(), LaterThan);
        const HeapEntry popped = target.heap.back();
        target.heap.pop_back();

        Slot& slot = target.slots[popped.slot];
        const bool dead = !IsLiveLocked(target, popped);
        if (dead) {
            // Cancelled or session-replaced: skip and count toward
            // compaction.  The slot was already retired by the killer.
            target.discarded -= 1;
            target.discardedTotal += 1;
            continue;
        }

        DueTask task;
        task.token.domain = domain;
        task.token.id = TaskId(popped.slot + 1);
        task.token.generation = popped.generation;
        task.body = std::move(slot.body);
        task.deadline = popped.deadline;
        task.sequence = popped.sequence;
        task.session = slot.session;

        slot.body = nullptr;
        slot.armed = false;
        slot.generation += 1;
        target.freeSlots.push_back(popped.slot);
        due.push_back(std::move(task));
    }

    if (target.discarded > 16 && target.discarded * 2 > target.heap.size()) {
        CompactLocked(target);
    }
    return due;
}
bool Scheduler::HasDue(ClockDomain domain, double now) {
    if (!ValidDomain(domain) || !std::isfinite(now)) {
        return false;
    }
    const std::lock_guard<std::mutex> guard(state_.mutex);
    Domain& target = PickerDomain(domain);
    while (!target.heap.empty() && !IsLiveLocked(target, target.heap.front())) {
        std::pop_heap(target.heap.begin(), target.heap.end(), LaterThan);
        target.heap.pop_back();
        --target.discarded;
        ++target.discardedTotal;
    }
    return !target.heap.empty() && target.heap.front().deadline <= now;
}
std::size_t Scheduler::DueCount(ClockDomain domain, double now) const {
    if (!ValidDomain(domain) || !std::isfinite(now)) {
        return 0;
    }
    const std::lock_guard<std::mutex> guard(state_.mutex);
    const Domain& target = PickerDomain(domain);
    std::size_t count = 0;
    for (const HeapEntry& entry : target.heap) {
        if (entry.deadline > now) {
            continue;
        }
        if (IsLiveLocked(target, entry)) {
            count += 1;
        }
    }
    return count;
}
std::size_t Scheduler::PendingCount(ClockDomain domain) const {
    if (!ValidDomain(domain)) {
        return 0;
    }
    const std::lock_guard<std::mutex> guard(state_.mutex);
    const Domain& target = PickerDomain(domain);
    std::size_t count = 0;
    for (const Slot& slot : target.slots) {
        if (slot.armed && slot.session == state_.session) {
            count += 1;
        }
    }
    return count;
}
std::size_t Scheduler::PendingCount() const {
    const std::lock_guard<std::mutex> guard(state_.mutex);
    std::size_t count = 0;
    for (const Domain& domain : state_.domains) {
        for (const Slot& slot : domain.slots) {
            if (slot.armed && slot.session == state_.session) {
                count += 1;
            }
        }
    }
    return count;
}
std::uint64_t Scheduler::NewSession() {
    std::vector<TaskBody> retired;
    return NewSession(retired);
}
std::uint64_t Scheduler::NewSession(std::vector<TaskBody>& retired) {
    const std::lock_guard<std::mutex> guard(state_.mutex);
    std::size_t capacity = retired.size();
    for (const Domain& domain : state_.domains) { capacity += domain.slots.size(); }
    retired.reserve(capacity);
    state_.session += 1;
    for (Domain& domain : state_.domains) {
        for (std::size_t slotIndex = 0; slotIndex < domain.slots.size(); ++slotIndex) {
            Slot& slot = domain.slots[slotIndex];
            if (slot.armed && slot.session != state_.session) {
                retired.push_back(RetireSlotLocked(domain, slotIndex));
            }
        }
        domain.discardedTotal += domain.heap.size();
        domain.heap.clear();
        domain.discarded = 0;
    }
    return state_.session;
}
std::uint64_t Scheduler::Session() const {
    const std::lock_guard<std::mutex> guard(state_.mutex);
    return state_.session;
}
std::size_t Scheduler::HeapSize(ClockDomain domain) const {
    if (!ValidDomain(domain)) {
        return 0;
    }
    const std::lock_guard<std::mutex> guard(state_.mutex);
    return PickerDomain(domain).heap.size();
}
std::size_t Scheduler::DiscardedCount(ClockDomain domain) const {
    if (!ValidDomain(domain)) {
        return 0;
    }
    const std::lock_guard<std::mutex> guard(state_.mutex);
    return PickerDomain(domain).discardedTotal;
}
}