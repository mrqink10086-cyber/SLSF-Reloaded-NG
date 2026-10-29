#pragma once

#include "slsf/core/Snapshots.h"
#include "slsf/core/Types.h"

#include <array>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace slsf {

// Operand indices are dense account indices (owning RegionId values).
struct CompiledDiffusionGraph {
    struct Operand {
        std::uint32_t source{};
        double coefficient{};
    };

    std::vector<Operand> operands;
    std::vector<std::uint32_t> inEdgeOffsets;
    std::uint32_t accountCount{};
};

// Each edge must reference known region keys, must not be a self loop, and must
// be unique per (from, to) pair.  Retention is finite; 0.0 keeps a disabled
// edge (compiled out), 0 < retention <= 1 transmits, values outside are
// invalid.  Edge order follows the input order.
Result<DiffusionGraph> BuildDiffusionGraph(const RegionSnapshot& regions,
                                           std::vector<EdgeDefinition> edges);

// Compiles a validated graph into the by-target CSR over account indices.
// accountKeys must be the sorted stable keys of the numeric rows (the same
// ordering used by BindStateToRegions).  Edges referencing keys outside
// accountKeys are a configuration error.
Result<CompiledDiffusionGraph> CompileDiffusionGraph(const DiffusionGraph& graph,
                                                     std::span<const StableRegionKey> accountKeys);

// Maps endpoints to canonical owners while preserving the sorted account-key row order.
// Drops owner-internal edges; projected parallel edges keep the greatest retention.
Result<CompiledDiffusionGraph> CompileDiffusionGraph(const DiffusionGraph& graph,
                                                     const RegionSnapshot& regions,
                                                     std::span<const StableRegionKey> accountKeys);

struct LegacyDefaultNode {
    std::string_view nodeKey;   // research topology key, e.g. "whiterun".
    StableRegionKey stable;
};

constexpr std::size_t kLegacyDefaultNodeCount = 21;
constexpr std::size_t kLegacyDefaultLinkCount = 39;
constexpr std::size_t kLegacyDefaultEdgeCount = 78;

// Builds the 78 directed default edges from SLSF_Reloaded_FameManager.psc
// default-to-default links (39 bidirectional pairs over 21 locations), applying
// defaultRetention to each.  Every research node must have exactly one binding.
Result<DiffusionGraph> BuildLegacyDefaultGraph(std::span<const LegacyDefaultNode> bindings,
                                               double defaultRetention);

}
