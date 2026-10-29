#include <catch2/catch_test_macros.hpp>

#include "slsf/core/Commands.h"
#include "slsf/core/Contracts.h"
#include "slsf/core/Snapshots.h"
#include "slsf/core/Types.h"
#include "slsf/core/persistence/PersistentState.h"

#include <cstdint>
#include <memory>
#include <type_traits>
#include <vector>

namespace {

static_assert(!std::is_convertible_v<std::uint32_t, slsf::LocationId>);
static_assert(!std::is_convertible_v<slsf::LocationId, std::uint32_t>);
static_assert(!std::is_convertible_v<std::uint32_t, slsf::RegionId>);
static_assert(!std::is_convertible_v<std::uint32_t, slsf::ApiLocationIndex>);
static_assert(!std::is_convertible_v<std::uint64_t, slsf::TaskId>);
static_assert(!std::is_same_v<slsf::LocationId, slsf::RegionId>);
static_assert(!std::is_same_v<slsf::RequestId, slsf::Revision>);
static_assert(std::is_constructible_v<slsf::LocationId, std::uint32_t>);

} // namespace

TEST_CASE("legacy contract counts are fixed", "[contracts]") {
    STATIC_REQUIRE(slsf::kLegacyInputEventCount == 52);
    STATIC_REQUIRE(slsf::kLegacyReplyEventCount == 7);
    STATIC_REQUIRE(slsf::kLegacyCategoryCount == 25);
    STATIC_REQUIRE(slsf::kFameCategoryCount == 25);
    STATIC_REQUIRE(slsf::kFameCategoryNames.size() == 25);
    STATIC_REQUIRE(slsf::kEspFameCategoryOrder.size() == 25);
    STATIC_REQUIRE(slsf::kDocumentedFameCategoryOrder.size() == 25);
    REQUIRE(slsf::BuildId() == "SLSFCore/0.1.0");
}

TEST_CASE("fail constructs value and void results", "[contracts]") {
    slsf::Result<void> voidResult = slsf::Fail(slsf::Error::Code::InvalidArgument, "void failure");
    REQUIRE_FALSE(voidResult.has_value());
    REQUIRE(voidResult.error().code == slsf::Error::Code::InvalidArgument);

    slsf::Result<int> intResult = slsf::Fail(slsf::Error::Code::OutOfRange, "int failure");
    REQUIRE_FALSE(intResult.has_value());
    REQUIRE(intResult.error().code == slsf::Error::Code::OutOfRange);
}

TEST_CASE("fame category lookup handles ESP spelling", "[contracts]") {
    REQUIRE(slsf::ToString(slsf::FameCategory::Tattoo) == "Tattoo");
    REQUIRE(slsf::ToString(slsf::FameCategory::CumDump) == "Cum Dump");
    REQUIRE(slsf::ParseFameCategory("tattoo") == slsf::FameCategory::Tattoo);
    REQUIRE(slsf::ParseFameCategory("Tattoo") == slsf::FameCategory::Tattoo);
    REQUIRE(slsf::ParseFameCategory("cum dump") == slsf::FameCategory::CumDump);
    REQUIRE(slsf::ParseFameCategory("Likes Men") == slsf::FameCategory::LikesMen);
    REQUIRE_FALSE(slsf::ParseFameCategory("WhoreEvent").has_value());
    REQUIRE(slsf::kLegacyNonCategoryFlags.front() == "Whore Event");
    REQUIRE_FALSE(slsf::ParseFameCategory("not a category").has_value());
}

TEST_CASE("fame categories have a stable ESP order", "[contracts]") {
    REQUIRE(slsf::kEspFameCategoryOrder.front() == slsf::FameCategory::Whore);
    REQUIRE(slsf::kEspFameCategoryOrder[1] == slsf::FameCategory::Slut);
    REQUIRE(slsf::kEspFameCategoryOrder[20] == slsf::FameCategory::Tattoo);
    REQUIRE(slsf::kEspFameCategoryOrder[21] == slsf::FameCategory::CumDump);
    REQUIRE(slsf::kDocumentedFameCategoryOrder.front() == slsf::FameCategory::Slut);
    REQUIRE(slsf::kDocumentedFameCategoryOrder[7] == slsf::FameCategory::Group);
    REQUIRE(slsf::kDocumentedFameCategoryOrder[20] == slsf::FameCategory::Tattoo);
}

TEST_CASE("strong ids do not convert", "[contracts]") {
    const slsf::LocationId location{42};
    const slsf::RegionId region{43};
    const slsf::TaskId task{44};

    REQUIRE(location.value == 42);
    REQUIRE(region.value == 43);
    REQUIRE(task.value == 44);
    REQUIRE(static_cast<bool>(location));
    REQUIRE_FALSE(static_cast<bool>(slsf::LocationId{}));
}

TEST_CASE("zero is a valid dense location and public location index", "[ids]") {
    REQUIRE(static_cast<bool>(slsf::LocationId{0}));
    REQUIRE(static_cast<bool>(slsf::RegionId{0}));
    REQUIRE(static_cast<bool>(slsf::ApiLocationIndex{0}));
    REQUIRE_FALSE(static_cast<bool>(slsf::LocationId{}));
    REQUIRE_FALSE(static_cast<bool>(slsf::RegionId{}));
    REQUIRE_FALSE(static_cast<bool>(slsf::ApiLocationIndex{}));
    REQUIRE_FALSE(static_cast<bool>(slsf::TaskId{}));
}

TEST_CASE("unknown facts are not false", "[contracts]") {
    const slsf::Fact unknown;
    REQUIRE(unknown.state == slsf::FactState::Unknown);
    REQUIRE_FALSE(unknown.Value().has_value());
    REQUIRE_FALSE(unknown.IsKnownFalse());

    REQUIRE(slsf::Fact::Known(false).IsKnownFalse());
    REQUIRE(slsf::Fact::Known(true).IsKnownTrue());
    REQUIRE(slsf::Fact{slsf::FactState::Missing}.Value() == std::nullopt);
}

TEST_CASE("snapshots are value objects", "[contracts]") {
    slsf::SceneSnapshot original;
    original.status = slsf::Status::Ready;
    original.tags = {"slow"};
    original.annotations.emplace_back("source", "test");

    auto copy = original;
    original.tags.push_back("moved");
    original.annotations.emplace_back("later", "mutation");

    REQUIRE(copy.tags == std::vector<std::string>{"slow"});
    REQUIRE(copy.annotations.size() == 1);
    REQUIRE(copy.IsUsable());

    auto shared = std::make_shared<const slsf::SceneSnapshot>(copy);
    REQUIRE(shared->tags.size() == 1);
    REQUIRE(shared->epoch == original.epoch);

    slsf::PublishedState published;
    published.revision = slsf::Revision{3};
    published.fame = {1.0, 2.0};
    auto publishedCopy = published;
    published.fame[0] = 9.0;
    REQUIRE(publishedCopy.fame[0] == 1.0);
    REQUIRE(publishedCopy.revision.value == 3);
}

TEST_CASE("game time keeps day and second units separate", "[contracts]") {
    const auto day = slsf::GameTime::FromDays(1.0);
    const auto fromSeconds = slsf::GameTime::FromSeconds(86400.0);
    REQUIRE(day.AsSeconds() == 86400.0);
    REQUIRE(fromSeconds.AsDays() == 1.0);
    REQUIRE(slsf::RealSeconds{1.0}.value == 1.0);
    REQUIRE(slsf::ActiveSeconds{2.0}.value == 2.0);
}
