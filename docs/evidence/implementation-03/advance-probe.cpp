#include "slsf/core/reputation/Settlement.h"

#include "slsf/core/persistence/PersistentState.h"
#include "slsf/core/reputation/ReputationState.h"

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#ifndef PROBE_CURRENT
#error PROBE_CURRENT must be 0 for before-src or 1 for current source
#endif

namespace {

using Clock = std::chrono::steady_clock;
using slsf::AccountState;
using slsf::AdvanceInputs;
using slsf::Configuration;
using slsf::CustomRegionState;
using slsf::DiffusionGraph;
using slsf::EdgeDefinition;
using slsf::FameCategory;
using slsf::LegacyDefaultNode;
using slsf::Parameters;
using slsf::StableRegionKey;
using slsf::State;

constexpr std::array<std::string_view, 21> kDefaultNodes{{
    "whiterun", "winterhold", "windhelm", "solitude", "riften", "markarth",
    "morthal", "dawnstar", "falkreath", "raven_rock", "riverwood", "rorikstead",
    "ivarstead", "shors_stone", "dragon_bridge", "karthwasten", "skaal_village",
    "largashbur", "dushnikh_yal", "mor_khazgur", "narzulbur",
}};

struct MinRandom final : slsf::IRandom {
    std::uint64_t calls{};
    std::int32_t UniformInt(std::int32_t lo, std::int32_t) override {
        ++calls;
        return lo;
    }
};

struct CaseSpec {
    const char* name;
    std::uint32_t accounts;
    bool legacyDefault;
    bool custom;
    std::uint32_t noOpIterations;
    std::uint32_t activeIterations;
};

constexpr std::array<CaseSpec, 4> kCases{{
    {"default21x78", 21, true, false, 4000, 1000},
    {"uniform128x384", 128, false, false, 1000, 250},
    {"uniform512x1536", 512, false, false, 200, 50},
    {"uniform512x1536-custom", 512, false, true, 100, 20},
}};

constexpr double kStartDay = 1.0;
constexpr double kActiveStepDays = 0.00001;
constexpr std::uint32_t kRounds = 7;

StableRegionKey Key(std::uint32_t value) {
    return StableRegionKey{"advance-probe.esp", value};
}

std::vector<StableRegionKey> MakeKeys(std::uint32_t count) {
    std::vector<StableRegionKey> keys;
    keys.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        keys.push_back(Key(i + 1));
    }
    return keys;
}

DiffusionGraph MakeGraph(const CaseSpec& spec) {
    DiffusionGraph graph;
    if (spec.legacyDefault) {
        std::vector<LegacyDefaultNode> bindings;
        bindings.reserve(kDefaultNodes.size());
        for (std::size_t i = 0; i < kDefaultNodes.size(); ++i) {
            bindings.push_back(LegacyDefaultNode{kDefaultNodes[i], Key(static_cast<std::uint32_t>(i + 1))});
        }
        auto built = slsf::BuildLegacyDefaultGraph(bindings, 0.6);
        if (!built) {
            throw std::runtime_error("BuildLegacyDefaultGraph failed: " + built.error().message);
        }
        return std::move(*built);
    }

    graph.edges.reserve(static_cast<std::size_t>(spec.accounts) * 3);
    for (std::uint32_t source = 0; source < spec.accounts; ++source) {
        for (std::uint32_t distance = 1; distance <= 3; ++distance) {
            const auto target = (source + distance) % spec.accounts;
            graph.edges.push_back(EdgeDefinition{Key(source + 1), Key(target + 1), 0.6});
        }
    }
    return graph;
}

State MakeState(const CaseSpec& spec, const std::vector<StableRegionKey>& keys) {
    State state;
    state.accounts.resize(spec.accounts);
    state.accountKeys = keys;
    for (std::uint32_t index = 0; index < spec.accounts; ++index) {
        auto& account = state.accounts[index];
        for (std::size_t slot = 0; slot < slsf::kFameCategoryCount; ++slot) {
            account.local.values[slot] = static_cast<double>((index * 13 + slot * 7) % 150 + 1);
            account.remote.values[slot] = static_cast<double>((index * 17 + slot * 11) % 150 + 1);
            account.flags[slot] = static_cast<std::uint8_t>((index + slot) % 7 == 0);
        }
        account.settledAt = kStartDay;
        account.remoteSettledAt = kStartDay;
        slsf::RecomputeAccount(account);
    }
    if (spec.custom) {
        state.customRegions.reserve(spec.accounts);
        for (std::uint32_t index = 0; index < spec.accounts; ++index) {
            state.customRegions.push_back(CustomRegionState{"custom-" + std::to_string(index), keys[index]});
        }
    }
    slsf::RebuildDerivedSets(state);
    return state;
}

void HashByte(std::uint64_t& hash, std::uint8_t byte) {
    hash ^= byte;
    hash *= 1099511628211ull;
}

void HashU64(std::uint64_t& hash, std::uint64_t value) {
    for (unsigned shift = 0; shift < 64; shift += 8) {
        HashByte(hash, static_cast<std::uint8_t>(value >> shift));
    }
}

void HashDouble(std::uint64_t& hash, double value) {
    HashU64(hash, std::bit_cast<std::uint64_t>(value));
}

void HashString(std::uint64_t& hash, std::string_view value) {
    HashU64(hash, value.size());
    for (const unsigned char byte : value) {
        HashByte(hash, byte);
    }
}

std::uint64_t Checksum(const State& state, const slsf::AdvanceResult& result,
                       const MinRandom& rng, const AdvanceInputs& inputs) {
    std::uint64_t hash = 14695981039346656037ull;
    HashU64(hash, state.accounts.size());
    HashU64(hash, state.revision.value);
    HashU64(hash, state.epoch.session);
    HashU64(hash, state.epoch.index);
    HashU64(hash, state.epoch.scene);
    HashU64(hash, result.revision.value);
    HashU64(hash, static_cast<std::uint64_t>(result.status));
    HashDouble(hash, result.advancedTo.days);
    HashU64(hash, inputs.scheduledChanges.size());
    HashU64(hash, rng.calls);

    for (std::size_t index = 0; index < state.accounts.size(); ++index) {
        const auto& key = state.accountKeys[index];
        HashString(hash, key.plugin);
        HashU64(hash, key.localFormId);
        const AccountState& account = state.accounts[index];
        for (double value : account.local.values) HashDouble(hash, value);
        for (double value : account.remote.values) HashDouble(hash, value);
        for (double value : account.fame.values) HashDouble(hash, value);
        for (std::uint8_t value : account.flags) HashByte(hash, value);
        for (double value : account.protectionUntilDay) HashDouble(hash, value);
        HashDouble(hash, account.settledAt);
        HashDouble(hash, account.remoteSettledAt);
        HashByte(hash, account.anonymity);
        for (std::uint8_t value : account.auxiliaryState) HashByte(hash, value);
        HashU64(hash, account.positiveMask.Bits());
        HashU64(hash, account.localMask.Bits());
        HashU64(hash, account.flaggedMask.Bits());
    }
    for (const auto count : state.positiveRegionCounts) HashU64(hash, count);
    for (const auto count : state.localRegionCounts) HashU64(hash, count);
    for (const auto count : state.flaggedRegionCounts) HashU64(hash, count);
    HashU64(hash, state.derived.numericPositive.Bits());
    HashU64(hash, state.derived.numericLocal.Bits());
    HashU64(hash, state.derived.flagged.Bits());
    HashU64(hash, state.derived.providerActive.Bits());
    HashU64(hash, state.derived.relationshipForbidden.Bits());
    HashU64(hash, state.customRegions.size());
    for (const auto& custom : state.customRegions) {
        HashString(hash, custom.name);
        HashString(hash, custom.key.plugin);
        HashU64(hash, custom.key.localFormId);
    }
    return hash;
}

std::uint64_t Nanos(Clock::duration duration) {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count());
}

double NowFor(const char* workload, std::uint32_t callIndex) {
    if (std::string_view(workload) == "noop") {
        return kStartDay;
    }
    return kStartDay + static_cast<double>(callIndex) * kActiveStepDays;
}

struct RoundResult {
    std::uint64_t perAdvanceNs{};
    std::uint64_t prepareNs{};
    std::uint64_t warmNs{};
    std::uint64_t checksum{};
    std::uint64_t rngCalls{};
};

RoundResult RunRound(const CaseSpec& spec, const DiffusionGraph& graph,
                     const std::vector<StableRegionKey>& keys, const Parameters& parameters,
                     Configuration& configuration, const char* workload, bool preparedMode,
                     std::uint32_t iterations) {
    State state = MakeState(spec, keys);
    MinRandom rng;
    AdvanceInputs inputs;
    inputs.graph = &graph;
    inputs.parameters = &parameters;
    inputs.configuration = &configuration;
    inputs.rng = &rng;

    std::uint64_t prepareNs = 0;
#if PROBE_CURRENT
    if (preparedMode) {
        const auto prepareStart = Clock::now();
        auto prepared = slsf::PrepareSettlement(state, graph, configuration);
        prepareNs = Nanos(Clock::now() - prepareStart);
        if (!prepared) {
            throw std::runtime_error("PrepareSettlement failed: " + prepared.error().message);
        }
        inputs.prepared = *prepared;
        inputs.workspace = slsf::MakeSettlementWorkspace();
    }
#else
    if (preparedMode) {
        throw std::runtime_error("prepared mode is only available for current source");
    }
#endif

    slsf::AdvanceResult lastResult;
    const auto warmStart = Clock::now();
    auto warm = slsf::Advance(state, slsf::GameTime{NowFor(workload, 1)}, inputs);
    const std::uint64_t warmNs = Nanos(Clock::now() - warmStart);
    if (!warm) {
        throw std::runtime_error("warm-up Advance failed: " + warm.error().message);
    }
    lastResult = *warm;

    const auto measureStart = Clock::now();
    for (std::uint32_t index = 0; index < iterations; ++index) {
        const auto result = slsf::Advance(state,
            slsf::GameTime{NowFor(workload, index + 2)}, inputs);
        if (!result) {
            throw std::runtime_error("measured Advance failed: " + result.error().message);
        }
        lastResult = *result;
    }
    const std::uint64_t elapsedNs = Nanos(Clock::now() - measureStart);
    return RoundResult{elapsedNs / iterations, prepareNs, warmNs,
                       Checksum(state, lastResult, rng, inputs), rng.calls};
}

std::uint64_t Median(std::array<std::uint64_t, kRounds> values) {
    std::sort(values.begin(), values.end());
    return values[kRounds / 2];
}

void RunCase(const CaseSpec& spec, const char* workload, bool preparedMode) {
    const auto graph = MakeGraph(spec);
    const auto keys = MakeKeys(spec.accounts);
    const Parameters parameters{2.0, 1.0, 1.0, 0.6, 150.0};
    Configuration configuration;
    const auto iterations = std::string_view(workload) == "noop"
                                ? spec.noOpIterations : spec.activeIterations;
    std::array<std::uint64_t, kRounds> timings{};
    std::array<std::uint64_t, kRounds> prepareTimes{};
    std::array<std::uint64_t, kRounds> warmTimes{};
    std::uint64_t expectedChecksum = 0;
    std::uint64_t rngCalls = 0;
    for (std::uint32_t round = 0; round < kRounds; ++round) {
        const auto result = RunRound(spec, graph, keys, parameters, configuration,
                                     workload, preparedMode, iterations);
        timings[round] = result.perAdvanceNs;
        prepareTimes[round] = result.prepareNs;
        warmTimes[round] = result.warmNs;
        if (round == 0) {
            expectedChecksum = result.checksum;
            rngCalls = result.rngCalls;
        } else if (result.checksum != expectedChecksum || result.rngCalls != rngCalls) {
            throw std::runtime_error("round-to-round business checksum mismatch");
        }
    }

#if PROBE_CURRENT
    constexpr std::string_view build = "current";
#else
    constexpr std::string_view build = "before";
#endif
    std::cout << "RESULT build=" << build
              << " case=" << spec.name
              << " work=" << workload
              << " mode=" << (preparedMode ? "prepared" : "raw")
              << " rounds=" << kRounds
              << " iterations=" << iterations
              << " edges=" << graph.edges.size()
              << " median_ns=" << Median(timings)
              << " prepare_ns=" << Median(prepareTimes)
              << " warm_ns=" << Median(warmTimes)
              << " checksum=" << std::hex << std::setw(16) << std::setfill('0')
              << expectedChecksum << std::dec << std::setfill(' ')
              << " rng_calls=" << rngCalls << '\n';
}

} // namespace

int main(int argc, char** argv) {
    bool preparedMode = false;
    if (argc > 1) {
        if (std::string_view(argv[1]) == "--prepared") {
            preparedMode = true;
        } else if (std::string_view(argv[1]) != "--raw") {
            std::cerr << "usage: advance-probe [--raw|--prepared]\n";
            return 2;
        }
    }
#if !PROBE_CURRENT
    if (preparedMode) {
        std::cerr << "prepared mode is only available for current source\n";
        return 2;
    }
#endif

    try {
        for (const auto& spec : kCases) {
            RunCase(spec, "noop", preparedMode);
            RunCase(spec, "active", preparedMode);
        }
    } catch (const std::exception& error) {
        std::cerr << "advance-probe: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
