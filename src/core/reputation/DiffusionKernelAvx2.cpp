#include "DiffusionKernelInternal.h"

#include <algorithm>
#include <immintrin.h>

namespace slsf::detail {

void ComputeIncomingRowsAvx2(NumericRowSpan incoming,
                             ConstNumericRowSpan local,
                             ConstNumericRowSpan remote,
                             const CompiledDiffusionGraph& graph,
                             double cutoff) {
    const auto rows = graph.accountCount;
    if (incoming.size() < rows || local.size() < rows || remote.size() < rows) {
        return;
    }

    const __m256d cutoffBroadcast = _mm256_set1_pd(cutoff);
    const __m256d zero = _mm256_setzero_pd();
    for (std::uint32_t target = 0; target < rows; ++target) {
        __m256d accumulators[7];
        for (auto& accumulator : accumulators) {
            accumulator = zero;
        }
        const auto end = graph.inEdgeOffsets[target + 1];
        for (auto index = graph.inEdgeOffsets[target]; index < end; ++index) {
            const auto& operand = graph.operands[index];
            const __m256d weight = _mm256_set1_pd(operand.coefficient);
            const double* sourceLocal = local[operand.source].values.data();
            const double* sourceRemote = remote[operand.source].values.data();
            for (int vectorIndex = 0; vectorIndex < 7; ++vectorIndex) {
                const auto maxPair = _mm256_max_pd(
                    _mm256_loadu_pd(sourceLocal + 4 * vectorIndex),
                    _mm256_loadu_pd(sourceRemote + 4 * vectorIndex));
                __m256d candidate = _mm256_mul_pd(weight, maxPair);
                // maxpd returns the second operand for NaN, so NaN candidates
                // become zero like the scalar rule.
                candidate = _mm256_max_pd(candidate, zero);
                accumulators[vectorIndex] = _mm256_max_pd(accumulators[vectorIndex],
                                                          candidate);
            }
        }
        double* destination = incoming[target].values.data();
        for (int vectorIndex = 0; vectorIndex < 7; ++vectorIndex) {
            const __m256d passes = _mm256_cmp_pd(accumulators[vectorIndex], cutoffBroadcast,
                                                 _CMP_GE_OQ);
            const __m256d masked = _mm256_and_pd(accumulators[vectorIndex], passes);
            _mm256_storeu_pd(destination + 4 * vectorIndex, masked);
        }
        std::fill(destination + kFameCategoryCount,
                  destination + kFameSlotCount, 0.0);
    }
}

void RelaxRemoteRowsAvx2(NumericRowSpan remote,
                         ConstNumericRowSpan incoming,
                         double retreat,
                         double cap) {
    const std::size_t rows = std::min(remote.size(), incoming.size());
    const __m256d zero = _mm256_setzero_pd();
    const __m256d capBroadcast = _mm256_set1_pd(cap);
    const __m256d retreatBroadcast = _mm256_set1_pd(retreat);
    for (std::size_t row = 0; row < rows; ++row) {
        double* target = remote[row].values.data();
        const double* goal = incoming[row].values.data();
        std::size_t slot = 0;
        for (; slot + 4 <= kFameCategoryCount; slot += 4) {
            const __m256d currentValues = _mm256_loadu_pd(target + slot);
            const __m256d goalValues = _mm256_loadu_pd(goal + slot);
            const __m256d positiveGoalMask = _mm256_cmp_pd(goalValues, zero, _CMP_GT_OQ);
            const __m256d positiveGoal = _mm256_blendv_pd(zero, goalValues, positiveGoalMask);
            const __m256d capMask = _mm256_cmp_pd(positiveGoal, capBroadcast, _CMP_LT_OQ);
            const __m256d destination = _mm256_blendv_pd(capBroadcast, positiveGoal, capMask);
            const __m256d arrivalMask = _mm256_cmp_pd(destination, currentValues, _CMP_GE_OQ);
            const __m256d retreated = _mm256_sub_pd(currentValues, retreatBroadcast);
            const __m256d keepRetreated = _mm256_cmp_pd(retreated, destination, _CMP_GE_OQ);
            const __m256d retreatResult = _mm256_blendv_pd(
                destination, retreated, keepRetreated);
            const __m256d result = _mm256_blendv_pd(retreatResult, destination, arrivalMask);
            _mm256_storeu_pd(target + slot, result);
        }
        for (; slot < kFameCategoryCount; ++slot) {
            target[slot] = detail::RelaxRemoteSlot(target[slot], goal[slot], retreat, cap);
        }
        std::fill(target + kFameCategoryCount, target + kFameSlotCount, 0.0);
    }
}

}
