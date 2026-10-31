#pragma once

#include "slsf/core/gameplay/AnimationClassifier.h"
#include "slsf/integrations/ExternalState.h"
#include "slsf/integrations/PapyrusCalls.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace slsf::integrations {

enum class DdngDeviceSlot : std::uint8_t {
    Hood = 0,
    Harness,
    Belt,
    Bra,
    HeavyBondage,
    ArmCuffs,
    ArmCuffsFront,
    Armbinder,
    ArmbinderElbow,
    Gloves,
    LegCuffs,
    Boots,
    Gag,
    GagPanel,
    Suit,
    Corset,
    Blindfold,
    Lockable,
};

inline constexpr std::size_t kDdngDeviceSlotCount = 18;

[[nodiscard]] const char* DdngDeviceSlotName(DdngDeviceSlot value) noexcept;
[[nodiscard]] constexpr std::uint32_t DdngSlotBit(DdngDeviceSlot value) noexcept {
    return 1u << static_cast<std::uint32_t>(value);
}
[[nodiscard]] constexpr bool HasDdngDevice(std::uint32_t mask,
                                           DdngDeviceSlot value) noexcept {
    return (mask & DdngSlotBit(value)) != 0;
}

namespace ddng_bondage {
inline constexpr std::uint32_t HandsBound = 0x1;
inline constexpr std::uint32_t HandsBoundNoAnim = 0x2;
inline constexpr std::uint32_t GaggedBlocking = 0x4;
inline constexpr std::uint32_t ChastifiedGenital = 0x8;
inline constexpr std::uint32_t ChastifiedAnal = 0x10;
inline constexpr std::uint32_t ChastifiedBreasts = 0x20;
inline constexpr std::uint32_t Blindfolded = 0x40;
inline constexpr std::uint32_t Mittens = 0x80;
inline constexpr std::uint32_t Boots = 0x100;
inline constexpr std::uint32_t Total = 0x200;
}

struct DdngActorState {
    StableFormKey actor;
    std::uint32_t bondage{};
    std::uint32_t devices{};
};

struct DdngSnapshot {
    ExternalAvailability availability{ExternalAvailability::Unknown};
    ExternalProviderInfo provider;
    Epoch epoch{};
    std::uint64_t equipmentRevision{};
    std::vector<DdngActorState> actors;

    [[nodiscard]] const DdngActorState* Find(const StableFormKey& actor) const;
};

[[nodiscard]] bool ParseDdngWire(std::string_view wire, const Epoch& expected,
                                 DdngSnapshot& out, std::string& error);

[[nodiscard]] Fact DdngInstallationFact(const DdngSnapshot& snapshot);

struct DdngApplyResult {
    bool applied{};
    std::string detail;
};

void MarkDdngFactsUnavailable(gameplay::AnimationDeviousFacts& facts, FactState state);
[[nodiscard]] DdngApplyResult ApplyDdngToAnimation(const DdngSnapshot& snapshot,
                                                   const StableFormKey& actor,
                                                   gameplay::AnimationDeviousFacts& facts);

struct DdngOptions {
    ExternalTimingPolicy timing;
    std::string script{"SLSF_Reloaded_DDProbe"};
    std::string function{"CollectScene"};
    std::uint32_t requiredApiVersion{2};
};

class DeviousDevices {
public:
    using Completion = std::function<void(const DdngSnapshot&, ExternalGate)>;

    DeviousDevices(PapyrusCalls& calls, DdngOptions options = {});
    ~DeviousDevices();

    DeviousDevices(const DeviousDevices&) = delete;
    DeviousDevices& operator=(const DeviousDevices&) = delete;

    RequestId RequestState(const Epoch& epoch, const std::vector<StableFormKey>& actors,
                           std::int64_t nowMs, Completion completion = {});
    void OnEquipmentChanged(const Epoch& epoch);
    void Advance(std::int64_t nowMs);

    [[nodiscard]] ExternalGate Gate() const;
    [[nodiscard]] DdngSnapshot Snapshot() const;
    [[nodiscard]] std::uint64_t Revision() const;
    [[nodiscard]] std::uint32_t Attempts() const;
    [[nodiscard]] std::string BuildRequestPayload(const Epoch& epoch,
                                                  const std::vector<StableFormKey>& actors) const;

    void Cancel();
    void Reset();

private:
    struct Impl;
    std::shared_ptr<Impl> impl_;
};

}
