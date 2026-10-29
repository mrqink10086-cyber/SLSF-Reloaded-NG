#include "slsf/core/reputation/DiffusionKernel.h"

#include "DiffusionKernelInternal.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

#if defined(_M_X64) || defined(__x86_64__)
#include <immintrin.h>
#include <intrin.h>
#define SLSF_KERNEL_HAS_CPUID 1
#endif

namespace slsf {
namespace {

#ifdef SLSF_KERNEL_HAS_CPUID
// Kept in the baseline TU: CPUID/XGETBV only, no AVX2 intrinsics.
bool CpuidAvx2() noexcept {
    std::int32_t registers[4] = {};
    __cpuid(registers, 0);
    if (registers[0] < 7) {
        return false;
    }
    __cpuid(registers, 1);
    const bool osxsave = (registers[2] & (1u << 27)) != 0;
    const bool avx = (registers[2] & (1u << 28)) != 0;
    if (!osxsave || !avx) {
        return false;
    }
    // YMM state must be enabled by the OS, checked through XCR0.
    const auto xcr0 = static_cast<std::uint64_t>(_xgetbv(_XCR_XFEATURE_ENABLED_MASK));
    if ((xcr0 & 0x6u) != 0x6u) {
        return false;
    }
    __cpuidex(registers, 7, 0);
    return (registers[1] & (1u << 5)) != 0;
}
#endif

inline double SourceFame(ConstNumericRowSpan local, ConstNumericRowSpan remote,
                         std::uint32_t source, std::size_t slot) noexcept {
    const double localValue = local[source].values[slot];
    const double remoteValue = remote[source].values[slot];
    return localValue >= remoteValue ? localValue : remoteValue;
}

inline double IncomingSlotCandidateUnchecked(const CompiledDiffusionGraph& graph,
                                             std::uint32_t target,
                                             std::size_t slot,
                                             ConstNumericRowSpan local,
                                             ConstNumericRowSpan remote,
                                             double cutoff) noexcept {
    double best = 0.0;
    const auto end = graph.inEdgeOffsets[target + 1];
    for (auto index = graph.inEdgeOffsets[target]; index < end; ++index) {
        const auto& operand = graph.operands[index];
        const double candidate =
            operand.coefficient * SourceFame(local, remote, operand.source, slot);
        if (candidate > best) {
            best = candidate;
        }
    }
    return best < cutoff ? 0.0 : best;
}

void ComputeIncomingRowsScalar(NumericRowSpan incoming,
                               ConstNumericRowSpan local,
                               ConstNumericRowSpan remote,
                               const CompiledDiffusionGraph& graph,
                               double cutoff) {
    const auto rows = graph.accountCount;
    for (std::uint32_t target = 0; target < rows; ++target) {
        for (std::size_t slot = 0; slot < kFameCategoryCount; ++slot) {
            incoming[target].values[slot] =
                IncomingSlotCandidateUnchecked(graph, target, slot, local, remote, cutoff);
        }
    }
}

void RelaxRemoteRowsScalar(NumericRowSpan remote,
                           ConstNumericRowSpan incoming,
                           double retreat,
                           double cap,
                           std::size_t rows) {
    for (std::size_t row = 0; row < rows; ++row) {
        double* target = remote[row].values.data();
        const double* goal = incoming[row].values.data();
        for (std::size_t slot = 0; slot < kFameCategoryCount; ++slot) {
            target[slot] = detail::RelaxRemoteSlot(target[slot], goal[slot], retreat, cap);
        }
        std::fill(target + kFameCategoryCount, target + kFameSlotCount, 0.0);
    }
}

}

bool KernelSupportsAvx2() noexcept {
#ifdef SLSF_KERNEL_HAS_CPUID
    static const bool supported = CpuidAvx2();
    return supported;
#else
    return false;
#endif
}

double IncomingSlotCandidate(const CompiledDiffusionGraph& graph,
                             std::uint32_t target,
                             std::size_t slot,
                             ConstNumericRowSpan local,
                             ConstNumericRowSpan remote,
                             double cutoff) {
    if (target >= graph.accountCount || slot >= kFameCategoryCount ||
        static_cast<std::size_t>(target) + 1 >= graph.inEdgeOffsets.size()) {
        return 0.0;
    }
    const auto begin = graph.inEdgeOffsets[target];
    const auto end = graph.inEdgeOffsets[target + 1];
    if (begin > end || end > graph.operands.size() ||
        local.size() < graph.accountCount || remote.size() < graph.accountCount) {
        return 0.0;
    }
    if (!std::isfinite(cutoff)) {
        return 0.0;
    }
    cutoff = std::max(0.0, cutoff);
    for (auto index = begin; index < end; ++index) {
        if (graph.operands[index].source >= graph.accountCount) {
            return 0.0;
        }
    }
    return IncomingSlotCandidateUnchecked(graph, target, slot, local, remote, cutoff);
}

bool IsValidCompiledGraph(const CompiledDiffusionGraph& graph) noexcept {
    if (graph.inEdgeOffsets.size() != static_cast<std::size_t>(graph.accountCount) + 1) {
        return false;
    }
    if (graph.inEdgeOffsets.empty() || graph.inEdgeOffsets.front() != 0) {
        return false;
    }
    for (std::size_t index = 1; index < graph.inEdgeOffsets.size(); ++index) {
        if (graph.inEdgeOffsets[index] < graph.inEdgeOffsets[index - 1]) {
            return false;
        }
    }
    if (static_cast<std::size_t>(graph.inEdgeOffsets.back()) != graph.operands.size()) {
        return false;
    }
    for (const auto& operand : graph.operands) {
        if (operand.source >= graph.accountCount) {
            return false;
        }
    }
    return true;
}

void ComputeIncomingRows(NumericRowSpan incoming,
                         ConstNumericRowSpan local,
                         ConstNumericRowSpan remote,
                         const CompiledDiffusionGraph& graph,
                         double cutoff,
                         DiffusionBackend backend) {
    const auto rows = graph.accountCount;
    if (incoming.size() < rows) {
        return;
    }
    for (std::uint32_t row = 0; row < rows; ++row) {
        incoming[row] = NumericRow{};
    }
    if (!IsValidCompiledGraph(graph) || local.size() < rows || remote.size() < rows) {
        return;
    }

    // A non-finite cutoff makes every candidate fail in every backend.
    if (!std::isfinite(cutoff)) {
        return;
    }
    cutoff = std::max(0.0, cutoff);

    const bool avx2Available = KernelSupportsAvx2();
    if (backend == DiffusionBackend::Auto) {
        backend = avx2Available ? DiffusionBackend::Avx2 : DiffusionBackend::Scalar;
    }
    if (backend == DiffusionBackend::Avx2) {
        if (!avx2Available) {
            ComputeIncomingRowsScalar(incoming, local, remote, graph, cutoff);
            return;
        }
        detail::ComputeIncomingRowsAvx2(incoming, local, remote, graph, cutoff);
        return;
    }
    ComputeIncomingRowsScalar(incoming, local, remote, graph, cutoff);
}

void RelaxRemoteRows(NumericRowSpan remote,
                     ConstNumericRowSpan incoming,
                     double elapsedDays,
                     double relaxationPerDay,
                     DiffusionBackend backend,
                     double maxFame) {
    const double elapsed = std::max(0.0, elapsedDays);
    const double retreat = std::max(0.0, relaxationPerDay) * elapsed;
    // A transmitted result is clamped to non-negative maxFame (150 when invalid).
    const double cap = std::isfinite(maxFame) && maxFame >= 0.0 ? maxFame : 150.0;
    const std::size_t rows = std::min(remote.size(), incoming.size());
    if (rows == 0) {
        return;
    }

    const bool avx2Available = KernelSupportsAvx2();
    if (backend == DiffusionBackend::Auto) {
        backend = avx2Available ? DiffusionBackend::Avx2 : DiffusionBackend::Scalar;
    }
    if (backend == DiffusionBackend::Avx2 && avx2Available) {
        detail::RelaxRemoteRowsAvx2(remote.first(rows), incoming.first(rows), retreat, cap);
        return;
    }
    RelaxRemoteRowsScalar(remote, incoming, retreat, cap, rows);
}

}
