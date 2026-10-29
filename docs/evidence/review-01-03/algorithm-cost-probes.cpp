#include "slsf/core/reputation/DiffusionGraph.h"
#include "slsf/core/reputation/ReputationState.h"
#include "slsf/core/reputation/Settlement.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace {

using namespace slsf;
using Clock = std::chrono::steady_clock;

class FixedRng final : public IRandom {
public:
    std::int32_t UniformInt(std::int32_t lo, std::int32_t) override { return lo; }
};

StableRegionKey Key(std::uint32_t id) { return {"algorithm-cost.esp", id}; }

RegionSnapshot MakeRegions(std::uint32_t count) {
    RegionSnapshot snapshot;
    snapshot.regions.reserve(count);
    for (std::uint32_t id = 1; id <= count; ++id) {
        snapshot.regions.push_back(Key(id));
    }
    return snapshot;
}

std::vector<EdgeDefinition> MakeEdges(std::uint32_t nodeCount, std::size_t edgeCount) {
    std::vector<EdgeDefinition> edges;
    edges.reserve(edgeCount);
    for (std::uint32_t source = 0; source < nodeCount && edges.size() < edgeCount; ++source) {
        for (std::uint32_t distance = 1; distance < nodeCount && edges.size() < edgeCount;
             ++distance) {
            const std::uint32_t target = (source + distance) % nodeCount;
            edges.push_back({Key(source + 1), Key(target + 1), 0.6});
        }
    }
    return edges;
}

double AverageNanoseconds(std::size_t iterations, const auto& operation) {
    const auto start = Clock::now();
    for (std::size_t i = 0; i < iterations; ++i) {
        operation();
    }
    const auto elapsed = std::chrono::duration<double, std::nano>(Clock::now() - start).count();
    return elapsed / static_cast<double>(iterations);
}

bool GraphCompileCosts() {
    constexpr struct {
        std::uint32_t nodes;
        std::size_t edges;
        std::size_t iterations;
    } cases[]{{21, 78, 10000}, {128, 384, 5000}, {512, 1536, 1000}};
    std::uint64_t checksum = 0;
    for (const auto& test : cases) {
        const auto regions = MakeRegions(test.nodes);
        auto built = BuildDiffusionGraph(regions, MakeEdges(test.nodes, test.edges));
        if (!built) {
            std::cout << "graph.build_error nodes=" << test.nodes
                      << " edges=" << test.edges << '\n';
            return false;
        }
        State state;
        if (!BindStateToRegions(state, regions)) {
            return false;
        }
        const auto compiled = CompileDiffusionGraph(*built, state.accountKeys);
        if (!compiled) {
            return false;
        }
        Parameters parameters{0.0, 0.0, 30.0, 0.6, 150.0};
        FixedRng rng;
        AdvanceInputs inputs{&*built, &parameters, nullptr, {}, &rng,
                             DiffusionBackend::Scalar};
        if (!Advance(state, GameTime{1.0}, inputs)) {
            return false;
        }

        const double compileNs = AverageNanoseconds(test.iterations, [&] {
            const auto fresh = CompileDiffusionGraph(*built, state.accountKeys);
            if (fresh) {
                checksum += fresh->operands.size();
            }
        });
        const double noOpAdvanceNs = AverageNanoseconds(test.iterations, [&] {
            const auto result = Advance(state, GameTime{1.0}, inputs);
            if (result) {
                checksum += result->revision.value;
            }
        });
        const double validateCompiledNs = AverageNanoseconds(test.iterations, [&] {
            checksum += IsValidCompiledGraph(*compiled) ? 1u : 0u;
        });
        std::cout << "graph.nodes=" << test.nodes << " graph.edges=" << test.edges
                  << " iterations=" << test.iterations
                  << " compile_ns=" << compileNs
                  << " no_op_Advance_ns=" << noOpAdvanceNs
                  << " validate_compiled_ns=" << validateCompiledNs << '\n';
    }
    std::cout << "graph.checksum=" << checksum << '\n';
    return true;
}

double AdvanceWithCustomCount(std::uint32_t accountCount, std::size_t customCount,
                              std::size_t iterations) {
    const auto regions = MakeRegions(accountCount);
    State state;
    if (!BindStateToRegions(state, regions)) {
        return -1.0;
    }
    state.customRegions.reserve(customCount);
    for (std::size_t index = 0; index < customCount; ++index) {
        state.customRegions.push_back(CustomRegionState{"custom", Key(static_cast<std::uint32_t>(index + 1))});
    }
    DiffusionGraph graph;
    Parameters parameters{0.0, 0.0, 30.0, 0.6, 150.0};
    FixedRng rng;
    AdvanceInputs inputs{&graph, &parameters, nullptr, {}, &rng,
                         DiffusionBackend::Scalar};
    std::uint64_t currentDay = 0;
    const auto averageNs = AverageNanoseconds(iterations, [&] {
        ++currentDay;
        const auto result = Advance(state, GameTime{static_cast<double>(currentDay)}, inputs);
        if (!result) {
            currentDay = 0;
        }
    });
    return averageNs;
}

bool CustomLookupCosts() {
    constexpr std::uint32_t accounts = 512;
    constexpr std::size_t iterations = 40;
    const double noCustomNs = AdvanceWithCustomCount(accounts, 0, iterations);
    const double allCustomNs = AdvanceWithCustomCount(accounts, accounts, iterations);
    std::cout << "custom.accounts=" << accounts << " iterations=" << iterations
              << " custom.none_Advance_ns=" << noCustomNs
              << " custom.all_Advance_ns=" << allCustomNs
              << " ratio=" << allCustomNs / noCustomNs << '\n';
    return noCustomNs > 0.0 && allCustomNs > 0.0;
}

} // namespace

int main() {
    std::cout << std::fixed << std::setprecision(2);
    const bool ok = GraphCompileCosts() && CustomLookupCosts();
    std::cout << "algorithm-cost-probes=" << (ok ? "PASS" : "FAIL") << '\n';
    return ok ? 0 : 1;
}
