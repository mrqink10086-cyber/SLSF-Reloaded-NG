#pragma once

#include "slsf/core/Commands.h"
#include "slsf/core/Types.h"

#include <array>
#include <optional>
#include <string_view>

namespace slsf {

// Gain/decay tier caps and the night window come from the legacy MCM
// (SLSF_Reloaded_MCM.psc).  Keys mirror the legacy property names; the runtime
// configuration snapshot is a double map, so boolean flags use the legacy
// convention: value != 0 means enabled.
inline constexpr std::string_view kGainConfigKeyMultiplier = "FameChangeMultiplier";
inline constexpr std::string_view kGainConfigKeyUseGlobal = "UseGlobalFameMultiplier";
inline constexpr std::string_view kGainConfigKeyCategoryMultiplierPrefix = "FameCategoryMultiplier"; // + index 0..24
inline constexpr std::string_view kGainConfigKeyNightReduction = "ReduceFameAtNight";
inline constexpr std::string_view kGainConfigKeyNightStart = "NightStart";       // hours, default 22
inline constexpr std::string_view kGainConfigKeyNightEnd = "NightEnd";           // hours, default 6
inline constexpr std::string_view kGainConfigKeyDecayTimeNeeded = "DecayTimeNeeded"; // legacy half-hour batches, default 24
inline constexpr std::string_view kGainConfigKeyGainVeryHigh = "MaxVHighFameGain"; // default 2
inline constexpr std::string_view kGainConfigKeyGainHigh = "MaxHighFameGain";     // default 4
inline constexpr std::string_view kGainConfigKeyGainMedium = "MaxMedFameGain";    // default 6
inline constexpr std::string_view kGainConfigKeyGainLow = "MaxLowFameGain";       // default 8
inline constexpr std::string_view kGainConfigKeyGainVeryLow = "MaxVLowFameGain";  // default 10
inline constexpr std::string_view kGainConfigKeyDecayVeryHigh = "MaxVHighFameDecay"; // default 1
inline constexpr std::string_view kGainConfigKeyDecayHigh = "MaxHighFameDecay";      // default 2
inline constexpr std::string_view kGainConfigKeyDecayMedium = "MaxMedFameDecay";     // default 3
inline constexpr std::string_view kGainConfigKeyDecayLow = "MaxLowFameDecay";        // default 4
inline constexpr std::string_view kGainConfigKeyDecayVeryLow = "MaxVLowFameDecay";   // default 5

inline constexpr double kLegacyMaxFameValue = 150.0;
inline constexpr double kLegacyGainTierBoundaries[4] = {100.0, 75.0, 50.0, 25.0};
inline constexpr double kLegacyManualSpreadMinSource = 10.0;
inline constexpr double kLegacyManualSpreadMinPercent = 10.0;
inline constexpr double kLegacyManualSpreadMaxPercent = 100.0;
inline constexpr double kLegacyDefaultNightStart = 22.0;
inline constexpr double kLegacyDefaultNightEnd = 6.0;

struct CompiledFameRules {
    std::array<double, kFameCategoryCount> categoryMultipliers = [] {
        std::array<double, kFameCategoryCount> values{};
        values.fill(1.0);
        return values;
    }();
    std::array<int, 5> gainTierCaps{10, 8, 6, 4, 2};
    std::array<int, 5> decayTierCaps{5, 4, 3, 2, 1};
    bool nightReduction{true};
    double nightStart{kLegacyDefaultNightStart};
    double nightEnd{kLegacyDefaultNightEnd};
    double decayProtectionDays{0.5};
};

// Tier-cap arrays use LegacyFameTier indices: veryLow through veryHigh.
CompiledFameRules CompileFameRules(const Configuration& configuration);

// Per-category fame multiplier, legacy MCM order: global multiplier while
// UseGlobalFameMultiplier is on, otherwise the FameCategoryMultiplier<i> entry.
double LegacyFameMultiplier(const Configuration& configuration, std::uint32_t categoryIndex);

bool LegacyNightWindow(double hourOfDay, double nightStart, double nightEnd) noexcept;

// Tier index 0..4 (veryLow..veryHigh) for a previous value against the legacy
// boundaries {100, 75, 50, 25}.
int LegacyFameTier(double previousValue) noexcept;

struct GainFameInputs {
    double previousFame{};             // published fame (max(local, remote)) before the gain.
    double iterationMultiplier{1.0};   // legacy GainIterationMultiplier.
    double maxFameValue{150.0};

    // Pre-resolved multiplier; when absent the category multiplier is
    // resolved from configuration per call.
    std::optional<double> resolvedMultiplier;

    bool nightReduction{};
    double hourOfDay{};
    double nightStart{22.0};
    double nightEnd{6.0};
    Fact foreplay{};
};

struct GainFameResult {
    int gained{};            // points to add to the local row (>= 1).
    bool halved{};
    bool flooredToMinimum{};
};

// Gain rule (legacy GainFame).  One uniform RandomInt(1, tierCap) sample; the
// multiplier is halved when night reduction is enabled, the hour is inside the
// night window, and foreplay is KnownTrue.  The scaled product truncates
// toward zero, is scaled by the iteration multiplier, is floored at one point,
// and the caller caps previous + gained at maxFameValue.
GainFameResult RuleGainFame(const GainFameInputs& inputs,
                            IRandom& rng,
                            const Configuration& configuration,
                            std::uint32_t categoryIndex);

GainFameResult RuleGainFame(const GainFameInputs& inputs,
                            IRandom& rng,
                            const CompiledFameRules& rules,
                            std::uint32_t categoryIndex);

struct ManualSpreadResult {
    double newValue{};
    bool percentClamped{};
};

// Manual spread rule (legacy Spread): requires source >= minSource, clamps
// the percentage into [minPercent, maxPercent] and yields
//   newValue = min(maxFameValue, floor(target + source * percent / 100)),
// with a lower bound of zero.  The source row is unchanged.
Result<ManualSpreadResult> RuleManualSpreadFame(double sourceValue,
                                                double targetValue,
                                                int percent,
                                                double maxFameValue = kLegacyMaxFameValue);

// Manual decay amount (legacy external manual fame decay): bounds clamp into
// [0, min(kLegacyMaxFameValue, maxFameValue)]; min >= max uses the fixed
// minimum; otherwise one uniform sample.  The caller applies
// newFame = max(0, previous - amount).
Result<int> RuleManualDecayAmount(int minDecay, int maxDecay, IRandom& rng,
                                  double maxFameValue = kLegacyMaxFameValue);

struct DecayRollResult {
    int amount{};     // points per owed period for this roll (>= 0).
    int tierIndex{};  // 0..4, veryLow..veryHigh.
    bool truncated{};
};

// Decay roll (legacy DecayFame tier): one uniform sample in [1, max(1,
// tierCap(previous))] times the multiplier truncates toward zero per owed
// period; a custom-location veryHigh roll uses the multiplier directly.  The
// caller floors the batch total at one point.
DecayRollResult RuleDecayRoll(double previousValue,
                              double multiplier,
                              IRandom& rng,
                              const Configuration& configuration,
                              bool customLocation = false);

DecayRollResult RuleDecayRoll(double previousValue,
                              double multiplier,
                              IRandom& rng,
                              const CompiledFameRules& rules,
                              bool customLocation = false);

}
