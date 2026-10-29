#pragma once

#include "slsf/core/reputation/DiffusionKernel.h"

namespace slsf::detail {

inline double RelaxRemoteSlot(double current, double goal, double retreat,
                               double cap) noexcept {
    const double nonnegativeGoal = 0.0 < goal ? goal : 0.0;
    const double destination = nonnegativeGoal < cap ? nonnegativeGoal : cap;
    if (destination >= current) {
        return destination;
    }
    const double retreated = current - retreat;
    return retreated >= destination ? retreated : destination;
}

// AVX2 leaf; only ComputeIncomingRows may call it.  Requires
// KernelSupportsAvx2(), IsValidCompiledGraph(graph), and spans covering
// graph.accountCount rows.
void ComputeIncomingRowsAvx2(NumericRowSpan incoming,
                             ConstNumericRowSpan local,
                             ConstNumericRowSpan remote,
                             const CompiledDiffusionGraph& graph,
                             double cutoff);

void RelaxRemoteRowsAvx2(NumericRowSpan remote,
                         ConstNumericRowSpan incoming,
                         double retreat,
                         double cap);

}
