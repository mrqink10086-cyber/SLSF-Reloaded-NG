#include "slsf/engine/EventBridge.h"

#include <utility>

namespace slsf::engine {

EventBridge::EventBridge(Lifecycle& lifecycle, Handler handler)
    : lifecycle_(lifecycle), handler_(std::move(handler)) {}

bool EventBridge::Enqueue(std::string topic, std::string payload) {
    // Capture the epoch before validating the gate.  A menu/load can switch
    // sessions between these calls; stamping the old epoch is safe because
    // Flush drops it, while reading the epoch after the check could commit an
    // old-session callback into the newly activated session.
    const Epoch epoch = lifecycle_.CurrentEpoch();
    if (topic.empty() || !lifecycle_.BusinessOpen() || !lifecycle_.IsEpochCurrent(epoch)) {
        std::lock_guard lock(mutex_);
        ++counters_.rejectedClosed;
        return false;
    }
    AsyncMessage message{
        .topic = std::move(topic),
        .payload = std::move(payload),
        .epoch = epoch,
    };
    std::lock_guard lock(mutex_);
    if (queue_.size() >= kMaxPendingMessages) {
        ++counters_.rejectedOverflow;
        return false;
    }
    queue_.push_back(std::move(message));
    ++counters_.enqueued;
    return true;
}

std::size_t EventBridge::Flush() {
    std::size_t committed = 0;
    // Only drain what was queued when this flush started.  A handler that
    // enqueues more work cannot keep one round spinning forever.
    std::size_t budget = 0;
    {
        std::lock_guard lock(mutex_);
        budget = queue_.size();
    }
    while (budget-- > 0) {
        AsyncMessage message;
        {
            std::lock_guard lock(mutex_);
            if (queue_.empty()) {
                break;
            }
            message = std::move(queue_.front());
            queue_.pop_front();
        }
        if (!lifecycle_.IsEpochCurrent(message.epoch)) {
            std::lock_guard lock(mutex_);
            ++counters_.droppedStale;
            continue;
        }
        bool ok = false;
        if (handler_) {
            try {
                handler_(message);
                ok = true;
            } catch (...) {
                ok = false;
            }
        }
        {
            std::lock_guard lock(mutex_);
            if (ok) {
                ++counters_.committed;
                ++committed;
            } else {
                ++counters_.handlerFailures;
            }
        }
    }
    return committed;
}

std::size_t EventBridge::Pending() const {
    std::lock_guard lock(mutex_);
    return queue_.size();
}

EventBridge::Counters EventBridge::Stats() const {
    std::lock_guard lock(mutex_);
    return counters_;
}

} // namespace slsf::engine
