#include "slsf/core/Commands.h"
#include "slsf/core/NumericRow.h"
#include "slsf/core/Snapshots.h"
#include "slsf/core/Types.h"
#include "slsf/core/reputation/DiffusionGraph.h"
#include "slsf/core/reputation/DiffusionKernel.h"
#include "slsf/core/reputation/ReputationState.h"
#include "slsf/core/reputation/Settlement.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <utility>
#include <vector>

namespace {

using slsf::NumericRow;
using slsf::NumericRowBuffer;

class BenchDie final : public slsf::IRandom {
public:
    std::int32_t UniformInt(std::int32_t low, std::int32_t high) override {
        std::uint64_t x = state_;
        x ^= x >> 12;
        x ^= x << 25;
        x ^= x >> 27;
        state_ = x;
        const std::uint64_t next = x * 0x2545F4914F9C4F3DULL;
        const std::uint64_t span = static_cast<std::uint64_t>(high - low) + 1;
        return low + static_cast<std::int32_t>(next % span);
    }

private:
    std::uint64_t state_{0x1170};
};

struct Timer {
    std::chrono::steady_clock::time_point start{std::chrono::steady_clock::now()};
    double Seconds() const {
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    }
};

std::vector<slsf::StableRegionKey> MakeKeys(std::uint32_t accountCount) {
    std::vector<slsf::StableRegionKey> keys;
    keys.reserve(accountCount);
    for (std::uint32_t i = 0; i < accountCount; ++i) {
        keys.push_back(slsf::StableRegionKey{"bench.esp", i + 1});
    }
    std::sort(keys.begin(), keys.end());
    return keys;
}

slsf::DiffusionGraph MakeGraph(std::uint32_t accountCount) {
    const auto keys = MakeKeys(accountCount);
    const double retention = 0.6;
    slsf::DiffusionGraph graph;
    for (std::uint32_t source = 0; source < accountCount; ++source) {
        const std::uint32_t targets[] = {(source + 1u) % accountCount,
                                         (source + 7u) % accountCount,
                                         (source + 13u) % accountCount};
        for (const std::uint32_t targetIndex : targets) {
            if (targetIndex == source) {
                continue;
            }
            graph.edges.push_back(
                slsf::EdgeDefinition{keys[source], keys[targetIndex], retention});
        }
    }
    return graph;
}

void FillRows(NumericRowBuffer& rows, std::size_t count) {
    BenchDie die;
    rows.assign(count, NumericRow{});
    for (auto& row : rows) {
        for (std::size_t slot = 0; slot < slsf::kFameCategoryCount; ++slot) {
            row[slot] = static_cast<double>(die.UniformInt(0, 150));
        }
    }
}

void BenchKernel(std::uint32_t accountCount, int iterations) {
    const auto graph = MakeGraph(accountCount);
    const auto keys = MakeKeys(accountCount);
    const auto compiled = slsf::CompileDiffusionGraph(graph, keys);
    if (!compiled.has_value()) {
        std::fprintf(stderr, "BENCH compile failed: %s\n", compiled.error().message.c_str());
        std::exit(3);
    }

    NumericRowBuffer local;
    NumericRowBuffer remote;
    FillRows(local, accountCount);
    FillRows(remote, accountCount);
    NumericRowBuffer scalar(accountCount);
    NumericRowBuffer avx(accountCount);

    slsf::ComputeIncomingRows(scalar, local, remote, *compiled, 30.0,
                              slsf::DiffusionBackend::Scalar);
    if (slsf::KernelSupportsAvx2()) {
        slsf::ComputeIncomingRows(avx, local, remote, *compiled, 30.0,
                                  slsf::DiffusionBackend::Avx2);
        for (std::uint32_t i = 0; i < accountCount; ++i) {
            for (std::size_t slot = 0; slot < slsf::kFameSlotCount; ++slot) {
                if (avx[i][slot] != scalar[i][slot]) {
                    std::fprintf(stderr,
                                 "BENCH parity accounts=%u row=%u slot=%zu scalar=%.17g avx2=%.17g\n",
                                 accountCount, i, slot, scalar[i][slot], avx[i][slot]);
                    std::exit(3);
                }
            }
        }
    }

    auto Loop = [&](slsf::DiffusionBackend backend, NumericRowBuffer& rows) {
        Timer timer;
        volatile double sink = 0.0;
        for (int iteration = 0; iteration < iterations; ++iteration) {
            slsf::ComputeIncomingRows(rows, local, remote, *compiled, 30.0, backend);
            sink += rows[static_cast<std::size_t>(iteration) % accountCount]
                         [static_cast<std::size_t>(iteration) % slsf::kFameCategoryCount];
        }
        return std::pair<double, double>{timer.Seconds(), sink};
    };
    (void)Loop;

    const auto scalarRun = Loop(slsf::DiffusionBackend::Scalar, scalar);
    const auto avxRun = slsf::KernelSupportsAvx2()
                            ? Loop(slsf::DiffusionBackend::Avx2, avx)
                            : std::pair<double, double>{0.0, 0.0};

    const double relaxElapsed = 0.29;
    NumericRowBuffer relaxScalar = remote;
    Timer relaxTimer;
    for (int iteration = 0; iteration < iterations; ++iteration) {
        slsf::RelaxRemoteRows(relaxScalar, scalar, relaxElapsed, 1.0,
                              slsf::DiffusionBackend::Scalar);
    }
    const double relaxScalarSeconds = relaxTimer.Seconds();

    std::printf(
        "BENCH incoming accounts=%u edges=%u iters=%d scalar=%.2fms (%.1fns/row) "
        "avx2=%.2fms (%.1fns/row, %.2fx) relax_scalar=%.2fms (%.1fns/row)\n",
        accountCount, static_cast<unsigned>(compiled->operands.size()), iterations,
        scalarRun.first * 1e3, scalarRun.first * 1e9 / (iterations * accountCount),
        avxRun.first * 1e3, avxRun.first * 1e9 / (iterations * accountCount),
        slsf::KernelSupportsAvx2() ? scalarRun.first / avxRun.first : 0.0,
        relaxScalarSeconds * 1e3, relaxScalarSeconds * 1e9 / (iterations * accountCount));
}

void BenchSettlement(std::uint32_t accountCount, int batches) {
    const auto graph = MakeGraph(accountCount);
    const auto keys = MakeKeys(accountCount);
    const auto compiled = slsf::CompileDiffusionGraph(graph, keys);
    if (!compiled.has_value()) {
        std::fprintf(stderr, "BENCH settlement compile failed: %s\n",
                     compiled.error().message.c_str());
        std::exit(3);
    }

    slsf::State state;
    state.accounts.resize(accountCount);
    state.accountKeys = keys;
    state.locationAccounts.resize(keys.size());
    for (std::size_t i = 0; i < keys.size(); ++i) {
        state.locationAccounts[i] = slsf::RegionId(static_cast<std::uint32_t>(i));
    }
    BenchDie stateDie;
    for (auto& account : state.accounts) {
        for (std::size_t slot = 0; slot < slsf::kFameCategoryCount; ++slot) {
            account.local[slot] = static_cast<double>(stateDie.UniformInt(0, 150));
            account.remote[slot] = static_cast<double>(stateDie.UniformInt(0, 120));
        }
        account.settledAt = 0.0;
    }

    const slsf::Parameters parameters = slsf::LegacyDefaultParameters();
    BenchDie die;
    slsf::AdvanceInputs inputs;
    inputs.graph = &graph;
    inputs.parameters = &parameters;
    inputs.rng = &die;

    Timer timer;
    double now = 0.0;
    for (int batch = 0; batch < batches; ++batch) {
        now += (batch % 7 == 6) ? 2.6 : 0.29;
        if (batch % 5 == 4) {
            slsf::ExplicitCommand command;
            command.kind = slsf::ExplicitCommandKind::Set;
            command.target =
                slsf::RegionId(static_cast<std::uint32_t>(stateDie.UniformInt(
                    0, static_cast<std::int32_t>(accountCount) - 1)));
            command.category = static_cast<slsf::FameCategory>(stateDie.UniformInt(
                0, static_cast<std::int32_t>(slsf::kFameCategoryCount) - 1));
            command.value = static_cast<double>(stateDie.UniformInt(1, 40));
            slsf::RuleContext context;
            context.clock = slsf::GameTime{now};
            context.rng = &stateDie;
            std::ignore = slsf::ApplyExplicit(state, command, context);
        }
        const auto advanced = slsf::Advance(state, slsf::GameTime{now}, inputs);
        if (!advanced.has_value()) {
            std::fprintf(stderr, "BENCH advance failed: %s\n",
                         advanced.error().message.c_str());
            std::exit(3);
        }
        std::ignore = slsf::MakePublishedState(state);
    }
    const double seconds = timer.Seconds();
    std::printf(
        "BENCH settlement accounts=%u batches=%d advance+publish=%.1fµs/batch total=%.3fs\n",
        accountCount, batches, seconds * 1e6 / batches, seconds);
}

}

int main() {
    std::printf("BENCH seed=1170 avx2=%s\n", slsf::KernelSupportsAvx2() ? "yes" : "no");
    BenchKernel(21, 20000);
    BenchKernel(64, 4000);
    BenchKernel(128, 1500);
    BenchSettlement(21, 2000);
    BenchSettlement(64, 800);
    return 0;
}
