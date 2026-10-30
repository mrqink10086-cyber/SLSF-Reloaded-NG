#pragma once

#include <condition_variable>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace slsf::integrations {

enum class CapabilityState : std::uint8_t {
    Unqueried = 0,
    Waiting,
    Available,
    Missing,
    Incompatible,
};

[[nodiscard]] const char* CapabilityStateName(CapabilityState state) noexcept;

struct CapabilityRequirement {
    std::string id;
    std::string displayName;
    std::string business;
    std::uint32_t minimumVersion{0};
    std::uint32_t maximumVersion{0xFFFFFFFFu};
    std::uint32_t requiredApiVersion{1};
    std::size_t minimumStructSize{0};
    std::size_t requiredFunctionCount{0};
};

struct CapabilityProbeResult {
    bool loaded{false};
    bool pending{false};
    std::uint32_t version{0};
    std::uint32_t apiVersion{0};
    std::size_t structSize{0};
    std::vector<const void*> functions;
};

using CapabilityQuery = std::function<CapabilityProbeResult(const CapabilityRequirement&)>;

struct CapabilityRecord {
    CapabilityRequirement requirement;
    CapabilityState state{CapabilityState::Unqueried};
    std::uint32_t version{0};
    std::uint32_t apiVersion{0};
    std::size_t structSize{0};
    std::size_t functionCount{0};
    std::uint32_t queryCount{0};
    std::string reason;
};

struct CapabilityWaitPolicy {
    std::chrono::milliseconds joinWait{10000};
    std::chrono::milliseconds waitingTimeout{60000};
    std::uint32_t maxWaitingProbes{8};
};

class CapabilityRegistry {
public:
    explicit CapabilityRegistry(CapabilityQuery query);

    CapabilityRegistry(const CapabilityRegistry&) = delete;
    CapabilityRegistry& operator=(const CapabilityRegistry&) = delete;

    bool Require(CapabilityRequirement requirement);

    CapabilityState Ensure(std::string_view id);

    void EnsureAll();

    void SetWaitPolicy(CapabilityWaitPolicy policy);

    [[nodiscard]] std::optional<CapabilityRecord> Find(std::string_view id) const;
    [[nodiscard]] bool IsAvailable(std::string_view id) const;
    [[nodiscard]] std::vector<CapabilityRecord> Snapshot() const;
    [[nodiscard]] std::size_t Size() const;

private:
    struct Entry {
        CapabilityRequirement requirement;
        CapabilityRecord record;
        bool querying{false};
        std::thread::id queryThread;
        std::condition_variable completed;
        std::chrono::steady_clock::time_point waitingSince{};
        std::uint32_t waitingProbes{0};
    };

    mutable std::mutex mutex_;
    CapabilityQuery query_;
    std::deque<Entry> entries_;
    std::map<std::thread::id, Entry*> waitingOn_;
    CapabilityWaitPolicy policy_;
};

[[nodiscard]] CapabilityRecord EvaluateCapability(const CapabilityRequirement& requirement,
                                                  const CapabilityProbeResult& probe);

}
