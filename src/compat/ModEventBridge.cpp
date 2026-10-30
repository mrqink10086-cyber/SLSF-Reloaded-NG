#include "slsf/compat/ModEventBridge.h"

#include <array>
#include <mutex>
#include <unordered_map>

namespace slsf::compat {

namespace {

ModEventValue ZeroValue(ModEventType type) {
    ModEventValue v;
    v.type = type;
    switch (type) {
        case ModEventType::Bool: v.boolValue = false; break;
        case ModEventType::Int: v.intValue = 0; break;
        case ModEventType::Float: v.floatValue = 0.0; break;
        case ModEventType::String: v.stringValue.clear(); break;
        case ModEventType::Form: v.hasForm = false; v.formValue = {}; break;
        default: v.type = ModEventType::None; break;
    }
    return v;
}
ModEventParameter P(std::string name, ModEventType type,
                   std::optional<ModEventValue> def = std::nullopt) {
    return ModEventParameter{std::move(name), type, std::move(def)};
}
ModEventParameter S(std::string name, const char* def = nullptr) {
    if (def != nullptr) {
        ModEventValue v;
        v.type = ModEventType::String;
        v.stringValue = def;
        return P(std::move(name), ModEventType::String, std::move(v));
    }
    return P(std::move(name), ModEventType::String);
}
ModEventParameter B(std::string name) { return P(std::move(name), ModEventType::Bool); }
ModEventParameter I(std::string name) { return P(std::move(name), ModEventType::Int); }
ModEventParameter F(std::string name) { return P(std::move(name), ModEventType::Float); }
ModEventParameter L(std::string name) { return P(std::move(name), ModEventType::Form); }

using C = ModEventContract;

const std::array<C, 52> kInputContracts = {
    C{"SLSF_Reloaded_SendFameGainRoll", "OnExternalFameGainRoll",
     {S("EventLocation")}},
    C{"SLSF_Reloaded_SendFameGain", "OnExternalFameGain",
     {S("Category"), S("EventLocation"), I("MinFame"), I("MaxFame")}},
    C{"SLSF_Reloaded_SendManualFameGain", "OnExternalManualFameGain",
     {S("Category"), S("EventLocation"), I("MinIncrease"), I("MaxIncrease")}},
    C{"SLSF_Reloaded_SendManualFameGainAllInLocation", "OnExternalManualFameGainAllInLocation",
     {S("EventLocation"), I("MinIncrease"), I("MaxIncrease")}},
    C{"SLSF_Reloaded_SendManualFameGainAll", "OnExternalManualFameGainAll",
     {I("MinIncrease"), I("MaxIncrease")}},
    C{"SLSF_Reloaded_SendFameDecay", "OnExternalFameDecay", {}},
    C{"SLSF_Reloaded_SendManualFameDecay", "OnExternalManualFameDecay",
     {S("EventLocation"), S("Category"), I("MinDecay"), I("MaxDecay")}},
    C{"SLSF_Reloaded_SendManualFameDecayAllInLocation", "OnExternalManualFameDecayAllInLocation",
     {S("EventLocation"), I("MinDecay"), I("MaxDecay")}},
    C{"SLSF_Reloaded_SendManualFameDecayAll", "OnExternalManualFameDecayAll",
     {I("MinDecay"), I("MaxDecay")}},
    C{"SLSF_Reloaded_SendFameSpreadRoll", "OnExternalFameSpreadRoll", {}},
    C{"SLSF_Reloaded_SendFameSpread", "OnExternalFameSpread",
     {L("SpreadFromLocation")}},
    C{"SLSF_Reloaded_SendFameSpreadByName", "OnExternalFameSpreadByName",
     {S("SpreadFromLocation")}},
    C{"SLSF_Reloaded_SendManualFameSpread", "OnExternalManualFameSpread",
     {S("SpreadFromLocation"), S("SpreadToLocation"), S("Category"), I("PercentToSpread")}},
    C{"SLSF_Reloaded_SendLocationRegister", "OnExternalLocationRegister",
     {L("LocationToRegister")}},
    C{"SLSF_Reloaded_SendLocationUnregister", "OnExternalLocationUnregister",
     {L("LocationToUnregister")}},
    C{"SLSF_Reloaded_RegisterMod", "OnExternalModRegister",
     {S("ModName", "-EMPTY-")}},
    C{"SLSF_Reloaded_UnregisterMod", "OnExternalModUnregister",
     {S("ModName", "-EMPTY-")}},
    C{"SLSF_Reloaded_SetSlutFlag", "OnExternalSlutFlag",
     {S("ModName"), B("Flag")}},
    C{"SLSF_Reloaded_SetWhoreFlag", "OnExternalWhoreFlag",
     {S("ModName"), B("Flag")}},
    C{"SLSF_Reloaded_SetExhibitionistFlag", "OnExternalExhibitionistFlag",
     {S("ModName"), B("Flag")}},
    C{"SLSF_Reloaded_SetOralFlag", "OnExternalOralFlag",
     {S("ModName"), B("Flag")}},
    C{"SLSF_Reloaded_SetAnalFlag", "OnExternalAnalFlag",
     {S("ModName"), B("Flag")}},
    C{"SLSF_Reloaded_SetNastyFlag", "OnExternalNastyFlag",
     {S("ModName"), B("Flag")}},
    C{"SLSF_Reloaded_SetPregnantFlag", "OnExternalPregnantFlag",
     {S("ModName"), B("Flag")}},
    C{"SLSF_Reloaded_SetDominantFlag", "OnExternalDominantFlag",
     {S("ModName"), B("Flag")}},
    C{"SLSF_Reloaded_SetSubmissiveFlag", "OnExternalSubmissiveFlag",
     {S("ModName"), B("Flag")}},
    C{"SLSF_Reloaded_SetSadistFlag", "OnExternalSadistFlag",
     {S("ModName"), B("Flag")}},
    C{"SLSF_Reloaded_SetMasochistFlag", "OnExternalMasochistFlag",
     {S("ModName"), B("Flag")}},
    C{"SLSF_Reloaded_SetGentleFlag", "OnExternalGentleFlag",
     {S("ModName"), B("Flag")}},
    C{"SLSF_Reloaded_SetLikesMenFlag", "OnExternalLikesMenFlag",
     {S("ModName"), B("Flag")}},
    C{"SLSF_Reloaded_SetLikesWomenFlag", "OnExternalLikesWomenFlag",
     {S("ModName"), B("Flag")}},
    C{"SLSF_Reloaded_SetLikesOrcFlag", "OnExternalLikesOrcFlag",
     {S("ModName"), B("Flag")}},
    C{"SLSF_Reloaded_SetLikesKhajiitFlag", "OnExternalLikesKhajiitFlag",
     {S("ModName"), B("Flag")}},
    C{"SLSF_Reloaded_SetLikesArgonianFlag", "OnExternalLikesArgonianFlag",
     {S("ModName"), B("Flag")}},
    C{"SLSF_Reloaded_SetBestialityFlag", "OnExternalBestialityFlag",
     {S("ModName"), B("Flag")}},
    C{"SLSF_Reloaded_SetGroupFlag", "OnExternalGroupFlag",
     {S("ModName"), B("Flag")}},
    C{"SLSF_Reloaded_SetBoundFlag", "OnExternalBoundFlag",
     {S("ModName"), B("Flag")}},
    C{"SLSF_Reloaded_SetTattooFlag", "OnExternalTattooFlag",
     {S("ModName"), B("Flag")}},
    C{"SLSF_Reloaded_SetCumDumpFlag", "OnExternalCumDumpFlag",
     {S("ModName"), B("Flag")}},
    C{"SLSF_Reloaded_SetUnfaithfulFlag", "OnExternalUnfaithfulFlag",
     {S("ModName"), B("Flag")}},
    C{"SLSF_Reloaded_SetCuckFlag", "OnExternalCuckFlag",
     {S("ModName"), B("Flag")}},
    C{"SLSF_Reloaded_SetAirheadFlag", "OnExternalAirheadFlag",
     {S("ModName"), B("Flag")}},
    C{"SLSF_Reloaded_SetWhoreEventFlag", "OnExternalWhoreEventFlag",
     {S("ModName"), B("Flag")}},
    C{"SLSF_Reloaded_SetSexWorkerFlag", "OnExternalSexWorkerFlag",
     {S("ModName"), B("Flag")}},
    C{"SLSF_Reloaded_EnableFameType", "OnEnableFameType",
     {S("FameType"), S("ModName"), B("Flag")}},
    C{"SLSF_Reloaded_RequestLocation", "OnRequestLocation",
     {B("Strict")}},
    C{"SLSF_Reloaded_RequestRecognitionTime", "OnRequestRecognitionTime",
     {S("FameLocation")}},
    C{"SLSF_Reloaded_RequestFame", "OnRequestFame",
     {S("LocationName"), S("Category")}},
    C{"SLSF_Reloaded_RequestCumVisibility", "OnRequestCumVisibility",
     {S("CumLocation")}},
    C{"SLSF_Reloaded_RequestFlagState", "OnRequestFlagState",
     {S("FlagName")}},
    C{"SLSF_Reloaded_RequestModFlagState", "OnRequestModFlagState",
     {S("ModName"), S("FlagName")}},
    C{"SLSF_Reloaded_RequestModRegisterState", "OnRequestModRegisterState",
     {S("ModName")}},
};

const std::array<C, 7> kReplyContracts = {
    C{"SLSF_Reloaded_ReturnRequestedLocation", "",
     {S("LocationName")}},
    C{"SLSF_Reloaded_ReturnRecognitionTime", "",
     {F("RecognitionTime")}},
    C{"SLSF_Reloaded_ReturnRequestedFame", "",
     {S("LocationName"), S("Category"), I("RequestedFame")}},
    C{"SLSF_Reloaded_ReturnRequestedCum", "",
     {B("Visible")}},
    C{"SLSF_Reloaded_ReturnFlagState", "",
     {S("FlagName"), S("FlagExists"), B("RequestedFlagState")}},
    C{"SLSF_Reloaded_ReturnModFlagState", "",
     {S("ModName"), B("IsModRegistered"), S("FlagName"), S("FlagExists"),
      B("RequestedModFlagState")}},
    C{"SLSF_Reloaded_ReturnModRegisteredState", "",
     {S("ModName"), B("IsModRegistered")}},
};

struct Tables {
    std::unordered_map<std::string, std::size_t> inputIndex;
    std::unordered_map<std::string, std::size_t> replyIndex;
};

const Tables& Indexes() {
    static const Tables tables = [] {
        Tables t;
        for (std::size_t i = 0; i < kInputContracts.size(); ++i) {
            t.inputIndex.emplace(kInputContracts[i].event, i);
        }
        for (std::size_t i = 0; i < kReplyContracts.size(); ++i) {
            t.replyIndex.emplace(kReplyContracts[i].event, i);
        }
        return t;
    }();
    return tables;
}
Result<ModEvent> ParseContracted(const C* contract,
                                 const std::vector<ModEventValue>& values,
                                 const char* kind) {
    if (contract == nullptr) {
        return Fail(Error::Code::NotFound,
                    std::string("unknown ") + kind + " ModEvent");
    }
    if (values.size() > contract->parameters.size()) {
        return Fail(Error::Code::InvalidArgument,
                    "too many parameters for " + contract->event);
    }
    ModEvent out;
    out.eventName = contract->event;
    out.handler = contract->handler;
    out.parameterNames.reserve(contract->parameters.size());
    out.values.reserve(contract->parameters.size());
    for (std::size_t i = 0; i < contract->parameters.size(); ++i) {
        const auto& param = contract->parameters[i];
        out.parameterNames.push_back(param.name);
        if (i < values.size()) {
            const auto& v = values[i];
            if (v.type != param.type) {
                return Fail(Error::Code::InvalidArgument,
                            "parameter type mismatch at index " +
                                std::to_string(i) + " for " + contract->event);
            }
            out.values.push_back(v);
        } else if (param.defaultValue) {
            out.values.push_back(*param.defaultValue);
        } else {
            out.values.push_back(ZeroValue(param.type));
        }
    }
    return out;
}
const char* ModEventTypeToString(ModEventType type) noexcept {
    switch (type) {
        case ModEventType::None: return "None";
        case ModEventType::Bool: return "Bool";
        case ModEventType::Int: return "Int";
        case ModEventType::Float: return "Float";
        case ModEventType::String: return "String";
        case ModEventType::Form: return "Form";
    }
    return "None";
}
}
const ModEventValue* ModEvent::Find(const std::string& name) const {
    for (std::size_t i = 0; i < parameterNames.size(); ++i) {
        if (parameterNames[i] == name) {
            return &values[i];
        }
    }
    return nullptr;
}
bool ModEvent::BoolValue(const std::string& name, bool fallback) const {
    if (const auto* v = Find(name)) {
        return v->boolValue;
    }
    return fallback;
}
std::int64_t ModEvent::IntValue(const std::string& name,
                                std::int64_t fallback) const {
    if (const auto* v = Find(name)) {
        return v->intValue;
    }
    return fallback;
}
double ModEvent::FloatValue(const std::string& name, double fallback) const {
    if (const auto* v = Find(name)) {
        return v->floatValue;
    }
    return fallback;
}
std::string ModEvent::StringValue(const std::string& name,
                                 const std::string& fallback) const {
    if (const auto* v = Find(name)) {
        return v->stringValue;
    }
    return fallback;
}
const StableFormKey* ModEvent::FormValue(const std::string& name) const {
    if (const auto* v = Find(name)) {
        if (v->hasForm) {
            return &v->formValue;
        }
        return nullptr;
    }
    return nullptr;
}
const std::vector<ModEventContract>& ModEventBridge::InputContracts() {
    static const std::vector<ModEventContract> contracts(kInputContracts.begin(),
                                                        kInputContracts.end());
    return contracts;
}
const std::vector<ModEventContract>& ModEventBridge::ReplyContracts() {
    static const std::vector<ModEventContract> contracts(kReplyContracts.begin(),
                                                        kReplyContracts.end());
    return contracts;
}
std::size_t ModEventBridge::InputCount() noexcept {
    return kInputContracts.size();
}
std::size_t ModEventBridge::ReplyCount() noexcept {
    return kReplyContracts.size();
}
const ModEventContract* ModEventBridge::FindInput(const std::string& eventName) const noexcept {
    const auto it = Indexes().inputIndex.find(eventName);
    if (it == Indexes().inputIndex.end()) {
        return nullptr;
    }
    return &kInputContracts[it->second];
}
const ModEventContract* ModEventBridge::FindReply(const std::string& eventName) const noexcept {
    const auto it = Indexes().replyIndex.find(eventName);
    if (it == Indexes().replyIndex.end()) {
        return nullptr;
    }
    return &kReplyContracts[it->second];
}
Result<ModEvent> ModEventBridge::ParseInput(
    const std::string& eventName, const std::vector<ModEventValue>& values) const {
    return ParseContracted(FindInput(eventName), values, "input");
}
Result<ModEvent> ModEventBridge::ParseReply(
    const std::string& eventName, const std::vector<ModEventValue>& values) const {
    return ParseContracted(FindReply(eventName), values, "reply");
}
bool ModEventBridge::ValidateReply(const ModEvent& reply) const {
    const C* contract = FindReply(reply.eventName);
    if (contract == nullptr) {
        return false;
    }
    if (reply.values.size() != contract->parameters.size()) {
        return false;
    }
    for (std::size_t i = 0; i < contract->parameters.size(); ++i) {
        if (reply.values[i].type != contract->parameters[i].type) {
            return false;
        }
    }
    return true;
}
bool ModEventBridge::RegisterHandler(const std::string& eventName,
                                     EventHandler handler) {
    if (FindInput(eventName) == nullptr) {
        return false;
    }
    std::lock_guard lock(handlersMutex_);
    handlers_.emplace(eventName, std::move(handler));
    return true;
}
Result<ModEvent> ModEventBridge::Dispatch(
    const std::string& eventName, const std::vector<ModEventValue>& values) {
    auto result = ParseInput(eventName, values);
    if (!result) {
        return result;
    }
    ModEvent event = std::move(*result);
    EventHandler handler;
    {
        std::lock_guard lock(handlersMutex_);
        const auto it = handlers_.find(eventName);
        if (it != handlers_.end()) {
            handler = it->second;
        }
    }
    if (handler) {
        handler(event);
    }
    return event;
}
void ModEventBridge::SetReplySink(ReplySink sink) {
    std::lock_guard lock(sinkMutex_);
    replySink_ = std::move(sink);
}
bool ModEventBridge::SendReply(const ModEvent& reply) {
    if (!ValidateReply(reply)) {
        return false;
    }
    ReplySink sink;
    {
        std::lock_guard lock(sinkMutex_);
        sink = replySink_;
    }
    if (sink) {
        sink(reply.eventName, reply.values);
    }
    return true;
}
std::size_t ModEventBridge::HandlerCount() const noexcept {
    std::lock_guard lock(handlersMutex_);
    return handlers_.size();
}
}