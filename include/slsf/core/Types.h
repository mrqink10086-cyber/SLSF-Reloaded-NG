#pragma once

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace slsf {

struct LocationId {
    static constexpr auto invalid = (std::numeric_limits<std::uint32_t>::max)();
    std::uint32_t value{invalid};

    constexpr LocationId() noexcept = default;
    constexpr explicit LocationId(std::uint32_t raw) noexcept : value(raw) {}

    constexpr explicit operator bool() const noexcept { return value != invalid; }
    friend constexpr bool operator==(const LocationId&, const LocationId&) = default;
    friend constexpr auto operator<=>(const LocationId&, const LocationId&) = default;
};

struct RegionId {
    static constexpr auto invalid = (std::numeric_limits<std::uint32_t>::max)();
    std::uint32_t value{invalid};

    constexpr RegionId() noexcept = default;
    constexpr explicit RegionId(std::uint32_t raw) noexcept : value(raw) {}

    constexpr explicit operator bool() const noexcept { return value != invalid; }
    friend constexpr bool operator==(const RegionId&, const RegionId&) = default;
    friend constexpr auto operator<=>(const RegionId&, const RegionId&) = default;
};

struct ApiLocationIndex {
    static constexpr auto invalid = (std::numeric_limits<std::uint16_t>::max)();
    std::uint16_t value{invalid};

    constexpr ApiLocationIndex() noexcept = default;
    constexpr explicit ApiLocationIndex(std::uint16_t raw) noexcept : value(raw) {}

    constexpr explicit operator bool() const noexcept { return value != invalid; }
    friend constexpr bool operator==(const ApiLocationIndex&, const ApiLocationIndex&) = default;
    friend constexpr auto operator<=>(const ApiLocationIndex&, const ApiLocationIndex&) = default;
};

struct TaskId {
    std::uint64_t value{};

    constexpr TaskId() noexcept = default;
    constexpr explicit TaskId(std::uint64_t raw) noexcept : value(raw) {}

    constexpr explicit operator bool() const noexcept { return value != 0; }
    friend constexpr bool operator==(const TaskId&, const TaskId&) = default;
    friend constexpr auto operator<=>(const TaskId&, const TaskId&) = default;
};

struct RequestId {
    std::uint64_t value{};

    constexpr RequestId() noexcept = default;
    constexpr explicit RequestId(std::uint64_t raw) noexcept : value(raw) {}

    constexpr explicit operator bool() const noexcept { return value != 0; }
    friend constexpr bool operator==(const RequestId&, const RequestId&) = default;
    friend constexpr auto operator<=>(const RequestId&, const RequestId&) = default;
};

struct Revision {
    std::uint64_t value{};

    constexpr Revision() noexcept = default;
    constexpr explicit Revision(std::uint64_t raw) noexcept : value(raw) {}

    constexpr explicit operator bool() const noexcept { return value != 0; }
    friend constexpr bool operator==(const Revision&, const Revision&) = default;
    friend constexpr auto operator<=>(const Revision&, const Revision&) = default;
};

struct Epoch {
    std::uint64_t session{};
    std::uint64_t index{};
    std::uint64_t scene{};

    friend constexpr bool operator==(const Epoch&, const Epoch&) = default;
    friend constexpr auto operator<=>(const Epoch&, const Epoch&) = default;
};

enum class Status : std::uint8_t {
    Ready = 0,
    NotReady,
    Missing,
    Incompatible,
    Invalid,
    Stale,
};

constexpr std::string_view ToString(Status status) noexcept {
    switch (status) {
    case Status::Ready: return "Ready";
    case Status::NotReady: return "NotReady";
    case Status::Missing: return "Missing";
    case Status::Incompatible: return "Incompatible";
    case Status::Invalid: return "Invalid";
    case Status::Stale: return "Stale";
    }
    return "Invalid";
}

// Game days are the unit used by Papyrus Utility.GetCurrentGameTime().
struct GameTime {
    double days{};

    constexpr GameTime() noexcept = default;
    constexpr explicit GameTime(double value) noexcept : days(value) {}

    static constexpr GameTime FromDays(double value) noexcept { return GameTime(value); }
    static constexpr GameTime FromSeconds(double value) noexcept {
        return GameTime(value / 86400.0);
    }

    constexpr double AsDays() const noexcept { return days; }
    constexpr double AsSeconds() const noexcept { return days * 86400.0; }

    friend constexpr bool operator==(const GameTime&, const GameTime&) = default;
    friend constexpr auto operator<=>(const GameTime&, const GameTime&) = default;
};

struct RealSeconds {
    double value{};

    constexpr RealSeconds() noexcept = default;
    constexpr explicit RealSeconds(double seconds) noexcept : value(seconds) {}

    friend constexpr bool operator==(const RealSeconds&, const RealSeconds&) = default;
    friend constexpr auto operator<=>(const RealSeconds&, const RealSeconds&) = default;
};

struct ActiveSeconds {
    double value{};

    constexpr ActiveSeconds() noexcept = default;
    constexpr explicit ActiveSeconds(double seconds) noexcept : value(seconds) {}

    friend constexpr bool operator==(const ActiveSeconds&, const ActiveSeconds&) = default;
    friend constexpr auto operator<=>(const ActiveSeconds&, const ActiveSeconds&) = default;
};

struct StableFormKey {
    std::string plugin;
    std::uint32_t localFormId{};

    friend bool operator==(const StableFormKey&, const StableFormKey&) = default;
    friend auto operator<=>(const StableFormKey&, const StableFormKey&) = default;
};

struct StableRegionKey {
    std::string plugin;
    std::uint32_t localFormId{};

    friend bool operator==(const StableRegionKey&, const StableRegionKey&) = default;
    friend auto operator<=>(const StableRegionKey&, const StableRegionKey&) = default;
};

// The canonical runtime order is the order stored in the ESP VMAD FameType
// array.
enum class FameCategory : std::uint8_t {
    Whore = 0,
    Slut,
    Exhibitionist,
    Oral,
    Anal,
    Nasty,
    Pregnant,
    Dominant,
    Submissive,
    Sadist,
    Masochist,
    Gentle,
    LikesMen,
    LikesWomen,
    LikesOrc,
    LikesKhajiit,
    LikesArgonian,
    Bestiality,
    Group,
    Bound,
    Tattoo,
    CumDump,
    Unfaithful,
    Cuck,
    Airhead,
    Count,
};

inline constexpr std::size_t kFameCategoryCount = 25;
inline constexpr std::size_t kFameSlotCount = 32;

inline constexpr std::array<FameCategory, kFameCategoryCount> kEspFameCategoryOrder = {
    FameCategory::Whore,
    FameCategory::Slut,
    FameCategory::Exhibitionist,
    FameCategory::Oral,
    FameCategory::Anal,
    FameCategory::Nasty,
    FameCategory::Pregnant,
    FameCategory::Dominant,
    FameCategory::Submissive,
    FameCategory::Sadist,
    FameCategory::Masochist,
    FameCategory::Gentle,
    FameCategory::LikesMen,
    FameCategory::LikesWomen,
    FameCategory::LikesOrc,
    FameCategory::LikesKhajiit,
    FameCategory::LikesArgonian,
    FameCategory::Bestiality,
    FameCategory::Group,
    FameCategory::Bound,
    FameCategory::Tattoo,
    FameCategory::CumDump,
    FameCategory::Unfaithful,
    FameCategory::Cuck,
    FameCategory::Airhead,
};

// The order used by the 25-category list in the legacy event documentation.
inline constexpr std::array<FameCategory, kFameCategoryCount> kDocumentedFameCategoryOrder = {
    FameCategory::Slut,
    FameCategory::Whore,
    FameCategory::Exhibitionist,
    FameCategory::Oral,
    FameCategory::Anal,
    FameCategory::Nasty,
    FameCategory::Pregnant,
    FameCategory::Group,
    FameCategory::Dominant,
    FameCategory::Submissive,
    FameCategory::Sadist,
    FameCategory::Masochist,
    FameCategory::Gentle,
    FameCategory::LikesMen,
    FameCategory::LikesWomen,
    FameCategory::LikesOrc,
    FameCategory::LikesKhajiit,
    FameCategory::LikesArgonian,
    FameCategory::Bestiality,
    FameCategory::Bound,
    FameCategory::Tattoo,
    FameCategory::CumDump,
    FameCategory::Unfaithful,
    FameCategory::Cuck,
    FameCategory::Airhead,
};

inline constexpr std::array<std::string_view, kFameCategoryCount> kFameCategoryNames = {
    "Whore",
    "Slut",
    "Exhibitionist",
    "Oral",
    "Anal",
    "Nasty",
    "Pregnant",
    "Dominant",
    "Submissive",
    "Sadist",
    "Masochist",
    "Gentle",
    "Likes Men",
    "Likes Women",
    "Likes Orc",
    "Likes Khajiit",
    "Likes Argonian",
    "Bestiality",
    "Group",
    "Bound",
    "Tattoo",
    "Cum Dump",
    "Unfaithful",
    "Cuck",
    "Airhead",
};

static_assert(static_cast<std::size_t>(FameCategory::Count) == kFameCategoryCount);
static_assert(kFameCategoryCount <= kFameSlotCount);

template <std::size_t N>
constexpr bool IsCompleteFameOrder(const std::array<FameCategory, N>& order) noexcept {
    if constexpr (N != kFameCategoryCount) {
        return false;
    }
    std::array<bool, kFameCategoryCount> seen{};
    for (const auto category : order) {
        const auto index = static_cast<std::size_t>(category);
        if (index >= kFameCategoryCount || seen[index]) {
            return false;
        }
        seen[index] = true;
    }
    return true;
}

static_assert(IsCompleteFameOrder(kEspFameCategoryOrder));
static_assert(IsCompleteFameOrder(kDocumentedFameCategoryOrder));

constexpr bool IsValidFameCategory(FameCategory category) noexcept {
    return static_cast<std::uint8_t>(category) < kFameCategoryCount;
}

// Canonical identity (enum/ESP order); documented indices are presentation-only.
constexpr std::size_t FameCategoryIndex(FameCategory category) noexcept {
    return IsValidFameCategory(category) ? static_cast<std::size_t>(category)
                                         : kFameCategoryCount;
}

constexpr FameCategory FameCategoryFromIndex(std::size_t index) noexcept {
    return index < kFameCategoryCount ? static_cast<FameCategory>(index)
                                      : FameCategory::Count;
}

constexpr std::uint32_t FameCategoryBit(FameCategory category) noexcept {
    return IsValidFameCategory(category)
               ? (std::uint32_t{1} << static_cast<std::uint32_t>(category))
               : std::uint32_t{0};
}

constexpr std::size_t EspFameCategoryIndex(FameCategory category) noexcept {
    return FameCategoryIndex(category);
}

constexpr FameCategory FameCategoryFromEspIndex(std::size_t index) noexcept {
    return FameCategoryFromIndex(index);
}

constexpr std::size_t DocumentedFameCategoryIndex(FameCategory category) noexcept {
    for (std::size_t i = 0; i < kDocumentedFameCategoryOrder.size(); ++i) {
        if (kDocumentedFameCategoryOrder[i] == category) {
            return i;
        }
    }
    return kFameCategoryCount;
}

constexpr FameCategory FameCategoryFromDocumentedIndex(std::size_t index) noexcept {
    return index < kDocumentedFameCategoryOrder.size()
               ? kDocumentedFameCategoryOrder[index]
               : FameCategory::Count;
}

constexpr std::string_view ToString(FameCategory category) noexcept {
    if (!IsValidFameCategory(category)) {
        return "Invalid";
    }
    return kFameCategoryNames[static_cast<std::size_t>(category)];
}

namespace detail {
inline std::string NormalizeCategoryName(std::string_view name) {
    std::string out;
    out.reserve(name.size());
    bool previousSpace = true;
    for (char ch : name) {
        if (ch == '_' || ch == '-') {
            ch = ' ';
        }
        const auto c = static_cast<unsigned char>(ch);
        if (c == ' ') {
            if (!previousSpace) {
                out.push_back(' ');
            }
            previousSpace = true;
            continue;
        }
        if (c >= 'A' && c <= 'Z') {
            out.push_back(static_cast<char>(c - 'A' + 'a'));
        } else {
            out.push_back(static_cast<char>(c));
        }
        previousSpace = false;
    }
    while (!out.empty() && out.back() == ' ') {
        out.pop_back();
    }
    return out;
}
}

inline std::optional<FameCategory> ParseFameCategory(std::string_view name) {
    const auto normalized = detail::NormalizeCategoryName(name);
    for (std::size_t i = 0; i < kFameCategoryNames.size(); ++i) {
        if (detail::NormalizeCategoryName(kFameCategoryNames[i]) == normalized) {
            return static_cast<FameCategory>(i);
        }
    }
    if (normalized == "tattoo") {
        return FameCategory::Tattoo;
    }
    if (normalized == "cum dump" || normalized == "cumdump") {
        return FameCategory::CumDump;
    }
    return std::nullopt;
}

struct Error {
    enum class Code : std::uint8_t {
        InvalidArgument = 0,
        NotFound,
        OutOfRange,
        InvalidState,
        Unsupported,
        Io,
        Internal,
    };

    Code code{Code::Internal};
    std::string message;

    friend bool operator==(const Error&, const Error&) = default;
};

template <class T>
using Result = std::expected<T, Error>;

inline std::unexpected<Error> Fail(Error::Code code, std::string message) {
    return std::unexpected<Error>{Error{code, std::move(message)}};
}

enum class FactState : std::uint8_t {
    KnownTrue = 0,
    KnownFalse,
    Unknown,
    Missing,
    Incompatible,
    Stale,
};

struct Fact {
    FactState state{FactState::Unknown};

    constexpr Fact() noexcept = default;
    constexpr explicit Fact(FactState value) noexcept : state(value) {}

    static constexpr Fact Known(bool value) noexcept {
        return Fact(value ? FactState::KnownTrue : FactState::KnownFalse);
    }

    constexpr std::optional<bool> Value() const noexcept {
        if (state == FactState::KnownTrue) {
            return true;
        }
        if (state == FactState::KnownFalse) {
            return false;
        }
        return std::nullopt;
    }

    constexpr bool IsKnownFalse() const noexcept { return state == FactState::KnownFalse; }
    constexpr bool IsKnownTrue() const noexcept { return state == FactState::KnownTrue; }

    friend constexpr bool operator==(const Fact&, const Fact&) = default;
};

class IRandom {
public:
    virtual ~IRandom() = default;
    virtual std::int32_t UniformInt(std::int32_t lo, std::int32_t hi) = 0;
};

enum class DiffusionBackend : std::uint8_t {
    Auto = 0,
    Scalar = 1,
    Avx2 = 2,
};

}

namespace std {
template <>
struct hash<slsf::StableFormKey> {
    std::size_t operator()(const slsf::StableFormKey& key) const noexcept {
        std::size_t seed = std::hash<std::string>{}(key.plugin);
        seed ^= std::hash<std::uint32_t>{}(key.localFormId) + 0x9e3779b9u + (seed << 6) + (seed >> 2);
        return seed;
    }
};

template <>
struct hash<slsf::StableRegionKey> {
    std::size_t operator()(const slsf::StableRegionKey& key) const noexcept {
        std::size_t seed = std::hash<std::string>{}(key.plugin);
        seed ^= std::hash<std::uint32_t>{}(key.localFormId) + 0x9e3779b9u + (seed << 6) + (seed >> 2);
        return seed;
    }
};
}
