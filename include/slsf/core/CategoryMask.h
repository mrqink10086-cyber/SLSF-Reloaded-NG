#pragma once

#include "slsf/core/Types.h"

#include <cstddef>
#include <bit>
#include <cstdint>
#include <optional>
#include <type_traits>
#include <utility>

namespace slsf {

// Bits use canonical FameCategory enum values; only kFameCategoryCount low bits are valid.
class CategoryMask {
public:
    using MaskBits = std::uint32_t;

    static constexpr MaskBits kValidBits =
        (MaskBits{1} << static_cast<unsigned>(kFameCategoryCount)) - MaskBits{1};

    constexpr CategoryMask() noexcept = default;

    static constexpr std::optional<CategoryMask> FromBits(MaskBits bits) noexcept {
        if ((bits & ~kValidBits) != 0) {
            return std::nullopt;
        }
        return CategoryMask(bits, Validated{});
    }

    static constexpr CategoryMask FromCategory(FameCategory category) noexcept {
        return CategoryMask(FameCategoryBit(category), Validated{});
    }

    constexpr MaskBits Bits() const noexcept { return bits_; }
    constexpr bool Empty() const noexcept { return bits_ == 0; }
    constexpr bool Any() const noexcept { return bits_ != 0; }

    constexpr bool Test(FameCategory category) const noexcept {
        return IsValidFameCategory(category) &&
               (bits_ & FameCategoryBit(category)) != 0;
    }

    constexpr bool Set(FameCategory category) noexcept {
        if (!IsValidFameCategory(category)) {
            return false;
        }
        bits_ |= FameCategoryBit(category);
        return true;
    }

    constexpr bool Reset(FameCategory category) noexcept {
        if (!IsValidFameCategory(category)) {
            return false;
        }
        bits_ &= ~FameCategoryBit(category);
        return true;
    }

    constexpr void Clear() noexcept { bits_ = 0; }

    constexpr std::size_t Count() const noexcept {
        std::size_t count = 0;
        for (std::size_t i = 0; i < kFameCategoryCount; ++i) {
            if ((bits_ & (MaskBits{1} << static_cast<unsigned>(i))) != 0) {
                ++count;
            }
        }
        return count;
    }

    // Visits set categories in canonical (ascending enum) order.
    template <typename Fn>
    constexpr void ForEach(Fn&& fn) const {
        auto remaining = bits_;
        while (remaining != 0) {
            fn(static_cast<FameCategory>(std::countr_zero(remaining)));
            remaining &= remaining - 1;
        }
    }

    friend constexpr bool operator==(CategoryMask, CategoryMask) noexcept = default;

    friend constexpr CategoryMask operator|(CategoryMask lhs, CategoryMask rhs) noexcept {
        return CategoryMask(lhs.bits_ | rhs.bits_, Validated{});
    }

    friend constexpr CategoryMask operator&(CategoryMask lhs, CategoryMask rhs) noexcept {
        return CategoryMask(lhs.bits_ & rhs.bits_, Validated{});
    }

    constexpr CategoryMask& operator|=(CategoryMask rhs) noexcept {
        bits_ |= rhs.bits_;
        return *this;
    }

    constexpr CategoryMask& operator&=(CategoryMask rhs) noexcept {
        bits_ &= rhs.bits_;
        return *this;
    }

    constexpr CategoryMask operator~() const noexcept {
        return CategoryMask(kValidBits & ~bits_, Validated{});
    }

private:
    struct Validated {};

    constexpr CategoryMask(MaskBits bits, Validated) noexcept : bits_(bits) {}

    MaskBits bits_{};
};

static_assert(CategoryMask::kValidBits == 0x01FFFFFFu);
static_assert(std::is_trivially_copyable_v<CategoryMask>);

}
