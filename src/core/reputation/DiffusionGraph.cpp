#include "slsf/core/reputation/DiffusionGraph.h"

#include "slsf/core/Snapshots.h"
#include "slsf/core/location/RegionRegistry.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <unordered_map>

namespace slsf {
namespace {

bool IsFiniteRetention(double value) noexcept {
    return std::isfinite(value) && value > 0.0 && value <= 1.0;
}

// Research default topology: 21 locations, 39 undirected links.  Node order
// and link list are copied verbatim from
// docs/research/default-reputation-topology.proposed.json.
constexpr std::array<std::string_view, kLegacyDefaultNodeCount> kLegacyDefaultNodes{{
    "whiterun",    "winterhold", "windhelm",  "solitude",     "riften",
    "markarth",    "morthal",    "dawnstar",  "falkreath",    "raven_rock",
    "riverwood",   "rorikstead", "ivarstead", "shors_stone",  "dragon_bridge",
    "karthwasten", "skaal_village", "largashbur", "dushnikh_yal", "mor_khazgur",
    "narzulbur",
}};

constexpr std::array<std::pair<std::string_view, std::string_view>, kLegacyDefaultLinkCount>
    kLegacyDefaultLinks{{{"dawnstar", "whiterun"},       {"whiterun", "windhelm"},
                         {"riverwood", "whiterun"},      {"rorikstead", "whiterun"},
                         {"ivarstead", "whiterun"},      {"falkreath", "whiterun"},
                         {"dawnstar", "winterhold"},     {"windhelm", "winterhold"},
                         {"shors_stone", "windhelm"},    {"narzulbur", "windhelm"},
                         {"raven_rock", "windhelm"},     {"dragon_bridge", "solitude"},
                         {"morthal", "solitude"},        {"dawnstar", "solitude"},
                         {"riften", "shors_stone"},      {"ivarstead", "riften"},
                         {"largashbur", "riften"},       {"karthwasten", "markarth"},
                         {"markarth", "rorikstead"},     {"dushnikh_yal", "markarth"},
                         {"falkreath", "markarth"},      {"dawnstar", "morthal"},
                         {"dragon_bridge", "morthal"},   {"morthal", "rorikstead"},
                         {"falkreath", "riverwood"},     {"falkreath", "ivarstead"},
                         {"falkreath", "rorikstead"},    {"raven_rock", "skaal_village"},
                         {"riverwood", "rorikstead"},    {"ivarstead", "riverwood"},
                         {"dushnikh_yal", "rorikstead"}, {"karthwasten", "rorikstead"},
                         {"dragon_bridge", "rorikstead"}, {"ivarstead", "shors_stone"},
                         {"ivarstead", "largashbur"},    {"narzulbur", "shors_stone"},
                         {"dragon_bridge", "karthwasten"}, {"dragon_bridge", "mor_khazgur"},
                         {"karthwasten", "mor_khazgur"}}};

const std::string_view* FindDefaultNode(std::string_view nodeKey) {
    const auto it = std::find(kLegacyDefaultNodes.begin(), kLegacyDefaultNodes.end(), nodeKey);
    if (it == kLegacyDefaultNodes.end()) {
        return nullptr;
    }
    return &*it;
}

Result<std::vector<RegionId>> BuildCanonicalOwnerByRegion(const RegionSnapshot& regions) {
    if (regions.regions.size() > RegionId::invalid) {
        return Fail(Error::Code::InvalidArgument, "too many regions to compile a graph for");
    }
    if (!std::is_sorted(regions.regions.begin(), regions.regions.end())) {
        return Fail(Error::Code::InvalidArgument, "region keys are not sorted");
    }
    if (std::adjacent_find(regions.regions.begin(), regions.regions.end()) != regions.regions.end()) {
        return Fail(Error::Code::InvalidArgument, "duplicate region key");
    }

    const auto count = regions.regions.size();
    std::vector<RegionId> ownerOf(count);
    std::vector<std::uint8_t> hasOwnerRule(count, 0);
    for (const OwnerRule& rule : regions.ownership) {
        const auto owner = FindRegion(regions, rule.owner);
        if (!owner) {
            return Fail(Error::Code::InvalidArgument, "owner rule references an unknown region");
        }
        if (hasOwnerRule[owner->value] != 0) {
            return Fail(Error::Code::InvalidArgument, "duplicate owner rule");
        }
        hasOwnerRule[owner->value] = 1;
        for (const StableRegionKey& childKey : rule.children) {
            const auto child = FindRegion(regions, childKey);
            if (!child) {
                return Fail(Error::Code::InvalidArgument,
                            "owner rule references an unknown child region");
            }
            if (*child == *owner) {
                return Fail(Error::Code::InvalidArgument, "region owns itself");
            }
            if (ownerOf[child->value]) {
                return Fail(Error::Code::InvalidArgument, "region has multiple owners");
            }
            ownerOf[child->value] = *owner;
        }
    }

    std::vector<RegionId> canonical(count);
    std::vector<std::uint8_t> visiting(count, 0);
    std::vector<RegionId> path;
    path.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        if (canonical[index]) {
            continue;
        }
        path.clear();
        auto current = RegionId(static_cast<std::uint32_t>(index));
        while (!canonical[current.value]) {
            if (visiting[current.value] != 0) {
                return Fail(Error::Code::InvalidArgument, "region ownership cycle");
            }
            visiting[current.value] = 1;
            path.push_back(current);
            if (!ownerOf[current.value]) {
                canonical[current.value] = current;
                break;
            }
            current = ownerOf[current.value];
        }
        const auto root = canonical[current.value];
        for (const RegionId region : path) {
            canonical[region.value] = root;
            visiting[region.value] = 0;
        }
    }
    return canonical;
}

}

Result<DiffusionGraph> BuildDiffusionGraph(const RegionSnapshot& regions,
                                           std::vector<EdgeDefinition> edges) {
    DiffusionGraph built;
    built.edges.reserve(edges.size());

    auto resolve = [&regions](const StableRegionKey& key,
                              std::string_view what) -> Result<std::monostate> {
        if (key.plugin.empty()) {
            return Fail(Error::Code::InvalidArgument, std::string(what) + " references no plugin");
        }
        if (!FindRegion(regions, key)) {
            return Fail(Error::Code::InvalidArgument, std::string(what) + " references an unknown region");
        }
        return {};
    };

    for (const EdgeDefinition& edge : edges) {
        if (auto fromCheck = resolve(edge.from, "edge source"); !fromCheck) {
            return std::unexpected(fromCheck.error());
        }
        if (auto toCheck = resolve(edge.to, "edge target"); !toCheck) {
            return std::unexpected(toCheck.error());
        }
        if (edge.from == edge.to) {
            return Fail(Error::Code::InvalidArgument, "diffusion edges must not self loop");
        }
        if (!std::isfinite(edge.retention)) {
            return Fail(Error::Code::InvalidArgument, "edge retention is not finite");
        }
        if (edge.retention < 0.0 || edge.retention > 1.0) {
            return Fail(Error::Code::InvalidArgument, "edge retention out of range");
        }
        for (const EdgeDefinition& existing : built.edges) {
            if (existing.from == edge.from && existing.to == edge.to) {
                return Fail(Error::Code::InvalidArgument, "duplicate diffusion edge");
            }
        }
        built.edges.push_back(edge);
    }

    return built;
}

Result<CompiledDiffusionGraph> CompileDiffusionGraph(const DiffusionGraph& graph,
                                                     std::span<const StableRegionKey> accountKeys) {
    if (accountKeys.size() >= std::numeric_limits<std::uint32_t>::max()) {
        return Fail(Error::Code::InvalidArgument, "too many accounts to compile a graph for");
    }
    const auto count = static_cast<std::uint32_t>(accountKeys.size());
    if (count == 0) {
        return Fail(Error::Code::InvalidArgument, "no accounts to compile a graph for");
    }
    if (!std::is_sorted(accountKeys.begin(), accountKeys.end())) {
        return Fail(Error::Code::InvalidArgument, "account keys are not sorted");
    }
    if (std::adjacent_find(accountKeys.begin(), accountKeys.end()) != accountKeys.end()) {
        return Fail(Error::Code::InvalidArgument, "duplicate account key");
    }

    CompiledDiffusionGraph compiled;
    compiled.accountCount = count;
    compiled.inEdgeOffsets.assign(count + 1, 0);
    compiled.operands.reserve(graph.edges.size());

    struct ResolvedEdge {
        std::uint32_t source;
        std::uint32_t target;
        double coefficient;
    };
    std::vector<ResolvedEdge> resolved;
    resolved.reserve(graph.edges.size());

    for (const EdgeDefinition& edge : graph.edges) {
        if (edge.retention == 0.0) {
            continue;
        }
        if (!std::isfinite(edge.retention) || edge.retention < 0.0 ||
            edge.retention > 1.0) {
            return Fail(Error::Code::InvalidArgument, "diffusion retention out of range");
        }
        const auto source = std::lower_bound(accountKeys.begin(), accountKeys.end(), edge.from);
        const auto target = std::lower_bound(accountKeys.begin(), accountKeys.end(), edge.to);
        if (source == accountKeys.end() || *source != edge.from || target == accountKeys.end() ||
            *target != edge.to) {
            return Fail(Error::Code::InvalidArgument, "edge references a region outside the numeric rows");
        }
        if (source == target) {
            return Fail(Error::Code::InvalidArgument, "diffusion edges reject self loops");
        }
        ResolvedEdge resolvedEdge{};
        resolvedEdge.source = static_cast<std::uint32_t>(source - accountKeys.begin());
        resolvedEdge.target = static_cast<std::uint32_t>(target - accountKeys.begin());
        resolvedEdge.coefficient = edge.retention;
        resolved.push_back(resolvedEdge);
    }

    std::sort(resolved.begin(), resolved.end(), [](const ResolvedEdge& a, const ResolvedEdge& b) {
        if (a.target != b.target) {
            return a.target < b.target;
        }
        return a.source < b.source;
    });
    if (resolved.size() >= std::numeric_limits<std::uint32_t>::max()) {
        return Fail(Error::Code::InvalidArgument, "too many diffusion operands to compile");
    }
    for (std::size_t index = 1; index < resolved.size(); ++index) {
        if (resolved[index].target == resolved[index - 1].target &&
            resolved[index].source == resolved[index - 1].source) {
            return Fail(Error::Code::InvalidArgument, "duplicate diffusion edge");
        }
    }

    for (const ResolvedEdge& resolvedEdge : resolved) {
        ++compiled.inEdgeOffsets[resolvedEdge.target + 1];
    }
    for (std::uint32_t target = 1; target <= count; ++target) {
        compiled.inEdgeOffsets[target] += compiled.inEdgeOffsets[target - 1];
    }
    compiled.operands.resize(resolved.size());
    for (const ResolvedEdge& resolvedEdge : resolved) {
        const auto offset = compiled.inEdgeOffsets[resolvedEdge.target]++;
        compiled.operands[offset].source = resolvedEdge.source;
        compiled.operands[offset].coefficient = resolvedEdge.coefficient;
    }
    for (std::uint32_t target = count; target > 0; --target) {
        compiled.inEdgeOffsets[target] = compiled.inEdgeOffsets[target - 1];
    }
    compiled.inEdgeOffsets[0] = 0;

    return compiled;
}

Result<CompiledDiffusionGraph> CompileDiffusionGraph(const DiffusionGraph& graph,
                                                     const RegionSnapshot& regions,
                                                     std::span<const StableRegionKey> accountKeys) {
    const auto canonicalOwners = BuildCanonicalOwnerByRegion(regions);
    if (!canonicalOwners) {
        return std::unexpected(canonicalOwners.error());
    }

    DiffusionGraph projected;
    projected.edges.reserve(graph.edges.size());
    for (const EdgeDefinition& edge : graph.edges) {
        if (!std::isfinite(edge.retention) || edge.retention < 0.0 || edge.retention > 1.0) {
            return Fail(Error::Code::InvalidArgument, "diffusion retention out of range");
        }
        if (edge.from == edge.to) {
            return Fail(Error::Code::InvalidArgument, "diffusion edges reject self loops");
        }
        const auto source = FindRegion(regions, edge.from);
        const auto target = FindRegion(regions, edge.to);
        if (!source || !target) {
            return Fail(Error::Code::InvalidArgument,
                        "diffusion edge references an unknown region");
        }
        const auto canonicalSource = (*canonicalOwners)[source->value];
        const auto canonicalTarget = (*canonicalOwners)[target->value];
        if (canonicalSource == canonicalTarget) {
            continue;
        }
        projected.edges.push_back(EdgeDefinition{regions.regions[canonicalSource.value],
                                                  regions.regions[canonicalTarget.value],
                                                  edge.retention});
    }

    std::sort(projected.edges.begin(), projected.edges.end(),
              [](const EdgeDefinition& lhs, const EdgeDefinition& rhs) {
                  if (lhs.from != rhs.from) {
                      return lhs.from < rhs.from;
                  }
                  return lhs.to < rhs.to;
              });
    std::vector<EdgeDefinition> coalesced;
    coalesced.reserve(projected.edges.size());
    for (const EdgeDefinition& edge : projected.edges) {
        if (!coalesced.empty() && coalesced.back().from == edge.from &&
            coalesced.back().to == edge.to) {
            coalesced.back().retention = std::max(coalesced.back().retention, edge.retention);
        } else {
            coalesced.push_back(edge);
        }
    }
    projected.edges = std::move(coalesced);
    return CompileDiffusionGraph(projected, accountKeys);
}

Result<DiffusionGraph> BuildLegacyDefaultGraph(std::span<const LegacyDefaultNode> bindings,
                                               double defaultRetention) {
    if (!std::isfinite(defaultRetention) || defaultRetention < 0.0 ||
        defaultRetention > 1.0) {
        return Fail(Error::Code::InvalidArgument, "default retention out of range");
    }
    if (bindings.size() != kLegacyDefaultNodeCount) {
        return Fail(Error::Code::InvalidArgument, "the legacy default topology binds 21 locations");
    }
    std::unordered_map<std::string_view, const StableRegionKey*> bound;
    bound.reserve(kLegacyDefaultNodeCount);
    for (const LegacyDefaultNode& binding : bindings) {
        const auto key = FindDefaultNode(binding.nodeKey);
        if (key == nullptr) {
            return Fail(Error::Code::InvalidArgument, "unknown legacy topology node");
        }
        if (binding.stable.plugin.empty()) {
            return Fail(Error::Code::InvalidArgument, "unbound legacy topology node");
        }
        if (!bound.emplace(binding.nodeKey, &binding.stable).second) {
            return Fail(Error::Code::InvalidArgument, "duplicate legacy topology binding");
        }
    }

    for (auto first = bound.begin(); first != bound.end(); ++first) {
        for (auto second = std::next(first); second != bound.end(); ++second) {
            if (*first->second == *second->second) {
                return Fail(Error::Code::InvalidArgument,
                            "legacy topology binds two nodes to the same region");
            }
        }
    }

    DiffusionGraph built;
    built.edges.reserve(kLegacyDefaultEdgeCount);
    for (std::size_t first = 0; first < kLegacyDefaultLinks.size(); ++first) {
        for (std::size_t second = first + 1; second < kLegacyDefaultLinks.size(); ++second) {
            const auto& [a, b] = kLegacyDefaultLinks[first];
            const auto& [c, d] = kLegacyDefaultLinks[second];
            if (a == b) {
                return Fail(Error::Code::InvalidArgument, "legacy topology link is a self loop");
            }
            if ((a == c && b == d) || (a == d && b == c)) {
                return Fail(Error::Code::InvalidArgument, "duplicate legacy topology link");
            }
        }
    }
    for (const auto& [a, b] : kLegacyDefaultLinks) {
        for (const auto [src, dst] : {std::pair{a, b}, std::pair{b, a}}) {
            const auto source = bound.find(src);
            const auto target = bound.find(dst);
            if (source == bound.end() || target == bound.end()) {
                return Fail(Error::Code::InvalidArgument,
                            "legacy topology references an unbound node");
            }
            if (source->second == target->second) {
                return Fail(Error::Code::InvalidArgument,
                            "legacy topology binds two nodes to the same region");
            }
            EdgeDefinition edge{};
            edge.from = *source->second;
            edge.to = *target->second;
            edge.retention = defaultRetention;
            built.edges.push_back(edge);
        }
    }
    return built;
}

}
