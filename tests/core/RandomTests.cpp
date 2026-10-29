
#include <catch2/catch_test_macros.hpp>

#include "slsf/core/Commands.h"
#include "slsf/core/NumericRow.h"
#include "slsf/core/Types.h"
#include "slsf/core/reputation/DiffusionGraph.h"
#include "slsf/core/reputation/DiffusionKernel.h"
#include "slsf/core/reputation/GainRules.h"
#include "slsf/core/reputation/ReputationState.h"
#include "slsf/core/reputation/Settlement.h"

#include "../reference/DiffusionReference.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

using slsf::AdvanceInputs;
using slsf::CompiledDiffusionGraph;
using slsf::Configuration;
using slsf::DiffusionGraph;
using slsf::FameCategory;
using slsf::GameTime;
using slsf::IRandom;
using slsf::NumericRow;
using slsf::Parameters;
using slsf::RegionId;
using slsf::StableRegionKey;
using slsf::State;

constexpr std::uint64_t kBaseSeed = 1170u;

std::size_t ReferenceTier(double fame) {
    if (fame >= 100.0) return 4;
    if (fame >= 75.0) return 3;
    if (fame >= 50.0) return 2;
    if (fame >= 25.0) return 1;
    return 0;
}

int LegacyGainTierCap(std::size_t tierIndex) {
    static constexpr int kCaps[5] = {10, 8, 6, 4, 2};
    return kCaps[tierIndex < 5 ? tierIndex : 4];
}


class Rng {
public:
    explicit Rng(std::uint64_t seed) : state_(seed == 0 ? 0x9E3779B97F4A7C15ULL : seed) {}

    static Rng ForCase(std::size_t caseIndex) { return Rng{kBaseSeed + caseIndex * 7919u}; }

    std::uint64_t Next() {
        std::uint64_t z = (state_ += 0x9E3779B97F4A7C15ULL);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }

    int Int(int low, int high) {
        if (high < low) {
            std::swap(low, high);
        }
        const std::uint64_t span = static_cast<std::uint64_t>(high - low) + 1;
        return low + static_cast<int>(Next() % span);
    }

    double Fraction() { return static_cast<double>(Next() >> 11) * (1.0 / 9007199254740992.0); }

    double Range(double low, double high) { return low + (high - low) * Fraction(); }

    bool Bool() { return (Next() & 1u) != 0u; }

    double Retention() {
        static constexpr double kValues[6] = {0.0, 0.25, 0.5, 0.6, 0.75, 1.0};
        return kValues[static_cast<std::size_t>(Int(0, 5))];
    }

private:
    std::uint64_t state_;
};

class SharedDie final : public IRandom {
public:
    explicit SharedDie(diffref::IDie& die) : die_(&die) {}

    std::int32_t UniformInt(std::int32_t low, std::int32_t high) override {
        return die_->UniformInt(low, high);
    }

private:
    diffref::IDie* die_;
};

class LowestDie final : public diffref::IDie {
public:
    int UniformInt(int low, int high) override {
        static_cast<void>(high);
        return low;
    }
};


bool Close(double a, double b, double tolerance = 1e-9) {
    return std::fabs(a - b) <= tolerance;
}

std::string DescribeRow(const diffref::Row& row) {
    std::ostringstream out;
    out << std::setprecision(6) << std::fixed;
    out << '[' << row[0];
    for (std::size_t slot = 1; slot < diffref::kCategories; ++slot) {
        out << ',' << row[slot];
    }
    out << ']';
    return out.str();
}

std::string DescribeRow(const NumericRow& row) {
    diffref::Row converted{};
    for (std::size_t slot = 0; slot < diffref::kSlots; ++slot) {
        converted[slot] = row[slot];
    }
    return DescribeRow(converted);
}

std::string DescribeEdges(const DiffusionGraph& graph) {
    std::ostringstream out;
    for (std::size_t index = 0; index < graph.edges.size(); ++index) {
        const auto& edge = graph.edges[index];
        if (index != 0) {
            out << ' ';
        }
        out << '(' << edge.from.localFormId << "->" << edge.to.localFormId << ':'
            << std::setprecision(3) << std::defaultfloat << edge.retention << ')';
    }
    return out.str();
}


State MakeState(const std::vector<StableRegionKey>& accountKeys,
                const std::vector<diffref::Row>& local,
                const std::vector<diffref::Row>& remote,
                const std::vector<diffref::Row>& protection,
                const std::vector<double>& settled,
                const std::vector<double>& remoteSettled) {
    State state;
    const auto count = accountKeys.size();
    state.accounts.resize(count);
    state.accountKeys = accountKeys;
    state.locationAccounts.resize(count);
    for (std::size_t index = 0; index < count; ++index) {
        state.locationAccounts[index] = RegionId(static_cast<std::uint32_t>(index));
        for (std::size_t slot = 0; slot < diffref::kSlots; ++slot) {
            state.accounts[index].local[slot] = local[index][slot];
            state.accounts[index].remote[slot] = remote[index][slot];
            state.accounts[index].fame[slot] =
                std::max(local[index][slot], remote[index][slot]);
            state.accounts[index].protectionUntilDay[slot] = protection[index][slot];
        }
        state.accounts[index].settledAt = settled[index];
        state.accounts[index].remoteSettledAt = remoteSettled[index];
        slsf::RecomputeAccount(state.accounts[index]);
    }
    for (std::size_t index = 0; index < count; ++index) {
        const auto& account = state.accounts[index];
        state.derived.numericPositive |= account.positiveMask;
        state.derived.numericLocal |= account.localMask;
        state.derived.flagged |= account.flaggedMask;
        account.positiveMask.ForEach([&](FameCategory category) {
            ++state.positiveRegionCounts[static_cast<std::size_t>(category)];
        });
        account.localMask.ForEach([&](FameCategory category) {
            ++state.localRegionCounts[static_cast<std::size_t>(category)];
        });
        account.flaggedMask.ForEach([&](FameCategory category) {
            ++state.flaggedRegionCounts[static_cast<std::size_t>(category)];
        });
    }
    return state;
}

diffref::ReferenceGraph ToReferenceGraph(const DiffusionGraph& graph,
                                         const std::vector<StableRegionKey>& keys) {
    diffref::ReferenceGraph referenceGraph;
    referenceGraph.accountCount = static_cast<std::uint32_t>(keys.size());
    std::vector<std::vector<diffref::Edge>> rows(keys.size());
    for (const auto& edge : graph.edges) {
        const auto source = std::lower_bound(keys.begin(), keys.end(), edge.from);
        const auto target = std::lower_bound(keys.begin(), keys.end(), edge.to);
        if (source == keys.end() || *source != edge.from ||
            target == keys.end() || *target != edge.to) {
            throw std::invalid_argument("reference edge references an unknown key");
        }
        if (!(edge.retention > 0.0) || edge.retention > 1.0) {
            continue;
        }
        const std::uint32_t sourceIndex =
            static_cast<std::uint32_t>(std::distance(keys.begin(), source));
        const std::uint32_t targetIndex =
            static_cast<std::uint32_t>(std::distance(keys.begin(), target));
        rows[targetIndex].push_back(
            diffref::Edge{sourceIndex, targetIndex, edge.retention});
    }
    referenceGraph.offsets.push_back(0);
    for (auto& row : rows) {
        std::sort(row.begin(), row.end(),
                  [](const diffref::Edge& a, const diffref::Edge& b) {
                      return a.source < b.source;
                  });
        referenceGraph.operands.insert(referenceGraph.operands.end(), row.begin(), row.end());
        referenceGraph.offsets.push_back(
            static_cast<std::uint32_t>(referenceGraph.operands.size()));
    }
    return referenceGraph;
}


struct RandomCaseSetup {
    DiffusionGraph graph{};
    std::vector<StableRegionKey> accountKeys;
    std::vector<diffref::Row> local;
    std::vector<diffref::Row> remote;
    std::vector<diffref::Row> protection;
    std::vector<double> settled;
    std::vector<double> remoteSettled;
    std::vector<diffref::ScheduledChange> scheduled;
    double now{0.0};
    Parameters parameters{};
    std::string describe;
};

RandomCaseSetup MakeRandomCase(Rng& rng, std::size_t accountCount) {
    RandomCaseSetup setup;
    setup.accountKeys.reserve(accountCount);
    for (std::size_t index = 0; index < accountCount; ++index) {
        setup.accountKeys.push_back(
            StableRegionKey{"random.esp", static_cast<std::uint32_t>(index + 1)});
    }

    DiffusionGraph graph;
    const std::size_t edgeCount = static_cast<std::size_t>(rng.Int(1, static_cast<int>(accountCount * 3)));
    for (int attempt = 0;
         attempt < static_cast<int>(edgeCount * 4) && graph.edges.size() < edgeCount; ++attempt) {
        std::uint32_t from = static_cast<std::uint32_t>(rng.Int(0, static_cast<int>(accountCount) - 1));
        std::uint32_t to = static_cast<std::uint32_t>(rng.Int(0, static_cast<int>(accountCount) - 1));
        if (from == to) {
            continue;
        }
        if (from > to) {
            std::swap(from, to);
        }
        const bool duplicate = std::any_of(graph.edges.begin(), graph.edges.end(),
                                           [&](const slsf::EdgeDefinition& edge) {
                                               return edge.from.localFormId == from + 1 &&
                                                      edge.to.localFormId == to + 1;
                                           });
        if (duplicate) {
            continue;
        }
        graph.edges.push_back(slsf::EdgeDefinition{setup.accountKeys[from], setup.accountKeys[to],
                                                   rng.Retention()});
    }
    setup.graph = graph;

    setup.local.assign(accountCount, diffref::Row{});
    setup.remote.assign(accountCount, diffref::Row{});
    setup.protection.assign(accountCount, diffref::Row{});
    setup.settled.assign(accountCount, 0.0);
    setup.remoteSettled.assign(accountCount, 0.0);

    for (std::size_t index = 0; index < accountCount; ++index) {
        for (std::size_t slot = 0; slot < diffref::kCategories; ++slot) {
            const int kind = rng.Int(0, 5);
            double value = 0.0;
            switch (kind) {
            case 0:
                value = 0.0;
                break;
            case 1:
                value = static_cast<double>(rng.Int(0, 25));
                break;
            case 2:
                value = static_cast<double>(rng.Int(26, 60));
                break;
            case 3:
                value = static_cast<double>(rng.Int(61, 149));
                break;
            case 4:
                value = 150.0;
                break;
            default:
                value = rng.Range(0.0, 150.0);
                break;
            }
            setup.local[index][slot] = value;
            if (rng.Bool()) {
                setup.remote[index][slot] = static_cast<double>(rng.Int(0, 150));
            }
            if (rng.Int(0, 2) == 0) {
                setup.protection[index][slot] = rng.Range(0.0, 12.0);
            }
        }
    }

    setup.now = static_cast<double>(rng.Int(20, 80)) + rng.Range(0.0, 0.5);
    for (std::size_t index = 0; index < accountCount; ++index) {
        setup.settled[index] = std::max(0.0, setup.now - static_cast<double>(rng.Int(0, 40)));
        if (rng.Bool()) {
            setup.remoteSettled[index] = setup.settled[index];
        }
        if (rng.Int(0, 6) == 0) {
            setup.settled[index] = setup.now + rng.Range(0.0, 4.0);
            if (rng.Bool()) {
                setup.remoteSettled[index] = setup.settled[index];
            }
        }
    }

    const int scheduledCount = rng.Int(0, 4);
    for (int change = 0; change < scheduledCount; ++change) {
        std::uint32_t account = static_cast<std::uint32_t>(rng.Int(0, static_cast<int>(accountCount) - 1));
        const std::size_t category =
            static_cast<std::size_t>(rng.Int(0, static_cast<int>(diffref::kCategories) - 1));
        const bool duplicate = std::any_of(
            setup.scheduled.begin(), setup.scheduled.end(), [&](const diffref::ScheduledChange& entry) {
                return entry.account == account && entry.category == category;
            });
        if (duplicate) {
            continue;
        }
        const double value = static_cast<double>(rng.Int(0, 150));
        const double at = rng.Bool() ? setup.now - rng.Range(0.0, 8.0)
                                     : setup.now + rng.Range(0.1, 5.0);
        setup.scheduled.push_back(diffref::ScheduledChange{
            at, diffref::ScheduledKind::Set, account, category, value});
    }
    std::sort(setup.scheduled.begin(), setup.scheduled.end(),
              [](const diffref::ScheduledChange& a, const diffref::ScheduledChange& b) {
                  return a.at < b.at;
              });

    setup.parameters.decayPerDay = rng.Range(0.0, 4.0);
    setup.parameters.relaxationPerDay = rng.Range(0.0, 4.0);
    setup.parameters.spreadThreshold = static_cast<double>(rng.Int(0, 60));
    setup.parameters.spreadRetention = 0.6;
    setup.parameters.maxFame = 150.0;

    std::ostringstream describeStream;
    describeStream << "accounts=" << accountCount << " edges=" << DescribeEdges(setup.graph)
                   << " now=" << setup.now
                   << " decayPerDay=" << setup.parameters.decayPerDay
                   << " relaxationPerDay=" << setup.parameters.relaxationPerDay
                   << " spreadThreshold=" << setup.parameters.spreadThreshold
                   << " scheduled=" << setup.scheduled.size();
    setup.describe = describeStream.str();
    return setup;
}

AdvanceInputs MakeAdvanceInputs(const RandomCaseSetup& setup, IRandom& rng) {
    AdvanceInputs inputs;
    inputs.graph = &setup.graph;
    inputs.parameters = &setup.parameters;
    inputs.rng = &rng;
    for (const auto& change : setup.scheduled) {
        slsf::ScheduledChange converted;
        converted.at = GameTime{change.at};
        converted.command.kind = slsf::ExplicitCommandKind::Set;
        converted.command.target = RegionId{change.account};
        converted.command.category = static_cast<FameCategory>(change.category);
        converted.command.value = change.value;
        inputs.scheduledChanges.push_back(converted);
    }
    return inputs;
}


void RequireAdvanceParity(std::size_t accountCount, std::size_t caseIndex) {
    const std::uint64_t seed = kBaseSeed + caseIndex * 7919u;
    Rng rng = Rng::ForCase(caseIndex);
    const auto setup = MakeRandomCase(rng, accountCount);
    const auto compiledResult = slsf::CompileDiffusionGraph(setup.graph, setup.accountKeys);
    REQUIRE(compiledResult.has_value());
    const auto referenceGraph = ToReferenceGraph(setup.graph, setup.accountKeys);

    diffref::Die referenceDie{seed};
    const diffref::Parameters referenceParameters{
        setup.parameters.decayPerDay, setup.parameters.relaxationPerDay,
        setup.parameters.spreadThreshold, setup.parameters.maxFame};
    const auto reference = diffref::AdvanceReference(
        setup.local, setup.remote, setup.settled, setup.remoteSettled, setup.protection,
        referenceGraph, referenceParameters, setup.now, referenceDie, 1.0, setup.scheduled);

    auto scalarState = MakeState(setup.accountKeys, setup.local, setup.remote, setup.protection,
                                 setup.settled, setup.remoteSettled);
    diffref::Die scalarDie{seed};
    SharedDie scalarAdapter{scalarDie};
    auto scalarInputs = MakeAdvanceInputs(setup, scalarAdapter);
    scalarInputs.backend = slsf::DiffusionBackend::Scalar;
    const auto scalarResult = slsf::Advance(scalarState, GameTime{setup.now}, scalarInputs);
    REQUIRE(scalarResult.has_value());

    auto avxState = MakeState(setup.accountKeys, setup.local, setup.remote, setup.protection,
                              setup.settled, setup.remoteSettled);
    diffref::Die avxDie{seed};
    SharedDie avxAdapter{avxDie};
    auto avxInputs = MakeAdvanceInputs(setup, avxAdapter);
    avxInputs.backend = slsf::DiffusionBackend::Avx2;
    const auto avxResult = slsf::Advance(avxState, GameTime{setup.now}, avxInputs);
    REQUIRE(avxResult.has_value());

    for (std::size_t index = 0; index < accountCount; ++index) {
        double localDelta = 0.0;
        double avxLocalDelta = 0.0;
        double remoteDelta = 0.0;
        double avxRemoteDelta = 0.0;
        for (std::size_t slot = 0; slot < diffref::kSlots; ++slot) {
            localDelta = std::max(localDelta,
                                  std::fabs(reference.accounts[index].local[slot] -
                                            scalarState.accounts[index].local[slot]));
            avxLocalDelta = std::max(avxLocalDelta,
                                     std::fabs(reference.accounts[index].local[slot] -
                                               avxState.accounts[index].local[slot]));
            remoteDelta = std::max(remoteDelta,
                                   std::fabs(reference.accounts[index].remote[slot] -
                                             scalarState.accounts[index].remote[slot]));
            avxRemoteDelta = std::max(avxRemoteDelta,
                                      std::fabs(reference.accounts[index].remote[slot] -
                                                avxState.accounts[index].remote[slot]));
        }

        INFO("seed=" << seed << " case=" << caseIndex << " account=" << index << ' '
                     << setup.describe);
        INFO("reference local " << DescribeRow(reference.accounts[index].local));
        INFO("scalar local    " << DescribeRow(scalarState.accounts[index].local));
        INFO("avx2 local      " << DescribeRow(avxState.accounts[index].local));
        INFO("reference remote " << DescribeRow(reference.accounts[index].remote));
        INFO("scalar remote    " << DescribeRow(scalarState.accounts[index].remote));
        INFO("avx2 remote      " << DescribeRow(avxState.accounts[index].remote));
        INFO("maxdeltas local=" << localDelta << " avxLocal=" << avxLocalDelta
                                << " remote=" << remoteDelta << " avxRemote=" << avxRemoteDelta);

        CHECK(localDelta <= 1e-9);
        CHECK(avxLocalDelta <= 1e-9);
        CHECK(remoteDelta <= 1e-9);
        CHECK(avxRemoteDelta <= 1e-9);
        CHECK(Close(reference.accounts[index].settledAt, scalarState.accounts[index].settledAt, 0.0));
        CHECK(Close(reference.accounts[index].settledAt, avxState.accounts[index].settledAt, 0.0));

        CHECK(Close(reference.accounts[index].remoteSettledAt,
                    scalarState.accounts[index].remoteSettledAt, 0.0));
        CHECK(Close(reference.accounts[index].remoteSettledAt,
                    avxState.accounts[index].remoteSettledAt, 0.0));

        for (std::size_t slot = 0; slot < diffref::kSlots; ++slot) {
            CHECK(scalarState.accounts[index].fame[slot] ==
                  std::max(scalarState.accounts[index].local[slot],
                           scalarState.accounts[index].remote[slot]));
        }
    }

    for (std::size_t index = 0; index < accountCount; ++index) {
        CHECK(avxState.accounts[index].local == scalarState.accounts[index].local);
        CHECK(avxState.accounts[index].remote == scalarState.accounts[index].remote);
        CHECK(avxState.accounts[index].settledAt == scalarState.accounts[index].settledAt);
        CHECK(avxState.accounts[index].remoteSettledAt == scalarState.accounts[index].remoteSettledAt);
    }

    for (std::size_t category = 0; category < slsf::kFameCategoryCount; ++category) {
        std::size_t positiveCount = 0;
        std::size_t localPositiveCount = 0;
        for (std::size_t index = 0; index < accountCount; ++index) {
            if (std::max(reference.accounts[index].local[category],
                         reference.accounts[index].remote[category]) > 0.0) {
                ++positiveCount;
            }
            if (reference.accounts[index].local[category] > 0.0) {
                ++localPositiveCount;
            }
        }
        CAPTURE(category, positiveCount, localPositiveCount, seed);
        INFO("counts case=" << caseIndex << ' ' << setup.describe);
        CHECK(scalarState.positiveRegionCounts[category] == positiveCount);
        CHECK(scalarState.localRegionCounts[category] == localPositiveCount);
    }
}


TEST_CASE("random gain parity against the reference", "[random][gain][differential]") {
    constexpr std::size_t kCases = 400;
    for (std::size_t caseIndex = 0; caseIndex < kCases; ++caseIndex) {
        Rng rng = Rng::ForCase(caseIndex);
        const double previous = static_cast<double>(rng.Int(0, 150));
        const std::size_t tier = ReferenceTier(previous);
        const int sample = rng.Int(1, LegacyGainTierCap(tier));
        const double iteration = rng.Range(0.25, 3.0);
        const double multiplier = rng.Range(0.0, 2.5);
        const bool nightEnabled = rng.Bool();
        const double hour = rng.Range(0.0, 24.0);
        const bool foreplay = rng.Bool();

        class Replay final : public IRandom {
        public:
            explicit Replay(int sample) : sample_(sample) {}

            std::int32_t UniformInt(std::int32_t low, std::int32_t high) override {
                if (sample_ < low) {
                    return low;
                }
                if (sample_ > high) {
                    return high;
                }
                return sample_;
            }

        private:
            int sample_;
        } replay{sample};

        slsf::GainFameInputs inputs;
        inputs.previousFame = previous;
        inputs.iterationMultiplier = iteration;
        inputs.maxFameValue = 150.0;
        inputs.resolvedMultiplier = multiplier;
        inputs.nightReduction = nightEnabled;
        inputs.hourOfDay = hour;
        inputs.nightStart = 22.0;
        inputs.nightEnd = 6.0;
        inputs.foreplay = slsf::Fact::Known(foreplay);
        const Configuration configuration;
        const auto result = slsf::RuleGainFame(inputs, replay, configuration, 0);

        diffref::GainInputs referenceInputs;
        referenceInputs.previousFame = previous;
        referenceInputs.sample = sample;
        referenceInputs.iterationMultiplier = iteration;
        referenceInputs.maxFame = 150.0;
        referenceInputs.multiplier = multiplier;
        referenceInputs.nightEnabled = nightEnabled;
        referenceInputs.hour = hour;
        referenceInputs.nightStart = 22.0;
        referenceInputs.nightEnd = 6.0;
        referenceInputs.foreplay = foreplay;
        const auto referenceOutcome = diffref::Gain(referenceInputs);
        const double expectedNext =
            std::clamp(previous + static_cast<double>(referenceOutcome.gained), 0.0, 150.0);
        const double actualNext = std::clamp(previous + static_cast<double>(result.gained), 0.0, 150.0);

        INFO("seed=" << kBaseSeed + caseIndex * 7919u << " case=" << caseIndex
                     << " previous=" << previous << " tier=" << tier << " sample=" << sample
                     << " iteration=" << iteration << " multiplier=" << multiplier
                     << " hour=" << hour << " nightEnabled=" << nightEnabled
                     << " foreplay=" << foreplay);
        CHECK(result.gained == referenceOutcome.gained);
        CHECK(result.halved == referenceOutcome.halved);
        CHECK(result.flooredToMinimum == referenceOutcome.floored);
        CHECK(actualNext == expectedNext);
    }
}

TEST_CASE("random decay roll parity against the reference", "[random][decay][differential]") {
    constexpr std::size_t kCases = 400;
    for (std::size_t caseIndex = 0; caseIndex < kCases; ++caseIndex) {
        Rng rng = Rng::ForCase(caseIndex);
        const double previous = static_cast<double>(rng.Int(0, 150));
        const double multiplier = rng.Range(0.0, 2.5);
        const int owedPeriods = rng.Int(1, 6);

        diffref::Die productionDie{kBaseSeed + caseIndex * 7919u};
        diffref::Die referenceDie{kBaseSeed + caseIndex * 7919u};
        SharedDie adapter{productionDie};
        const Configuration configuration;
        const auto result = slsf::RuleDecayRoll(previous, multiplier, adapter, configuration);
        const diffref::DecayInputs referenceInputs{previous, multiplier, owedPeriods};
        const auto referenceOutcome = diffref::DecayRoll(referenceInputs, referenceDie, 150.0);

        INFO("seed=" << kBaseSeed + caseIndex * 7919u << " case=" << caseIndex
                     << " previous=" << previous << " multiplier=" << multiplier
                     << " owed=" << owedPeriods);
        CHECK(result.amount == referenceOutcome.perPeriodAmount);
        CHECK(result.truncated == (referenceOutcome.perPeriodAmount == 0));
    }
}

TEST_CASE("random manual spread parity against the reference", "[random][spread][differential]") {
    constexpr std::size_t kCases = 300;
    for (std::size_t caseIndex = 0; caseIndex < kCases; ++caseIndex) {
        Rng rng = Rng::ForCase(caseIndex);
        const double source = static_cast<double>(rng.Int(10, 150));
        const double target = static_cast<double>(rng.Bool() ? rng.Int(0, 149) : 150);
        const int percent = rng.Int(-10, 130);

        const auto production = slsf::RuleManualSpreadFame(source, target, percent, 150.0);
        const diffref::SpreadInputs referenceInputs{source, target, percent, 150.0};
        const auto referenceOutcome = diffref::ManualSpread(referenceInputs);

        INFO("seed=" << kBaseSeed + caseIndex * 7919u << " case=" << caseIndex
                     << " source=" << source << " target=" << target << " percent=" << percent);
        REQUIRE(production.has_value());
        CHECK(production.value().newValue == referenceOutcome.newValue);
    }
}

TEST_CASE("random manual decay parity against the reference", "[random][decay][differential]") {
    constexpr std::size_t kCases = 200;
    for (std::size_t caseIndex = 0; caseIndex < kCases; ++caseIndex) {
        Rng rng = Rng::ForCase(caseIndex);
        const int minDecay = rng.Int(0, 100);
        const int maxDecay = rng.Int(0, 150);

        diffref::Die productionDie{kBaseSeed + caseIndex * 7919u + 31u};
        diffref::Die referenceDie{kBaseSeed + caseIndex * 7919u + 31u};
        SharedDie adapter{productionDie};
        const Configuration configuration;
        const auto production = slsf::RuleManualDecayAmount(minDecay, maxDecay, adapter, 150.0);
        REQUIRE(production.has_value());
        const auto referenceAmount = diffref::ManualDecayAmount(minDecay, maxDecay, referenceDie, 150.0);

        INFO("seed=" << kBaseSeed + caseIndex * 7919u + 31u << " case=" << caseIndex
                     << " min=" << minDecay << " max=" << maxDecay);
        CHECK(production.value() == referenceAmount);
    }
}


TEST_CASE("random advance parity scalar avx2 reference", "[random][advance][differential]") {
    constexpr std::size_t kCases = 60;
    for (std::size_t caseIndex = 0; caseIndex < kCases; ++caseIndex) {
        const std::size_t accountCount = 2 + (caseIndex % 5);
        RequireAdvanceParity(accountCount, caseIndex);
    }
}

TEST_CASE("random advance rejects null rng", "[random][advance][sanity]") {
    constexpr std::size_t kCases = 10;
    for (std::size_t caseIndex = 0; caseIndex < kCases; ++caseIndex) {
        Rng rng = Rng::ForCase(caseIndex);
        const auto setup = MakeRandomCase(rng, 2 + (caseIndex % 3));
        auto state = MakeState(setup.accountKeys, setup.local, setup.remote, setup.protection,
                               setup.settled, setup.remoteSettled);
        AdvanceInputs inputs;
        inputs.graph = &setup.graph;
        inputs.parameters = &setup.parameters;
        inputs.rng = nullptr;
        const auto result = slsf::Advance(state, GameTime{setup.now}, inputs);
        INFO("case=" << caseIndex << ' ' << setup.describe);
        CHECK_FALSE(result.has_value());
    }
}

TEST_CASE("random advance repeated settle is idempotent", "[random][advance][idempotence]") {
    constexpr std::size_t kCases = 25;
    for (std::size_t caseIndex = 0; caseIndex < kCases; ++caseIndex) {
        Rng rng = Rng::ForCase(caseIndex);
        const auto setup = MakeRandomCase(rng, 2 + (caseIndex % 3));
        auto state = MakeState(setup.accountKeys, setup.local, setup.remote, setup.protection,
                               setup.settled, setup.remoteSettled);
        State preState = state;
        const auto compiledResult = slsf::CompileDiffusionGraph(setup.graph, setup.accountKeys);
        REQUIRE(compiledResult.has_value());
        diffref::Die sharedDie{kBaseSeed + caseIndex * 7919u};
        SharedDie adapter{sharedDie};
        auto inputs = MakeAdvanceInputs(setup, adapter);
        const auto first = slsf::Advance(state, GameTime{setup.now}, inputs);
        REQUIRE(first.has_value());
        CHECK(std::none_of(inputs.scheduledChanges.begin(), inputs.scheduledChanges.end(),
                           [&](const slsf::ScheduledChange& change) {
                               return change.at.days <= setup.now;
                           }));

        std::vector<NumericRow> localRows(state.accounts.size());
        std::vector<NumericRow> remoteRows(state.accounts.size());
        for (std::size_t index = 0; index < state.accounts.size(); ++index) {
            localRows[index] = state.accounts[index].local;
            remoteRows[index] = state.accounts[index].remote;
        }

        State idempotentState = state;
        AdvanceInputs replayInputs = inputs;
        const auto second = slsf::Advance(idempotentState, GameTime{setup.now}, replayInputs);
        REQUIRE(second.has_value());

        INFO("case=" << caseIndex << ' ' << setup.describe);
        for (std::size_t index = 0; index < state.accounts.size(); ++index) {
            CAPTURE(index);
            CHECK(idempotentState.accounts[index].local == localRows[index]);
            CHECK(idempotentState.accounts[index].remote == remoteRows[index]);
            CHECK(idempotentState.accounts[index].fame == state.accounts[index].fame);
            CHECK(Close(idempotentState.accounts[index].settledAt,
                        state.accounts[index].settledAt, 0.0));
        }
    }
}

TEST_CASE("an exact half-day boundary consumes one decay period on both paths",
          "[random][advance][differential]") {
    const std::vector<StableRegionKey> keys{StableRegionKey{"boundary.esp", 1}};
    std::vector<diffref::Row> local(1);
    std::vector<diffref::Row> remote(1);
    std::vector<diffref::Row> protection(1);
    local[0][0] = 10.0;
    const std::vector<double> settled{0.0};
    const std::vector<double> remoteSettled{0.0};
    State state = MakeState(keys, local, remote, protection, settled, remoteSettled);

    diffref::Die productionDie{kBaseSeed};
    SharedDie adapter{productionDie};
    DiffusionGraph graph;
    const Parameters parameters{2.0, 0.0, 10.0, 0.6, 150.0};
    AdvanceInputs inputs;
    inputs.graph = &graph;
    inputs.parameters = &parameters;
    inputs.rng = &adapter;
    const auto produced = slsf::Advance(state, GameTime{0.5}, inputs);
    REQUIRE(produced.has_value());

    diffref::Die referenceDie{kBaseSeed};
    diffref::ReferenceGraph referenceGraph;
    referenceGraph.accountCount = 1;
    referenceGraph.offsets = {0, 0};
    diffref::Parameters referenceParameters;
    referenceParameters.decayPerDay = 2.0;
    referenceParameters.relaxationPerDay = 0.0;
    referenceParameters.spreadThreshold = 10.0;
    referenceParameters.maxFame = 150.0;
    referenceParameters.protectionDays = 0.5;
    const auto reference = diffref::AdvanceReference(
        local, remote, settled, remoteSettled, protection, referenceGraph,
        referenceParameters, 0.5, referenceDie);

    REQUIRE(reference.accounts.size() == 1);
    REQUIRE(state.accounts.size() == 1);
    CHECK(state.accounts[0].local[0] == reference.accounts[0].local[0]);
    CHECK(state.accounts[0].remote[0] == reference.accounts[0].remote[0]);
    CHECK(state.accounts[0].settledAt == reference.accounts[0].settledAt);
    CHECK(state.accounts[0].settledAt == 0.5);
    CHECK(state.accounts[0].local[0] < 10.0);
}

TEST_CASE("scheduled set settles at its business time before the remaining interval",
          "[random][advance][differential]") {
    const std::vector<StableRegionKey> keys{StableRegionKey{"scheduled.esp", 1}};
    std::vector<diffref::Row> local(1);
    std::vector<diffref::Row> remote(1);
    std::vector<diffref::Row> protection(1);
    const std::vector<double> settled{0.0};
    const std::vector<double> remoteSettled{0.0};
    DiffusionGraph graph;
    Parameters parameters{2.0, 0.0, 10.0, 0.6, 150.0};
    LowestDie productionDie;
    SharedDie productionAdapter{productionDie};
    auto state = MakeState(keys, local, remote, protection, settled, remoteSettled);
    AdvanceInputs inputs;
    inputs.graph = &graph;
    inputs.parameters = &parameters;
    inputs.rng = &productionAdapter;
    slsf::ScheduledChange scheduled;
    scheduled.at = GameTime{0.5};
    scheduled.command.kind = slsf::ExplicitCommandKind::Set;
    scheduled.command.target = RegionId{0};
    scheduled.command.category = FameCategory{0};
    scheduled.command.value = 10.0;
    inputs.scheduledChanges.push_back(scheduled);
    const auto produced = slsf::Advance(state, GameTime{1.0}, inputs);
    REQUIRE(produced.has_value());

    diffref::ReferenceGraph referenceGraph;
    referenceGraph.accountCount = 1;
    referenceGraph.offsets = {0, 0};
    const diffref::Parameters referenceParameters{2.0, 0.0, 10.0, 150.0};
    LowestDie referenceDie;
    const auto reference = diffref::AdvanceReference(
        local, remote, settled, remoteSettled, protection, referenceGraph,
        referenceParameters, 1.0, referenceDie, 1.0,
        {diffref::ScheduledChange{0.5, diffref::ScheduledKind::Set, 0, 0, 10.0}});

    REQUIRE(reference.accounts.size() == 1);
    CHECK(reference.accounts[0].local[0] == 9.0);
    CHECK(state.accounts[0].local[0] == 9.0);
    CHECK(state.accounts[0].local[0] == reference.accounts[0].local[0]);
    CHECK(state.accounts[0].settledAt == reference.accounts[0].settledAt);
    CHECK(state.accounts[0].settledAt == 1.0);
}

TEST_CASE("scheduled clear resets the target decay anchor to its business time",
          "[random][advance][differential]") {
    const std::vector<StableRegionKey> keys{StableRegionKey{"scheduled-clear.esp", 1}};
    std::vector<diffref::Row> local(1);
    std::vector<diffref::Row> remote(1);
    std::vector<diffref::Row> protection(1);
    local[0][0] = 10.0;
    const std::vector<double> settled{0.0};
    const std::vector<double> remoteSettled{0.0};
    DiffusionGraph graph;
    Parameters parameters{1.0, 0.0, 10.0, 0.6, 150.0};
    LowestDie productionDie;
    SharedDie productionAdapter{productionDie};
    auto state = MakeState(keys, local, remote, protection, settled, remoteSettled);
    AdvanceInputs inputs;
    inputs.graph = &graph;
    inputs.parameters = &parameters;
    inputs.rng = &productionAdapter;
    slsf::ScheduledChange scheduled;
    scheduled.at = GameTime{0.25};
    scheduled.command.kind = slsf::ExplicitCommandKind::Clear;
    scheduled.command.target = RegionId{0};
    scheduled.command.category = FameCategory{0};
    inputs.scheduledChanges.push_back(scheduled);
    const auto produced = slsf::Advance(state, GameTime{0.5}, inputs);
    REQUIRE(produced.has_value());

    diffref::ReferenceGraph referenceGraph;
    referenceGraph.accountCount = 1;
    referenceGraph.offsets = {0, 0};
    const diffref::Parameters referenceParameters{1.0, 0.0, 10.0, 150.0};
    LowestDie referenceDie;
    const auto reference = diffref::AdvanceReference(
        local, remote, settled, remoteSettled, protection, referenceGraph,
        referenceParameters, 0.5, referenceDie, 1.0,
        {diffref::ScheduledChange{0.25, diffref::ScheduledKind::Clear, 0, 0, 0.0}});

    REQUIRE(reference.accounts.size() == 1);
    CHECK(state.accounts[0].local[0] == 0.0);
    CHECK(state.accounts[0].local[0] == reference.accounts[0].local[0]);
    CHECK(state.accounts[0].settledAt == reference.accounts[0].settledAt);
    CHECK(state.accounts[0].settledAt == 0.25);
}

}
