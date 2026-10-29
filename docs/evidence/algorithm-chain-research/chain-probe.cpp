#include "slsf/core/CategoryMask.h"
#include "slsf/core/reputation/DiffusionGraph.h"
#include "slsf/core/reputation/DiffusionKernel.h"
#include "slsf/core/reputation/ReputationState.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>
#include <intrin.h>

namespace {
using namespace slsf;
using Clock = std::chrono::steady_clock;
volatile std::uint64_t sink{};
constexpr std::size_t rounds = 7;
#define NOINLINE __declspec(noinline)

void Consume(std::uint64_t value) {
    sink = sink ^ value;
    std::atomic_signal_fence(std::memory_order_seq_cst);
}

template <class Fn>
double MedianNs(std::size_t iterations, Fn&& op) {
    for (std::size_t i = 0; i < 32; ++i) Consume(op(i));
    std::array<double, rounds> samples{};
    for (auto& sample : samples) {
        const auto start = Clock::now();
        for (std::size_t i = 0; i < iterations; ++i) Consume(op(i));
        sample = std::chrono::duration<double, std::nano>(Clock::now() - start).count()
                 / static_cast<double>(iterations);
    }
    std::sort(samples.begin(), samples.end());
    return samples[rounds / 2];
}

StableRegionKey Key(std::uint32_t id) {
    return {"algorithm-chain-research-plugin.esp", id + 1};
}

State MakeState(std::uint32_t n) {
    State s;
    s.accounts.resize(n);
    for (std::uint32_t i = 0; i < n; ++i) {
        s.accountKeys.push_back(Key(i));
        s.locationAccounts.push_back(RegionId{i});
        for (std::size_t j = 0; j < kFameCategoryCount; ++j) {
            s.accounts[i].local[j] = static_cast<double>((i * 19 + j * 7) % 151);
            s.accounts[i].remote[j] = static_cast<double>((i * 13 + j * 11) % 151);
            s.accounts[i].fame[j] = std::max(s.accounts[i].local[j], s.accounts[i].remote[j]);
            s.accounts[i].protectionUntilDay[j] = 0.25 * static_cast<double>(j);
        }
    }
    return s;
}

DiffusionGraph MakeGraph(std::uint32_t n) {
    if (n == 21) {
        constexpr std::array names{
            "whiterun", "winterhold", "windhelm", "solitude", "riften", "markarth",
            "morthal", "dawnstar", "falkreath", "raven_rock", "riverwood", "rorikstead",
            "ivarstead", "shors_stone", "dragon_bridge", "karthwasten", "skaal_village",
            "largashbur", "dushnikh_yal", "mor_khazgur", "narzulbur"};
        std::vector<LegacyDefaultNode> bindings;
        for (std::uint32_t i = 0; i < n; ++i) bindings.push_back({names[i], Key(i)});
        auto graph = BuildLegacyDefaultGraph(bindings, 0.6);
        if (!graph || graph->edges.size() != 78) throw std::runtime_error("legacy graph build");
        return std::move(*graph);
    }
    // Three outgoing and three incoming edges per node, spread across the graph.
    RegionSnapshot regions;
    for (std::uint32_t i = 0; i < n; ++i) regions.regions.push_back(Key(i));
    std::vector<EdgeDefinition> edges;
    for (std::uint32_t source = 0; source < n; ++source) {
        for (const auto offset : {1u, n / 4 + 1, n / 2 + 1}) {
            edges.push_back({Key(source), Key((source + offset) % n), 0.6});
        }
    }
    auto built = BuildDiffusionGraph(regions, std::move(edges));
    if (!built) throw std::runtime_error("synthetic graph build");
    return std::move(*built);
}

void CopySnapshots(const State& s, NumericRowBuffer& l, NumericRowBuffer& r) {
    for (std::size_t i = 0; i < s.accounts.size(); ++i) {
        l[i] = s.accounts[i].local;
        r[i] = s.accounts[i].remote;
    }
}

void CheckEqual(const NumericRowBuffer& a, const NumericRowBuffer& b) {
    if (a.size() != b.size()) throw std::runtime_error("row count differs");
    for (std::size_t i = 0; i < a.size(); ++i) {
        for (std::size_t j = 0; j < kFameSlotCount; ++j) {
            if (std::bit_cast<std::uint64_t>(a[i][j]) != std::bit_cast<std::uint64_t>(b[i][j]))
                throw std::runtime_error("row element differs");
        }
    }
}

void GraphCosts() {
    for (const auto n : {21u, 128u, 512u}) {
        auto state = MakeState(n);
        const auto graph = MakeGraph(n);
        const auto compiled = CompileDiffusionGraph(graph, state.accountKeys);
        if (!compiled || !IsValidCompiledGraph(*compiled)) throw std::runtime_error("compile");
        NumericRowBuffer local(n), remote(n), freshOutput(n), cachedOutput(n), scalar(n);
        CopySnapshots(state, local, remote);
        const auto iterations = n == 512 ? 1500u : 5000u;
        std::vector<std::uint32_t> degrees;
        for (std::uint32_t i = 0; i < n; ++i)
            degrees.push_back(compiled->inEdgeOffsets[i + 1] - compiled->inEdgeOffsets[i]);
        for (std::size_t sample = 0; sample < 17; ++sample) {
            local[(sample * 7) % n][sample % kFameCategoryCount] += 0.25;
            auto fresh = CompileDiffusionGraph(graph, state.accountKeys);
            if (!fresh) throw std::runtime_error("fresh compile");
            ComputeIncomingRows(freshOutput, local, remote, *fresh, 30.0, DiffusionBackend::Avx2);
            ComputeIncomingRows(cachedOutput, local, remote, *compiled, 30.0, DiffusionBackend::Avx2);
            ComputeIncomingRows(scalar, local, remote, *compiled, 30.0, DiffusionBackend::Scalar);
            CheckEqual(freshOutput, cachedOutput);
            CheckEqual(scalar, cachedOutput);
        }
        const auto mutation = [&](std::size_t i) {
            local[i % n][i % kFameCategoryCount] = 80.0 + static_cast<double>(i % 67);
        };
        const auto freshNs = MedianNs(iterations, [&](std::size_t i) {
            mutation(i);
            auto fresh = CompileDiffusionGraph(graph, state.accountKeys);
            if (!fresh) throw std::runtime_error("timed compile");
            ComputeIncomingRows(freshOutput, local, remote, *fresh, 30.0, DiffusionBackend::Avx2);
            return std::bit_cast<std::uint64_t>(freshOutput[i % n][i % kFameCategoryCount]);
        });
        const auto cachedNs = MedianNs(iterations, [&](std::size_t i) {
            mutation(i);
            ComputeIncomingRows(cachedOutput, local, remote, *compiled, 30.0, DiffusionBackend::Avx2);
            return std::bit_cast<std::uint64_t>(cachedOutput[i % n][i % kFameCategoryCount]);
        });
        const auto validateNs = MedianNs(iterations, [&](std::size_t) {
            return static_cast<std::uint64_t>(IsValidCompiledGraph(*compiled));
        });
        std::cout << "csr nodes=" << n << " edges=" << graph.edges.size()
                  << " indegree_min=" << *std::min_element(degrees.begin(), degrees.end())
                  << " indegree_max=" << *std::max_element(degrees.begin(), degrees.end())
                  << " iterations=" << iterations << " compile_compute_ns=" << freshNs
                  << " cached_compute_ns=" << cachedNs << " validation_ns=" << validateNs
                  << " ratio=" << freshNs / cachedNs << " bitwise_equal=yes\n";
    }
}

NOINLINE std::uint64_t ScanMembership(const State& s, std::size_t start) {
    std::uint64_t result = 0;
    const auto n = s.accountKeys.size();
    for (std::size_t step = 0; step < n; ++step) {
        const auto index = (step + start) % n;
        const auto found = std::any_of(s.customRegions.begin(), s.customRegions.end(),
            [&](const CustomRegionState& custom) { return custom.key == s.accountKeys[index]; });
        result = result * 33u + static_cast<std::uint64_t>(found);
    }
    return result;
}

NOINLINE std::uint64_t DenseMembership(const std::vector<std::uint8_t>& byRegion,
                                      std::size_t start) {
    std::uint64_t result = 0;
    const auto n = byRegion.size();
    for (std::size_t step = 0; step < n; ++step) {
        const auto index = (step + start) % n;
        result = result * 33u + byRegion[index];
    }
    return result;
}

void MembershipCosts() {
    for (const auto n : {21u, 128u, 512u}) {
        for (const auto customCount : {0u, 1u, n / 4u, n}) {
            auto state = MakeState(n);
            for (std::uint32_t j = 0; j < customCount; ++j)
                state.customRegions.push_back({"custom-name", state.accountKeys[j]});
            std::vector<std::uint8_t> byRegion(n, 0);
            for (std::uint32_t j = 0; j < n; ++j) {
                byRegion[j] = std::any_of(state.customRegions.begin(), state.customRegions.end(),
                    [&](const CustomRegionState& custom) { return custom.key == state.accountKeys[j]; });
            }
            for (std::uint32_t start = 0; start < n; ++start)
                if (ScanMembership(state, start) != DenseMembership(byRegion, start))
                    throw std::runtime_error("membership differs");
            const auto iterations = n == 512 ? 200u : 1000u;
            const auto scanNs = MedianNs(iterations, [&](std::size_t i) { return ScanMembership(state, i % n); });
            const auto denseNs = MedianNs(iterations, [&](std::size_t i) { return DenseMembership(byRegion, i % n); });
            std::cout << "membership nodes=" << n << " custom=" << customCount
                      << " iterations=" << iterations << " scan_batch_ns=" << scanNs
                      << " dense_batch_ns=" << denseNs << " ratio=" << scanNs / denseNs
                      << " same_answers=yes\n";
        }
    }
}

NOINLINE std::uint64_t ConstructCopy(const State& source, std::size_t index) {
    State copy = source;
    std::atomic_signal_fence(std::memory_order_seq_cst);
    return std::bit_cast<std::uint64_t>(copy.accounts[index].local[0]);
}

NOINLINE std::uint64_t AssignCopy(const State& source, State& dest, std::size_t index) {
    dest = source;
    std::atomic_signal_fence(std::memory_order_seq_cst);
    return std::bit_cast<std::uint64_t>(dest.accounts[index].local[0]);
}

NOINLINE std::uint64_t ReuseSnapshots(const State& source, NumericRowBuffer& local,
                                     NumericRowBuffer& remote, std::size_t index) {
    CopySnapshots(source, local, remote);
    std::atomic_signal_fence(std::memory_order_seq_cst);
    return std::bit_cast<std::uint64_t>(local[index][0]);
}

void LayoutCosts() {
    std::cout << "layout NumericRow_size=" << sizeof(NumericRow) << " NumericRow_align=" << alignof(NumericRow)
              << " AccountState_size=" << sizeof(AccountState) << " AccountState_align=" << alignof(AccountState)
              << " Operand_size=" << sizeof(CompiledDiffusionGraph::Operand)
              << " Operand_align=" << alignof(CompiledDiffusionGraph::Operand)
              << " local_offset=" << offsetof(AccountState, local)
              << " remote_offset=" << offsetof(AccountState, remote)
              << " fame_offset=" << offsetof(AccountState, fame)
              << " flags_offset=" << offsetof(AccountState, flags)
              << " protection_offset=" << offsetof(AccountState, protectionUntilDay)
              << " Operand_source_offset=" << offsetof(CompiledDiffusionGraph::Operand, source)
              << " Operand_coefficient_offset=" << offsetof(CompiledDiffusionGraph::Operand, coefficient) << '\n';
    for (const auto n : {21u, 128u, 512u}) {
        auto source = MakeState(n);
        for (std::uint32_t i = 0; i < n / 4u; ++i)
            source.customRegions.push_back({"custom-region-with-long-name", source.accountKeys[i]});
        State reused = source;
        NumericRowBuffer local(n), remote(n);
        CopySnapshots(source, local, remote);
        const auto iterations = n == 512 ? 2000u : 5000u;
        const auto mutate = [&](std::size_t i) {
            source.accounts[i % n].local[0] = static_cast<double>(i % 151u);
        };
        const auto constructNs = MedianNs(iterations, [&](std::size_t i) {
            mutate(i); return ConstructCopy(source, i % n);
        });
        const auto assignNs = MedianNs(iterations, [&](std::size_t i) {
            mutate(i); return AssignCopy(source, reused, i % n);
        });
        const auto snapshotNs = MedianNs(iterations, [&](std::size_t i) {
            mutate(i); return ReuseSnapshots(source, local, remote, i % n);
        });
        CopySnapshots(source, local, remote);
        for (std::uint32_t i = 0; i < n; ++i) {
            if (local[i] != source.accounts[i].local || remote[i] != source.accounts[i].remote)
                throw std::runtime_error("snapshot differs");
        }
        std::cout << "copy nodes=" << n << " account_bytes=" << n * sizeof(AccountState)
                  << " LR_bytes=" << n * sizeof(NumericRow) * 2u
                  << " iterations=" << iterations << " State_construct_destroy_ns=" << constructNs
                  << " State_reused_assignment_ns=" << assignNs << " reused_LR_snapshot_ns=" << snapshotNs
                  << " snapshot_equal=yes\n";
    }
}

NOINLINE std::uint64_t ScanMasks(const std::vector<CategoryMask>& masks,
                                const std::array<std::uint64_t, 25>& values) {
    std::uint64_t sum = 0;
    for (const auto mask : masks) {
        for (std::size_t j = 0; j < kFameCategoryCount; ++j)
            if (mask.Test(static_cast<FameCategory>(j))) sum = sum * 33u + values[j];
    }
    return sum;
}

NOINLINE std::uint64_t EnumerateMasks(const std::vector<CategoryMask>& masks,
                                     const std::array<std::uint64_t, 25>& values) {
    std::uint64_t sum = 0;
    for (const auto mask : masks)
        mask.ForEach([&](FameCategory c) { sum = sum * 33u + values[static_cast<std::size_t>(c)]; });
    return sum;
}

void MaskCosts() {
    for (const auto active : {0u, 1u, 6u, 25u}) {
        std::vector<CategoryMask> masks(512);
        for (std::uint32_t i = 0; i < masks.size(); ++i) {
            for (std::uint32_t j = 0; j < active; ++j)
                masks[i].Set(static_cast<FameCategory>((i + j * 4u) % 25u));
            if (masks[i].Count() != active) throw std::runtime_error("mask count");
        }
        std::array<std::uint64_t, 25> values{};
        std::iota(values.begin(), values.end(), 1u);
        if (ScanMasks(masks, values) != EnumerateMasks(masks, values))
            throw std::runtime_error("mask order differs");
        constexpr std::size_t iterations = 1000;
        const auto scanNs = MedianNs(iterations, [&](std::size_t i) {
            values[i % 25u] = i; return ScanMasks(masks, values);
        });
        const auto enumNs = MedianNs(iterations, [&](std::size_t i) {
            values[i % 25u] = i; return EnumerateMasks(masks, values);
        });
        std::cout << "mask batch=512 active=" << active << " iterations=" << iterations
                  << " scan25_ns=" << scanNs << " setbit_ns=" << enumNs
                  << " ratio=" << scanNs / enumNs << " same_ascending_answers=yes\n";
    }
}

void Cpu() {
    std::array<char, 49> brand{};
    int reg[4]{};
    __cpuid(reg, static_cast<int>(0x80000000u));
    if (static_cast<std::uint32_t>(reg[0]) >= 0x80000004u) {
        for (int i = 0; i < 3; ++i) {
            __cpuid(reg, static_cast<int>(0x80000002u) + i);
            std::memcpy(brand.data() + i * 16, reg, 16);
        }
    }
    std::cout << "cpu=" << brand.data() << " avx2=" << KernelSupportsAvx2()
              << " compiler=" << __clang_version__ << " rounds=" << rounds
              << " summary=median flags=-std=c++23,-O2,-DNDEBUG,AVX2-only-kernel-TU\n";
}
}

int main() {
    std::cout << std::fixed << std::setprecision(2);
    try {
        Cpu();
        if (!KernelSupportsAvx2()) throw std::runtime_error("AVX2 unavailable");
        GraphCosts();
        MembershipCosts();
        LayoutCosts();
        MaskCosts();
        std::cout << "checksum=" << sink << " chain-probe=PASS\n";
        return 0;
    } catch (const std::exception& error) {
        std::cout << "chain-probe=FAIL error=" << error.what() << '\n';
        return 1;
    }
}
