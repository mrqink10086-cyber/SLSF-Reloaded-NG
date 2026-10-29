#include "slsf/core/reputation/GainRules.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

namespace slsf {
namespace {

int Saturate(double value) noexcept {
    if (std::isnan(value)) {
        return 0;
    }
    if (value >= 2147483647.0) {
        return 2147483647;
    }
    if (value <= -2147483648.0) {
        return -2147483648;
    }
    return static_cast<int>(value);
}

double ValidMultiplier(double value) noexcept {
    return std::isfinite(value) && value >= 0.0 ? value : 1.0;
}

constexpr std::array<std::string_view, 5> kGainCapKeys = {
    kGainConfigKeyGainVeryLow,
    kGainConfigKeyGainLow,
    kGainConfigKeyGainMedium,
    kGainConfigKeyGainHigh,
    kGainConfigKeyGainVeryHigh,
};
constexpr std::array<double, 5> kGainCapDefaults = {10.0, 8.0, 6.0, 4.0, 2.0};
constexpr std::array<std::string_view, 5> kDecayCapKeys = {
    kGainConfigKeyDecayVeryLow,
    kGainConfigKeyDecayLow,
    kGainConfigKeyDecayMedium,
    kGainConfigKeyDecayHigh,
    kGainConfigKeyDecayVeryHigh,
};
constexpr std::array<double, 5> kDecayCapDefaults = {5.0, 4.0, 3.0, 2.0, 1.0};
constexpr std::array<int, 5> kCustomDecayCaps = {5, 4, 3, 2, 1};

int ReadTierCap(const Configuration& configuration, int tierIndex,
                const std::array<std::string_view, 5>& keys,
                const std::array<double, 5>& defaults) {
    const auto index = static_cast<std::size_t>(tierIndex);
    return Saturate(configuration.Get(keys[index], defaults[index]));
}

GainFameResult CalculateGainFame(const GainFameInputs& inputs, double multiplier,
                                 int sample) noexcept {
    const double iterationMultiplier =
        std::isfinite(inputs.iterationMultiplier) && inputs.iterationMultiplier >= 0.0
            ? inputs.iterationMultiplier
            : 1.0;

    const bool night = LegacyNightWindow(inputs.hourOfDay, inputs.nightStart, inputs.nightEnd);
    const bool foreplay = inputs.foreplay.state == FactState::KnownTrue;
    const bool halved = inputs.nightReduction && night && foreplay;
    const double effectiveMultiplier = halved ? multiplier / 2.0 : multiplier;
    const double scaled = static_cast<double>(Saturate(static_cast<double>(sample)
                                                       * effectiveMultiplier))
                          * iterationMultiplier;
    const bool flooredToMinimum = scaled < 1.0;
    const double effectiveGain = flooredToMinimum ? 1.0 : scaled;
    const int points = std::max(1, Saturate(effectiveGain));
    return GainFameResult{points, halved, flooredToMinimum};
}

GainFameResult RollGainFame(const GainFameInputs& inputs, IRandom& rng, double multiplier,
                            int tierCap) {
    const int sample = rng.UniformInt(1, std::max(1, tierCap));
    return CalculateGainFame(inputs, multiplier, sample);
}

DecayRollResult CalculateDecayRoll(int tierIndex, double multiplier, int sample,
                                   bool directMultiplier) noexcept {
    const int amount = directMultiplier
                           ? std::max(0, Saturate(multiplier))
                           : Saturate(static_cast<double>(sample) * multiplier);
    return DecayRollResult{std::max(0, amount), tierIndex, amount < 1};
}

DecayRollResult RollDecayFame(int tierIndex, double multiplier, IRandom& rng, int tierCap,
                              bool customLocation) {
    const bool directMultiplier = customLocation && tierIndex == 4;
    if (directMultiplier) {
        return CalculateDecayRoll(tierIndex, multiplier, 0, true);
    }

    const int sample = rng.UniformInt(1, std::max(1, tierCap));
    return CalculateDecayRoll(tierIndex, multiplier, sample, false);
}

}

double LegacyFameMultiplier(const Configuration& configuration, std::uint32_t categoryIndex) {
    const auto useGlobal = configuration.Get(kGainConfigKeyUseGlobal, 1.0);
    const double resolved = [&] {
        if (useGlobal != 0.0) {
            return configuration.Get(kGainConfigKeyMultiplier, 1.0);
        }
        // The legacy MCM keeps one multiplier slot per fame category in the
        // same order as the FameType array.
        std::string key(kGainConfigKeyCategoryMultiplierPrefix);
        key += std::to_string(static_cast<unsigned>(categoryIndex));
        return configuration.Get(key, 1.0);
    }();
    return ValidMultiplier(resolved);
}

CompiledFameRules CompileFameRules(const Configuration& configuration) {
    CompiledFameRules rules;
    if (configuration.Get(kGainConfigKeyUseGlobal, 1.0) != 0.0) {
        rules.categoryMultipliers.fill(
            ValidMultiplier(configuration.Get(kGainConfigKeyMultiplier, 1.0)));
    } else {
        for (std::uint32_t categoryIndex = 0; categoryIndex < kFameCategoryCount;
             ++categoryIndex) {
            std::string key(kGainConfigKeyCategoryMultiplierPrefix);
            key += std::to_string(static_cast<unsigned>(categoryIndex));
            rules.categoryMultipliers[categoryIndex] =
                ValidMultiplier(configuration.Get(key, 1.0));
        }
    }

    for (std::size_t tierIndex = 0; tierIndex < rules.gainTierCaps.size(); ++tierIndex) {
        rules.gainTierCaps[tierIndex] = ReadTierCap(
            configuration, static_cast<int>(tierIndex), kGainCapKeys, kGainCapDefaults);
        rules.decayTierCaps[tierIndex] = ReadTierCap(
            configuration, static_cast<int>(tierIndex), kDecayCapKeys, kDecayCapDefaults);
    }

    rules.nightReduction = configuration.Get(kGainConfigKeyNightReduction, 1.0) != 0.0;
    rules.nightStart = configuration.Get(kGainConfigKeyNightStart, kLegacyDefaultNightStart);
    rules.nightEnd = configuration.Get(kGainConfigKeyNightEnd, kLegacyDefaultNightEnd);

    constexpr double kHalfHourAsDays = 0.5 / 24.0;
    const double batches = configuration.Get(kGainConfigKeyDecayTimeNeeded, 24.0);
    rules.decayProtectionDays =
        std::isfinite(batches) && batches >= 0.0 ? std::floor(batches) * kHalfHourAsDays : 0.0;
    return rules;
}

bool LegacyNightWindow(double hourOfDay, double nightStart, double nightEnd) noexcept {
    if (nightStart == nightEnd) {
        return false;
    }
    if (nightStart < nightEnd) {
        return hourOfDay > nightStart && hourOfDay < nightEnd;
    }
    return hourOfDay > nightStart || hourOfDay < nightEnd;
}

int LegacyFameTier(double previousValue) noexcept {
    for (int index = 0; index < 4; ++index) {
        if (previousValue >= kLegacyGainTierBoundaries[index]) {
            return 4 - index;
        }
    }
    return 0;
}

GainFameResult RuleGainFame(const GainFameInputs& inputs,
                            IRandom& rng,
                            const Configuration& configuration,
                            std::uint32_t categoryIndex) {
    const double multiplier = inputs.resolvedMultiplier
                                  ? *inputs.resolvedMultiplier
                                  : LegacyFameMultiplier(configuration, categoryIndex);
    const int tierIndex = LegacyFameTier(inputs.previousFame);
    const int tierCap = ReadTierCap(configuration, tierIndex, kGainCapKeys, kGainCapDefaults);
    return RollGainFame(inputs, rng, multiplier, tierCap);
}

GainFameResult RuleGainFame(const GainFameInputs& inputs,
                            IRandom& rng,
                            const CompiledFameRules& rules,
                            std::uint32_t categoryIndex) {
    const double multiplier = inputs.resolvedMultiplier
                                  ? *inputs.resolvedMultiplier
                                  : categoryIndex < rules.categoryMultipliers.size()
                                        ? rules.categoryMultipliers[categoryIndex]
                                        : 1.0;
    const int tierIndex = LegacyFameTier(inputs.previousFame);
    return RollGainFame(inputs, rng, multiplier,
                        rules.gainTierCaps[static_cast<std::size_t>(tierIndex)]);
}

Result<ManualSpreadResult> RuleManualSpreadFame(double sourceValue,
                                                double targetValue,
                                                int percent,
                                                double maxFameValue) {
    if (!std::isfinite(sourceValue) || !std::isfinite(targetValue) ||
        !std::isfinite(maxFameValue) || maxFameValue < 0.0) {
        return Fail(Error::Code::InvalidArgument, "spread inputs are not finite");
    }
    if (sourceValue < kLegacyManualSpreadMinSource) {
        return Fail(Error::Code::InvalidArgument, "manual spread requires a source of at least "
                                         + std::to_string(static_cast<int>(kLegacyManualSpreadMinSource)));
    }
    const double doublePercent = static_cast<double>(percent);
    if (!std::isfinite(doublePercent)) {
        return Fail(Error::Code::InvalidArgument, "spread inputs are not finite");
    }

    ManualSpreadResult result;
    result.percentClamped =
        doublePercent < kLegacyManualSpreadMinPercent || doublePercent > kLegacyManualSpreadMaxPercent;
    const double clampedPercent = std::min(kLegacyManualSpreadMaxPercent,
                                           std::max(kLegacyManualSpreadMinPercent, doublePercent));

    const double contribution = sourceValue * (clampedPercent / 100.0);
    const double sum = std::max(0.0, targetValue + contribution);
    result.newValue = std::min(maxFameValue, std::floor(sum));
    return result;
}

Result<int> RuleManualDecayAmount(int minDecay, int maxDecay, IRandom& rng, double maxFameValue) {
    if (!std::isfinite(maxFameValue) || maxFameValue < 0.0) {
        return Fail(Error::Code::InvalidArgument, "decay bounds are not finite");
    }
    const int cap = Saturate(std::min(kLegacyMaxFameValue, maxFameValue));
    if (minDecay < 0) {
        minDecay = 0;
    } else if (minDecay > cap) {
        minDecay = cap;
    }
    if (maxDecay < 0) {
        maxDecay = 0;
    } else if (maxDecay > cap) {
        maxDecay = cap;
    }
    if (minDecay == 0 && maxDecay == 0) {
        return 0;
    }
    if (minDecay >= maxDecay) {
        return minDecay;
    }
    return rng.UniformInt(minDecay, maxDecay);
}

DecayRollResult RuleDecayRoll(double previousValue,
                              double multiplier,
                              IRandom& rng,
                              const Configuration& configuration,
                              bool customLocation) {
    const int tierIndex = LegacyFameTier(previousValue);
    const int tierCap = customLocation
                            ? kCustomDecayCaps[static_cast<std::size_t>(tierIndex)]
                            : ReadTierCap(configuration, tierIndex, kDecayCapKeys,
                                          kDecayCapDefaults);
    return RollDecayFame(tierIndex, multiplier, rng, tierCap, customLocation);
}

DecayRollResult RuleDecayRoll(double previousValue,
                              double multiplier,
                              IRandom& rng,
                              const CompiledFameRules& rules,
                              bool customLocation) {
    const int tierIndex = LegacyFameTier(previousValue);
    const int tierCap = customLocation
                            ? kCustomDecayCaps[static_cast<std::size_t>(tierIndex)]
                            : rules.decayTierCaps[static_cast<std::size_t>(tierIndex)];
    return RollDecayFame(tierIndex, multiplier, rng, tierCap, customLocation);
}

}
