#pragma once

#include "slsf/integrations/ExternalState.h"
#include "slsf/integrations/PapyrusCalls.h"

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace slsf::integrations {

enum class ExternalMod : std::uint8_t {
    PublicWhore = 0,
    FillHerUp,
    FertilityMode,
    EstrusChaurus,
    EstrusSpider,
    HentaiPregnancy,
    FameComments,
    Bimbos,
};

inline constexpr std::size_t kExternalModCount = 8;

[[nodiscard]] const char* ExternalModName(ExternalMod value) noexcept;
[[nodiscard]] const char* ExternalModEsp(ExternalMod value) noexcept;

struct ExternalModStatus {
    ExternalMod mod{};
    ExternalAvailability availability{ExternalAvailability::Unknown};
    std::string version;
};

struct ExternalActorModState {
    StableFormKey actor;
    bool isFemale{};
    bool fertilityModeBreeder{};
    bool hentaiPregnant{};
    bool estrusChaurusBreeder{};
    bool estrusSpiderBreeder{};
    bool inflationKnown{};
    double inflation{};
    std::optional<int> fertilityModeRank;
    std::optional<int> hentaiPregnancyRank;
};

struct ExternalSceneSnapshot {
    ExternalAvailability availability{ExternalAvailability::Unknown};
    ExternalProviderInfo provider;
    Epoch epoch{};
    bool playerPresent{};
    bool publicWhoreActive{};
    std::vector<ExternalModStatus> mods;
    std::vector<ExternalActorModState> actors;

    [[nodiscard]] const ExternalModStatus* FindMod(ExternalMod mod) const;
    [[nodiscard]] const ExternalActorModState* FindActor(const StableFormKey& actor) const;
};

[[nodiscard]] bool ParseExternalWire(std::string_view wire, const Epoch& expected,
                                     ExternalSceneSnapshot& out, std::string& error);

[[nodiscard]] bool ExternalModAvailable(const ExternalSceneSnapshot& snapshot,
                                        ExternalMod mod);
[[nodiscard]] Fact ExternalProviderFact(const ExternalSceneSnapshot& snapshot);
[[nodiscard]] Fact ExternalModFact(const ExternalSceneSnapshot& snapshot, ExternalMod mod);
[[nodiscard]] Fact ExternalPublicWhoreFact(const ExternalSceneSnapshot& snapshot);

struct ExternalModsOptions {
    ExternalTimingPolicy timing;
    std::string script{"SLSF_Reloaded_ExternalProbe"};
    std::string function{"Collect"};
    std::uint32_t requiredApiVersion{1};
};

class ExternalMods {
public:
    using Completion =
        std::function<void(const ExternalSceneSnapshot&, ExternalGate)>;

    ExternalMods(PapyrusCalls& calls, ExternalModsOptions options = {});
    ~ExternalMods();

    ExternalMods(const ExternalMods&) = delete;
    ExternalMods& operator=(const ExternalMods&) = delete;

    RequestId RequestState(const Epoch& epoch, const std::vector<StableFormKey>& actors,
                           std::int64_t nowMs, Completion completion = {});
    void OnStateChanged(const Epoch& epoch);
    void OnModEvent(std::string_view eventName, std::int64_t nowMs);
    void Advance(std::int64_t nowMs);

    [[nodiscard]] ExternalGate Gate() const;
    [[nodiscard]] ExternalSceneSnapshot Snapshot() const;
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
