#pragma once

#include "slsf/core/Types.h"

#include <array>
#include <cstddef>
#include <span>
#include <type_traits>
#include <vector>

namespace slsf {

inline constexpr std::size_t kNumericRowBytes = kFameSlotCount * sizeof(double);
inline constexpr std::size_t kNumericRowAlignment = 32;

// 32 doubles: 25 business slots plus zero padding copied but never interpreted.
struct alignas(kNumericRowAlignment) NumericRow {
    std::array<double, kFameSlotCount> values{};

    static constexpr std::size_t size() noexcept { return kFameSlotCount; }

    constexpr double& operator[](FameCategory category) noexcept {
        return values[static_cast<std::size_t>(category)];
    }

    constexpr const double& operator[](FameCategory category) const noexcept {
        return values[static_cast<std::size_t>(category)];
    }

    constexpr double& operator[](std::size_t slot) noexcept { return values[slot]; }
    constexpr const double& operator[](std::size_t slot) const noexcept { return values[slot]; }

    constexpr bool PaddingIsZero() const noexcept {
        for (std::size_t i = kFameCategoryCount; i < kFameSlotCount; ++i) {
            if (values[i] != 0.0) {
                return false;
            }
        }
        return true;
    }

    friend bool operator==(const NumericRow&, const NumericRow&) = default;
};

static_assert(sizeof(double) == 8);
static_assert(sizeof(NumericRow) == kNumericRowBytes);
static_assert(kNumericRowBytes == 256);
static_assert(alignof(NumericRow) >= kNumericRowAlignment);
static_assert(sizeof(NumericRow) % alignof(NumericRow) == 0);
static_assert(std::is_trivially_copyable_v<NumericRow>);

// std::vector honors over-alignment, so the block and every row are aligned.
using NumericRowBuffer = std::vector<NumericRow>;
using NumericRowSpan = std::span<NumericRow>;
using ConstNumericRowSpan = std::span<const NumericRow>;

namespace numeric {

void ClearRow(NumericRow& row) noexcept;
void CopyRow(NumericRow& destination, const NumericRow& source) noexcept;

// Maximum over the 25 business slots; padding is deliberately ignored.
double MaxSlot(const NumericRow& row) noexcept;

} // namespace numeric

} // namespace slsf
