#pragma once

#include "slsf/core/Types.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace slsf {

inline constexpr std::size_t kLegacyInputEventCount = 52;
inline constexpr std::size_t kLegacyReplyEventCount = 7;
inline constexpr std::size_t kLegacyFunctionCount = 124;
inline constexpr std::size_t kLegacyCategoryCount = kFameCategoryCount;

enum class LegacyValueType : std::uint8_t {
    None = 0,
    Bool,
    Int,
    Float,
    String,
    Form,
    Actor,
    ReferenceAlias,
    Quest,
    StringArray,
    FormArray,
    ActorArray,
    Script,
    Unknown,
};

constexpr std::string_view ToString(LegacyValueType type) noexcept {
    switch (type) {
    case LegacyValueType::None: return "None";
    case LegacyValueType::Bool: return "Bool";
    case LegacyValueType::Int: return "Int";
    case LegacyValueType::Float: return "Float";
    case LegacyValueType::String: return "String";
    case LegacyValueType::Form: return "Form";
    case LegacyValueType::Actor: return "Actor";
    case LegacyValueType::ReferenceAlias: return "ReferenceAlias";
    case LegacyValueType::Quest: return "Quest";
    case LegacyValueType::StringArray: return "String[]";
    case LegacyValueType::FormArray: return "Form[]";
    case LegacyValueType::ActorArray: return "Actor[]";
    case LegacyValueType::Script: return "Script";
    case LegacyValueType::Unknown: return "Unknown";
    }
    return "Unknown";
}
struct LegacyParameter {
    std::string name;
    LegacyValueType type{LegacyValueType::Unknown};
    std::optional<std::string> defaultValue;

    friend bool operator==(const LegacyParameter&, const LegacyParameter&) = default;
};

struct LegacyEventContract {
    std::string event;
    std::string handler;
    std::vector<LegacyParameter> parameters;

    friend bool operator==(const LegacyEventContract&, const LegacyEventContract&) = default;
};

struct LegacyFunctionContract {
    std::string id;
    std::string script;
    std::string scriptType;
    std::string name;
    LegacyValueType returnType{LegacyValueType::None};
    std::vector<LegacyParameter> parameters;
    bool isGlobal{};
    bool isNative{};
    bool isLatent{};
    bool isPublic{};
    std::string classification;
    std::optional<std::string> invalidInputResult;
    std::string source;
};

struct LegacyRecordIdentity {
    std::string signature;
    std::string editorId;
    std::string formId;
    std::string plugin;
    std::vector<std::string> vmadScripts;
    bool isPublic{};
};

struct LegacyFlagContract {
    std::string name;
    std::optional<FameCategory> category;
    bool deprecated{};
    std::vector<std::string> aliases;
};

inline constexpr std::array<std::string_view, 2> kLegacyNonCategoryFlags = {
    "Whore Event",
    "Sex Worker",
};

std::string_view BuildId() noexcept;

}