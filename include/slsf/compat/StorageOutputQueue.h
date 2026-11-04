#pragma once

#include "slsf/core/Types.h"
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <vector>

namespace slsf::compat {
struct StorageOutput {
    Epoch epoch;
    std::string token;
    std::string key;
    std::vector<std::int32_t> values;
    std::uint64_t request{};
};

class StorageOutputQueue {
public:
    void Submit(std::vector<StorageOutput> batch) {
        if (!batch.empty() && active_ && active_->epoch != batch.front().epoch) active_.reset();
        pending_.assign(std::make_move_iterator(batch.begin()), std::make_move_iterator(batch.end()));
    }
    std::optional<StorageOutput> Take() {
        if (active_ || pending_.empty()) return std::nullopt;
        active_ = std::move(pending_.front());
        active_->request = ++request_;
        pending_.pop_front();
        return active_;
    }
    void Complete(std::uint64_t request) { if (active_ && active_->request == request) active_.reset(); }
    bool Current(const std::string& token, Epoch epoch) const {
        return active_ && active_->token == token && active_->epoch == epoch;
    }
private:
    std::optional<StorageOutput> active_;
    std::deque<StorageOutput> pending_;
    std::uint64_t request_{};
};
}
