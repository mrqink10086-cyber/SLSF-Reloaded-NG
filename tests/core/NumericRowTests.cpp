#include <catch2/catch_test_macros.hpp>

#include "slsf/core/NumericRow.h"

#include <cstdint>
#include <vector>

namespace {

std::uintptr_t AddressOf(const void* pointer) {
    return reinterpret_cast<std::uintptr_t>(pointer);
}
}
static_assert(sizeof(slsf::NumericRow) == 256);
static_assert(alignof(slsf::NumericRow) == 32);
static_assert(sizeof(slsf::NumericRow) / sizeof(double) == slsf::kFameSlotCount);
static_assert(slsf::kNumericRowBytes == 256);
static_assert(slsf::kNumericRowAlignment == 32);

TEST_CASE("numeric rows allocate all 32 slots and start zeroed", "[numeric][layout]") {
    slsf::NumericRow row{};
    REQUIRE(row.values.size() == slsf::kFameSlotCount);
    for (const double value : row.values) {
        REQUIRE(value == 0.0);
    }
    REQUIRE(row.PaddingIsZero());

    row[slsf::FameCategory::Whore] = 4.25;
    row[slsf::FameCategory::Airhead] = -2.0;
    REQUIRE(row[slsf::FameCategory::Whore] == 4.25);
    REQUIRE(row[24] == -2.0);
    REQUIRE(row[slsf::FameCategory::Whore] == row[0]);

    row[30] = 9.0;
    REQUIRE_FALSE(row.PaddingIsZero());
}
TEST_CASE("numeric row buffers keep stride and address alignment", "[numeric][layout]") {
    slsf::NumericRowBuffer rows(4);
    REQUIRE(rows.size() == 4);
    REQUIRE(AddressOf(rows.data()) % slsf::kNumericRowAlignment == 0);

    for (std::size_t i = 0; i < rows.size(); ++i) {
        REQUIRE(AddressOf(&rows[i]) % slsf::kNumericRowAlignment == 0);
        if (i > 0) {
            REQUIRE(AddressOf(&rows[i]) - AddressOf(&rows[i - 1]) == sizeof(slsf::NumericRow));
        }
        for (const double value : rows[i].values) {
            REQUIRE(value == 0.0);
        }
    }

    rows.resize(64);
    REQUIRE(AddressOf(rows.data()) % slsf::kNumericRowAlignment == 0);
    REQUIRE(AddressOf(&rows.back()) % slsf::kNumericRowAlignment == 0);
}
TEST_CASE("numeric row operations preserve padding and ignore it when scanning", "[numeric]") {
    slsf::NumericRow source{};
    source[slsf::FameCategory::Slut] = 12.5;
    source[slsf::FameCategory::Cuck] = 3.0;
    source[30] = 99.0; // padding is copied but never scanned as a category

    slsf::NumericRow destination{};
    slsf::numeric::CopyRow(destination, source);
    REQUIRE(destination[slsf::FameCategory::Slut] == 12.5);
    REQUIRE(destination[slsf::FameCategory::Cuck] == 3.0);
    REQUIRE(destination[30] == 99.0);
    REQUIRE(destination == source);
    REQUIRE(slsf::numeric::MaxSlot(destination) == 12.5);

    slsf::numeric::ClearRow(destination);
    for (const double value : destination.values) {
        REQUIRE(value == 0.0);
    }
}