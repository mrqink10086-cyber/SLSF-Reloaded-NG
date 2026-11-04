#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#include <Windows.h>
#ifdef GetObject
#undef GetObject
#endif

namespace logs = SKSE::log;

#include "slsf/compat/VmNatives.h"
#include <unordered_set>

#include "slsf/compat/PapyrusBindings.h"
#include "slsf/api/PublicApi.h"
#include "slsf/engine/RecordLookup.h"
#include "slsf/compat/ModEventBridge.h"
#include "slsf/compat/LegacyOutputs.h"
#include "slsf/compat/StorageOutputQueue.h"
#include "slsf/io/JsonTransfer.h"

#include <RE/B/BSTSmartPointer.h>
#include <RE/F/FormTypes.h>
#include <RE/I/IStackCallbackFunctor.h>
#include <RE/N/NativeFunctionBase.h>
#include <RE/P/PackUnpack.h>
#include <RE/S/Stack.h>
#include <RE/S/StackFrame.h>
#include <RE/T/TypeInfo.h>
#include <RE/V/Variable.h>
#include <RE/V/VirtualMachine.h>

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace slsf::compat {

namespace {

using slsf::FameCategory;
using slsf::LegacyValueType;
using slsf::StableFormKey;
using compat::ModEventBridge;
using compat::ModEventValue;

std::mutex stateMutex;
VmNatives::ApiProvider apiProvider;
ModEventBridge* bridgeInstance = nullptr;
std::function<Epoch()> currentEpoch;
std::function<bool(Epoch)> validEpoch;
VmNatives::Counters countersImpl;
std::atomic<bool> nativeKindTrace{true};
std::atomic<bool> legacyLogging{true};
std::atomic<int> legacyLogLevel{0};

slsf::api::PublicApi* CurrentApi() {
    VmNatives::ApiProvider provider;
    {
        std::lock_guard guard(stateMutex);
        provider = apiProvider;
    }
    return provider ? provider() : nullptr;
}

Epoch ReplyEpoch() {
    std::lock_guard guard(stateMutex);
    return currentEpoch ? currentEpoch() : Epoch{};
}

bool ReplyCurrent(Epoch epoch) {
    std::lock_guard guard(stateMutex);
    return validEpoch && validEpoch(epoch);
}

ModEventValue BoolResult(bool value) {
    ModEventValue out;
    out.type = ModEventType::Bool;
    out.boolValue = value;
    return out;
}
ModEventValue IntResult(std::int64_t value) {
    ModEventValue out;
    out.type = ModEventType::Int;
    out.intValue = value;
    return out;
}
ModEventValue FloatResult(double value) {
    ModEventValue out;
    out.type = ModEventType::Float;
    out.floatValue = value;
    return out;
}
ModEventValue StringResult(std::string value) {
    ModEventValue out;
    out.type = ModEventType::String;
    out.stringValue = std::move(value);
    return out;
}
ModEventValue NoneResult() {
    ModEventValue out;
    out.type = ModEventType::None;
    return out;
}

RE::TESForm* ResolveLoadedForm(const StableFormKey& key) {
    return slsf::engine::LookupLoadedRecord<RE::TESForm>(
        RE::TESDataHandler::GetSingleton(), key);
}

RE::TESGlobal* ResolveLoadedGlobal(const StableFormKey& key) {
    return slsf::engine::LookupLoadedRecord<RE::TESGlobal>(
        RE::TESDataHandler::GetSingleton(), key);
}

class PapyrusLogSink final : public RE::BSTEventSink<RE::BSScript::LogEvent> {
public:
    RE::BSEventNotifyControl ProcessEvent(
        const RE::BSScript::LogEvent* event,
        RE::BSTEventSource<RE::BSScript::LogEvent>*) override {
        if (event && event->errorMsg && *event->errorMsg) {
            logs::info("papyrus[{}] {}", static_cast<int>(event->severity), event->errorMsg);
        }
        return RE::BSEventNotifyControl::kContinue;
    }
};

PapyrusLogSink g_papyrusLogSink;

class NoopCallback final : public RE::BSScript::IStackCallbackFunctor {public:
    void operator()(RE::BSScript::Variable) override {}
    void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}
};

class ReplyCreateCallback final : public RE::BSScript::IStackCallbackFunctor {
public:
    ReplyCreateCallback(std::string eventName, std::vector<ModEventValue> values,
                        Epoch epoch, std::int32_t handle = 0, std::size_t next = 0)
        : eventName_(std::move(eventName)), values_(std::move(values)),
          epoch_(epoch), handle_(handle), next_(next) {}

    void operator()(RE::BSScript::Variable result) override {
        auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        if (!vm) {
            ++countersImpl.implFallbacks;
            return;
        }
        if (handle_ == 0) {
            if (!result.IsInt() || result.GetSInt() == 0) {
                logs::error("compat: ModEvent.Create failed for {}", eventName_);
                ++countersImpl.implFallbacks;
                return;
            }
            handle_ = result.GetSInt();
        }
        if (next_ > values_.size()) {
            if (result.IsBool() && result.GetBool()) ++countersImpl.repliesSent;
            else logs::error("compat: ModEvent.Send failed for {}", eventName_);
            return;
        }
        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> none{new NoopCallback()};
        if (!ReplyCurrent(epoch_)) {
            vm->DispatchStaticCall("ModEvent", "Release",
                RE::MakeFunctionArguments(std::int32_t(handle_)), none);
            return;
        }
        RE::BSScript::IFunctionArguments* args = nullptr;
        const char* methodName = "Send";
        if (next_ == values_.size()) {
            args = RE::MakeFunctionArguments(std::int32_t(handle_));
        } else {
            const auto& value = values_[next_];
            switch (value.type) {
            case ModEventType::String:
                methodName = "PushString";
                args = RE::MakeFunctionArguments(std::int32_t(handle_), std::string(value.stringValue));
                break;
            case ModEventType::Int: {
                methodName = "PushInt";
                const std::int32_t pushed = static_cast<std::int32_t>(value.intValue);
                args = RE::MakeFunctionArguments(std::int32_t(handle_), std::int32_t(pushed));
                break;
            }
            case ModEventType::Float: {
                methodName = "PushFloat";
                const float pushed = static_cast<float>(value.floatValue);
                args = RE::MakeFunctionArguments(std::int32_t(handle_), float(pushed));
                break;
            }
            case ModEventType::Bool: {
                methodName = "PushBool";
                const bool pushed = value.boolValue;
                args = RE::MakeFunctionArguments(std::int32_t(handle_), bool(pushed));
                break;
            }
            case ModEventType::Form:
                methodName = "PushForm";
                args = RE::MakeFunctionArguments(std::int32_t(handle_),
                    static_cast<RE::TESForm*>(value.hasForm ? ResolveLoadedForm(value.formValue) : nullptr));
                break;
            default:
                methodName = nullptr;
                break;
            }
        }
        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> continuation{
            new ReplyCreateCallback(eventName_, values_, epoch_, handle_, next_ + 1)};
        if (!methodName || !args || !vm->DispatchStaticCall("ModEvent", methodName, args, continuation)) {
            ++countersImpl.implFallbacks;
            logs::error("compat: reply {} failed at argument {}", eventName_, next_);
            vm->DispatchStaticCall("ModEvent", "Release",
                RE::MakeFunctionArguments(std::int32_t(handle_)), none);
        }
    }

    void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}

private:
    std::string eventName_;
    std::vector<ModEventValue> values_;
    Epoch epoch_;
    std::int32_t handle_{};
    std::size_t next_{};
};

class EngineGlobalsOutputSink final : public LegacyOutputSink {
    class Completion final : public RE::BSScript::IStackCallbackFunctor {
    public:
        explicit Completion(std::uint64_t request) : request_(request) {}
        void operator()(RE::BSScript::Variable) override {
            if (auto* tasks = SKSE::GetTaskInterface()) tasks->AddTask([request = request_] {
                { std::lock_guard lock(outputMutex); outputQueue.Complete(request); }
                Pump();
            });
        }
        void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}
    private:
        std::uint64_t request_;
    };
public:
    void Flush() {
        auto* api = CurrentApi();
        auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        if (!api || !vm) return;
        const auto epoch = api->CurrentEpoch();
        const auto token = std::to_string(epoch.session) + ":" + std::to_string(epoch.index) + ":" + std::to_string(epoch.scene) + ":" + std::to_string(api->LastPublishedRevision().value);
        std::vector<StorageOutput> batch;
        for (auto& [key, values] : lists_) {
            batch.push_back({epoch, token, key, std::move(values)});
        }
        { std::lock_guard lock(outputMutex); outputQueue.Submit(std::move(batch)); }
        Pump();
    }
    static bool OutputCurrent(RE::StaticFunctionTag*, std::string token) {
        std::lock_guard lock(outputMutex);
        const auto epoch = ReplyEpoch();
        return outputQueue.Current(token, epoch) && ReplyCurrent(epoch);
    }
    static void Pump() {
        for (;;) {
            std::optional<StorageOutput> next;
            { std::lock_guard lock(outputMutex); next = outputQueue.Take(); }
            if (!next) return;
            auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
            if (vm && ReplyCurrent(next->epoch)) {
                RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback{new Completion(next->request)};
                if (vm->DispatchStaticCall("SLSFR_GameplayNative", "PublishStorage", RE::MakeFunctionArguments(std::string(next->token), std::string(next->key), std::vector<std::int32_t>(next->values)), callback)) return;
                logs::error("compat: StorageUtil dispatch failed key={} token={}", next->key, next->token);
            }
            { std::lock_guard lock(outputMutex); outputQueue.Complete(next->request); }
        }
    }
    void SetCurrentFameGlobal(const StableFormKey& key, int value) override {
        if (auto* toast = ResolveLoadedGlobal(key)) {
            toast->value = static_cast<float>(value);
        }
    }
    void SetStorageIntList(const std::string& key, std::size_t locationIndex,
                           int value) override {
        auto& values = lists_[key];
        if (values.size() <= locationIndex) values.resize(locationIndex + 1);
        values[locationIndex] = value;
    }
    void SetLegacyBridgeGlobal(const StableFormKey& key, int value) override {
        if (auto* toast = ResolveLoadedGlobal(key)) {
            toast->value = static_cast<float>(value);
        }
    }
private:
    std::map<std::string, std::vector<std::int32_t>> lists_;
    inline static std::mutex outputMutex;
    inline static StorageOutputQueue outputQueue;
};

using ImplBody = std::function<void(std::vector<ModEventValue>&, ModEventValue&)>;

class BridgeFunction final : public RE::BSScript::NF_util::NativeFunctionBase {
public:
    BridgeFunction(RE::BSScript::Internal::VirtualMachine& vm,
                   const PapyrusFunction& spec, bool isStatic, ImplBody body)
        : RE::BSScript::NF_util::NativeFunctionBase(
              spec.name, spec.script, isStatic,
              static_cast<std::uint16_t>(spec.parameters.size())),
          spec_(spec),
          body_(std::move(body)) {
        _isLatent = true;
        _retType.SetType(RawTypeFor(vm, spec.returnType, spec.returnTypeName));
        for (std::size_t i = 0; i < spec.parameters.size(); ++i) {
            _descTable.entries[i].first = spec.parameters[i].name;
            _descTable.entries[i].second.SetType(RawTypeFor(vm, spec.parameters[i].type,
                                                           spec.parameters[i].typeName));
        }
    }

    bool HasStub() const override { return true; }

    bool MarshallAndDispatch(RE::BSScript::Variable& base,
                             RE::BSScript::Internal::VirtualMachine& vm,
                             RE::VMStackID stack,
                             RE::BSScript::Variable& a_resultValue,
                             const RE::BSScript::StackFrame& a_frame) const override {
        std::vector<ModEventValue> args;
        args.reserve(spec_.parameters.size());
        const auto page = a_frame.GetPageForFrame();
        for (std::size_t i = 0; i < spec_.parameters.size(); ++i) {
            const auto& variable = a_frame.GetStackFrameVariable(
                static_cast<std::uint32_t>(i), page);
            args.push_back(ConvertArg(variable, spec_.parameters[i].type));
        }
        if (!_isStatic && (!base.IsObject() || !base.GetObject())) return RE::BSScript::kFailed;
        auto* tasks = SKSE::GetTaskInterface();
        if (!tasks || !body_) return RE::BSScript::kFailed;
        const auto epoch = ReplyEpoch();
        const auto self = base.IsObject() && base.GetObject() ? base.GetObject()->GetHandle() : RE::VMHandle{};
        tasks->AddTask([this, args = std::move(args), epoch, self, vm = &vm, stack]() mutable {
            if (!ReplyCurrent(epoch)) return;
            ModEventValue result = NoneResult();
            bool validSelf = _isStatic;
            if (!_isStatic) {
                auto* quest = engine::LookupLoadedRecord<RE::TESQuest>(RE::TESDataHandler::GetSingleton(), spec_.quest);
                if (quest) {
                    auto* policy = vm->GetObjectHandlePolicy();
                    if (spec_.aliasIndex < 0) validSelf = self == policy->GetHandleForObject(RE::FormType::Quest, quest);
                    else for (auto* alias : quest->aliases) if (alias && alias->aliasID == static_cast<std::uint32_t>(spec_.aliasIndex)) validSelf = self == policy->GetHandleForObject(alias->GetVMTypeID(), alias);
                }
            }
            if (validSelf) {
            try {
                body_(args, result);
                ++countersImpl.nativeCalls;
                if (nativeKindTrace.exchange(false, std::memory_order_relaxed)) {
                    logs::info("compat: native hit {} with {} args", spec_.id, args.size());
                }
            } catch (...) {
                logs::error("compat: native impl crashed for {}", spec_.id);
            }
            }
            RE::BSScript::Variable packed;
            PackResult(packed, result, spec_.returnType);
            vm->ReturnFromLatent(stack, packed);
        });
        a_resultValue.SetNone();
        return RE::BSScript::kStarted;
    }

private:
    static RE::BSScript::TypeInfo::RawType RawTypeFor(
        RE::BSScript::Internal::VirtualMachine& vm, LegacyValueType type, std::string name) {
        using RawType = RE::BSScript::TypeInfo::RawType;
        switch (type) {
        case LegacyValueType::None: return RawType::kNone;
        case LegacyValueType::Bool: return RawType::kBool;
        case LegacyValueType::Int: return RawType::kInt;
        case LegacyValueType::Float: return RawType::kFloat;
        case LegacyValueType::String: return RawType::kString;
        case LegacyValueType::StringArray: return RawType::kStringArray;
        default: break;
        }
        const bool array = type == LegacyValueType::FormArray || type == LegacyValueType::ActorArray;
        if (name.ends_with("[]")) name.resize(name.size() - 2);
        if (name.empty()) name = type == LegacyValueType::Actor || type == LegacyValueType::ActorArray
                                    ? "Actor" : "Form";
        RE::BSTSmartPointer<RE::BSScript::ObjectTypeInfo> objectType;
        if (!vm.GetScriptObjectType(name, objectType) || !objectType)
            throw std::runtime_error("cannot resolve Papyrus type " + name);
        return static_cast<RawType>(reinterpret_cast<std::uintptr_t>(objectType.get()) | (array ? 1u : 0u));
    }

    static ModEventValue ConvertArg(const RE::BSScript::Variable& variable,
                                    LegacyValueType type) {
        ModEventValue out;
        switch (type) {
        case LegacyValueType::String:
            out.type = ModEventType::String;
            out.stringValue = variable.Unpack<std::string>();
            break;
        case LegacyValueType::Int:
            out.type = ModEventType::Int;
            out.intValue = static_cast<std::int64_t>(variable.Unpack<std::int32_t>());
            break;
        case LegacyValueType::Float:
            out.type = ModEventType::Float;
            out.floatValue = static_cast<double>(variable.Unpack<float>());
            break;
        case LegacyValueType::Bool:
            out.type = ModEventType::Bool;
            out.boolValue = variable.Unpack<bool>();
            break;
        case LegacyValueType::Form:
        case LegacyValueType::Actor:
        case LegacyValueType::Quest:
        {
            out.type = ModEventType::Form;
            if (auto* form = variable.Unpack<RE::TESForm*>()) {
                out.formValue = slsf::engine::LoadedRecordKey(form);
                out.hasForm = true;
            }
            break;
        }
        case LegacyValueType::ReferenceAlias:
        case LegacyValueType::Script:
            out.type = ModEventType::Form;
            if (variable.IsObject()) {
                if (const auto object = variable.GetObject()) {
                    out.scriptHandle = object->GetHandle();
                    out.scriptName = object->GetTypeInfo()->GetName();
                }
            }
            break;
        case LegacyValueType::FormArray:
        case LegacyValueType::ActorArray: {
            out.type = ModEventType::Form;
            if (auto array = variable.GetArray()) {
                for (const auto& element : *array) {
                    const auto* form = element.Unpack<RE::TESForm*>();
                    out.formArray.push_back(form ? slsf::engine::LoadedRecordKey(form) : StableFormKey{});
                }
            }
            break;
        }
        case LegacyValueType::StringArray: {
            out.type = ModEventType::String;
            if (auto array = variable.GetArray()) {
                for (const auto& element : *array) out.stringArray.emplace_back(element.GetString());
            }
            break;
        }
        case LegacyValueType::None:
        default:
            out.type = ModEventType::None;
            break;
        }
        return out;
    }

    static void PackResult(RE::BSScript::Variable& out, const ModEventValue& value,
                           LegacyValueType returnType) {
        switch (returnType) {
        case LegacyValueType::StringArray:
            out.Pack(std::vector<std::string>(value.stringArray));
            break;
        case LegacyValueType::FormArray: {
            std::vector<RE::TESForm*> forms;
            for (const auto& key : value.formArray) forms.push_back(ResolveLoadedForm(key));
            out.Pack(std::move(forms));
            break;
        }
        case LegacyValueType::ActorArray: {
            std::vector<RE::Actor*> actors;
            for (const auto& key : value.formArray) {
                const auto form = ResolveLoadedForm(key);
                actors.push_back(form ? form->As<RE::Actor>() : nullptr);
            }
            out.Pack(std::move(actors));
            break;
        }
        case LegacyValueType::Bool:
            out.SetBool(value.boolValue);
            break;
        case LegacyValueType::Int:
            out.SetSInt(static_cast<std::int32_t>(value.intValue));
            break;
        case LegacyValueType::Float:
            out.SetFloat(static_cast<float>(value.floatValue));
            break;
        case LegacyValueType::String:
            out.SetString(value.stringValue);
            break;
        case LegacyValueType::Form:
        case LegacyValueType::Actor:
        case LegacyValueType::Quest:
        case LegacyValueType::ReferenceAlias:
        case LegacyValueType::Script:
            if (value.scriptHandle && !value.scriptName.empty()) {
                RE::BSTSmartPointer<RE::BSScript::Object> object;
                auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
                if (vm && vm->FindBoundObject(value.scriptHandle, value.scriptName.c_str(), object)) out.SetObject(object);
                else out.SetNone();
            } else if (value.hasForm) {
                out.Pack<RE::TESForm*>(ResolveLoadedForm(value.formValue));
            } else {
                out.SetNone();
            }
            break;
        case LegacyValueType::None:
        default:
            out.SetNone();
            break;
        }
    }

    const PapyrusFunction spec_;
    ImplBody body_;
};


int ArgInt(const ModEventValue& value) {
    return static_cast<int>(value.intValue);
}
bool ArgBool(const ModEventValue& value) {
    return value.boolValue;
}
double ArgFloat(const ModEventValue& value) {
    return value.floatValue;
}
const std::string& ArgString(const ModEventValue& value) {
    return value.stringValue;
}

int LegacyCalculateFameSpread(double from, double target) {
    if (from < 10.0) return 0;
    const double spread = from * 0.2;
    const int value = static_cast<int>(target + spread);
    return value > 150 ? 150 : value;
}

ImplBody MakeImpl(const PapyrusFunction& spec) {
    const std::string id = spec.id;
    if (id == "SLSF_Reloaded_DataManager.GetFameValue") {
        return [](std::vector<ModEventValue>& args, ModEventValue& out) {
            out = IntResult(0);
            if (auto* api = CurrentApi(); api && args.size() >= 2) {
                out = IntResult(api->GetFameValue(ArgString(args[0]),
                                                  ArgString(args[1]))
                                    .value_or(0));
            }
        };
    }
    if (id == "SLSF_Reloaded_DataManager.SetFameValue") {
        return [](std::vector<ModEventValue>& args, ModEventValue&) {
            if (auto* api = CurrentApi(); api && args.size() >= 3) {
                (void)api->SetFameValue(ArgString(args[0]), ArgString(args[1]),
                                        ArgInt(args[2]));
            }
        };
    }
    if (id == "SLSF_Reloaded_DataManager.GetExternalFlags") {
        return [](std::vector<ModEventValue>& args, ModEventValue& out) {
            out = BoolResult(false);
            if (auto* api = CurrentApi(); api && args.size() >= 1) {
                out = BoolResult(api->GetAggregatedFlag(ArgString(args[0])));
            }
        };
    }
    if (id == "SLSF_Reloaded_DataManager.RegisterExternalMod") {
        return [](std::vector<ModEventValue>& args, ModEventValue&) {
            if (auto* api = CurrentApi(); api && args.size() >= 1) {
                (void)api->RegisterMod(ArgString(args[0]));
            }
        };
    }
    if (id == "SLSF_Reloaded_DataManager.CountExternalMods") {
        return [](std::vector<ModEventValue>&, ModEventValue& out) {
            int64_t count = 0;
            if (auto* api = CurrentApi()) {
                count = static_cast<int64_t>(api->CountExternalMods());
            }
            out = IntResult(count);
        };
    }
    if (id == "SLSF_Reloaded_DataManager.CleanExternalModList") {
        return [](std::vector<ModEventValue>&, ModEventValue&) {
            auto* api = CurrentApi();
            auto* data = RE::TESDataHandler::GetSingleton();
            if (!api || !data) return;
            for (const auto& name : api->RegisteredMods()) {
                if (!data->LookupModByName(name)) (void)api->UnregisterMod(name);
            }
        };
    }
    if (id == "SLSF_Reloaded_DataManager.UnregisterExternalMod") {
        return [](std::vector<ModEventValue>& args, ModEventValue&) {
            if (auto* api = CurrentApi(); api && args.size() >= 1) {
                (void)api->UnregisterMod(ArgString(args[0]));
            }
        };
    }
    if (id == "SLSF_Reloaded_DataManager.GetModFlagState") {
        return [](std::vector<ModEventValue>& args, ModEventValue& out) {
            out = BoolResult(false);
            if (auto* api = CurrentApi(); api && args.size() >= 2) {
                out = BoolResult(api->GetModFlag(ArgString(args[0]),
                                                 ArgString(args[1])));
            }
        };
    }
    if (id == "SLSF_Reloaded_DataManager.DoesFlagExist") {
        return [](std::vector<ModEventValue>& args, ModEventValue& out) {
            out = StringResult("No");
            if (args.size() >= 1) {
                out = StringResult(slsf::api::PublicApi::IsKnownFlag(ArgString(args[0])) ? "Yes" : "No");
            }
        };
    }
    if (id == "SLSF_Reloaded_DataManager.IsModRegistered") {
        return [](std::vector<ModEventValue>& args, ModEventValue& out) {
            out = BoolResult(false);
            if (auto* api = CurrentApi(); api && args.size() >= 1) {
                out = BoolResult(api->IsModRegistered(ArgString(args[0])));
            }
        };
    }
    if (id == "SLSF_Reloaded_DataManager.SetExternalFlags") {
        return [](std::vector<ModEventValue>& args, ModEventValue&) {
            if (auto* api = CurrentApi(); api && args.size() >= 3) {
                api->SetExternalFlag(ArgString(args[0]), ArgString(args[1]), ArgBool(args[2]));
            }
        };
    }
    if (id == "SLSF_Reloaded_DataManager.CheckSexWorker") {
        return [](std::vector<ModEventValue>&, ModEventValue& out) {
            out = BoolResult(false);
            if (auto* api = CurrentApi()) {
                out = BoolResult(api->GetAggregatedFlag("Sex Worker"));
            }
        };
    }
    static const std::unordered_map<std::string_view, const char*> kWipeCategories{
        {"SLSF_Reloaded_FameManager.WipeCuckFame", "Cuck"},
        {"SLSF_Reloaded_FameManager.WipeUnfaithfulFame", "Unfaithful"},
        {"SLSF_Reloaded_FameManager.WipeAirheadFame", "Airhead"},
        {"SLSF_Reloaded_FameManager.WipeCumDumpFame", "Cum Dump"},
    };
    if (const auto wipe = kWipeCategories.find(std::string_view(id));
        wipe != kWipeCategories.end()) {
        const auto* category = wipe->second;
        return [category](std::vector<ModEventValue>&, ModEventValue&) {
            auto* api = CurrentApi();
            if (!api) return;
            for (const auto& name : api->RegisteredLocations()) (void)api->ClearFame(name, category);
        };
    }
    if (id == "SLSF_Reloaded_FameManager.CalculateFameSpread") {
        return [](std::vector<ModEventValue>& args, ModEventValue& out) {
            out = IntResult(0);
            if (args.size() < 2) return;
            out = IntResult(LegacyCalculateFameSpread(ArgFloat(args[0]),
                                                      ArgFloat(args[1])));
        };
    }
    if (id == "SLSF_Reloaded_FameManager.ClearFame") {
        return [](std::vector<ModEventValue>& args, ModEventValue&) {
            if (auto* api = CurrentApi(); api && args.size() >= 1 && !ArgString(args[0]).empty()) {
                for (const auto& flagName : slsf::api::PublicApi::FlagNames()) {
                    (void)api->ClearFame(ArgString(args[0]), flagName);
                }
            }
        };
    }
    if (id == "SLSF_Reloaded_FameManager.ClearAllFame") {
        return [](std::vector<ModEventValue>&, ModEventValue&) {
            auto* api = CurrentApi();
            if (!api) return;
            for (const auto& name : api->RegisteredLocations()) (void)api->ClearAllFame(name);
        };
    }
    if (id == "SLSF_Reloaded_FameManager.SpreadFame") {
        return [](std::vector<ModEventValue>& args, ModEventValue&) {
            if (auto* api = CurrentApi(); api && args.size() >= 1) {
                (void)api->SpreadPulse({ArgString(args[0])});
            }
        };
    }
    if (id == "SLSF_Reloaded_FameManager.SpreadFameRoll") {
        return [](std::vector<ModEventValue>&, ModEventValue&) {
            auto* api = CurrentApi();
            if (!api) return;
            (void)api->SpreadPulse(api->RegisteredLocations());
        };
    }
    if (id == "SLSF_Reloaded_FameManager.GainFame") {
        return [](std::vector<ModEventValue>& args, ModEventValue&) {
            if (auto* api = CurrentApi(); api && args.size() >= 2) {
                slsf::GainFameInputs inputs;
                inputs.previousFame =
                    static_cast<double>(api->GetFameValue(ArgString(args[1]),
                                                          ArgString(args[0])).value_or(0));
                if (args.size() >= 3) inputs.foreplay = Fact::Known(ArgBool(args[2]));
                (void)api->GainFame(ArgString(args[1]), ArgString(args[0]), inputs);
            }
        };
    }
    if (id == "SLSF_Reloaded_FameManager.UpdateGlobals") {
        return [](std::vector<ModEventValue>&, ModEventValue&) {
            auto* api = CurrentApi();
            if (!api) return;
            EngineGlobalsOutputSink sink;
            (void)api->PublishCurrentFameGlobals(sink);
        };
    }
    if (id == "SLSF_Reloaded_LocationManager.IsLocationValid") {
        return [](std::vector<ModEventValue>& args, ModEventValue& out) {
            out = BoolResult(false);
            if (auto* api = CurrentApi(); api && args.size() >= 1) {
                out = BoolResult(api->ResolveRegionByName(ArgString(args[0])).has_value());
            }
        };
    }
    if (id == "SLSF_Reloaded_LocationManager.CurrentLocationName") {
        return [](std::vector<ModEventValue>&, ModEventValue& out) {
            out = StringResult("$NoneText");
            if (auto* api = CurrentApi()) {
                const auto value = api->CurrentLocation();
                if (!value.empty()) {
                    out = StringResult(value);
                }
            }
        };
    }
    if (id == "SLSF_Reloaded_Logger.Log") {
        return [](std::vector<ModEventValue>& args, ModEventValue&) {
            if (args.size() >= 1 && legacyLogging.load()) {
                const int level = args.size() >= 2 ? ArgInt(args[1]) : 0;
                if (level >= legacyLogLevel.load()) logs::info("legacy log({}): {}", level, ArgString(args[0]));
            }
        };
    }
    if (id == "SLSF_Reloaded_Logger.EnableLogging") {
        return [](std::vector<ModEventValue>& args, ModEventValue&) {
            if (!args.empty()) legacyLogging.store(ArgBool(args[0]));
        };
    }
    if (id == "SLSF_Reloaded_Logger.SetLogLevel") {
        return [](std::vector<ModEventValue>& args, ModEventValue&) {
            if (!args.empty()) legacyLogLevel.store(ArgInt(args[0]));
        };
    }
    if (id == "SLSF_Reloaded_Data_Exporter.RunExport" ||
        id == "SLSF_Reloaded_Data_Exporter.ExportData") {
        return [](std::vector<ModEventValue>& args, ModEventValue&) {
            if (args.size() < 1) return;
            auto* api = CurrentApi();
            if (!api) return;
            const auto document = api->ExportLegacy();
            std::error_code ec;
            const std::filesystem::path dir("Data/SKSE/Plugins/StorageUtilData/SLSF_Reloaded");
            std::filesystem::create_directories(dir, ec);
            if (ec) return;
            const std::filesystem::path outFile =
                dir / std::filesystem::path(ArgString(args[0]) + " SLSF Reloaded Data.json");
            std::ofstream stream(outFile, std::ios::binary | std::ios::trunc);
            if (!stream) return;
            stream << document.Write();
        };
    }
    if (id == "SLSF_Reloaded_Data_Importer.RunImport" ||
        id == "SLSF_Reloaded_Data_Importer.ImportData") {
        return [](std::vector<ModEventValue>& args, ModEventValue&) {
            if (args.size() < 1) return;
            auto* api = CurrentApi();
            if (!api) return;
            const std::filesystem::path inFile =
                std::filesystem::path("Data/SKSE/Plugins/StorageUtilData/SLSF_Reloaded") /
                (ArgString(args[0]) + " SLSF Reloaded Data.json");
            std::ifstream stream(inFile, std::ios::binary);
            if (!stream) return;
            const std::string bytes((std::istreambuf_iterator<char>(stream)),
                                    std::istreambuf_iterator<char>());
            std::string parseError;
            const auto parsed = io::JsonValue::Parse(bytes, 4u << 20, &parseError);
            if (!parsed) return;
            const auto import =
                io::ExtractLegacyImport(parsed, slsf::api::PublicApi::FlagNames());
            if (!import) return;
            const auto result = api->ImportLegacy(*import);
            if (result.status != Status::Ready) logs::error("legacy import rejected: {}", result.message);
        };
    }
    static const std::unordered_set<std::string> gameplayScripts{
        "SLSF_Reloaded_DynamicAnonymity", "SLSF_Reloaded_LocationManager",
        "SLSF_Reloaded_FameManager", "SLSF_Reloaded_ModIntegration",
        "SLSF_Reloaded_PlayerScript", "SLSF_Reloaded_SexAnimationAnalyzer",
        "SLSF_Reloaded_Uninstall", "SLSF_Reloaded_VisibilityManager"};
    if (gameplayScripts.contains(spec.script)) return [id](std::vector<ModEventValue>& args, ModEventValue& out) {
        if (auto* api = CurrentApi(); api && !api->CallGameplay(id, args, out))
            logs::debug("gameplay: {} awaiting facts or rejected", id);
    };
    return nullptr;
}

ImplBody EventSinkImpl(const ModEventContract& contract) {
    return [name = contract.event](std::vector<ModEventValue>& args,
                                   ModEventValue& out) {
        auto* bridge = VmNatives::Bridge();
        out.type = ModEventType::None;
        if (!bridge) {
            ++countersImpl.implFallbacks;
            return;
        }
        ++countersImpl.sinkEvents;
        if (bridge->Dispatch(name, args)) {
            return;
        }
        ++countersImpl.implFallbacks;
    };
}

void DeliverNowInternal(const std::string& eventName,
                        const std::vector<ModEventValue>& values) {
    auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
    if (!vm) {
        ++countersImpl.implFallbacks;
        return;
    }
    RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> create{
        new ReplyCreateCallback(eventName, values, ReplyEpoch())};
    const std::string createName = eventName;
    auto* createArgs = RE::MakeFunctionArguments(std::string(eventName));
    if (!vm->DispatchStaticCall("ModEvent", "Create", createArgs, create)) {
        ++countersImpl.implFallbacks;
        return;
    }
}

}

const VmNatives::Counters& VmNatives::Stats() noexcept {
    return countersImpl;
}

void VmNatives::PublishOutputs() {
    if (auto* api = CurrentApi()) {
        EngineGlobalsOutputSink sink;
        (void)api->PublishAllOutputs(sink);
        sink.Flush();
    }
}

void VmNatives::ResetStats() noexcept {
    countersImpl = Counters{};
}

slsf::api::PublicApi* VmNatives::Api() {
    return CurrentApi();
}

ModEventBridge* VmNatives::Bridge() {
    return bridgeInstance;
}

void VmNatives::BindApiProvider(ApiProvider provider) {
    std::lock_guard guard(stateMutex);
    apiProvider = std::move(provider);
}

void VmNatives::BindBridge(ModEventBridge* bridge) {
    bridgeInstance = bridge;
}

void VmNatives::BindEpochProbe(std::function<Epoch()> current,
                               std::function<bool(Epoch)> valid) {
    std::lock_guard guard(stateMutex);
    currentEpoch = std::move(current);
    validEpoch = std::move(valid);
}

void VmNatives::DeliverReply(const std::string& eventName,
                             const std::vector<ModEventValue>& values) {
    auto* tasks = SKSE::GetTaskInterface();
    if (!tasks) {
        ++countersImpl.implFallbacks;
        return;
    }
    ++countersImpl.repliesScheduled;
    DeliverNowInternal(eventName, values);
}

void VmNatives::DeliverReplyNow(const std::string& eventName,
                                const std::vector<ModEventValue>& values) {
    DeliverNowInternal(eventName, values);
}

class FrozenProbeArgs final {
public:
    FrozenProbeArgs() : args_(RE::MakeFunctionArguments()) {}
    RE::BSScript::IFunctionArguments* Get() const { return args_; }

private:
    RE::BSScript::IFunctionArguments* args_;
};

void VmNatives::KickFrozenProbe() {
    auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
    if (!vm) {
        logs::info("compat: frozen probe skipped (no vm)");
        return;
    }
    RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> none{
        new NoopCallback()};
    auto* args = RE::MakeFunctionArguments();
    if (vm->DispatchStaticCall("SLSFR_FrozenProbe", "RunAll", args, none)) {
        logs::info("compat: frozen probe dispatched");
    } else {
        logs::info("compat: frozen probe absent");
    }
}

bool VmNatives::RegisterPapyrusNatives(RE::BSScript::Internal::VirtualMachine* vm) {
    if (!vm) {
        return false;
    }
    bool helpers = true;
    const auto bindHelper = [&](const char* name, const char* script, auto callback) { return vm->BindNativeMethod(new RE::NativeFunction(name, script, callback)); };
    const auto checked = [&](bool success, const char* name) { if (!success) logs::error("compat: helper binding failed for SLSFR_GameplayNative.{}", name); helpers = success && helpers; };
    checked(bindHelper("OutputCurrent", "SLSFR_GameplayNative", EngineGlobalsOutputSink::OutputCurrent), "OutputCurrent");
    checked(bindHelper("Equipped", "SLSFR_GameplayNative", +[](RE::StaticFunctionTag*, RE::TESForm* form) {
        const auto* file = form ? form->GetFile(0) : nullptr;
        if (!file) return;
        const auto epoch = ReplyEpoch(); const StableFormKey key{std::string(file->GetFilename()), form->GetLocalFormID()};
        if (auto* tasks = SKSE::GetTaskInterface()) tasks->AddTask([epoch, key] {
            if (ReplyCurrent(epoch)) if (auto* api = CurrentApi()) {
                ModEventValue value; value.type = ModEventType::Form; value.hasForm = true; value.formValue = key;
                ModEventValue result; api->CallGameplay("Equipped", {value}, result);
            }
        });
    }), "Equipped");
    checked(bindHelper("Refresh", "SLSFR_GameplayNative", +[](RE::StaticFunctionTag*, std::string source) {
        const auto epoch = ReplyEpoch();
        if (auto* tasks = SKSE::GetTaskInterface()) tasks->AddTask([epoch, source = std::move(source)] {
            if (ReplyCurrent(epoch)) if (auto* api = CurrentApi()) { ModEventValue result; api->CallGameplay(source, {}, result); }
        });
    }), "Refresh");
    checked(bindHelper("SceneEvent", "SLSFR_GameplayNative", +[](RE::StaticFunctionTag*, std::string topic, std::int32_t thread) {
        const auto epoch = ReplyEpoch();
        if (auto* tasks = SKSE::GetTaskInterface()) tasks->AddTask([epoch, topic = std::move(topic), thread] {
            if (ReplyCurrent(epoch)) if (auto* api = CurrentApi()) {
                ModEventValue t; t.type = ModEventType::String; t.stringValue = topic;
                ModEventValue id; id.type = ModEventType::Int; id.intValue = thread;
                ModEventValue result; api->CallGameplay("SceneEvent", {t, id}, result);
            }
        });
    }), "SceneEvent");
    checked(bindHelper("ObserveNpc", "SLSFR_GameplayNative", +[](RE::StaticFunctionTag*, RE::Actor* actor) {
        if (!actor) return;
        const auto epoch = ReplyEpoch(); const auto* file = actor->GetFile(0);
        if (!file) return;
        const StableFormKey key{std::string(file->GetFilename()), actor->GetLocalFormID()};
        if (auto* tasks = SKSE::GetTaskInterface()) tasks->AddTask([epoch, key] {
            if (ReplyCurrent(epoch)) if (auto* api = CurrentApi()) {
                ModEventValue arg; arg.type = ModEventType::Form; arg.hasForm = true; arg.formValue = key;
                ModEventValue result; api->CallGameplay("ObserveNpc", {arg}, result);
            }
        });
    }), "ObserveNpc");
    static const PapyrusBindings bindings;
    std::size_t registered = 0;
    std::size_t expected = 0;
    for (const auto& fn : bindings.Functions()) {
        if (!fn.isPublic) {
            continue;
        }
        if (fn.id == "SLSF_Reloaded_ModIntegration.GetFHUData" ||
            fn.id == "SLSF_Reloaded_ModIntegration.GetFHUInflation" ||
            fn.id == "SLSF_Reloaded_ModIntegration.GetPWData" ||
            fn.id == "SLSF_Reloaded_ModIntegration.GetLegacyConfig") {
            continue;
        }
        const bool isStatic = fn.binding == PapyrusBindingKind::GlobalNative ||
                              fn.binding == PapyrusBindingKind::Global;
        ++expected;
        ImplBody body = MakeImpl(fn);
        if (!body) {
            ++countersImpl.implFallbacks;
            logs::error("compat: implementation missing for {}", fn.id);
            continue;
        }
        try {
        if (!vm->BindNativeMethod(new BridgeFunction(*vm, fn, isStatic, std::move(body)))) {
            ++countersImpl.implFallbacks;
            logs::error("compat: binding failed for {}", fn.id);
            continue;
        }
        ++registered;
        } catch (const std::exception& error) {
            ++countersImpl.implFallbacks;
            logs::error("compat: binding failed for {}: {}", fn.id, error.what());
        }
    }
    for (const auto& contract : ModEventBridge::InputContracts()) {
        std::vector<PapyrusParameter> params;
        for (const auto& p : contract.parameters) {
            PapyrusParameter entry;
            entry.name = p.name;
            entry.typeName = p.type == ModEventType::Form ? "Form" : "";
            entry.type = p.type == ModEventType::None ? LegacyValueType::None
                        : p.type == ModEventType::Bool ? LegacyValueType::Bool
                        : p.type == ModEventType::Int ? LegacyValueType::Int
                        : p.type == ModEventType::Float ? LegacyValueType::Float
                        : p.type == ModEventType::String ? LegacyValueType::String
                        : LegacyValueType::Form;
            params.push_back(entry);
        }
        PapyrusFunction sink;
        sink.id = "SLSFR_EventSink." + contract.handler;
        sink.script = "SLSFR_EventSink";
        sink.name = contract.handler;
        sink.returnTypeName = "None";
        sink.returnType = LegacyValueType::None;
        sink.parameters = std::move(params);
        sink.isPublic = false;
        if (!vm->BindNativeMethod(new BridgeFunction(*vm, sink, true, EventSinkImpl(contract)))) {
            ++countersImpl.implFallbacks;
            logs::error("compat: event binding failed for {}", sink.id);
            continue;
        }
        ++registered;
    }
    vm->RegisterForLogEvent(&g_papyrusLogSink);
    logs::info("compat: registered {} papyrus bridge functions", registered);
    return helpers && registered == expected + kLegacyInputEventCount;
}

}
