#include <catch2/catch_test_macros.hpp>

#include "LocationReference.h"
#include "slsf/core/Snapshots.h"
#include "slsf/core/location/LocationIndex.h"
#include "slsf/core/location/RegionRegistry.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace {

slsf::StableFormKey Form(std::uint32_t id, std::string plugin = "loc.esp") {
    return slsf::StableFormKey{std::move(plugin), id};
}

slsf::StableRegionKey Region(std::uint32_t id) {
    return slsf::StableRegionKey{"region.esp", id};
}

slsf::LocationRecord Record(std::uint32_t id, std::uint32_t parent, std::string name,
                            std::optional<std::uint32_t> region = std::nullopt) {
    slsf::LocationRecord record;
    record.formId = Form(id);
    record.parentFormId = parent == 0 ? slsf::StableFormKey{} : Form(parent);
    record.displayName = std::move(name);
    if (region) {
        record.stableKey = Region(*region);
    }
    return record;
}

std::optional<slsf::LocationId> IdOf(const slsf::LocationIndexSnapshot& index,
                                     std::uint32_t formId) {
    return slsf::FindLocation(index, Form(formId));
}

}

TEST_CASE("location index builds dense sorted identities and intervals", "[locations]") {
    const std::vector<slsf::LocationRecord> records = {
        Record(30, 20, "Same"),
        Record(10, 0, "Capital"),
        Record(40, 10, "Same"),
        Record(20, 10, "District"),
    };

    const auto built = slsf::BuildLocationIndex(records);
    REQUIRE(built.has_value());
    const auto& index = *built;
    REQUIRE(index.records.size() == 4);
    REQUIRE(index.records[0].formId.localFormId == 10);
    REQUIRE(index.records[1].formId.localFormId == 20);
    REQUIRE(index.records[2].formId.localFormId == 30);
    REQUIRE(index.records[3].formId.localFormId == 40);

    const auto capital = IdOf(index, 10);
    const auto district = IdOf(index, 20);
    const auto shop = IdOf(index, 30);
    const auto same = IdOf(index, 40);
    REQUIRE(capital);
    REQUIRE(district);
    REQUIRE(shop);
    REQUIRE(same);
    REQUIRE(slsf::IsAncestor(index, *capital, *shop, false));
    REQUIRE(slsf::IsAncestor(index, *capital, *shop, true));
    REQUIRE_FALSE(slsf::IsAncestor(index, *shop, *capital, false));
    REQUIRE_FALSE(slsf::IsAncestor(index, *capital, *capital, false));
    REQUIRE(slsf::IsAncestor(index, *capital, *capital, true));
    REQUIRE_FALSE(slsf::IsAncestor(index, *district, *same, true));
    REQUIRE(slsf::IsAncestor(index, *capital, *same, false));
    REQUIRE_FALSE(slsf::IsAncestor(index, slsf::LocationId{}, *same, true));
    REQUIRE_FALSE(slsf::IsAncestor(index, *capital, slsf::LocationId(99), true));
    REQUIRE_FALSE(IdOf(index, 0));
    REQUIRE_FALSE(IdOf(index, 5));
    REQUIRE_FALSE(IdOf(index, 50));
    REQUIRE_FALSE(slsf::FindLocation(index, slsf::StableFormKey{"other.esp", 10}));
}

TEST_CASE("location index reports malformed forests", "[locations]") {
    REQUIRE(slsf::BuildLocationIndex(std::vector<slsf::LocationRecord>{}).has_value());

    auto duplicate = std::vector<slsf::LocationRecord>{
        Record(10, 0, "A"),
        Record(10, 0, "B"),
    };
    REQUIRE_FALSE(slsf::BuildLocationIndex(duplicate).has_value());

    auto missing = std::vector<slsf::LocationRecord>{Record(10, 20, "A")};
    REQUIRE_FALSE(slsf::BuildLocationIndex(missing).has_value());

    auto cycle = std::vector<slsf::LocationRecord>{
        Record(10, 20, "A"),
        Record(20, 10, "B"),
    };
    REQUIRE_FALSE(slsf::BuildLocationIndex(cycle).has_value());

    auto self = std::vector<slsf::LocationRecord>{Record(10, 10, "A")};
    REQUIRE_FALSE(slsf::BuildLocationIndex(self).has_value());

    auto badParent = std::vector<slsf::LocationRecord>{Record(10, 0, "A")};
    badParent[0].parentFormId = slsf::StableFormKey{"", 10};
    REQUIRE_FALSE(slsf::BuildLocationIndex(badParent).has_value());
}

TEST_CASE("deep location chains are iterative", "[locations]") {
    std::vector<slsf::LocationRecord> records;
    constexpr std::uint32_t count = 20000;
    records.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        records.push_back(Record(i + 1, i, "Deep"));
    }
    const auto built = slsf::BuildLocationIndex(records);
    REQUIRE(built.has_value());
    const auto root = slsf::LocationId(0);
    const auto leaf = slsf::LocationId(count - 1);
    REQUIRE(slsf::IsAncestor(*built, root, leaf, false));
    REQUIRE_FALSE(slsf::IsAncestor(*built, leaf, root, false));
}

TEST_CASE("region priority follows registration order before ancestry distance", "[locations]") {
    const std::vector<slsf::LocationRecord> records = {
        Record(10, 0, "Root", 1),
        Record(20, 10, "Child", 3),
    };
    const auto index = slsf::BuildLocationIndex(records);
    REQUIRE(index.has_value());

    slsf::RegionDefinitions definitions;
    definitions.keys = {Region(1), Region(2), Region(3)};
    definitions.customOrder = {Region(1), Region(2), Region(3)};
    const auto regions = slsf::BindRegions(*index, definitions);
    REQUIRE(regions.has_value());

    const auto location = IdOf(*index, 20);
    REQUIRE(location);
    const auto resolved = slsf::ResolveRegion(*regions, *location);
    REQUIRE(resolved.has_value());
    REQUIRE(regions->regions[resolved->value] == Region(1));

    const std::vector<slsf::LocationRecord> ownedRecords = {
        Record(10, 0, "Root", 2),
        Record(20, 10, "Child", 3),
    };
    const auto ownedIndex = slsf::BuildLocationIndex(ownedRecords);
    REQUIRE(ownedIndex.has_value());
    slsf::RegionDefinitions ownedDefinitions;
    ownedDefinitions.keys = {Region(1), Region(2), Region(3)};
    ownedDefinitions.ownerRules.push_back(
        slsf::OwnerRule{Region(1), {Region(3)}});
    ownedDefinitions.customOrder = {Region(1), Region(2), Region(3)};
    const auto ownedRegions = slsf::BindRegions(*ownedIndex, ownedDefinitions);
    REQUIRE(ownedRegions.has_value());
    const auto ownedLocation = IdOf(*ownedIndex, 20);
    REQUIRE(ownedLocation);
    const auto ownedResolved = slsf::ResolveRegion(*ownedRegions, *ownedLocation);
    REQUIRE(ownedResolved.has_value());
    REQUIRE(ownedRegions->regions[ownedResolved->value] == Region(1));
}

TEST_CASE("random location forests match the reference implementation", "[locations]") {
    std::mt19937 rng(1170);
    std::uniform_int_distribution<std::uint32_t> countDistribution(1, 12);
    std::uniform_int_distribution<std::uint32_t> percent(0, 99);

    for (std::uint32_t iteration = 0; iteration < 10000; ++iteration) {
        const auto count = countDistribution(rng);
        std::vector<slsf::LocationRecord> canonical;
        canonical.reserve(count);
        for (std::uint32_t i = 0; i < count; ++i) {
            const auto parent = i == 0 ? 0 : (i - 1);
            const auto region = percent(rng) < 60 ? std::optional<std::uint32_t>((i % 6) + 1)
                                                  : std::nullopt;
            canonical.push_back(Record(i + 1, parent == 0 ? 0 : parent + 1,
                                       "DuplicateName", region));
            canonical.back().formId = Form(i + 1);
            canonical.back().parentFormId =
                parent == 0 ? slsf::StableFormKey{} : Form(parent + 1);
        }

        auto input = canonical;
        std::shuffle(input.begin(), input.end(), rng);
        bool injectedBad = false;
        if (percent(rng) < 10) {
            input.back().parentFormId = Form(9999);
            injectedBad = true;
        } else if (percent(rng) < 15 && input.size() > 1) {
            input[0].parentFormId = input[1].formId;
            input[1].parentFormId = input[0].formId;
            injectedBad = true;
        } else if (percent(rng) < 20) {
            input.back().parentFormId = input.back().formId;
            injectedBad = true;
        } else if (percent(rng) < 25 && input.size() > 1) {
            input[1].formId = input[0].formId;
            injectedBad = true;
        }

        const auto built = slsf::BuildLocationIndex(input);
        if (injectedBad) {
            REQUIRE_FALSE(built.has_value());
            continue;
        }
        REQUIRE(built.has_value());
        const auto& index = *built;

        for (std::uint32_t i = 0; i < count; ++i) {
            const auto id = IdOf(index, i + 1);
            if (!id || slsf::test::FindLocationReference(index.records, Form(i + 1)) != id) {
                FAIL("lookup differs for form " << i + 1);
            }
        }
        for (std::uint32_t a = 0; a < count; ++a) {
            for (std::uint32_t b = 0; b < count; ++b) {
                const auto ancestor = IdOf(index, a + 1);
                const auto location = IdOf(index, b + 1);
                if (slsf::IsAncestor(index, *ancestor, *location, false) !=
                    slsf::test::IsAncestorReference(index.records, *ancestor, *location,
                                                    false)) {
                    FAIL("exclusive ancestor differs at " << a << "," << b);
                }
                if (slsf::IsAncestor(index, *ancestor, *location, true) !=
                    slsf::test::IsAncestorReference(index.records, *ancestor, *location,
                                                    true)) {
                    FAIL("inclusive ancestor differs at " << a << "," << b);
                }
            }
        }

        slsf::RegionDefinitions definitions;
        for (std::uint32_t region = 1; region <= 6; ++region) {
            definitions.keys.push_back(Region(region));
        }
        slsf::OwnerRule ownerRule;
        ownerRule.owner = Region(1);
        ownerRule.children = {Region(2), Region(3)};
        definitions.ownerRules.push_back(ownerRule);
        slsf::OwnerRule secondRule;
        secondRule.owner = Region(4);
        secondRule.children = {Region(5)};
        definitions.ownerRules.push_back(secondRule);
        for (std::uint32_t region = 6; region >= 1; --region) {
            definitions.customOrder.push_back(Region(region));
        }
        definitions.apiAliases.push_back(slsf::ApiAlias{" Capital ", Region(2)});

        const auto regions = slsf::BindRegions(index, definitions);
        REQUIRE(regions.has_value());
        for (std::uint32_t i = 0; i < count; ++i) {
            const auto location = IdOf(index, i + 1);
            if (slsf::ResolveRegion(*regions, *location) !=
                slsf::test::ResolveRegionReference(index, definitions, *location)) {
                FAIL("region differs for form " << i + 1);
            }
        }
        for (std::size_t i = 0; i < definitions.keys.size(); ++i) {
            const auto expected = static_cast<std::uint16_t>(
                std::find(definitions.customOrder.begin(), definitions.customOrder.end(),
                          definitions.keys[i]) -
                definitions.customOrder.begin());
            REQUIRE(regions->regionApiIndex[i].value == expected);
        }
        const auto alias = slsf::ResolveLegacyLocation(*regions, "CAPITAL");
        REQUIRE(alias.has_value());
        REQUIRE(alias->value == 4);

        auto reordered = canonical;
        std::shuffle(reordered.begin(), reordered.end(), rng);
        const auto rebuilt = slsf::BuildLocationIndex(reordered);
        REQUIRE(rebuilt.has_value());
        for (std::uint32_t a = 0; a < count; ++a) {
            for (std::uint32_t b = 0; b < count; ++b) {
                const auto firstA = IdOf(index, a + 1);
                const auto firstB = IdOf(index, b + 1);
                const auto secondA = IdOf(*rebuilt, a + 1);
                const auto secondB = IdOf(*rebuilt, b + 1);
                if (slsf::IsAncestor(index, *firstA, *firstB, true) !=
                    slsf::IsAncestor(*rebuilt, *secondA, *secondB, true)) {
                    FAIL("reordered ancestor differs at " << a << "," << b);
                }
            }
        }
    }
}
