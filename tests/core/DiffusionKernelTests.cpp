#include <catch2/catch_test_macros.hpp>

#include "slsf/core/NumericRow.h"
#include "slsf/core/reputation/DiffusionGraph.h"
#include "slsf/core/reputation/DiffusionKernel.h"

#include <algorithm>
#include <bit>
#include <cstdint>
#include <limits>
#include <vector>

namespace {

slsf::CompiledDiffusionGraph StarGraph(std::size_t count,
                                       const std::vector<double>& coefficients) {
    slsf::CompiledDiffusionGraph graph;
    graph.accountCount = static_cast<std::uint32_t>(count);
    graph.inEdgeOffsets.assign(count + 1, 0);
    if (count == 0) {
        return graph;
    }
    for (std::size_t source = 1; source < count && source <= coefficients.size();
         ++source) {
        slsf::CompiledDiffusionGraph::Operand operand{};
        operand.source = static_cast<std::uint32_t>(source);
        operand.coefficient = coefficients[source - 1];
        graph.operands.push_back(operand);
    }
    graph.inEdgeOffsets[1] = static_cast<std::uint32_t>(graph.operands.size());
    for (std::size_t target = 2; target <= count; ++target) {
        graph.inEdgeOffsets[target] = graph.inEdgeOffsets[1];
    }
    return graph;
}

}

TEST_CASE("incoming rows keep the strongest same-category candidate", "[kernel]") {
    const auto graph = StarGraph(3, {1.0, 1.0});
    slsf::NumericRowBuffer local(3);
    local[1][slsf::FameCategory::Whore] = 25.0;
    local[2][slsf::FameCategory::Whore] = 20.0;
    const slsf::NumericRowBuffer remote(3);

    slsf::NumericRowBuffer incoming(3);
    slsf::ComputeIncomingRows(incoming, local, remote, graph, 10.0,
                              slsf::DiffusionBackend::Scalar);
    REQUIRE(incoming[0][slsf::FameCategory::Whore] == 25.0);
    REQUIRE(incoming[0].PaddingIsZero());
    for (std::size_t index = 1; index < incoming.size(); ++index) {
        REQUIRE(incoming[index] == slsf::NumericRow{});
    }

    slsf::ComputeIncomingRows(incoming, local, remote, graph, 26.0,
                              slsf::DiffusionBackend::Scalar);
    REQUIRE(incoming[0][slsf::FameCategory::Whore] == 0.0);

    const auto weighted = StarGraph(3, {0.5, 0.9});
    slsf::ComputeIncomingRows(incoming, local, remote, weighted, 5.0,
                              slsf::DiffusionBackend::Scalar);
    REQUIRE(incoming[0][slsf::FameCategory::Whore] == 18.0);
}

TEST_CASE("remote rows arrive fully and retreat along the remaining in-edges",
          "[kernel]") {
    slsf::NumericRow row{};
    row[slsf::FameCategory::Whore] = 20.0;
    auto working = std::vector{row};

    auto incomingEmpty = std::vector{slsf::NumericRow{}};
    slsf::RelaxRemoteRows(working, incomingEmpty, 100.0, 1.0,
                          slsf::DiffusionBackend::Scalar);
    REQUIRE(working[0][slsf::FameCategory::Whore] == 0.0);

    slsf::NumericRow freshRow{};
    freshRow[slsf::FameCategory::Whore] = 0.0;
    auto fresh = std::vector{freshRow};
    auto incomingTen = std::vector{slsf::NumericRow{}};
    incomingTen[0][slsf::FameCategory::Whore] = 10.0;
    slsf::RelaxRemoteRows(fresh, incomingTen, 0.75, 1.0, slsf::DiffusionBackend::Scalar);
    REQUIRE(fresh[0][slsf::FameCategory::Whore] == 10.0);

    auto retreating = std::vector{row};
    auto incomingTenAgain = std::vector{slsf::NumericRow{}};
    incomingTenAgain[0][slsf::FameCategory::Whore] = 10.0;
    slsf::RelaxRemoteRows(retreating, incomingTenAgain, 1.0, 1.0,
                          slsf::DiffusionBackend::Scalar);
    REQUIRE(retreating[0][slsf::FameCategory::Whore] == 19.0);
    slsf::RelaxRemoteRows(retreating, incomingTenAgain, 40.0, 1.0,
                          slsf::DiffusionBackend::Scalar);
    REQUIRE(retreating[0][slsf::FameCategory::Whore] == 10.0);

    auto overtaken = std::vector{retreating[0]};
    auto incomingHigh = std::vector{slsf::NumericRow{}};
    incomingHigh[0][slsf::FameCategory::Whore] = 25.0;
    slsf::RelaxRemoteRows(overtaken, incomingHigh, 0.25, 1.0,
                          slsf::DiffusionBackend::Scalar);
    REQUIRE(overtaken[0][slsf::FameCategory::Whore] == 25.0);
}

TEST_CASE("remote relaxation backends preserve floating-point boundary semantics",
          "[kernel][avx2]") {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double infinity = std::numeric_limits<double>::infinity();
    const double negativeZero = -0.0;
    slsf::NumericRowBuffer initial(2);
    slsf::NumericRowBuffer incoming(2);
    initial[0][0] = -3.0;
    incoming[0][0] = nan;
    initial[0][1] = nan;
    incoming[0][1] = 5.0;
    initial[0][2] = infinity;
    incoming[0][2] = 2.0;
    initial[0][3] = -infinity;
    incoming[0][3] = 10.0;
    initial[0][4] = negativeZero;
    incoming[0][4] = negativeZero;
    initial[0][5] = 2.0;
    incoming[0][5] = 1.0;
    initial[0][6] = 4.0;
    incoming[0][6] = 2.0;
    initial[0][7] = 10.0;
    incoming[0][7] = infinity;
    initial[0][8] = 10.0;
    incoming[0][8] = -infinity;
    initial[0][9] = 10.0;
    incoming[0][9] = nan;
    initial[0][10] = 10.0;
    incoming[0][10] = 150.5;
    initial[0][11] = 10.0;
    incoming[0][11] = 150.0;
    initial[0][12] = 10.0;
    incoming[0][12] = negativeZero;
    initial[0][13] = 6.0;
    incoming[0][13] = 0.0;
    for (std::size_t slot = slsf::kFameCategoryCount; slot < slsf::kFameSlotCount; ++slot) {
        initial[0][slot] = static_cast<double>(slot + 1);
        incoming[0][slot] = nan;
    }
    initial[1][0] = 0.0;
    incoming[1][0] = 12.5;

    auto scalar = initial;
    auto avx2 = initial;
    auto automatic = initial;
    slsf::RelaxRemoteRows(scalar, incoming, 0.5, 2.0,
                          slsf::DiffusionBackend::Scalar);
    slsf::RelaxRemoteRows(avx2, incoming, 0.5, 2.0,
                          slsf::DiffusionBackend::Avx2);
    slsf::RelaxRemoteRows(automatic, incoming, 0.5, 2.0);
    REQUIRE(scalar[0][0] == 0.0);
    REQUIRE(scalar[0][1] == 5.0);
    REQUIRE(scalar[0][2] == infinity);
    REQUIRE(scalar[0][3] == 10.0);
    REQUIRE(std::bit_cast<std::uint64_t>(scalar[0][4]) ==
            std::bit_cast<std::uint64_t>(0.0));
    REQUIRE(scalar[0][5] == 1.0);
    REQUIRE(scalar[0][6] == 3.0);
    REQUIRE(scalar[0][7] == 150.0);
    REQUIRE(scalar[0][8] == 9.0);
    REQUIRE(scalar[0][9] == 9.0);
    REQUIRE(scalar[0][10] == 150.0);
    REQUIRE(scalar[0][11] == 150.0);
    REQUIRE(scalar[0][12] == 9.0);
    REQUIRE(scalar[0][13] == 5.0);
    REQUIRE(scalar[1][0] == 12.5);
    for (std::size_t row = 0; row < scalar.size(); ++row) {
        for (std::size_t slot = 0; slot < slsf::kFameSlotCount; ++slot) {
            REQUIRE(std::bit_cast<std::uint64_t>(scalar[row][slot]) ==
                    std::bit_cast<std::uint64_t>(avx2[row][slot]));
            REQUIRE(std::bit_cast<std::uint64_t>(scalar[row][slot]) ==
                    std::bit_cast<std::uint64_t>(automatic[row][slot]));
        }
        REQUIRE(scalar[row].PaddingIsZero());
        REQUIRE(avx2[row].PaddingIsZero());
    }

    auto scalarNegativeCap = initial;
    auto avx2NegativeCap = initial;
    slsf::RelaxRemoteRows(scalarNegativeCap, incoming, 0.0, 0.0,
                          slsf::DiffusionBackend::Scalar, negativeZero);
    slsf::RelaxRemoteRows(avx2NegativeCap, incoming, 0.0, 0.0,
                          slsf::DiffusionBackend::Avx2, negativeZero);
    REQUIRE(std::bit_cast<std::uint64_t>(scalarNegativeCap[1][0]) ==
            std::bit_cast<std::uint64_t>(negativeZero));
    REQUIRE(std::bit_cast<std::uint64_t>(scalarNegativeCap[1][0]) ==
            std::bit_cast<std::uint64_t>(avx2NegativeCap[1][0]));

    auto scalarInfiniteRetreat = initial;
    auto avx2InfiniteRetreat = initial;
    scalarInfiniteRetreat[0][0] = infinity;
    avx2InfiniteRetreat[0][0] = infinity;
    slsf::RelaxRemoteRows(scalarInfiniteRetreat, incoming, infinity, 1.0,
                          slsf::DiffusionBackend::Scalar);
    slsf::RelaxRemoteRows(avx2InfiniteRetreat, incoming, infinity, 1.0,
                          slsf::DiffusionBackend::Avx2);
    REQUIRE(scalarInfiniteRetreat[0][0] == 0.0);
    REQUIRE(std::bit_cast<std::uint64_t>(scalarInfiniteRetreat[0][0]) ==
            std::bit_cast<std::uint64_t>(avx2InfiniteRetreat[0][0]));
}

TEST_CASE("remote relaxation handles empty and short spans", "[kernel]") {
    slsf::NumericRowBuffer empty;
    slsf::RelaxRemoteRows(empty, empty, 1.0, 1.0);

    slsf::NumericRowBuffer remote(2);
    slsf::NumericRowBuffer incoming(1);
    remote[0][0] = 1.0;
    remote[1][0] = 2.0;
    incoming[0][0] = 5.0;
    slsf::RelaxRemoteRows(remote, incoming, 0.0, 1.0,
                          slsf::DiffusionBackend::Scalar);
    REQUIRE(remote[0][0] == 5.0);
    REQUIRE(remote[1][0] == 2.0);
    REQUIRE(remote[0].PaddingIsZero());
    REQUIRE(remote[1].PaddingIsZero());
}

TEST_CASE("kernels reject malformed graphs instead of reading through them",
          "[kernel][safety]") {
    slsf::NumericRowBuffer local(2);
    slsf::NumericRowBuffer remote(2);
    local[0][slsf::FameCategory::Whore] = 25.0;
    local[1][slsf::FameCategory::Whore] = 20.0;

    slsf::CompiledDiffusionGraph graph;
    graph.accountCount = 2;
    REQUIRE_FALSE(slsf::IsValidCompiledGraph(graph));

    slsf::NumericRowBuffer incoming(2);
    incoming[0][slsf::FameCategory::Whore] = 7.0;
    incoming[1][slsf::FameCategory::Whore] = 9.0;
    slsf::ComputeIncomingRows(incoming, local, remote, graph, 0.0,
                              slsf::DiffusionBackend::Scalar);
    REQUIRE(incoming[0] == slsf::NumericRow{});
    REQUIRE(incoming[1] == slsf::NumericRow{});
    REQUIRE(slsf::IncomingSlotCandidate(graph, 0, 0, local, remote, 0.0) == 0.0);

    graph.inEdgeOffsets = {0, 1, 1};
    graph.operands.push_back(slsf::CompiledDiffusionGraph::Operand{7, 1.0});
    REQUIRE_FALSE(slsf::IsValidCompiledGraph(graph));
    slsf::ComputeIncomingRows(incoming, local, remote, graph, 0.0,
                              slsf::DiffusionBackend::Scalar);
    REQUIRE(incoming[0] == slsf::NumericRow{});
    REQUIRE(slsf::IncomingSlotCandidate(graph, 0, 0, local, remote, 0.0) == 0.0);

    graph.inEdgeOffsets = {0, 1, 0};
    REQUIRE_FALSE(slsf::IsValidCompiledGraph(graph));

    const auto valid = StarGraph(2, {1.0});
    REQUIRE(slsf::IsValidCompiledGraph(valid));
    REQUIRE(slsf::IncomingSlotCandidate(valid, 99, 0, local, remote, 0.0) == 0.0);
    REQUIRE(slsf::IncomingSlotCandidate(valid, 0, slsf::kFameSlotCount, local, remote, 0.0) ==
            0.0);
    REQUIRE(slsf::IncomingSlotCandidate(valid, 0, 0, local, remote,
                                        std::numeric_limits<double>::quiet_NaN()) == 0.0);
}

TEST_CASE("scalar and AVX2 backends agree bit for bit", "[kernel][avx2]") {
    if (!slsf::KernelSupportsAvx2()) {
        WARN("host without AVX2: kernel parity covered by the differential suite");
        return;
    }

    const auto graph = StarGraph(4, {0.6, 1.0, 0.4});

    slsf::NumericRowBuffer local(4);
    slsf::NumericRowBuffer remote(4);
    std::uint64_t stream = 1170;
    const auto next = [&stream]() {
        std::uint64_t x = stream;
        x ^= x >> 12;
        x ^= x << 25;
        x ^= x >> 27;
        stream = x;
        return x * 0x2545F4914F9C4F3DULL;
    };
    for (auto* rows : {&local, &remote}) {
        for (auto& row : *rows) {
            for (std::size_t slot = 0; slot < slsf::kFameCategoryCount; ++slot) {
                row[slot] = static_cast<double>(next() % 151);
            }
        }
    }

    slsf::NumericRowBuffer scalar(4);
    slsf::NumericRowBuffer vector(4);
    slsf::ComputeIncomingRows(scalar, local, remote, graph, 0.0,
                              slsf::DiffusionBackend::Scalar);
    slsf::ComputeIncomingRows(vector, local, remote, graph, 0.0,
                              slsf::DiffusionBackend::Avx2);
    REQUIRE(scalar == vector);

    slsf::ComputeIncomingRows(scalar, local, remote, graph, 60.0,
                              slsf::DiffusionBackend::Scalar);
    slsf::ComputeIncomingRows(vector, local, remote, graph, 60.0,
                              slsf::DiffusionBackend::Avx2);
    REQUIRE(scalar == vector);

    const slsf::NumericRowBuffer retreatIncoming = local;
    slsf::RelaxRemoteRows(scalar, retreatIncoming, 0.75, 2.0,
                          slsf::DiffusionBackend::Scalar);
    slsf::RelaxRemoteRows(vector, retreatIncoming, 0.75, 2.0,
                          slsf::DiffusionBackend::Avx2);
    REQUIRE(scalar == vector);
}
