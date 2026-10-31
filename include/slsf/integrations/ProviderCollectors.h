#pragma once

#include "slsf/integrations/ExternalState.h"
#include <array>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace slsf::integrations {
struct CollectorContinuation {
    std::uint32_t stack;
    bool actors;
    bool operator==(const CollectorContinuation&) const = default;
};
inline constexpr std::size_t kMaxCollectorCalls = 4096;
std::vector<std::uint32_t> EncodeCollectorContinuations(std::span<const CollectorContinuation> calls);
bool DecodeCollectorContinuations(std::span<const std::uint32_t> words,
                                  std::vector<CollectorContinuation>& out);
struct CollectorRequest {
    Epoch epoch;
    std::vector<StableFormKey> actors;
};
bool ParseCollectorRequest(std::string_view wire, std::string_view header, CollectorRequest& out);
std::string WireEscape(std::string_view text);
std::string WireHex(std::uint32_t value);
inline constexpr std::array<std::string_view, 18> kDdngDeviceKeywords{
    "zad_DeviousHood", "zad_DeviousHarness", "zad_DeviousBelt", "zad_DeviousBra",
    "zad_DeviousHeavyBondage", "zad_DeviousArmCuffs", "zad_DeviousCuffsFront",
    "zad_DeviousArmbinder", "zad_DeviousArmbinderElbow", "zad_DeviousGloves",
    "zad_DeviousLegCuffs", "zad_DeviousBoots", "zad_DeviousGag", "zad_DeviousGagPanel",
    "zad_DeviousSuit", "zad_DeviousCorset", "zad_DeviousBlindfold", "zad_Lockable"};
}
