#pragma once

#include "slsf/core/Types.h"
#include <algorithm>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace slsf::compat {
struct StorageOutput {
    Epoch epoch;
    std::string key;
    std::vector<std::int32_t> values;
    std::uint64_t request{};
};

class StorageOutputQueue {
public:
    void Submit(std::vector<StorageOutput> batch) {
        if (batch.empty()) return;
        if (epoch_ != batch.front().epoch) {
            epoch_ = batch.front().epoch;
            active_.reset();
            pending_.clear();
            committed_.clear();
        }
        for (auto& output : batch) {
            const auto queued = std::ranges::find(pending_, output.key, &StorageOutput::key);
            const auto committed = committed_.find(output.key);
            const bool inFlight = active_ && active_->key == output.key;
            if (!inFlight && committed != committed_.end() && committed->second == output.values) {
                if (queued != pending_.end()) pending_.erase(queued);
            } else if (queued != pending_.end()) {
                *queued = std::move(output);
            } else {
                pending_.push_back(std::move(output));
            }
        }
    }
    std::optional<StorageOutput> Take() {
        if (active_ || pending_.empty()) return std::nullopt;
        active_ = std::move(pending_.front());
        active_->request = ++request_;
        pending_.pop_front();
        return active_;
    }
    void Complete(std::uint64_t request, bool written) {
        if (!active_ || active_->request != request) return;
        const auto& key = active_->key;
        if (written) {
            const auto queued = std::ranges::find(pending_, key, &StorageOutput::key);
            if (queued != pending_.end() && queued->values == active_->values) pending_.erase(queued);
            committed_[key] = std::move(active_->values);
        } else {
            committed_.erase(key);
        }
        active_.reset();
    }
    bool Current(std::string_view token, Epoch epoch) const {
        return active_ && token == std::to_string(active_->request) && active_->epoch == epoch;
    }
private:
    std::optional<Epoch> epoch_;
    std::optional<StorageOutput> active_;
    std::deque<StorageOutput> pending_;
    std::unordered_map<std::string, std::vector<std::int32_t>> committed_;
    std::uint64_t request_{};
};
}
