#include "slsf/runtime/ControlExecutor.h"

#include <utility>

namespace slsf::runtime {

std::uint64_t ControlExecutor::Post(Work work) {
    std::lock_guard guard(mutex_);
    const auto sequence = ++next_;
    queue_.emplace_back(sequence, std::move(work));
    return sequence;
}

bool ControlExecutor::Drain() {
    {
        std::lock_guard guard(mutex_);
        if (owner_ == std::thread::id{}) owner_ = std::this_thread::get_id();
        if (owner_ != std::this_thread::get_id() || draining_) return false;
        draining_ = true;
    }
    try {
        for (;;) {
            Work work;
            std::uint64_t sequence;
            {
                std::lock_guard guard(mutex_);
                if (queue_.empty()) {
                    draining_ = false;
                    return true;
                }
                sequence = queue_.front().first;
                work = std::move(queue_.front().second);
                queue_.pop_front();
            }
            work(sequence);
        }
    } catch (...) {
        std::lock_guard guard(mutex_);
        draining_ = false;
        throw;
    }
}

}
