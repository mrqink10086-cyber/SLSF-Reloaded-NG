#pragma once

#include "slsf/integrations/CapabilityRegistry.h"
#include "slsf/integrations/ExternalState.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace slsf::integrations {

inline constexpr std::size_t kRaceMenuOverlayLocationCount = 4;

enum class RaceMenuOverlayLocation : std::uint8_t { Body = 0, Hand, Feet, Face };

[[nodiscard]] const char* RaceMenuOverlayLocationName(RaceMenuOverlayLocation value) noexcept;

inline constexpr std::uint32_t kSkeeInterfaceExchangeMessage = 0x9E3779B9u;
inline constexpr std::uint32_t kSkeeOverlayInterfaceVersion = 2;

struct RaceMenuOverlayCounts {
    std::array<std::uint32_t, kRaceMenuOverlayLocationCount> counts{};
    friend bool operator==(const RaceMenuOverlayCounts&, const RaceMenuOverlayCounts&) = default;
};

struct RaceMenuSnapshot {
    ExternalAvailability availability{ExternalAvailability::Unknown};
    std::uint32_t overlayInterfaceVersion{};
    Epoch epoch{};
    RaceMenuOverlayCounts overlays;

    friend bool operator==(const RaceMenuSnapshot&, const RaceMenuSnapshot&) = default;
};

struct RaceMenuCapabilityRecord {
    std::string name;
    CapabilityState state{CapabilityState::Unqueried};
    std::uint32_t version{};
    std::string reason;

    friend bool operator==(const RaceMenuCapabilityRecord&, const RaceMenuCapabilityRecord&) = default;
};

struct RaceMenuOptions {
    ExternalTimingPolicy timing;
    std::uint32_t requiredOverlayVersion{kSkeeOverlayInterfaceVersion};
};

class RaceMenuExchangePort {
public:
    virtual ~RaceMenuExchangePort() = default;

    [[nodiscard]] virtual void* Exchange() noexcept = 0;
};

struct RaceMenuEnsureResult {
    ExternalGate gate{ExternalGate::Waiting};
    bool changed{};
};

class RaceMenu {
public:
    RaceMenu(RaceMenuExchangePort& port, RaceMenuOptions options = {});
    ~RaceMenu();

    RaceMenu(const RaceMenu&) = delete;
    RaceMenu& operator=(const RaceMenu&) = delete;

    [[nodiscard]] RaceMenuEnsureResult Ensure(const Epoch& epoch);
    [[nodiscard]] RaceMenuSnapshot Snapshot() const;
    [[nodiscard]] ExternalGate Gate() const;
    [[nodiscard]] std::uint32_t Attempts() const;
    [[nodiscard]] bool Dirty() const;
    [[nodiscard]] void Capabilities(std::vector<RaceMenuCapabilityRecord>& out) const;
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
