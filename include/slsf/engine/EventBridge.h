#pragma once

#include "slsf/core/Types.h"
#include "slsf/runtime/Lifecycle.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>

namespace slsf::engine {

struct AsyncMessage {
    std::string topic;
    std::string payload;
    Epoch epoch;
};

// Engine callbacks run on arbitrary threads and may complete after a menu,
// revert or A->B transition.  Enqueue stamps the session epoch while the
// business switch is open; Flush revalidates every stamp immediately before
// handing the message to the executor.  Messages are committed FIFO and each
// exactly once.
class EventBridge {
public:
    using Handler = std::function<void(const AsyncMessage&)>;

    // Bounds retained engine callbacks if a session never gets an executor
    // round; overflow is rejected instead of growing without limit.
    static constexpr std::size_t kMaxPendingMessages = 4096;

    struct Counters {
        std::uint64_t enqueued{};
        std::uint64_t committed{};
        std::uint64_t rejectedClosed{};
        std::uint64_t rejectedOverflow{};
        std::uint64_t droppedStale{};
        std::uint64_t handlerFailures{};
    };

    EventBridge(Lifecycle& lifecycle, Handler handler);

    bool Enqueue(std::string topic, std::string payload);
    std::size_t Flush();
    std::size_t Pending() const;
    Counters Stats() const;

private:
    Lifecycle& lifecycle_;
    Handler handler_;
    mutable std::mutex mutex_;
    std::deque<AsyncMessage> queue_;
    Counters counters_;
};

} // namespace slsf::engine
