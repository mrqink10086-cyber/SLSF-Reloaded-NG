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

slsf::StableFormKey Form(std::uint32_t id) {
    return slsf::StableFormKey{"loc.esp", id};
}

slsf::StableRegionKey Region(std::uint32_t id) {
    return slsf::StableRegionKey{"region.esp", id};
}

slsf::RegionDefinitions Definitions() {
    slsf::RegionDefinitions definitions;
    for (std::uint32_t i = 1; i <= 6; ++i) {
        definitions.keys.push_back(Region(i));
    }
    definitions.ownerRules.push_back(slsf::OwnerRule{Region(1), {Region(2), Region(3)}});
    definitions.ownerRules.push_back(slsf::OwnerRule{Region(4), {Region(5)}});
    definitions.customOrder = {Region(6), Region(5), Region(4), Region(3), Region(2), Region(1)};
    definitions.apiAliases.push_back(slsf::ApiAlias{"  The   Capital ", Region(2)});
    return definitions;
}

std::uint16_t ApiIndexFor(const slsf::RegionDefinitions& definitions,
                          const slsf::StableRegionKey& key) {
    const auto it = std::find(definitions.customOrder.begin(), definitions.customOrder.end(), key);
    return static_cast<std::uint16_t>(it - definitions.customOrder.begin());
}

}

TEST_CASE("random arbitrary location forests match the reference implementation", "[locations]") {
    std::mt19937_64 rng(1170);
    std::uniform_int_distribution<std::uint32_t> countDistribution(0, 12);
    std::uniform_int_distribution<std::uint32_t> percent(0, 99);
    std::uniform_int_distribution<std::uint32_t> parentDistribution(0, 1);

    for (std::uint32_t iteration = 0; iteration < 10000; ++iteration) {
        const auto count = countDistribution(rng);
        std::vector<slsf::LocationRecord> canonical;
        canonical.reserve(count);
        for (std::uint32_t i = 0; i < count; ++i) {
            slsf::LocationRecord record;
            record.formId = Form((i + 1) * 3);
            if (i > 0 && parentDistribution(rng) != 0) {
                record.parentFormId = Form((parentDistribution(rng) % i + 1) * 3);
            }
            const auto region = percent(rng) < 55 ? std::optional<std::uint32_t>((i % 6) + 1)
                                                  : std::nullopt;
            if (region) {
                record.stableKey = Region(*region);
            }
            record.displayName = "DuplicateName";
            canonical.push_back(std::move(record));
        }

        auto input = canonical;
        std::shuffle(input.begin(), input.end(), rng);
        bool injectedBad = false;
        if (percent(rng) < 8) {
            if (!input.empty()) {
                input.back().parentFormId = Form(9999);
                injectedBad = true;
            }
        } else if (percent(rng) < 12) {
            if (input.size() > 1) {
                input[0].parentFormId = input[1].formId;
                input[1].parentFormId = input[0].formId;
                injectedBad = true;
            }
        } else if (percent(rng) < 16) {
            if (!input.empty()) {
                input.back().parentFormId = input.back().formId;
                injectedBad = true;
            }
        } else if (percent(rng) < 20) {
            if (input.size() > 1) {
                input[1].formId = input[0].formId;
                injectedBad = true;
            }
        }

        const auto built = slsf::BuildLocationIndex(input);
        if (injectedBad) {
            REQUIRE_FALSE(built.has_value());
            continue;
        }
        REQUIRE(built.has_value());
        const auto& index = *built;
        REQUIRE(index.records.size() == canonical.size());

        for (const auto& record : canonical) {
            const auto id = slsf::FindLocation(index, record.formId);
            if (!id) {
                FAIL("location lookup failed for form " << record.formId.localFormId);
            }
            if (slsf::test::FindLocationReference(index.records, record.formId) != id) {
                FAIL("location lookup differs for form " << record.formId.localFormId);
            }
        }

        for (const auto& first : canonical) {
            for (const auto& second : canonical) {
                const auto ancestor = slsf::FindLocation(index, first.formId);
                const auto location = slsf::FindLocation(index, second.formId);
                if (slsf::IsAncestor(index, *ancestor, *location, false) !=
                    slsf::test::IsAncestorReference(index.records, *ancestor, *location, false)) {
                    FAIL("exclusive ancestor differs for forms "
                         << first.formId.localFormId << " and " << second.formId.localFormId);
                }
                if (slsf::IsAncestor(index, *ancestor, *location, true) !=
                    slsf::test::IsAncestorReference(index.records, *ancestor, *location, true)) {
                    FAIL("inclusive ancestor differs for forms "
                         << first.formId.localFormId << " and " << second.formId.localFormId);
                }
            }
        }

        const auto definitions = Definitions();
        const auto regions = slsf::BindRegions(index, definitions);
        REQUIRE(regions.has_value());
        for (const auto& record : canonical) {
            const auto location = slsf::FindLocation(index, record.formId);
            if (slsf::ResolveRegion(*regions, *location) !=
                slsf::test::ResolveRegionReference(index, definitions, *location)) {
                FAIL("region resolution differs for form " << record.formId.localFormId);
            }
        }
        for (const auto& key : definitions.keys) {
            const auto id = slsf::FindRegion(*regions, key);
            if (!id) {
                FAIL("region lookup failed for form " << key.localFormId);
            }
            if (regions->regionApiIndex[id->value].value != ApiIndexFor(definitions, key)) {
                FAIL("region api index differs for form " << key.localFormId);
            }
        }
        const auto alias = slsf::ResolveLegacyLocation(*regions, "the capital");
        REQUIRE(alias);
        REQUIRE(alias->value == 4);

        std::shuffle(input.begin(), input.end(), rng);
        const auto rebuilt = slsf::BuildLocationIndex(input);
        REQUIRE(rebuilt.has_value());
        for (const auto& first : canonical) {
            for (const auto& second : canonical) {
                const auto firstA = slsf::FindLocation(index, first.formId);
                const auto firstB = slsf::FindLocation(index, second.formId);
                const auto secondA = slsf::FindLocation(*rebuilt, first.formId);
                const auto secondB = slsf::FindLocation(*rebuilt, second.formId);
                if (slsf::IsAncestor(index, *firstA, *firstB, true) !=
                    slsf::IsAncestor(*rebuilt, *secondA, *secondB, true)) {
                    FAIL("reordered ancestor differs for forms "
                         << first.formId.localFormId << " and " << second.formId.localFormId);
                }
            }
        }
    }
}
