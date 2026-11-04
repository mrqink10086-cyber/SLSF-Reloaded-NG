#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#include <Windows.h>
#ifdef GetObject
#undef GetObject
#endif

namespace logs = SKSE::log;

#include "slsf/compat/VmNatives.h"

#include "slsf/compat/PapyrusBindings.h"
#include "slsf/api/PublicApi.h"
#include "slsf/engine/RecordLookup.h"
#include "slsf/compat/ModEventBridge.h"
#include "slsf/compat/LegacyOutputs.h"
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
VmNatives::Counters countersImpl;
std::atomic<bool> nativeKindTrace{true};

slsf::api::PublicApi* CurrentApi() {
    std::lock_guard guard(stateMutex);
    return apiProvider ? apiProvider() : nullptr;
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
        const RE::BSScript::LogEvent& event,
        RE::BSTEventSource<RE::BSScript::LogEvent>*) override {
        if (event.errorMsg && *event.errorMsg) {
            logs::info("papyrus[{}] {}", static_cast<int>(event.severity), event.errorMsg);
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
    ReplyCreateCallback(std::string eventName, std::vector<ModEventValue> values)
        : eventName_(std::move(eventName)), values_(std::move(values)) {}

    void operator()(RE::BSScript::Variable result) override {
        auto object = result.GetObject();
        if (!object) {
            ++countersImpl.implFallbacks;
            return;
        }
        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> none{
            new NoopCallback()};
        auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        if (!vm) {
            ++countersImpl.implFallbacks;
            return;
        }
        for (const auto& value : values_) {
            RE::BSScript::IFunctionArguments* args = nullptr;
            const char* methodName = nullptr;
            switch (value.type) {
            case ModEventType::String:
                methodName = "PushString";
                args = RE::MakeFunctionArguments(std::string(value.stringValue));
                break;
            case ModEventType::Int: {
                methodName = "PushInt";
                const std::int32_t pushed = static_cast<std::int32_t>(value.intValue);
                args = RE::MakeFunctionArguments(std::int32_t(pushed));
                break;
            }
            case ModEventType::Float: {
                methodName = "PushFloat";
                const float pushed = static_cast<float>(value.floatValue);
                args = RE::MakeFunctionArguments(float(pushed));
                break;
            }
            case ModEventType::Bool: {
                methodName = "PushBool";
                const bool pushed = value.boolValue;
                args = RE::MakeFunctionArguments(bool(pushed));
                break;
            }
            case ModEventType::Form:
                methodName = "PushForm";
                if (auto* form = ResolveLoadedForm(value.formValue)) {
                    args = RE::MakeFunctionArguments(static_cast<RE::TESForm*>(form));
                }
                break;
            default:
                methodName = nullptr;
                break;
            }
            if (methodName == nullptr || args == nullptr) continue;
            vm->DispatchMethodCall2(object->GetHandle(), "ModEvent", methodName,
                                    args, none);
            ++countersImpl.repliesSent;
        }
        vm->DispatchMethodCall2(object->GetHandle(), "ModEvent", "Send", nullptr, none);
    }

    void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}

private:
    std::string eventName_;
    std::vector<ModEventValue> values_;
};

class EngineGlobalsOutputSink final : public LegacyOutputSink {
public:
    void SetCurrentFameGlobal(const StableFormKey& key, int value) override {
        if (auto* toast = ResolveLoadedGlobal(key)) {
            toast->value = static_cast<float>(value);
        }
    }
    void SetStorageIntList(const std::string& key, std::size_t locationIndex,
                           int value) override {
        (void)key;
        (void)locationIndex;
        (void)value;
    }
    void SetLegacyBridgeGlobal(const StableFormKey& key, int value) override {
        if (auto* toast = ResolveLoadedGlobal(key)) {
            toast->value = static_cast<float>(value);
        }
    }
};

using ImplBody = std::function<void(std::vector<ModEventValue>&, ModEventValue&)>;

class BridgeFunction final : public RE::BSScript::NF_util::NativeFunctionBase {
public:
    BridgeFunction(const PapyrusFunction& spec, bool isStatic, ImplBody body)
        : RE::BSScript::NF_util::NativeFunctionBase(
              spec.name, spec.script, isStatic,
              static_cast<std::uint16_t>(spec.parameters.size())),
          spec_(spec),
          body_(std::move(body)) {
        _retType.SetType(RawTypeFor(spec.returnType));
        for (std::size_t i = 0; i < spec.parameters.size(); ++i) {
            _descTable.entries[i].first = spec.parameters[i].name;
            _descTable.entries[i].second.SetType(RawTypeFor(spec.parameters[i].type));
        }
    }

    bool HasStub() const override { return true; }

    bool MarshallAndDispatch(RE::BSScript::Variable&,
                             RE::BSScript::Internal::VirtualMachine&,
                             RE::VMStackID,
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
        ModEventValue result = NoneResult();
        if (body_) {
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
        PackResult(a_resultValue, result, spec_.returnType);
        return true;
    }

private:
    static RE::BSScript::TypeInfo::RawType RawTypeFor(LegacyValueType type) {
        using RawType = RE::BSScript::TypeInfo::RawType;
        switch (type) {
        case LegacyValueType::None: return RawType::kNone;
        case LegacyValueType::Bool: return RawType::kBool;
        case LegacyValueType::Int: return RawType::kInt;
        case LegacyValueType::Float: return RawType::kFloat;
        case LegacyValueType::String: return RawType::kString;
        case LegacyValueType::Form:
        case LegacyValueType::Actor:
        case LegacyValueType::Quest:
        case LegacyValueType::ReferenceAlias:
        case LegacyValueType::Script: return RawType::kObject;
        case LegacyValueType::StringArray: return RawType::kStringArray;
        case LegacyValueType::FormArray: return RawType::kObjectArray;
        case LegacyValueType::ActorArray: return RawType::kObjectArray;
        default: return RawType::kNone;
        }
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
        case LegacyValueType::ReferenceAlias:
        case LegacyValueType::Script: {
            out.type = ModEventType::Form;
            if (auto* form = variable.Unpack<RE::TESForm*>()) {
                out.formValue = slsf::engine::LoadedRecordKey(form);
                out.hasForm = true;
            }
            break;
        }
        case LegacyValueType::ActorArray: {
            out.type = ModEventType::Int;
            if (auto array = variable.GetArray()) {
                out.intValue = static_cast<std::int64_t>(array->size());
            } else {
                out.intValue = 0;
            }
            break;
        }
        case LegacyValueType::StringArray: {
            out.type = ModEventType::String;
            if (auto array = variable.GetArray()) {
                if (array->size() > 0) {
                    out.stringValue = std::string((*array)[0].GetString());
                }
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
            if (value.hasForm) {
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

// ---------------------------------------------------------------------------
// Impl dispatch for the 86 frozen public functions.
// ---------------------------------------------------------------------------

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
        return [](std::vector<ModEventValue>&, ModEventValue&) {};
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
            for (std::size_t i = 0; i < LegacyOutputs::kLegacyLocationIndexCount; ++i) {
                const auto name = LegacyOutputs::LegacyLocationName(i);
                if (name) {
                    (void)api->ClearFame(*name, category);
                }
            }
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
            for (std::size_t i = 0; i < LegacyOutputs::kLegacyLocationIndexCount; ++i) {
                const auto name = LegacyOutputs::LegacyLocationName(i);
                if (name) {
                    (void)api->ClearAllFame(*name);
                }
            }
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
            std::vector<std::string> sources;
            for (std::size_t i = 0; i < LegacyOutputs::kLegacyLocationIndexCount; ++i) {
                const auto name = LegacyOutputs::LegacyLocationName(i);
                if (name) {
                    sources.push_back(*name);
                }
            }
            (void)api->SpreadPulse(sources);
        };
    }
    if (id == "SLSF_Reloaded_FameManager.GainFame") {
        return [](std::vector<ModEventValue>& args, ModEventValue&) {
            if (auto* api = CurrentApi(); api && args.size() >= 2) {
                slsf::GainFameInputs inputs;
                inputs.previousFame =
                    static_cast<double>(api->GetFameValue(ArgString(args[1]),
                                                          ArgString(args[0])).value_or(0));
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
            if (args.size() >= 1) {
                const int level = args.size() >= 2 ? ArgInt(args[1]) : 0;
                logs::info("legacy log({}): {}", level, ArgString(args[0]));
            }
        };
    }
    if (id == "SLSF_Reloaded_Logger.EnableLogging") {
        return [](std::vector<ModEventValue>&, ModEventValue&) {};
    }
    if (id == "SLSF_Reloaded_Logger.SetLogLevel") {
        return [](std::vector<ModEventValue>&, ModEventValue&) {};
    }
    if (id == "SLSF_Reloaded_Data_Exporter.RunExport" ||
        id == "SLSF_Reloaded_Data_Exporter.ExportData") {
        return [](std::vector<ModEventValue>& args, ModEventValue&) {
            if (args.size() < 1) return;
            auto* api = CurrentApi();
            if (!api) return;
            io::LegacyDocument document;
            for (const auto& flagName : slsf::api::PublicApi::FlagNames()) {
                for (std::size_t i = 0; i < LegacyOutputs::kLegacyLocationIndexCount; ++i) {
                    const auto location = LegacyOutputs::LegacyLocationName(i);
                    if (!location) continue;
                    const auto value = api->GetFameValue(*location, flagName);
                    if (value) {
                        document.SetInt(*location + "." + flagName, *value);
                    }
                }
            }
            std::error_code ec;
            const std::filesystem::path dir("Data/SKSE/Plugins/StorageUtilData/SLSF_Reloaded");
            std::filesystem::create_directories(dir, ec);
            if (ec) return;
            const std::filesystem::path outFile =
                dir / std::filesystem::path(ArgString(args[0]) + ".json");
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
                (ArgString(args[0]) + ".json");
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
            for (const auto& entry : import->fame) {
                (void)api->SetFameValue(entry.location,
                                        std::string(slsf::ToString(entry.category)),
                                        entry.value);
            }
            for (const auto& mod : import->modFlags) {
                if (!api->RegisterMod(mod.mod)) continue;
                for (const auto& [flag, enabled] : mod.flags) {
                    if (enabled) {
                        api->SetExternalFlag(mod.mod, flag, true);
                    }
                }
            }
        };
    }
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
        new ReplyCreateCallback(eventName, values)};
    const std::string createName = eventName;
    auto* createArgs = RE::MakeFunctionArguments(std::string(eventName));
    if (!vm->DispatchStaticCall("ModEvent", "Create", createArgs, create)) {
        ++countersImpl.implFallbacks;
        return;
    }
}

} // namespace

const VmNatives::Counters& VmNatives::Stats() noexcept {
    return countersImpl;
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
    static const PapyrusBindings bindings;
    std::size_t registered = 0;
    std::size_t expected = 0;
    for (const auto& fn : bindings.Functions()) {
        if (!fn.isPublic) {
            continue;
        }
        if (fn.id == "SLSF_Reloaded_ModIntegration.GetFHUData" ||
            fn.id == "SLSF_Reloaded_ModIntegration.GetPWData" ||
            fn.id == "SLSF_Reloaded_ModIntegration.GetLegacyConfig") {
            continue;
        }
        const bool isStatic = fn.binding == PapyrusBindingKind::GlobalNative ||
                              fn.binding == PapyrusBindingKind::Global;
        ImplBody body = MakeImpl(fn);
        if (!body) {
            ++countersImpl.implFallbacks;
            body = [](std::vector<ModEventValue>&, ModEventValue& out) {
                out.type = ModEventType::None;
                out.boolValue = false;
                out.intValue = 0;
                out.floatValue = 0.0;
            };
        }
        if (!vm->BindNativeMethod(new BridgeFunction(fn, isStatic, std::move(body)))) {
            ++countersImpl.implFallbacks;
            continue;
        }
        ++registered;
    }
    expected = registered;
    for (const auto& contract : ModEventBridge::InputContracts()) {
        std::vector<PapyrusParameter> params;
        for (const auto& p : contract.parameters) {
            PapyrusParameter entry;
            entry.name = p.name;
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
        if (!vm->BindNativeMethod(new BridgeFunction(sink, true, EventSinkImpl(contract)))) {
            ++countersImpl.implFallbacks;
            continue;
        }
        ++registered;
    }
    vm->RegisterForLogEvent(&g_papyrusLogSink);
    logs::info("compat: registered {} papyrus bridge functions", registered);
    return registered >= expected + kLegacyInputEventCount;
}

} // namespace slsf::compat
