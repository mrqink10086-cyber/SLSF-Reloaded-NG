#pragma once

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
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

// A provider-owned snapshot of one optional runtime dependency.  The registry
// validates it immediately and never retains the raw function pointers.
struct CapabilityProbeResult {
    bool loaded{false};
    // True when the dependency is known to be installable later (for example
    // a plugin whose kPostLoad broadcast has not run yet).  Pending results
    // become Waiting and are retried; a plain missing module is terminal.
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

// Process-lifetime registry for optional integrations.  Ensure() is
// serialized: a duplicate concurrent request observes the single in-flight
// result instead of probing twice.  Waiting is the only retryable terminal
// state, allowing a dependency loaded after our first message to complete its
// query later.
class CapabilityRegistry {
public:
    explicit CapabilityRegistry(CapabilityQuery query);

    CapabilityRegistry(const CapabilityRegistry&) = delete;
    CapabilityRegistry& operator=(const CapabilityRegistry&) = delete;

    // Returns false when the id is already registered with a different
    // requirement or the requirement is malformed.
    bool Require(CapabilityRequirement requirement);

    // Queries (or shares an in-flight query for) one capability.  Unknown ids
    // are reported as Incompatible rather than auto-registered.
    CapabilityState Ensure(std::string_view id);

    void EnsureAll();

    [[nodiscard]] std::optional<CapabilityRecord> Find(std::string_view id) const;
    [[nodiscard]] bool IsAvailable(std::string_view id) const;
    [[nodiscard]] std::vector<CapabilityRecord> Snapshot() const;
    [[nodiscard]] std::size_t Size() const;

private:
    struct Entry {
        CapabilityRequirement requirement;
        CapabilityRecord record;
        bool querying{false};
        std::condition_variable completed;
    };

    mutable std::mutex mutex_;
    CapabilityQuery query_;
    // Deque keeps Entry references stable while callers wait on completed.
    std::deque<Entry> entries_;
};

[[nodiscard]] CapabilityRecord EvaluateCapability(const CapabilityRequirement& requirement,
                                                  const CapabilityProbeResult& probe);

} // namespace slsf::integrations
