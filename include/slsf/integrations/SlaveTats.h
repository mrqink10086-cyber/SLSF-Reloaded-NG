#pragma once

#include "slsf/core/gameplay/Visibility.h"
#include "slsf/integrations/CapabilityRegistry.h"
#include "slsf/integrations/ExternalState.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace slsf::integrations {

inline constexpr std::size_t kSlaveTatsAreaCount =
    static_cast<std::size_t>(gameplay::kTattooAreaCount);

enum class SlaveTatsArea : std::size_t { Body = 0, Face, Hands, Feet };

[[nodiscard]] const char* SlaveTatsAreaName(SlaveTatsArea value) noexcept;

inline constexpr std::int64_t kSlaveTatsInterfaceMessageType = 1;
inline constexpr std::uint32_t kSlaveTatsInterfaceVersion = 1;
inline constexpr std::size_t kSlaveTatsKnownPrefixPointers = 15;

struct SlaveTatsPlacement {
    std::string name;
    std::int32_t slot{};
    bool operator==(const SlaveTatsPlacement&) const = default;
};

struct SlaveTatsAreaRecords {
    std::vector<SlaveTatsPlacement> placements;
    std::uint32_t rejectedEntries{};
    bool truncated{};

    [[nodiscard]] bool HasRecordAtSlot(std::int32_t slot) const noexcept;

    friend bool operator==(const SlaveTatsAreaRecords&, const SlaveTatsAreaRecords&) = default;
};

struct SlaveTatsSnapshot {
    ExternalAvailability availability{ExternalAvailability::Unknown};
    std::uint32_t interfaceVersion{};
    Epoch epoch{};
    std::array<SlaveTatsAreaRecords, kSlaveTatsAreaCount> areas;

    [[nodiscard]] const SlaveTatsAreaRecords& Area(SlaveTatsArea area) const;

    friend bool operator==(const SlaveTatsSnapshot&, const SlaveTatsSnapshot&) = default;
};

[[nodiscard]] std::vector<gameplay::TattooSlotFacts> SlaveTatsSlotFacts(
    const SlaveTatsAreaRecords& records, std::size_t capacity);

inline constexpr std::size_t kMaxSlaveTatsPlacements = 256;
inline constexpr std::size_t kMaxSlaveTatsNameBytes = 128;

struct JcString {
    std::uint64_t opaque{};

    friend constexpr bool operator==(const JcString&, const JcString&) = default;
};

struct JcTable {
    void* domain{};
    std::int32_t (*arrayCount)(void* domain, std::int32_t array){};
    std::int32_t (*arrayObject)(void* domain){};
    std::int32_t (*arrayGetObj)(void* domain, std::int32_t array, std::int32_t index,
                                std::int32_t fallback){};
    void (*arrayAddObj)(void* domain, std::int32_t array, std::int32_t value, std::int32_t index){};
    JcString (*mapGetStr)(void* domain, std::int32_t object, JcString key, JcString fallback){};
    std::int32_t (*mapGetInt)(void* domain, std::int32_t object, JcString key,
                              std::int32_t fallback){};
    std::int32_t (*release)(void* domain, std::int32_t object){};
    std::int32_t (*retain)(void* domain, std::int32_t object, JcString tag){};
};

class SlaveTatsPort {
public:
    virtual ~SlaveTatsPort() = default;

    [[nodiscard]] virtual const JcTable* Table() noexcept = 0;
    [[nodiscard]] virtual JcString Intern(const char* text) noexcept = 0;
    [[nodiscard]] virtual std::string Text(JcString value) noexcept = 0;
    [[nodiscard]] virtual void* ResolveActor(std::uint32_t formId) noexcept = 0;
};

class JcHandle {
public:
    JcHandle() noexcept = default;
    JcHandle(const JcTable* table, std::int32_t handle) noexcept;
    ~JcHandle();
    JcHandle(const JcHandle&) = delete;
    JcHandle& operator=(const JcHandle&) = delete;
    JcHandle(JcHandle&& other) noexcept;
    JcHandle& operator=(JcHandle&& other) noexcept;

    [[nodiscard]] std::int32_t get() const noexcept;
    [[nodiscard]] bool Owns() const noexcept;
    void Release() noexcept;

private:
    const JcTable* table_{};
    std::int32_t handle_{};
};

struct SlaveTatsCapability {
    CapabilityState state{CapabilityState::Unqueried};
    std::uint32_t version{};
    std::uint32_t confirmations{};
    std::uint32_t rejections{};
    std::string reason;

    friend bool operator==(const SlaveTatsCapability&, const SlaveTatsCapability&) = default;
};

struct SlaveTatsOptions {
    ExternalTimingPolicy timing;
};

struct SlaveTatsCollectResult {
    ExternalGate gate{ExternalGate::Waiting};
    bool changed{};
};

class SlaveTats {
public:
    SlaveTats(SlaveTatsPort& port, SlaveTatsOptions options = {});
    ~SlaveTats();

    SlaveTats(const SlaveTats&) = delete;
    SlaveTats& operator=(const SlaveTats&) = delete;

    [[nodiscard]] bool ConsumeInterfaceMessage(std::int64_t messageType, const void* data,
                                               std::size_t dataLen) noexcept;
    [[nodiscard]] SlaveTatsCapability Capability() const;

    [[nodiscard]] SlaveTatsCollectResult Collect(std::uint32_t actorFormId, const Epoch& epoch);
    [[nodiscard]] SlaveTatsSnapshot Snapshot() const;
    [[nodiscard]] ExternalGate Gate() const;
    [[nodiscard]] std::uint32_t Attempts() const;
    [[nodiscard]] bool Dirty() const;
    [[nodiscard]] std::string LastError() const;

    void MarkDirty(const Epoch& eventEpoch);
    void OnSessionChanged(const Epoch& epoch);
    void Cancel();
    void Reset();

private:
    struct Impl;
    std::shared_ptr<Impl> impl_;
};

}
