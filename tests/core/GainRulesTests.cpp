#include <catch2/catch_test_macros.hpp>

#include "slsf/core/Commands.h"
#include "slsf/core/Types.h"
#include "slsf/core/reputation/GainRules.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <initializer_list>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace {

class FixedSample final : public slsf::IRandom {
public:
    explicit FixedSample(int sample) : sample_(sample) {}
    std::int32_t UniformInt(std::int32_t lo, std::int32_t hi) override {
        lastLow = lo;
        lastHigh = hi;
        ++calls;
        if (sample_ < lo) {
            return lo;
        }
        if (sample_ > hi) {
            return hi;
        }
        return sample_;
    }

    int sample_{};
    int calls{};
    std::int32_t lastLow{};
    std::int32_t lastHigh{};
};

class SequenceSamples final : public slsf::IRandom {
public:
    explicit SequenceSamples(std::initializer_list<std::int32_t> samples) : samples_(samples) {}

    std::int32_t UniformInt(std::int32_t lo, std::int32_t hi) override {
        ranges.emplace_back(lo, hi);
        const auto sample = samples_[next_++ % samples_.size()];
        const auto result = std::clamp(sample, lo, hi);
        values.push_back(result);
        return result;
    }

    std::vector<std::pair<std::int32_t, std::int32_t>> ranges;
    std::vector<std::int32_t> values;

private:
    std::vector<std::int32_t> samples_;
    std::size_t next_{};
};

slsf::GainFameInputs GainInputs(double previous = 10.0) {
    slsf::GainFameInputs inputs;
    inputs.previousFame = previous;
    inputs.iterationMultiplier = 1.0;
    inputs.maxFameValue = 150.0;
    inputs.resolvedMultiplier = 1.0;
    inputs.nightStart = 22.0;
    inputs.nightEnd = 6.0;
    inputs.hourOfDay = 12.0;
    return inputs;
}

}

TEST_CASE("gain floors small products at one point and truncates toward zero", "[gain]") {
    const slsf::Configuration configuration;

    FixedSample three(3);
    auto inputs = GainInputs();
    inputs.resolvedMultiplier = 2.0;
    auto result = slsf::RuleGainFame(inputs, three, configuration, 0);
    REQUIRE(result.gained == 6);
    REQUIRE_FALSE(result.flooredToMinimum);
    REQUIRE_FALSE(result.halved);
    REQUIRE(three.calls == 1);
    REQUIRE(three.lastLow == 1);

    FixedSample two(2);
    inputs.resolvedMultiplier = 0.4;
    result = slsf::RuleGainFame(inputs, two, configuration, 0);
    REQUIRE(result.gained == 1);
    REQUIRE(result.flooredToMinimum);

    FixedSample gainThree(3);
    inputs.resolvedMultiplier = 2.0;
    inputs.iterationMultiplier = 1.5;
    result = slsf::RuleGainFame(inputs, gainThree, configuration, 0);
    REQUIRE(result.gained == 9);

    FixedSample capped(40);
    inputs.previousFame = 149.0;
    inputs.iterationMultiplier = 1.0;
    inputs.resolvedMultiplier = 1.0;
    result = slsf::RuleGainFame(inputs, capped, configuration, 0);
    REQUIRE(capped.lastLow == 1);
    REQUIRE(capped.lastHigh == 2);
    REQUIRE(result.gained == 2);
    REQUIRE(std::min(inputs.maxFameValue, inputs.previousFame + result.gained) == 150.0);
}

TEST_CASE("night reduction halves only for foreplay inside the window", "[gain]") {
    const slsf::Configuration configuration;
    auto inputs = GainInputs();
    inputs.nightReduction = true;
    inputs.resolvedMultiplier = 2.0;

    SECTION("daytime foreplay does not halve") {
        inputs.hourOfDay = 12.0;
        inputs.foreplay = slsf::Fact::Known(true);
        FixedSample die(4);
        const auto result = slsf::RuleGainFame(inputs, die, configuration, 0);
        REQUIRE_FALSE(result.halved);
        REQUIRE(result.gained == 8);
    }

    SECTION("night foreplay halves the multiplier before the sample") {
        inputs.hourOfDay = 23.0;
        inputs.foreplay = slsf::Fact::Known(true);
        FixedSample die(4);
        const auto result = slsf::RuleGainFame(inputs, die, configuration, 0);
        REQUIRE(result.halved);
        REQUIRE(result.gained == 4);
    }

    SECTION("unknown foreplay never halves") {
        inputs.hourOfDay = 23.5;
        inputs.foreplay = slsf::Fact{};
        FixedSample die(4);
        const auto result = slsf::RuleGainFame(inputs, die, configuration, 0);
        REQUIRE_FALSE(result.halved);
        REQUIRE(result.gained == 8);
    }

    SECTION("night needs the enabled flag") {
        inputs.hourOfDay = 3.0;
        inputs.foreplay = slsf::Fact::Known(true);
        inputs.nightReduction = false;
        FixedSample die(4);
        const auto result = slsf::RuleGainFame(inputs, die, configuration, 0);
        REQUIRE_FALSE(result.halved);
        REQUIRE(result.gained == 8);
    }
}

TEST_CASE("the night window uses strict legacy boundaries", "[gain]") {
    REQUIRE(slsf::LegacyNightWindow(23.5, 22.0, 6.0));
    REQUIRE(slsf::LegacyNightWindow(5.99, 22.0, 6.0));
    REQUIRE(slsf::LegacyNightWindow(2.0, 22.0, 6.0));
    REQUIRE_FALSE(slsf::LegacyNightWindow(22.0, 22.0, 6.0));
    REQUIRE_FALSE(slsf::LegacyNightWindow(6.0, 22.0, 6.0));
    REQUIRE_FALSE(slsf::LegacyNightWindow(12.0, 22.0, 6.0));

    REQUIRE_FALSE(slsf::LegacyNightWindow(12.0, 0.0, 6.0));
    REQUIRE(slsf::LegacyNightWindow(3.0, 0.0, 6.0));
    REQUIRE_FALSE(slsf::LegacyNightWindow(23.0, 0.0, 6.0));
    REQUIRE_FALSE(slsf::LegacyNightWindow(12.0, 22.0, 22.0));
    REQUIRE_FALSE(slsf::LegacyNightWindow(23.0, 22.0, 22.0));
}

TEST_CASE("non-finite multipliers never break the one-point gain floor", "[gain][safety]") {
    const slsf::Configuration configuration;
    FixedSample die(3);
    auto inputs = GainInputs();
    inputs.iterationMultiplier = std::numeric_limits<double>::quiet_NaN();
    auto result = slsf::RuleGainFame(inputs, die, configuration, 0);
    REQUIRE(result.gained == 3);
    REQUIRE_FALSE(result.flooredToMinimum);

    FixedSample infinite(3);
    inputs.iterationMultiplier = std::numeric_limits<double>::infinity();
    result = slsf::RuleGainFame(inputs, infinite, configuration, 0);
    REQUIRE(result.gained == 3);

    slsf::Configuration poisoned;
    poisoned.values[std::string(slsf::kGainConfigKeyMultiplier)] =
        std::numeric_limits<double>::quiet_NaN();
    FixedSample configured(4);
    inputs = GainInputs();
    result = slsf::RuleGainFame(inputs, configured, poisoned, 0);
    REQUIRE(result.gained == 4);
}

TEST_CASE("gain tiers follow the legacy fame boundaries", "[gain]") {
    REQUIRE(slsf::LegacyFameTier(0.0) == 0);
    REQUIRE(slsf::LegacyFameTier(24.9) == 0);
    REQUIRE(slsf::LegacyFameTier(25.0) == 1);
    REQUIRE(slsf::LegacyFameTier(49.9) == 1);
    REQUIRE(slsf::LegacyFameTier(50.0) == 2);
    REQUIRE(slsf::LegacyFameTier(74.9) == 2);
    REQUIRE(slsf::LegacyFameTier(75.0) == 3);
    REQUIRE(slsf::LegacyFameTier(99.9) == 3);
    REQUIRE(slsf::LegacyFameTier(100.0) == 4);
    REQUIRE(slsf::LegacyFameTier(150.0) == 4);
}

TEST_CASE("manual spread mirrors the legacy formula", "[gain][spread]") {
    auto result = slsf::RuleManualSpreadFame(100.0, 20.0, 30);
    REQUIRE(result.has_value());
    REQUIRE(result->newValue == 50.0);
    REQUIRE_FALSE(result->percentClamped);

    result = slsf::RuleManualSpreadFame(100.0, 20.0, 5);
    REQUIRE(result->newValue == 30.0);
    REQUIRE(result->percentClamped);
    result = slsf::RuleManualSpreadFame(150.0, 140.0, 150);
    REQUIRE(result->newValue == 150.0);
    REQUIRE(result->percentClamped);

    result = slsf::RuleManualSpreadFame(99.0, 20.0, 30);
    REQUIRE(result->newValue == 49.0);
    result = slsf::RuleManualSpreadFame(150.0, 150.0, 100);
    REQUIRE(result->newValue == 150.0);

    auto rejected = slsf::RuleManualSpreadFame(9.5, 20.0, 30);
    REQUIRE_FALSE(rejected);
}

TEST_CASE("manual decay amounts clamp into the legacy range", "[gain][decay]") {
    FixedSample dieFixed(5);
    auto result = slsf::RuleManualDecayAmount(37, 37, dieFixed);
    REQUIRE(result.has_value());
    REQUIRE(*result == 37);

    auto zero = slsf::RuleManualDecayAmount(0, 0, dieFixed);
    REQUIRE(zero.has_value());
    REQUIRE(*zero == 0);

    FixedSample skip(5);
    auto minimum = slsf::RuleManualDecayAmount(80, 20, skip);
    REQUIRE(minimum.has_value());
    REQUIRE(*minimum == 80);
    REQUIRE(skip.calls == 0);

    FixedSample normal(42);
    auto sample = slsf::RuleManualDecayAmount(-5, 900, normal);
    REQUIRE(sample.has_value());
    REQUIRE(*sample == 42);
    REQUIRE(normal.lastLow == 0);
    REQUIRE(normal.lastHigh == 150);
}

TEST_CASE("decay rolls follow the legacy tier caps", "[gain][decay]") {
    const slsf::Configuration configuration;

    FixedSample die(1);
    auto roll = slsf::RuleDecayRoll(120.0, 1.0, die, configuration);
    REQUIRE(roll.amount == 1);
    REQUIRE(roll.tierIndex == 4);
    REQUIRE_FALSE(roll.truncated);
    REQUIRE(die.lastLow == 1);
    REQUIRE(die.lastHigh == 1);

    FixedSample high(4);
    roll = slsf::RuleDecayRoll(10.0, 1.5, high, configuration);
    REQUIRE(roll.amount == 6);
    REQUIRE(roll.tierIndex == 0);
    REQUIRE(high.lastHigh == 5);

    FixedSample tiny(2);
    roll = slsf::RuleDecayRoll(10.0, 0.4, tiny, configuration);
    REQUIRE(roll.amount == 0);
    REQUIRE(roll.truncated);
}

TEST_CASE("fame configuration compiles into typed rule data", "[gain][decay][configuration]") {
    slsf::Configuration configuration;
    configuration.values[std::string(slsf::kGainConfigKeyUseGlobal)] = 0.0;
    configuration.values[std::string(slsf::kGainConfigKeyCategoryMultiplierPrefix) + "0"] = 2.5;
    configuration.values[std::string(slsf::kGainConfigKeyCategoryMultiplierPrefix) + "7"] =
        std::numeric_limits<double>::quiet_NaN();
    configuration.values[std::string(slsf::kGainConfigKeyCategoryMultiplierPrefix) + "24"] = 0.25;
    configuration.values[std::string(slsf::kGainConfigKeyGainVeryLow)] = 3.75;
    configuration.values[std::string(slsf::kGainConfigKeyGainVeryHigh)] = -2.0;
    configuration.values[std::string(slsf::kGainConfigKeyDecayVeryLow)] = 2.5;
    configuration.values[std::string(slsf::kGainConfigKeyDecayVeryHigh)] = 0.0;
    configuration.values[std::string(slsf::kGainConfigKeyNightReduction)] = 0.0;
    configuration.values[std::string(slsf::kGainConfigKeyNightStart)] = 19.0;
    configuration.values[std::string(slsf::kGainConfigKeyNightEnd)] = 5.0;
    configuration.values[std::string(slsf::kGainConfigKeyDecayTimeNeeded)] = 9.75;

    const auto rules = slsf::CompileFameRules(configuration);
    REQUIRE(rules.categoryMultipliers[0] == 2.5);
    REQUIRE(rules.categoryMultipliers[1] == 1.0);
    REQUIRE(rules.categoryMultipliers[7] == 1.0);
    REQUIRE(rules.categoryMultipliers[24] == 0.25);
    REQUIRE(rules.gainTierCaps[0] == 3);
    REQUIRE(rules.gainTierCaps[4] == -2);
    REQUIRE(rules.decayTierCaps[0] == 2);
    REQUIRE(rules.decayTierCaps[4] == 0);
    REQUIRE_FALSE(rules.nightReduction);
    REQUIRE(rules.nightStart == 19.0);
    REQUIRE(rules.nightEnd == 5.0);
    REQUIRE(rules.decayProtectionDays == 0.1875);

    slsf::Configuration globalConfiguration;
    globalConfiguration.values[std::string(slsf::kGainConfigKeyMultiplier)] = 1.75;
    const auto globalRules = slsf::CompileFameRules(globalConfiguration);
    REQUIRE(std::all_of(globalRules.categoryMultipliers.begin(),
                        globalRules.categoryMultipliers.end(),
                        [](double multiplier) { return multiplier == 1.75; }));
}

TEST_CASE("typed fame rules match configuration rules and RNG ranges", "[gain][decay][configuration]") {
    slsf::Configuration configuration;
    configuration.values[std::string(slsf::kGainConfigKeyUseGlobal)] = 0.0;
    for (std::uint32_t categoryIndex = 0; categoryIndex < slsf::kFameCategoryCount;
         ++categoryIndex) {
        configuration.values[std::string(slsf::kGainConfigKeyCategoryMultiplierPrefix) +
                             std::to_string(categoryIndex)] =
            0.25 + static_cast<double>(categoryIndex) / 8.0;
    }
    configuration.values[std::string(slsf::kGainConfigKeyGainVeryLow)] = 2.0;
    configuration.values[std::string(slsf::kGainConfigKeyGainLow)] = 3.0;
    configuration.values[std::string(slsf::kGainConfigKeyGainMedium)] = 4.0;
    configuration.values[std::string(slsf::kGainConfigKeyGainHigh)] = 5.0;
    configuration.values[std::string(slsf::kGainConfigKeyGainVeryHigh)] = 6.0;
    configuration.values[std::string(slsf::kGainConfigKeyDecayVeryLow)] = 1.0;
    configuration.values[std::string(slsf::kGainConfigKeyDecayLow)] = 2.0;
    configuration.values[std::string(slsf::kGainConfigKeyDecayMedium)] = 3.0;
    configuration.values[std::string(slsf::kGainConfigKeyDecayHigh)] = 4.0;
    configuration.values[std::string(slsf::kGainConfigKeyDecayVeryHigh)] = 5.0;
    const auto rules = slsf::CompileFameRules(configuration);

    SequenceSamples configurationRng{1, 3, 7, 2, 99};
    SequenceSamples typedRng{1, 3, 7, 2, 99};
    for (std::uint32_t categoryIndex = 0; categoryIndex < slsf::kFameCategoryCount;
         ++categoryIndex) {
        auto inputs = GainInputs(static_cast<double>((categoryIndex % 5) * 25));
        inputs.resolvedMultiplier.reset();
        inputs.nightReduction = rules.nightReduction;
        inputs.nightStart = rules.nightStart;
        inputs.nightEnd = rules.nightEnd;
        inputs.hourOfDay = 23.0;
        inputs.foreplay = slsf::Fact::Known(true);

        const auto configured =
            slsf::RuleGainFame(inputs, configurationRng, configuration, categoryIndex);
        const auto typed = slsf::RuleGainFame(inputs, typedRng, rules, categoryIndex);
        CHECK(typed.gained == configured.gained);
        CHECK(typed.halved == configured.halved);
        CHECK(typed.flooredToMinimum == configured.flooredToMinimum);
    }
    REQUIRE(typedRng.ranges == configurationRng.ranges);
    REQUIRE(typedRng.values == configurationRng.values);
    REQUIRE(typedRng.ranges.size() == slsf::kFameCategoryCount);

    constexpr std::array<double, 5> kTierValues = {0.0, 25.0, 50.0, 75.0, 100.0};
    for (std::size_t tierIndex = 0; tierIndex < kTierValues.size(); ++tierIndex) {
        for (const bool customLocation : {false, true}) {
            SequenceSamples configurationDecayRng{1, 3, 7};
            SequenceSamples typedDecayRng{1, 3, 7};
            const auto configured = slsf::RuleDecayRoll(
                kTierValues[tierIndex], 1.25, configurationDecayRng, configuration,
                customLocation);
            const auto typed = slsf::RuleDecayRoll(kTierValues[tierIndex], 1.25,
                                                   typedDecayRng, rules, customLocation);
            CHECK(typed.amount == configured.amount);
            CHECK(typed.tierIndex == configured.tierIndex);
            CHECK(typed.truncated == configured.truncated);
            CHECK(typedDecayRng.ranges == configurationDecayRng.ranges);
            CHECK(typedDecayRng.values == configurationDecayRng.values);
        }
    }
}
