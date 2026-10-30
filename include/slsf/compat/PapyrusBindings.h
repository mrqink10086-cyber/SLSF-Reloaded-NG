#pragma once

#include "slsf/core/Contracts.h"
#include "slsf/core/Types.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace slsf::compat {

struct PapyrusParameter {
    std::string name;
    LegacyValueType type{LegacyValueType::Unknown};
    bool hasDefault{false};
    bool defaultBool{false};
    std::int64_t defaultInt{0};
    double defaultFloat{0.0};
    std::string defaultString;
};

enum class PapyrusBindingKind : std::uint8_t {
    Global = 0,
    GlobalNative = 1,
    Quest = 2,
    ReferenceAlias = 3,
};

using BindingKind = PapyrusBindingKind;

struct PapyrusFunction {
    std::string id;
    std::string script;
    std::string name;
    std::string returnTypeName;
    LegacyValueType returnType{LegacyValueType::None};
    std::vector<PapyrusParameter> parameters;
    bool isGlobal{false};
    bool isNative{false};
    bool isLatent{false};
    bool isPublic{true};
    std::string classification;
    std::string invalidInputResult;
    PapyrusBindingKind binding{PapyrusBindingKind::Global};
    StableFormKey quest;
    std::string questEditorId;
    std::int32_t aliasIndex{-1};
    std::string aliasName;
};

struct PapyrusValue {
    LegacyValueType type{LegacyValueType::None};
    bool boolValue{false};
    std::int64_t intValue{0};
    double floatValue{0.0};
    std::string stringValue;
    StableFormKey formKey;
};

struct PapyrusInstance {
    // Origin plugin and local ID from the engine quest object, never its
    // load-order-dependent runtime FormID.
    StableFormKey quest;
    std::int32_t aliasIndex{-1};
};

class PapyrusBindings {
public:
    PapyrusBindings();

    [[nodiscard]] const std::vector<PapyrusFunction>& Functions() const noexcept;
    [[nodiscard]] const std::vector<PapyrusFunction>& PublicFunctions() const noexcept;
    [[nodiscard]] std::size_t PublicCount() const noexcept;
    [[nodiscard]] std::size_t InternalCount() const noexcept;

    [[nodiscard]] const PapyrusFunction* Find(const std::string& id) const;
    [[nodiscard]] const PapyrusFunction* Find(const std::string& script,
                                             const std::string& function) const;

    [[nodiscard]] bool IsBoundTo(const PapyrusFunction& function,
                                 const PapyrusInstance& instance) const;

    [[nodiscard]] bool ConvertArguments(const PapyrusFunction& function,
                                        std::vector<PapyrusValue> in,
                                        std::vector<PapyrusValue>& out) const;

    [[nodiscard]] PapyrusValue ConvertInvalidInput(const PapyrusFunction& function) const;

private:
    std::vector<PapyrusFunction> functions_;
    std::vector<PapyrusFunction> publicFunctions_;
    std::unordered_map<std::string, std::size_t> byId_;
};

}
