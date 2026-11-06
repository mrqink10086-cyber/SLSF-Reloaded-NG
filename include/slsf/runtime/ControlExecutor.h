#pragma once

#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <utility>

namespace slsf::runtime {

class ControlExecutor {
public:
    using Work = std::function<void(std::uint64_t)>;
    std::uint64_t Post(Work work);
    bool Drain();

private:
    std::mutex mutex_;
    std::deque<std::pair<std::uint64_t, Work>> queue_;
    std::uint64_t next_{};
    std::thread::id owner_;
    bool draining_{};
};

}
