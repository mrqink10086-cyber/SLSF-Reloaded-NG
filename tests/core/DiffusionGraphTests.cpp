#include <catch2/catch_test_macros.hpp>

#include "slsf/core/Snapshots.h"
#include "slsf/core/Types.h"
#include "slsf/core/location/RegionRegistry.h"
#include "slsf/core/reputation/DiffusionGraph.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace {

slsf::RegionSnapshot Regions(const std::vector<slsf::StableRegionKey>& keys) {
    slsf::RegionSnapshot snapshot;
    snapshot.regions = keys;
    return snapshot;
}

slsf::EdgeDefinition Edge(const slsf::StableRegionKey& from, const slsf::StableRegionKey& to,
                          double retention) {
    slsf::EdgeDefinition edge{};
    edge.from = from;
    edge.to = to;
    edge.retention = retention;
    return edge;
}

slsf::LocationRecord Location(std::uint32_t id, const slsf::StableRegionKey& region) {
    slsf::LocationRecord record;
    record.formId = slsf::StableFormKey{"locations.esp", id};
    record.stableKey = region;
    return record;
}

}

TEST_CASE("BuildDiffusionGraph validates edge definitions", "[graph]") {
    const std::vector<slsf::StableRegionKey> keys{
        slsf::StableRegionKey{"a.esp", 1}, slsf::StableRegionKey{"a.esp", 2},
        slsf::StableRegionKey{"b.esp", 1}};
    const auto snapshot = Regions(keys);

    const auto a1 = keys[0];
    const auto a2 = keys[1];
    const auto b1 = keys[2];

    auto unknown =
        slsf::BuildDiffusionGraph(snapshot, {Edge(slsf::StableRegionKey{"z.esp", 9}, a2, 1.0)});
    REQUIRE_FALSE(unknown);

    auto unknownTarget = slsf::BuildDiffusionGraph(snapshot, {Edge(a1, slsf::StableRegionKey{"x.esp", 3}, 0.5)});
    REQUIRE_FALSE(unknownTarget);

    auto undefined = slsf::BuildDiffusionGraph(snapshot, {Edge(slsf::StableRegionKey{}, a2, 1.0)});
    REQUIRE_FALSE(undefined);

    auto self = slsf::BuildDiffusionGraph(snapshot, {Edge(a1, a1, 1.0)});
    REQUIRE_FALSE(self);

    const slsf::EdgeDefinition base[] = {Edge(a1, b1, 0.5)};
    auto duplicate = slsf::BuildDiffusionGraph(snapshot, {base[0], Edge(a1, b1, 0.25)});
    REQUIRE_FALSE(duplicate);

    auto over = slsf::BuildDiffusionGraph(snapshot, {Edge(a1, b1, 1.0 + 1e-9)});
    REQUIRE_FALSE(over);
    auto negative = slsf::BuildDiffusionGraph(snapshot, {Edge(a1, b1, -0.1)});
    REQUIRE_FALSE(negative);
    auto notFinite = slsf::BuildDiffusionGraph(snapshot, {Edge(a1, b1, std::nan("retention"))});
    REQUIRE_FALSE(notFinite);

    auto accepted = slsf::BuildDiffusionGraph(snapshot, {Edge(a1, b1, 1.0), Edge(a1, a2, 0.0)});
    REQUIRE(accepted->edges.size() == 2);
    REQUIRE(accepted->edges[0].retention == 1.0);
    REQUIRE(accepted->edges[1].retention == 0.0);
    REQUIRE(accepted->edges[1].from == a1);
}

TEST_CASE("CompileDiffusionGraph produces a by-target CSR", "[graph]") {
    const slsf::StableRegionKey keys[] = {slsf::StableRegionKey{"esp.esp", 1},
                                          slsf::StableRegionKey{"esp.esp", 2},
                                          slsf::StableRegionKey{"esp.esp", 3}};

    slsf::DiffusionGraph graph;
    graph.edges = {
        Edge(keys[2], keys[0], 0.25),
        Edge(keys[1], keys[0], 1.0),
        Edge(keys[0], keys[2], 0.0),
        Edge(keys[0], keys[1], 0.5),
    };

    slsf::DiffusionGraph looping;
    looping.edges = {Edge(keys[0], keys[0], 0.5)};
    auto looped = slsf::CompileDiffusionGraph(looping, keys);
    REQUIRE_FALSE(looped);

    const std::vector<slsf::StableRegionKey> duplicateKeys{keys[0], keys[0], keys[2]};
    slsf::DiffusionGraph duplicateGraph;
    duplicateGraph.edges = {Edge(keys[0], keys[2], 1.0)};
    auto duplicate = slsf::CompileDiffusionGraph(duplicateGraph, duplicateKeys);
    REQUIRE_FALSE(duplicate);

    slsf::DiffusionGraph badRetention;
    badRetention.edges = {Edge(keys[0], keys[1], 2.0)};
    REQUIRE_FALSE(slsf::CompileDiffusionGraph(badRetention, keys));
    slsf::DiffusionGraph nanRetention;
    nanRetention.edges = {Edge(keys[0], keys[1], std::nan("retention"))};
    REQUIRE_FALSE(slsf::CompileDiffusionGraph(nanRetention, keys));

    auto compiled = slsf::CompileDiffusionGraph(graph, keys);
    REQUIRE(compiled.has_value());
    REQUIRE(compiled->accountCount == 3);

    REQUIRE(compiled->inEdgeOffsets == std::vector<std::uint32_t>{0, 2, 3, 3});
    REQUIRE(compiled->operands.size() == 3);
    REQUIRE(compiled->operands[0].source == 1);
    REQUIRE(compiled->operands[0].coefficient == 1.0);
    REQUIRE(compiled->operands[1].source == 2);
    REQUIRE(compiled->operands[1].coefficient == 0.25);
    REQUIRE(compiled->operands[2].source == 0);
    REQUIRE(compiled->operands[2].coefficient == 0.5);
}

TEST_CASE("CompileDiffusionGraph rejects edges outside the numeric rows", "[graph]") {
    const slsf::StableRegionKey keys[] = {slsf::StableRegionKey{"esp.esp", 1},
                                          slsf::StableRegionKey{"esp.esp", 2}};
    slsf::DiffusionGraph graph;
    graph.edges = {Edge(slsf::StableRegionKey{"elsewhere.esp", 4}, keys[0], 1.0)};

    auto compiled = slsf::CompileDiffusionGraph(graph, keys);
    REQUIRE_FALSE(compiled);

    slsf::DiffusionGraph empty;
    empty.edges = {};
    auto noAccounts = slsf::CompileDiffusionGraph(empty, {});
    REQUIRE_FALSE(noAccounts);
}

TEST_CASE("CompileDiffusionGraph projects canonical owners and merges parallel edges",
          "[graph][locations]") {
    const slsf::StableRegionKey owner{"regions.esp", 1};
    const slsf::StableRegionKey child{"regions.esp", 2};
    const slsf::StableRegionKey independent{"regions.esp", 3};
    const slsf::StableRegionKey nestedChild{"regions.esp", 4};
    const auto locationIndex = slsf::BuildLocationIndex(std::vector<slsf::LocationRecord>{
        Location(1, owner), Location(2, child), Location(3, independent),
        Location(4, nestedChild)});
    REQUIRE(locationIndex);

    slsf::RegionDefinitions definitions;
    definitions.keys = {owner, child, independent, nestedChild};
    definitions.ownerRules = {{owner, {child}}, {child, {nestedChild}}};
    definitions.customOrder = {independent, owner, child, nestedChild};
    const auto regions = slsf::BindRegions(*locationIndex, definitions);
    REQUIRE(regions);
    const auto ownerId = slsf::FindRegion(*regions, owner);
    const auto independentId = slsf::FindRegion(*regions, independent);
    REQUIRE(ownerId);
    REQUIRE(independentId);
    REQUIRE(slsf::ResolveRegion(*regions, slsf::LocationId(0)) == ownerId);
    REQUIRE(slsf::ResolveRegion(*regions, slsf::LocationId(1)) == ownerId);
    REQUIRE(slsf::ResolveRegion(*regions, slsf::LocationId(2)) == independentId);
    REQUIRE(slsf::ResolveRegion(*regions, slsf::LocationId(3)) == ownerId);

    const auto graph = slsf::BuildDiffusionGraph(
        *regions, {Edge(child, independent, 0.35), Edge(owner, independent, 0.8),
                   Edge(independent, nestedChild, 0.6), Edge(nestedChild, child, 0.9)});
    REQUIRE(graph);
    const auto& accountKeys = regions->regions;

    const auto unprojected = slsf::CompileDiffusionGraph(*graph, accountKeys);
    REQUIRE(unprojected);
    REQUIRE(unprojected->operands.size() == 4);
    REQUIRE(unprojected->inEdgeOffsets == std::vector<std::uint32_t>{0, 0, 1, 3, 4});
    REQUIRE(unprojected->operands[0].source == 3);
    REQUIRE(unprojected->operands[0].coefficient == 0.9);
    REQUIRE(unprojected->operands[2].source == 1);
    REQUIRE(unprojected->operands[2].coefficient == 0.35);

    const auto projected = slsf::CompileDiffusionGraph(*graph, *regions, accountKeys);
    REQUIRE(projected);
    REQUIRE(projected->accountCount == 4);
    REQUIRE(projected->inEdgeOffsets == std::vector<std::uint32_t>{0, 1, 1, 2, 2});
    REQUIRE(projected->operands.size() == 2);
    REQUIRE(projected->operands[0].source == independentId->value);
    REQUIRE(projected->operands[0].coefficient == 0.6);
    REQUIRE(projected->operands[1].source == ownerId->value);
    REQUIRE(projected->operands[1].coefficient == 0.8);
}

TEST_CASE("CompileDiffusionGraph resolves long owner chains iteratively", "[graph][locations]") {
    constexpr std::uint32_t chainLength = 4096;
    slsf::RegionDefinitions definitions;
    definitions.keys.reserve(chainLength + 1);
    definitions.ownerRules.reserve(chainLength);
    for (std::uint32_t id = 1; id <= chainLength + 1; ++id) {
        const slsf::StableRegionKey key{"regions.esp", id};
        definitions.keys.push_back(key);
        if (id < chainLength) {
            definitions.ownerRules.push_back(
                slsf::OwnerRule{key, {slsf::StableRegionKey{"regions.esp", id + 1}}});
        }
    }
    const slsf::StableRegionKey chainTail{"regions.esp", chainLength};
    const slsf::StableRegionKey independent{"regions.esp", chainLength + 1};
    const auto locationIndex = slsf::BuildLocationIndex(
        std::vector<slsf::LocationRecord>{Location(1, chainTail), Location(2, independent)});
    REQUIRE(locationIndex);
    const auto regions = slsf::BindRegions(*locationIndex, definitions);
    REQUIRE(regions);

    const auto graph = slsf::BuildDiffusionGraph(*regions, {Edge(chainTail, independent, 0.75)});
    REQUIRE(graph);
    const auto compiled = slsf::CompileDiffusionGraph(*graph, *regions, regions->regions);
    REQUIRE(compiled);
    REQUIRE(compiled->accountCount == chainLength + 1);
    REQUIRE(compiled->operands.size() == 1);
    REQUIRE(compiled->operands[0].source == 0);
    REQUIRE(compiled->operands[0].coefficient == 0.75);
}

TEST_CASE("BuildLegacyDefaultGraph expands the JSON topology", "[graph]") {
    const std::array<std::string_view, slsf::kLegacyDefaultNodeCount> expectedNodes{
        {"whiterun", "winterhold", "windhelm", "solitude", "riften", "markarth",
         "morthal", "dawnstar", "falkreath", "raven_rock", "riverwood", "rorikstead",
         "ivarstead", "shors_stone", "dragon_bridge", "karthwasten", "skaal_village",
         "largashbur", "dushnikh_yal", "mor_khazgur", "narzulbur"}};
    const std::array<std::string_view, slsf::kLegacyDefaultLinkCount> expectedLinks{
        {"dawnstar|whiterun", "whiterun|windhelm", "riverwood|whiterun",
         "rorikstead|whiterun", "ivarstead|whiterun", "falkreath|whiterun",
         "dawnstar|winterhold", "windhelm|winterhold", "shors_stone|windhelm",
         "narzulbur|windhelm", "raven_rock|windhelm", "dragon_bridge|solitude",
         "morthal|solitude", "dawnstar|solitude", "riften|shors_stone",
         "ivarstead|riften", "largashbur|riften", "karthwasten|markarth",
         "markarth|rorikstead", "dushnikh_yal|markarth", "falkreath|markarth",
         "dawnstar|morthal", "dragon_bridge|morthal", "morthal|rorikstead",
         "falkreath|riverwood", "falkreath|ivarstead", "falkreath|rorikstead",
         "raven_rock|skaal_village", "riverwood|rorikstead", "ivarstead|riverwood",
         "dushnikh_yal|rorikstead", "karthwasten|rorikstead",
         "dragon_bridge|rorikstead", "ivarstead|shors_stone", "ivarstead|largashbur",
         "narzulbur|shors_stone", "dragon_bridge|karthwasten",
         "dragon_bridge|mor_khazgur", "karthwasten|mor_khazgur"}};

    std::vector<slsf::LegacyDefaultNode> bindings;
    for (std::size_t index = 0; index < expectedNodes.size(); ++index) {
        bindings.push_back(slsf::LegacyDefaultNode{
            expectedNodes[index],
            slsf::StableRegionKey{"topology.esp", static_cast<std::uint32_t>(index + 1)}});
    }

    auto graph = slsf::BuildLegacyDefaultGraph(bindings, 0.6);
    REQUIRE(graph.has_value());
    REQUIRE(graph->edges.size() == slsf::kLegacyDefaultEdgeCount);

    auto nameOf = [&](const slsf::StableRegionKey& key) -> std::string {
        for (std::size_t index = 0; index < bindings.size(); ++index) {
            if (bindings[index].stable == key) {
                return std::string(bindings[index].nodeKey);
            }
        }
        return {};
    };
    std::vector<std::string> actual;
    actual.reserve(graph->edges.size());
    for (const auto& edge : graph->edges) {
        REQUIRE(edge.retention == 0.6);
        actual.push_back(nameOf(edge.from) + "|" + nameOf(edge.to));
    }
    std::sort(actual.begin(), actual.end());
    REQUIRE(std::adjacent_find(actual.begin(), actual.end()) == actual.end());

    std::vector<std::string> expected;
    expected.reserve(expectedLinks.size() * 2);
    for (const auto link : expectedLinks) {
        const auto separator = link.find('|');
        REQUIRE(separator != std::string_view::npos);
        const std::string from(link.substr(0, separator));
        const std::string to(link.substr(separator + 1));
        expected.push_back(from + "|" + to);
        expected.push_back(to + "|" + from);
    }
    std::sort(expected.begin(), expected.end());
    REQUIRE(actual == expected);

    std::vector<slsf::StableRegionKey> accountKeys;
    accountKeys.reserve(bindings.size());
    for (const auto& binding : bindings) {
        accountKeys.push_back(binding.stable);
    }
    std::sort(accountKeys.begin(), accountKeys.end());
    const auto compiled = slsf::CompileDiffusionGraph(*graph, accountKeys);
    REQUIRE(compiled);
    REQUIRE(compiled->accountCount == slsf::kLegacyDefaultNodeCount);
    REQUIRE(compiled->operands.size() == slsf::kLegacyDefaultEdgeCount);

    for (const auto& node : expectedNodes) {
        REQUIRE(std::any_of(actual.begin(), actual.end(), [&](const std::string& pair) {
            return pair.rfind(std::string(node) + "|", 0) == 0;
        }));
    }

    bindings[5].stable = slsf::StableRegionKey{};
    auto unbound = slsf::BuildLegacyDefaultGraph(bindings, 0.6);
    REQUIRE_FALSE(unbound);
    bindings[5].stable = slsf::StableRegionKey{"topology.esp", 6};
    bindings[5].nodeKey = "not_a_node";
    auto unknownNode = slsf::BuildLegacyDefaultGraph(bindings, 0.6);
    REQUIRE_FALSE(unknownNode);
    bindings[5].nodeKey = "falkreath";

    const auto solitudeStable = bindings[3].stable;
    bindings[3].stable = bindings[2].stable;
    REQUIRE_FALSE(slsf::BuildLegacyDefaultGraph(bindings, 0.6));
    bindings[3].stable = solitudeStable;

    bindings[1].stable = bindings[3].stable;
    REQUIRE_FALSE(slsf::BuildLegacyDefaultGraph(bindings, 0.6));
    bindings[1].stable = slsf::StableRegionKey{"topology.esp", 2};

    auto shortBindings = slsf::BuildLegacyDefaultGraph({bindings.begin(), bindings.begin() + 3}, 0.6);
    REQUIRE_FALSE(shortBindings);

    auto retention = slsf::BuildLegacyDefaultGraph(bindings, 1.5);
    REQUIRE_FALSE(retention);
}
