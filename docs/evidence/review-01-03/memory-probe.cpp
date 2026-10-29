#include "slsf/core/NumericRow.h"
#include "slsf/core/persistence/StateCodec.h"
#include "slsf/core/reputation/DiffusionGraph.h"
#include "slsf/core/reputation/DiffusionKernel.h"

#include <array>
#include <cassert>
#include <cstddef>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

void CheckLegacyGraphOwnsInputs() {
    slsf::DiffusionGraph graph;
    {
        constexpr std::array<std::string_view, slsf::kLegacyDefaultNodeCount> nodeKeys{
            "whiterun", "winterhold", "windhelm", "solitude", "riften", "markarth",
            "morthal", "dawnstar", "falkreath", "raven_rock", "riverwood", "rorikstead",
            "ivarstead", "shors_stone", "dragon_bridge", "karthwasten", "skaal_village",
            "largashbur", "dushnikh_yal", "mor_khazgur", "narzulbur"};
        std::vector<std::string> names;
        std::vector<slsf::LegacyDefaultNode> bindings;
        names.reserve(nodeKeys.size());
        bindings.reserve(nodeKeys.size());
        for (std::size_t i = 0; i < nodeKeys.size(); ++i) {
            names.emplace_back(nodeKeys[i]);
            bindings.push_back({names.back(), {"TemporaryPlugin" + std::to_string(i),
                                                static_cast<std::uint32_t>(i + 1)}});
        }
        auto built = slsf::BuildLegacyDefaultGraph(bindings, 0.6);
        assert(built);
        graph = std::move(*built);
    }
    assert(graph.edges.size() == slsf::kLegacyDefaultEdgeCount);
    for (const auto& edge : graph.edges) {
        assert(!edge.from.plugin.empty());
        assert(!edge.to.plugin.empty());
    }
}

void CheckKernelRowBoundsAndPadding() {
    std::cout << (slsf::KernelSupportsAvx2() ? "AVX2 supported; SIMD dispatch exercised\n"
                                            : "AVX2 unsupported; requested backend falls back to scalar\n");
    slsf::CompiledDiffusionGraph graph;
    graph.accountCount = 2;
    graph.inEdgeOffsets = {0, 1, 1};
    graph.operands = {{1, 0.5}};
    assert(slsf::IsValidCompiledGraph(graph));

    slsf::NumericRowBuffer local(2);
    slsf::NumericRowBuffer remote(2);
    slsf::NumericRowBuffer scalar(2);
    slsf::NumericRowBuffer vectorized(2);
    for (std::size_t slot = 0; slot < slsf::kFameSlotCount; ++slot) {
        local[1].values[slot] = static_cast<double>(slot + 1);
        remote[1].values[slot] = 0.0;
    }

    slsf::ComputeIncomingRows(scalar, local, remote, graph, 0.5,
                              slsf::DiffusionBackend::Scalar);
    slsf::ComputeIncomingRows(vectorized, local, remote, graph, 0.5,
                              slsf::DiffusionBackend::Avx2);
    for (std::size_t slot = 0; slot < slsf::kFameCategoryCount; ++slot) {
        assert(scalar[0].values[slot] == vectorized[0].values[slot]);
        assert(vectorized[0].values[slot] == static_cast<double>(slot + 1) * 0.5);
        assert(scalar[1].values[slot] == 0.0);
        assert(vectorized[1].values[slot] == 0.0);
    }
    for (std::size_t slot = slsf::kFameCategoryCount; slot < slsf::kFameSlotCount; ++slot) {
        assert(scalar[0].values[slot] == 0.0);
        assert(vectorized[0].values[slot] == 0.0);
        assert(scalar[1].values[slot] == 0.0);
        assert(vectorized[1].values[slot] == 0.0);
    }
}

void CheckCodecRejectsEveryTruncatedPrefix() {
    slsf::PersistentState state;
    slsf::LocationPersistentState location;
    location.key = {"Probe.esp", 7};
    location.local[0] = 42.0;
    state.locations.push_back(location);
    auto encoded = slsf::EncodeState(state);
    assert(encoded);
    auto decoded = slsf::DecodeState(*encoded);
    assert(decoded && *decoded == state);
    for (std::size_t length = 0; length < encoded->size(); ++length) {
        const auto prefix = std::span<const std::byte>{encoded->data(), length};
        assert(!slsf::DecodeState(prefix));
    }
}

} // namespace

int main() {
    CheckLegacyGraphOwnsInputs();
    CheckKernelRowBoundsAndPadding();
    CheckCodecRejectsEveryTruncatedPrefix();
}
