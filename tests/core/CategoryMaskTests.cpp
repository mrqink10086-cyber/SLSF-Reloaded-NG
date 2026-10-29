#include <catch2/catch_test_macros.hpp>

#include "slsf/core/CategoryMask.h"
#include "slsf/core/Types.h"

#include <cstdint>
#include <vector>

namespace {

using slsf::CategoryMask;
using slsf::FameCategory;

} // namespace

static_assert(slsf::FameCategoryIndex(FameCategory::Whore) == 0);
static_assert(slsf::FameCategoryIndex(FameCategory::Airhead) == 24);
static_assert(slsf::FameCategoryIndex(FameCategory::Count) == 25);
static_assert(slsf::EspFameCategoryIndex(FameCategory::Whore) == 0);
static_assert(slsf::EspFameCategoryIndex(FameCategory::Slut) == 1);
static_assert(slsf::EspFameCategoryIndex(FameCategory::Group) == 18);
static_assert(slsf::DocumentedFameCategoryIndex(FameCategory::Slut) == 0);
static_assert(slsf::DocumentedFameCategoryIndex(FameCategory::Whore) == 1);
static_assert(slsf::DocumentedFameCategoryIndex(FameCategory::Group) == 7);
static_assert(slsf::DocumentedFameCategoryIndex(FameCategory::Tattoo) == 20);
static_assert(slsf::FameCategoryFromDocumentedIndex(0) == FameCategory::Slut);
static_assert(slsf::FameCategoryFromDocumentedIndex(7) == FameCategory::Group);
static_assert(slsf::FameCategoryFromDocumentedIndex(24) == FameCategory::Airhead);
static_assert(slsf::FameCategoryFromEspIndex(18) == FameCategory::Group);

TEST_CASE("category bits use canonical identity not documentation order", "[category][mask]") {
    static_assert(slsf::FameCategoryBit(FameCategory::Whore) == 0x00000001u);
    static_assert(slsf::FameCategoryBit(FameCategory::Slut) == 0x00000002u);
    static_assert(slsf::FameCategoryBit(FameCategory::Group) == 0x00040000u);
    static_assert(slsf::FameCategoryBit(FameCategory::Tattoo) == 0x00100000u);
    static_assert(slsf::FameCategoryBit(FameCategory::Airhead) == 0x01000000u);
    static_assert(slsf::FameCategoryBit(FameCategory::Count) == 0u);

    const auto whore = CategoryMask::FromCategory(FameCategory::Whore);
    const auto airhead = CategoryMask::FromCategory(FameCategory::Airhead);
    REQUIRE(whore.Bits() == 0x00000001u);
    REQUIRE(airhead.Bits() == 0x01000000u);
    REQUIRE((whore | airhead).Bits() == 0x01000001u);
    REQUIRE((whore | airhead).Count() == 2);

    // Documented index 0 is Slut, not Whore; the mask must not follow it.
    REQUIRE_FALSE(CategoryMask::FromCategory(slsf::FameCategoryFromDocumentedIndex(0)).Test(
        FameCategory::Whore));
    REQUIRE(CategoryMask::FromCategory(slsf::FameCategoryFromDocumentedIndex(0)).Test(
        FameCategory::Slut));
}

TEST_CASE("category mask rejects invalid categories and high bits", "[category][mask]") {
    CategoryMask mask;
    REQUIRE(mask.Set(FameCategory::Whore));
    REQUIRE(mask.Set(FameCategory::Airhead));
    REQUIRE_FALSE(mask.Set(FameCategory::Count));
    REQUIRE_FALSE(mask.Reset(FameCategory::Count));
    REQUIRE_FALSE(mask.Test(FameCategory::Count));
    REQUIRE(mask.Bits() == 0x01000001u);
    REQUIRE((mask.Bits() & ~CategoryMask::kValidBits) == 0);

    REQUIRE(CategoryMask::FromBits(0x01FFFFFFu).has_value());
    REQUIRE_FALSE(CategoryMask::FromBits(0x02000000u).has_value());
    REQUIRE_FALSE(CategoryMask::FromBits(0xFFFFFFFFu).has_value());

    const auto full = CategoryMask::FromBits(0x01FFFFFFu);
    REQUIRE(full.has_value());
    REQUIRE(full->Count() == slsf::kFameCategoryCount);
    REQUIRE((~*full).Empty());
    REQUIRE((~CategoryMask{}).Bits() == CategoryMask::kValidBits);
    REQUIRE((~CategoryMask{}).Count() == slsf::kFameCategoryCount);
}

TEST_CASE("category mask iteration follows canonical order", "[category][mask]") {
    CategoryMask mask;
    REQUIRE(mask.Set(FameCategory::Airhead));
    REQUIRE(mask.Set(FameCategory::Group));
    REQUIRE(mask.Set(FameCategory::Whore));

    std::vector<FameCategory> visited;
    mask.ForEach([&](FameCategory category) { visited.push_back(category); });

    REQUIRE(visited.size() == 3);
    REQUIRE(visited[0] == FameCategory::Whore);
    REQUIRE(visited[1] == FameCategory::Group);
    REQUIRE(visited[2] == FameCategory::Airhead);

    mask.Clear();
    REQUIRE(mask.Empty());
    REQUIRE_FALSE(mask.Any());
}

TEST_CASE("all documented and ESP indices round trip to canonical identity", "[category][mask]") {
    for (std::size_t index = 0; index < slsf::kFameCategoryCount; ++index) {
        const auto documented = slsf::FameCategoryFromDocumentedIndex(index);
        REQUIRE(documented != FameCategory::Count);
        REQUIRE(slsf::DocumentedFameCategoryIndex(documented) == index);

        const auto esp = slsf::FameCategoryFromEspIndex(index);
        REQUIRE(esp != FameCategory::Count);
        REQUIRE(slsf::EspFameCategoryIndex(esp) == index);
        REQUIRE(static_cast<std::size_t>(esp) == index);
    }
    REQUIRE(slsf::FameCategoryFromDocumentedIndex(slsf::kFameCategoryCount) ==
            FameCategory::Count);
    REQUIRE(slsf::FameCategoryFromEspIndex(slsf::kFameCategoryCount) == FameCategory::Count);
    REQUIRE(slsf::DocumentedFameCategoryIndex(FameCategory::Count) ==
            slsf::kFameCategoryCount);
}
