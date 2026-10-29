#include <catch2/catch_test_macros.hpp>

#include "slsf/core/Snapshots.h"
#include "slsf/core/location/LocationIndex.h"
#include "slsf/core/location/RegionRegistry.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace {

slsf::StableFormKey Form(std::uint32_t id) {
    return slsf::StableFormKey{"loc.esp", id};
}

slsf::StableRegionKey Region(std::uint32_t id) {
    return slsf::StableRegionKey{"region.esp", id};
}

slsf::LocationRecord Record(std::uint32_t id, std::uint32_t parent,
                            std::optional<std::uint32_t> region) {
    slsf::LocationRecord record;
    record.formId = Form(id);
    record.parentFormId = parent == 0 ? slsf::StableFormKey{} : Form(parent);
    record.displayName = "Location";
    if (region) {
        record.stableKey = Region(*region);
    }
    return record;
}

slsf::LocationIndexSnapshot Tree() {
    auto built = slsf::BuildLocationIndex(std::vector<slsf::LocationRecord>{
        Record(30, 20, std::nullopt),
        Record(10, 0, 1),
        Record(40, 0, 3),
        Record(20, 10, 2),
    });
    REQUIRE(built.has_value());
    return *built;
}

slsf::RegionDefinitions Definitions() {
    slsf::RegionDefinitions definitions;
    definitions.keys = {Region(2), Region(1), Region(3), Region(4)};
    slsf::OwnerRule rule;
    rule.owner = Region(1);
    rule.children = {Region(2)};
    definitions.ownerRules.push_back(rule);
    definitions.customOrder = {Region(3), Region(2), Region(1), Region(4)};
    definitions.apiAliases.push_back(slsf::ApiAlias{"  Capital  ", Region(2)});
    return definitions;
}

slsf::RegionDefinitions OwnerChain(std::uint32_t count, bool rootAtEnd) {
    slsf::RegionDefinitions definitions;
    definitions.keys.reserve(count);
    definitions.ownerRules.reserve(count - 1);
    for (std::uint32_t i = 1; i <= count; ++i) {
        definitions.keys.push_back(Region(i));
        if (i < count) {
            definitions.ownerRules.push_back(slsf::OwnerRule{
                Region(rootAtEnd ? i + 1 : i), {Region(rootAtEnd ? i : i + 1)}});
        }
    }
    return definitions;
}

}

TEST_CASE("region aliases normalize case and whitespace", "[locations]") {
    REQUIRE(slsf::NormalizeAlias("  The   Capital ") == "the capital");
    REQUIRE(slsf::NormalizeAlias("THE CAPITAL") == "the capital");
    REQUIRE(slsf::NormalizeAlias("") == "");
    REQUIRE(slsf::NormalizeAlias("   ") == "");
    REQUIRE(slsf::NormalizeAlias("A\tB\nC") == "a b c");
}

TEST_CASE("regions bind ownership aliases and legacy indices", "[locations]") {
    const auto index = Tree();
    const auto definitions = Definitions();
    const auto bound = slsf::BindRegions(index, definitions);
    REQUIRE(bound.has_value());
    const auto& regions = *bound;

    REQUIRE(regions.regions.size() == 4);
    REQUIRE(regions.regions[0] == Region(1));
    REQUIRE(regions.regions[1] == Region(2));
    REQUIRE(regions.regions[2] == Region(3));
    REQUIRE(regions.regions[3] == Region(4));
    REQUIRE(regions.regionApiIndex[0].value == 2);
    REQUIRE(regions.regionApiIndex[1].value == 1);
    REQUIRE(regions.regionApiIndex[2].value == 0);
    REQUIRE(regions.regionApiIndex[3].value == 3);
    REQUIRE(slsf::FindRegion(regions, Region(3)) == slsf::RegionId(2));
    REQUIRE_FALSE(slsf::FindRegion(regions, Region(99)));

    REQUIRE(slsf::ResolveRegion(regions, slsf::LocationId(0)) == slsf::RegionId(0));
    REQUIRE(slsf::ResolveRegion(regions, slsf::LocationId(1)) == slsf::RegionId(0));
    REQUIRE(slsf::ResolveRegion(regions, slsf::LocationId(3)) == slsf::RegionId(2));
    REQUIRE_FALSE(slsf::ResolveRegion(regions, slsf::LocationId(99)));

    REQUIRE(slsf::ResolveRegion(regions, slsf::LocationId(2)) == slsf::RegionId(0));

    const auto alias = slsf::ResolveLegacyLocation(regions, "capital");
    REQUIRE(alias.has_value());
    REQUIRE(alias->value == 1);
    const auto padded = slsf::ResolveLegacyLocation(regions, "  CAPITAL  ");
    REQUIRE(padded.has_value());
    REQUIRE(padded->value == 1);
    REQUIRE_FALSE(slsf::ResolveLegacyLocation(regions, "unknown").has_value());
    REQUIRE_FALSE(slsf::ResolveLegacyLocation(regions, "  ").has_value());
}

TEST_CASE("owner child regions inherit their owner", "[locations]") {
    const auto index = Tree();
    auto definitions = Definitions();
    definitions.ownerRules[0].children.push_back(Region(3));
    const auto bound = slsf::BindRegions(index, definitions);
    REQUIRE(bound.has_value());
    REQUIRE(slsf::ResolveRegion(*bound, slsf::LocationId(1)) == slsf::RegionId(0));
    REQUIRE(slsf::ResolveRegion(*bound, slsf::LocationId(3)) == slsf::RegionId(0));
}

TEST_CASE("region ownership prefers legacy priority over nearest ancestor", "[locations]") {
    const auto index = Tree();
    auto definitions = Definitions();
    definitions.ownerRules[0].children.push_back(Region(3));
    const auto bound = slsf::BindRegions(index, definitions);
    REQUIRE(bound.has_value());
    REQUIRE(slsf::ResolveRegion(*bound, slsf::LocationId(3)) == slsf::RegionId(0));
}

TEST_CASE("child region registration priority survives ownership remapping",
          "[locations][regression]") {
    const auto index = slsf::BuildLocationIndex(std::vector<slsf::LocationRecord>{
        Record(10, 0, 3), Record(20, 10, 2)});
    REQUIRE(index);
    slsf::RegionDefinitions definitions;
    definitions.keys = {Region(1), Region(2), Region(3)};
    definitions.ownerRules = {slsf::OwnerRule{Region(1), {Region(2)}}};
    definitions.customOrder = {Region(2), Region(3), Region(1)};
    const auto bound = slsf::BindRegions(*index, definitions);
    REQUIRE(bound);
    REQUIRE(slsf::ResolveRegion(*bound, slsf::LocationId(0)) == slsf::RegionId(2));
    REQUIRE(slsf::ResolveRegion(*bound, slsf::LocationId(1)) == slsf::RegionId(0));
}

TEST_CASE("region resolution ignores out of range parent ids", "[locations]") {
    auto index = Tree();
    index.parents[1] = slsf::LocationId(99);
    const auto bound = slsf::BindRegions(index, Definitions());
    REQUIRE(bound.has_value());
    REQUIRE(slsf::ResolveRegion(*bound, slsf::LocationId(1)) == slsf::RegionId(0));
    for (std::uint32_t id = 0; id < 4; ++id) {
        REQUIRE(slsf::ResolveRegion(*bound, slsf::LocationId(id)).value().value < 4);
    }
}

TEST_CASE("registering an independent region preserves unrelated ownership", "[locations]") {
    const auto index = Tree();
    const auto baseline = slsf::BindRegions(index, Definitions());
    REQUIRE(baseline.has_value());

    auto extended = Definitions();
    extended.keys.push_back(Region(5));
    slsf::OwnerRule independent;
    independent.owner = Region(5);
    extended.ownerRules.push_back(independent);
    extended.customOrder.push_back(Region(5));
    const auto grown = slsf::BindRegions(index, extended);
    REQUIRE(grown.has_value());

    REQUIRE(slsf::ResolveRegion(*grown, slsf::LocationId(0)) ==
            slsf::ResolveRegion(*baseline, slsf::LocationId(0)));
    REQUIRE(slsf::ResolveRegion(*grown, slsf::LocationId(3)) ==
            slsf::ResolveRegion(*baseline, slsf::LocationId(3)));
    REQUIRE(slsf::ResolveRegion(*grown, slsf::LocationId(2)) ==
            slsf::ResolveRegion(*baseline, slsf::LocationId(2)));
    REQUIRE(slsf::ResolveLegacyLocation(*grown, "capital")->value == 1);
    REQUIRE(slsf::FindRegion(*grown, Region(5)) == slsf::RegionId(4));
}

TEST_CASE("region binding rejects malformed definitions", "[locations]") {
    const auto index = Tree();

    auto duplicateKeys = Definitions();
    duplicateKeys.keys.push_back(Region(1));
    REQUIRE_FALSE(slsf::BindRegions(index, duplicateKeys).has_value());

    auto emptyPlugin = Definitions();
    emptyPlugin.keys.push_back(slsf::StableRegionKey{"", 9});
    REQUIRE_FALSE(slsf::BindRegions(index, emptyPlugin).has_value());

    auto unknownOwner = Definitions();
    unknownOwner.ownerRules[0].owner = Region(99);
    REQUIRE_FALSE(slsf::BindRegions(index, unknownOwner).has_value());

    auto unknownChild = Definitions();
    unknownChild.ownerRules[0].children.push_back(Region(99));
    REQUIRE_FALSE(slsf::BindRegions(index, unknownChild).has_value());

    auto selfOwned = Definitions();
    selfOwned.ownerRules[0].children.push_back(Region(1));
    REQUIRE_FALSE(slsf::BindRegions(index, selfOwned).has_value());

    auto cycle = Definitions();
    slsf::OwnerRule reverse;
    reverse.owner = Region(2);
    reverse.children = {Region(1)};
    cycle.ownerRules.push_back(reverse);
    REQUIRE_FALSE(slsf::BindRegions(index, cycle).has_value());

    auto duplicateAlias = Definitions();
    duplicateAlias.apiAliases.push_back(slsf::ApiAlias{"CAPITAL", Region(1)});
    REQUIRE_FALSE(slsf::BindRegions(index, duplicateAlias).has_value());

    auto emptyAlias = Definitions();
    emptyAlias.apiAliases.push_back(slsf::ApiAlias{"   ", Region(1)});
    REQUIRE_FALSE(slsf::BindRegions(index, emptyAlias).has_value());

    auto unknownAliasTarget = Definitions();
    unknownAliasTarget.apiAliases.push_back(slsf::ApiAlias{"other", Region(99)});
    REQUIRE_FALSE(slsf::BindRegions(index, unknownAliasTarget).has_value());

    auto unknownCustom = Definitions();
    unknownCustom.customOrder.push_back(Region(99));
    REQUIRE_FALSE(slsf::BindRegions(index, unknownCustom).has_value());

    auto duplicateCustom = Definitions();
    duplicateCustom.customOrder.push_back(Region(1));
    REQUIRE_FALSE(slsf::BindRegions(index, duplicateCustom).has_value());

    auto multipleOwners = Definitions();
    slsf::OwnerRule secondOwner;
    secondOwner.owner = Region(3);
    secondOwner.children = {Region(2)};
    multipleOwners.ownerRules.push_back(secondOwner);
    REQUIRE_FALSE(slsf::BindRegions(index, multipleOwners).has_value());

    auto aliasOutsideCustomOrder = Definitions();
    aliasOutsideCustomOrder.customOrder = {Region(1)};
    REQUIRE_FALSE(slsf::BindRegions(index, aliasOutsideCustomOrder).has_value());
}

TEST_CASE("region binding validates owner graph without recursion", "[locations]") {
    const auto index = Tree();
    auto definitions = Definitions();
    slsf::OwnerRule left;
    left.owner = Region(1);
    left.children = {Region(2)};
    slsf::OwnerRule right;
    right.owner = Region(2);
    right.children = {Region(1)};
    definitions.ownerRules = {left, right};
    REQUIRE_FALSE(slsf::BindRegions(index, definitions).has_value());

    constexpr std::uint32_t count = 20000;
    for (const bool rootAtEnd : {false, true}) {
        const auto bound = slsf::BindRegions(index, OwnerChain(count, rootAtEnd));
        REQUIRE(bound.has_value());
        const auto root = slsf::RegionId(rootAtEnd ? count - 1 : 0);
        for (std::uint32_t i = 0; i < index.records.size(); ++i) {
            REQUIRE(slsf::ResolveRegion(*bound, slsf::LocationId(i)) == root);
        }
    }
}

TEST_CASE("region ownership reuses shared tails and keeps independent roots", "[locations]") {
    auto definitions = Definitions();
    definitions.keys.push_back(Region(5));
    definitions.ownerRules = {
        slsf::OwnerRule{Region(2), {Region(1)}},
        slsf::OwnerRule{Region(4), {Region(2), Region(3)}},
    };
    auto index = Tree();
    index.records.push_back(Record(50, 0, 5));
    index.parents.clear();
    index.entry.clear();
    const auto bound = slsf::BindRegions(index, definitions);
    REQUIRE(bound.has_value());
    for (std::uint32_t i = 0; i < 4; ++i) {
        REQUIRE(slsf::ResolveRegion(*bound, slsf::LocationId(i)) == slsf::RegionId(3));
    }
    REQUIRE(slsf::ResolveRegion(*bound, slsf::LocationId(4)) == slsf::RegionId(4));
}

TEST_CASE("empty custom order keeps owner rules ahead of registration order", "[locations]") {
    const auto built = slsf::BuildLocationIndex(std::vector<slsf::LocationRecord>{
        Record(10, 0, 1),
        Record(20, 10, 2),
    });
    REQUIRE(built);
    slsf::RegionDefinitions definitions;
    definitions.keys = {Region(1), Region(2), Region(3)};
    definitions.ownerRules.push_back(slsf::OwnerRule{Region(3), {Region(2)}});

    const auto bound = slsf::BindRegions(*built, definitions);
    REQUIRE(bound);
    REQUIRE(bound->customOrder == definitions.keys);
    REQUIRE(slsf::ResolveRegion(*bound, slsf::LocationId(0)) == slsf::RegionId(0));
    REQUIRE(slsf::ResolveRegion(*bound, slsf::LocationId(1)) == slsf::RegionId(2));
}

TEST_CASE("partial custom order still yields unique public indices", "[locations]") {
    const auto built = slsf::BuildLocationIndex(std::vector<slsf::LocationRecord>{
        Record(10, 0, 1),
        Record(20, 0, 2),
        Record(30, 0, 3),
    });
    REQUIRE(built);
    slsf::RegionDefinitions definitions;
    definitions.keys = {Region(1), Region(2), Region(3)};
    definitions.customOrder = {Region(3)};

    const auto bound = slsf::BindRegions(*built, definitions);
    REQUIRE(bound);
    REQUIRE(bound->customOrder ==
            std::vector<slsf::StableRegionKey>{Region(3), Region(1), Region(2)});
    REQUIRE(bound->regionApiIndex[0] == slsf::ApiLocationIndex(1));
    REQUIRE(bound->regionApiIndex[1] == slsf::ApiLocationIndex(2));
    REQUIRE(bound->regionApiIndex[2] == slsf::ApiLocationIndex(0));

    const auto nested = slsf::BuildLocationIndex(std::vector<slsf::LocationRecord>{
        Record(10, 20, 1),
        Record(20, 0, 2),
    });
    REQUIRE(nested);
    definitions.ownerRules.push_back(slsf::OwnerRule{Region(2), {Region(1)}});
    const auto nestedBound = slsf::BindRegions(*nested, definitions);
    REQUIRE(nestedBound);
    REQUIRE(slsf::ResolveRegion(*nestedBound, slsf::LocationId(0)) == slsf::RegionId(1));
}

TEST_CASE("hand-built location parent cycles are rejected", "[locations]") {
    slsf::LocationIndexSnapshot index;
    index.records = {Record(10, 20, 1), Record(20, 10, 2)};
    index.parents = {slsf::LocationId(1), slsf::LocationId(0)};
    slsf::RegionDefinitions definitions;
    definitions.keys = {Region(1), Region(2)};

    const auto bound = slsf::BindRegions(index, definitions);
    REQUIRE_FALSE(bound);
    REQUIRE(bound.error().code == slsf::Error::Code::InvalidState);

    slsf::LocationIndexSnapshot byForm;
    byForm.records = {Record(10, 20, 1), Record(20, 10, 2)};
    const auto byFormBound = slsf::BindRegions(byForm, definitions);
    REQUIRE_FALSE(byFormBound);
    REQUIRE(byFormBound.error().code == slsf::Error::Code::InvalidState);
}

TEST_CASE("region ownership detects a long cycle without location references", "[locations]") {
    constexpr std::uint32_t count = 20000;
    auto definitions = OwnerChain(count, true);
    definitions.ownerRules.push_back(slsf::OwnerRule{Region(1), {Region(count)}});
    const auto bound = slsf::BindRegions(slsf::LocationIndexSnapshot{}, definitions);
    REQUIRE_FALSE(bound.has_value());
    REQUIRE(bound.error().code == slsf::Error::Code::InvalidState);
}
