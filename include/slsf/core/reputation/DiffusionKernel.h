#pragma once

#include "slsf/core/NumericRow.h"
#include "slsf/core/reputation/DiffusionGraph.h"

namespace slsf {

// Reports whether the current process may execute the AVX2 kernel path; the
// scalar reference is always available.
bool KernelSupportsAvx2() noexcept;

// Validates the CSR invariants a CompiledDiffusionGraph must satisfy before a
// kernel may trust its offsets: one offset per account plus the terminator,
// monotonically non-decreasing, the terminator equal to the operand count,
// and every operand source inside the account range.  Kernel entry points
// reject an invalid graph instead of reading through it.
bool IsValidCompiledGraph(const CompiledDiffusionGraph& graph) noexcept;

// Computes the incoming rows of the compiled graph into `incoming`: each row
// is zeroed first, then every slot j < kFameCategoryCount stores
// max over in-edges(target) of coefficient * max(local, remote)[source].
// Candidates below `cutoff` do not count (a slot whose every candidate is
// below the cutoff receives exactly 0).  Slots kFameCategoryCount..31 stay
// zero.  Results are deterministic and identical for the scalar and AVX2
// backends on identical inputs.
//
// Requirements: incoming/local/remote must hold graph.accountCount rows.  An
// invalid graph or a short span yields zeroed incoming rows instead of an
// out-of-bounds read.
void ComputeIncomingRows(NumericRowSpan incoming,
                         ConstNumericRowSpan local,
                         ConstNumericRowSpan remote,
                         const CompiledDiffusionGraph& graph,
                         double cutoff,
                         DiffusionBackend backend = DiffusionBackend::Auto);

// Advances remote rows toward their incoming values over `elapsedDays`
// (fractional game days since the last settlement step):
//
//   incoming >= remote : arrival, remote = incoming (batch result preserved)
//   incoming <  remote : remote = max(incoming, remote - relaxationPerDay * elapsedDays)
//
// so when a stronger source drops away a row retreats toward the value still
// carried by remaining in-edges, and removed edges fade to zero over time.
// The scalar and AVX2 backends apply the same slot rules and clear padding slots.
void RelaxRemoteRows(NumericRowSpan remote,
                     ConstNumericRowSpan incoming,
                     double elapsedDays,
                     double relaxationPerDay,
                     DiffusionBackend backend = DiffusionBackend::Auto,
                     double maxFame = 150.0);

// The retained in-edge candidate weight of one target slot (0 when every
// candidate is below the cutoff).
double IncomingSlotCandidate(const CompiledDiffusionGraph& graph,
                             std::uint32_t target,
                             std::size_t slot,
                             ConstNumericRowSpan local,
                             ConstNumericRowSpan remote,
                             double cutoff);

}
