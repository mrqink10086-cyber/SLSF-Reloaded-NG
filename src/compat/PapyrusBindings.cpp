#include "slsf/compat/PapyrusBindings.h"

#include <algorithm>
#include <cctype>
#include <iterator>
#include <utility>
#include <vector>

#include "papyrus_function_table.inc"

namespace slsf::compat {

namespace {

bool SameQuest(const StableFormKey& expected, const StableFormKey& actual) {
    return !expected.plugin.empty() && expected.localFormId != 0 &&
           expected.localFormId == actual.localFormId &&
           std::equal(expected.plugin.begin(), expected.plugin.end(),
                      actual.plugin.begin(), actual.plugin.end(),
                      [](unsigned char a, unsigned char b) {
                          return std::tolower(a) == std::tolower(b);
                      });
}

bool CompatibleArgument(LegacyValueType declared, const PapyrusValue& in) {
    switch (declared) {
        case LegacyValueType::Bool:
            return in.type == LegacyValueType::Bool;
        case LegacyValueType::Int:
            return in.type == LegacyValueType::Int ||
                   in.type == LegacyValueType::Bool;
        case LegacyValueType::Float:
            return in.type == LegacyValueType::Float ||
                   in.type == LegacyValueType::Int ||
                   in.type == LegacyValueType::Bool;
        case LegacyValueType::String:
            return in.type == LegacyValueType::String;
        case LegacyValueType::Form:
            return in.type == LegacyValueType::Form ||
                   in.type == LegacyValueType::Actor;
        case LegacyValueType::Actor:
        case LegacyValueType::ReferenceAlias:
        case LegacyValueType::Quest:
        case LegacyValueType::StringArray:
        case LegacyValueType::FormArray:
        case LegacyValueType::ActorArray:
        case LegacyValueType::Script:
            return in.type == declared;
        default:
            return false;
    }
}
PapyrusValue ConvertArgument(LegacyValueType declared, PapyrusValue in) {
    switch (declared) {
        case LegacyValueType::Bool:
            in.type = LegacyValueType::Bool;
            break;
        case LegacyValueType::Int:
            if (in.type == LegacyValueType::Bool) in.intValue = in.boolValue ? 1 : 0;
            in.type = LegacyValueType::Int;
            break;
        case LegacyValueType::Float:
            if (in.type == LegacyValueType::Bool) in.floatValue = in.boolValue ? 1.0 : 0.0;
            else if (in.type == LegacyValueType::Int) in.floatValue = static_cast<double>(in.intValue);
            in.type = LegacyValueType::Float;
            break;
        case LegacyValueType::String:
            in.type = LegacyValueType::String;
            break;
        case LegacyValueType::Form:
            in.type = LegacyValueType::Form;
            break;
        default:
            in.type = declared;
            break;
    }
    return in;
}
PapyrusValue FillDefault(const PapyrusParameter& param) {
    PapyrusValue out;
    out.type = param.type;
    out.boolValue = param.defaultBool;
    out.intValue = param.defaultInt;
    out.floatValue = param.defaultFloat;
    out.stringValue = param.defaultString;
    return out;
}
}
PapyrusBindings::PapyrusBindings() {
    functions_.resize(std::size(detail::kTable));
    for (std::size_t i = 0; i < functions_.size(); ++i) {
        functions_[i] = detail::kTable[i];
        byId_.emplace(functions_[i].id, i);
    }
    publicFunctions_.reserve(86);
    for (const auto& fn : functions_) {
        if (fn.isPublic) publicFunctions_.push_back(fn);
    }
}
const std::vector<PapyrusFunction>& PapyrusBindings::Functions() const noexcept {
    return functions_;
}
const std::vector<PapyrusFunction>& PapyrusBindings::PublicFunctions() const noexcept {
    return publicFunctions_;
}
std::size_t PapyrusBindings::PublicCount() const noexcept {
    return publicFunctions_.size();
}
std::size_t PapyrusBindings::InternalCount() const noexcept {
    return functions_.size() - publicFunctions_.size();
}
const PapyrusFunction* PapyrusBindings::Find(const std::string& id) const {
    if (const auto it = byId_.find(id); it != byId_.end()) {
        return &functions_[it->second];
    }
    return nullptr;
}
const PapyrusFunction* PapyrusBindings::Find(const std::string& script,
                                            const std::string& function) const {
    return Find(script + "." + function);
}
bool PapyrusBindings::IsBoundTo(const PapyrusFunction& function,
                                const PapyrusInstance& instance) const {
    switch (function.binding) {
        case PapyrusBindingKind::Global:
        case PapyrusBindingKind::GlobalNative:
            return true;
        case PapyrusBindingKind::Quest:
            return SameQuest(function.quest, instance.quest) &&
                   instance.aliasIndex == -1;
        case PapyrusBindingKind::ReferenceAlias:
            return SameQuest(function.quest, instance.quest) &&
                   instance.aliasIndex == function.aliasIndex;
    }
    return false;
}
bool PapyrusBindings::ConvertArguments(const PapyrusFunction& function,
                                       std::vector<PapyrusValue> in,
                                       std::vector<PapyrusValue>& out) const {
    const auto& params = function.parameters;
    if (in.size() > params.size()) return false;

    std::vector<PapyrusValue> converted;
    converted.reserve(params.size());
    for (std::size_t i = 0; i < params.size(); ++i) {
        const auto& param = params[i];
        if (i < in.size()) {
            if (!CompatibleArgument(param.type, in[i])) return false;
            converted.push_back(ConvertArgument(param.type, std::move(in[i])));
        } else {
            if (!param.hasDefault) return false;
            converted.push_back(FillDefault(param));
        }
    }
    out = std::move(converted);
    return true;
}
PapyrusValue PapyrusBindings::ConvertInvalidInput(
    const PapyrusFunction& function) const {
    std::string text = function.invalidInputResult;
    if (text.size() >= 2 && text.front() == '"' && text.back() == '"') {
        text = text.substr(1, text.size() - 2);
    }
    PapyrusValue out;
    if (text.empty() || text.rfind("no-op", 0) == 0 || text == "None") {
        out.type = LegacyValueType::None;
        return out;
    }
    if (text == "0") {
        out.type = LegacyValueType::Int;
        out.intValue = 0;
        return out;
    }
    if (text == "0.0") {
        out.type = LegacyValueType::Float;
        out.floatValue = 0.0;
        return out;
    }
    if (text == "False") {
        out.type = LegacyValueType::Bool;
        out.boolValue = false;
        return out;
    }
    out.type = LegacyValueType::String;
    out.stringValue = text;
    return out;
}
}
