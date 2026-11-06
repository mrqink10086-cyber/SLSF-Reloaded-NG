#include "pch.h"
#include "slsf/boundary/AppearanceProviders.h"
#include "slsf/boundary/ProviderCollectors.h"
#include "slsf/engine/RecordLookup.h"
#include "slsf/integrations/ProviderCollectors.h"
#include "DdngV2.h"
#include <RE/I/IObjectHandlePolicy.h>
#include <RE/N/NativeFunction.h>
#include <atomic>
#include <map>
#include <mutex>

namespace slsf::boundary {
namespace {
using namespace integrations;
std::mutex providerMutex;
std::function<Epoch()> currentEpoch;
std::function<bool(const Epoch&)> epochIsCurrent;
DeviousDevicesAPI::DeviousDevicesAPI* ddng{};
HMODULE ddngModule{};
std::string ddngAvailability{"Unknown"};
std::atomic<std::uint64_t> revision{0};
using Continuation = integrations::CollectorContinuation;
std::map<std::uint64_t, Continuation> pending;
std::vector<Continuation> restored;
std::uint64_t nextCall{};
using integrations::kMaxCollectorCalls;

bool IsCurrent(const Epoch& epoch) {
    std::function<bool(const Epoch&)> probe;
    { std::lock_guard guard(providerMutex); probe = epochIsCurrent; }
    try { return probe && probe(epoch); } catch (...) { return false; }
}

class PinnedForm {
public:
    PinnedForm(RE::BSScript::IVirtualMachine* vm, RE::TESForm* form) : policy_(vm->GetObjectHandlePolicy()) {
        if (!form || !policy_) return;
        type_ = static_cast<RE::VMTypeID>(form->GetFormType());
        handle_ = policy_->GetHandleForObject(type_, form);
        if (handle_ != policy_->EmptyHandle() && policy_->IsHandleObjectAvailable(handle_)) {
            policy_->PersistHandle(handle_);
            pinned_ = true;
        }
    }
    ~PinnedForm() { if (pinned_) policy_->ReleaseHandle(handle_); }
    PinnedForm(const PinnedForm&) = delete;
    PinnedForm& operator=(const PinnedForm&) = delete;
    RE::TESForm* Get() const {
        if (!pinned_ || !policy_->IsHandleObjectAvailable(handle_) || !policy_->HandleIsType(type_, handle_)) return nullptr;
        return static_cast<RE::TESForm*>(policy_->GetObjectForHandle(type_, handle_));
    }
private:
    RE::BSScript::IObjectHandlePolicy* policy_{};
    RE::VMHandle handle_{};
    RE::VMTypeID type_{};
    bool pinned_{};
};

std::string EpochLine(const Epoch& epoch) {
    return "epoch\t" + std::to_string(epoch.session) + "\t" + std::to_string(epoch.index) + "\t" + std::to_string(epoch.scene) + "\n";
}
std::string Identity(RE::StaticFunctionTag*, RE::TESForm* form) {
    const auto key = engine::LoadedRecordKey(form);
    return key.plugin.empty() ? "" : WireEscape(key.plugin) + "\t" + WireHex(key.localFormId);
}
std::string Escape(RE::StaticFunctionTag*, std::string value) { return WireEscape(value); }
std::string Hex(RE::StaticFunctionTag*, std::uint32_t value) { return WireHex(value); }
std::string RequestEpoch(RE::StaticFunctionTag*, std::string payload) {
    CollectorRequest request;
    return ParseCollectorRequest(payload, "SLSFEXQ1", request) ? EpochLine(request.epoch) : "";
}
std::vector<RE::Actor*> RequestActors(RE::StaticFunctionTag*, std::string payload) {
    CollectorRequest request;
    std::vector<RE::Actor*> actors;
    if (!ParseCollectorRequest(payload, "SLSFEXQ1", request) || !IsCurrent(request.epoch)) return actors;
    for (const auto& key : request.actors) {
        auto* actor = engine::LookupLoadedRecord<RE::Actor>(RE::TESDataHandler::GetSingleton(), key);
        if (!actor) return {};
        actors.push_back(actor);
    }
    return actors;
}
std::string ActorRace(RE::StaticFunctionTag*, RE::Actor* actor) {
    if (!actor || !actor->GetRace()) return "unknown";
    const auto* name = actor->GetRace()->GetFormEditorID();
    const std::string_view race = name ? name : "";
    if (race == "OrcRace" || race == "OrcRaceVampire") return "orc";
    if (race == "KhajiitRace" || race == "KhajiitRaceVampire") return "khajiit";
    if (race == "ArgonianRace" || race == "ArgonianRaceVampire") return "argonian";
    return "other";
}
std::string CollectDdng(std::string_view payload) {
    CollectorRequest request;
    if (!ParseCollectorRequest(payload, "SLSFDDQ1", request)) return "";
    const std::string prefix = "SLSFDD1\nversion\t1\n" + EpochLine(request.epoch);
    if (!IsCurrent(request.epoch)) return prefix + "availability\tUnknown\n";
    if (!ddng) return prefix + "availability\t" + ddngAvailability + "\n";
    std::string rows;
    for (const auto& key : request.actors) {
        auto* actor = engine::LookupLoadedRecord<RE::Actor>(RE::TESDataHandler::GetSingleton(), key);
        if (!actor) return prefix + "availability\tUnknown\n";
        const auto worn = ddng->GetWornDevices(actor);
        std::uint32_t devices = 0;
        for (auto* armor : worn) {
            if (!armor) return prefix + "availability\tUnknown\n";
            for (std::size_t i = 0; i < kDdngDeviceKeywords.size(); ++i)
                if (armor->HasKeywordString(kDdngDeviceKeywords[i])) devices |= 1u << i;
        }
        const auto bondage = static_cast<std::uint32_t>(ddng->GetBondageState(actor));
        if (bondage >= 0x200) return prefix + "availability\tIncompatible\n";
        rows += "actor\t" + WireEscape(key.plugin) + "\t" + WireHex(key.localFormId) + "\t" + WireHex(bondage) + "\t" + WireHex(devices) + "\n";
    }
    if (!IsCurrent(request.epoch)) return prefix + "availability\tUnknown\n";
    return prefix + "availability\tAvailable\nprovider\tDDNG\tAPI2\t2\nrevision\t" + std::to_string(++revision) + "\n" + rows;
}
template <class Result, class Work>
bool OnGameThread(RE::BSScript::Internal::VirtualMachine* vm, RE::VMStackID stack, Work work) {
    auto* tasks = SKSE::GetTaskInterface();
    if (!tasks) return RE::BSScript::kFailed;
    std::function<Epoch()> probe;
    { std::lock_guard guard(providerMutex); probe = currentEpoch; }
    if (!probe) return RE::BSScript::kFailed;
    Epoch epoch;
    try { epoch = probe(); } catch (...) { return RE::BSScript::kFailed; }
    if (!IsCurrent(epoch)) return RE::BSScript::kFailed;
    std::uint64_t call;
    {
        std::lock_guard guard(providerMutex);
        if (pending.size() >= kMaxCollectorCalls || nextCall == UINT64_MAX) return RE::BSScript::kFailed;
        call = ++nextCall;
        pending.emplace(call, Continuation{stack, std::is_same_v<Result, std::vector<RE::Actor*>>});
    }
    try {
        tasks->AddTask([vm, stack, epoch, call, work = std::move(work)] {
            {
                std::lock_guard guard(providerMutex);
                if (pending.erase(call) == 0) return;
            }
            Result result{};
            try {
                if (IsCurrent(epoch)) {
                    if constexpr (std::is_invocable_v<Work, const Epoch&>) result = work(epoch);
                    else result = work();
                }
            } catch (...) { result = {}; }
            vm->ReturnLatentResult(stack, result);
        });
    } catch (...) {
        std::lock_guard guard(providerMutex);
        pending.erase(call);
        return RE::BSScript::kFailed;
    }
    return RE::BSScript::kStarted;
}
bool CollectDdngAsync(RE::BSScript::Internal::VirtualMachine* vm,
    RE::VMStackID stack, RE::StaticFunctionTag*, std::string payload) {
    return OnGameThread<std::string>(vm, stack, [payload = std::move(payload)] { return CollectDdng(payload); });
}
bool IdentityAsync(RE::BSScript::Internal::VirtualMachine* vm,
    RE::VMStackID stack, RE::StaticFunctionTag*, RE::TESForm* form) {
    auto pinned = std::make_shared<PinnedForm>(vm, form);
    return OnGameThread<std::string>(vm, stack, [pinned = std::move(pinned)] { return Identity(nullptr, pinned->Get()); });
}
bool ActorRaceAsync(RE::BSScript::Internal::VirtualMachine* vm,
    RE::VMStackID stack, RE::StaticFunctionTag*, RE::Actor* actor) {
    auto pinned = std::make_shared<PinnedForm>(vm, actor);
    return OnGameThread<std::string>(vm, stack, [pinned = std::move(pinned)] {
        auto* form = pinned->Get();
        return ActorRace(nullptr, form ? form->As<RE::Actor>() : nullptr);
    });
}
bool RequestActorsAsync(RE::BSScript::Internal::VirtualMachine* vm,
    RE::VMStackID stack, RE::StaticFunctionTag*, std::string payload) {
    return OnGameThread<std::vector<RE::Actor*>>(vm, stack,
        [payload = std::move(payload)] { return RequestActors(nullptr, payload); });
}
bool CollectRaceMenuAsync(RE::BSScript::Internal::VirtualMachine* vm,
    RE::VMStackID stack, RE::StaticFunctionTag*) {
    return OnGameThread<std::string>(vm, stack, [](const Epoch& epoch) {
        const auto snapshot = QueryRaceMenu(epoch);
        auto wire = std::string{"SLSFRM1\nversion\t1\n"} + EpochLine(epoch);
        if (!IsCurrent(epoch) || snapshot.availability != ExternalAvailability::Available)
            return wire + "availability\tUnknown\n";
        wire += "availability\tAvailable\ninterface\t" + std::to_string(snapshot.overlayInterfaceVersion) + "\n";
        constexpr std::array locations{"Body", "Hand", "Feet", "Face"};
        for (std::size_t index = 0; index < locations.size(); ++index)
            wire += std::string{"capacity\t"} + locations[index] + "\t" + std::to_string(snapshot.overlays.counts[index]) + "\n";
        return wire;
    });
}
bool CollectSlaveTatsAsync(RE::BSScript::Internal::VirtualMachine* vm,
    RE::VMStackID stack, RE::StaticFunctionTag*, RE::Actor* actor) {
    auto pinned = std::make_shared<PinnedForm>(vm, actor);
    return OnGameThread<std::string>(vm, stack, [pinned = std::move(pinned)](const Epoch& epoch) {
        auto wire = std::string{"SLSFST1\nversion\t1\n"} + EpochLine(epoch);
        auto* form = pinned->Get();
        auto* resolved = form ? form->As<RE::Actor>() : nullptr;
        if (!resolved) return wire + "availability\tUnknown\n";
        const auto snapshot = QuerySlaveTats(resolved, epoch);
        if (!IsCurrent(epoch) || snapshot.availability != ExternalAvailability::Available)
            return wire + "availability\tUnknown\n";
        wire += "availability\tAvailable\nactor\t" + Identity(nullptr, resolved) + "\ninterface\t" +
                std::to_string(snapshot.interfaceVersion) + "\n";
        constexpr std::array areas{"Body", "Face", "Hands", "Feet"};
        for (std::size_t index = 0; index < areas.size(); ++index) {
            const auto& area = snapshot.areas[index];
            wire += std::string{"area\t"} + areas[index] + "\t" + std::to_string(area.rejectedEntries) + "\t" +
                    (area.truncated ? "1\n" : "0\n");
            for (const auto& placement : area.placements)
                wire += std::string{"tattoo\t"} + areas[index] + "\t" + std::to_string(placement.slot) + "\t" +
                        WireEscape(placement.name) + "\n";
        }
        return wire;
    });
}
template <class Result, class Function>
class CollectorFunction final : public RE::NativeFunction<Function> {
    using Base = RE::NativeFunction<Function>;
public:
    CollectorFunction(std::string_view name, std::string_view script, Function callback)
        : Base(name, script, callback) {
        this->_retType = RE::BSScript::GetRawType<Result>{}();
        this->_isLatent = true;
    }
};
}
bool SaveProviderCollectorCalls(SKSE::SerializationInterface* serialization) {
    std::vector<Continuation> calls;
    {
        std::lock_guard guard(providerMutex);
        for (const auto& [id, call] : pending) calls.push_back(call);
    }
    const auto words = integrations::EncodeCollectorContinuations(calls);
    return serialization->WriteRecord(kCollectorCallsRecord, 1, words.data(),
                                      static_cast<std::uint32_t>(words.size() * sizeof(std::uint32_t)));
}
bool LoadProviderCollectorCalls(SKSE::SerializationInterface* serialization,
                               std::uint32_t version, std::uint32_t length,
                               std::vector<Continuation>& calls) {
    if (version != 1 || length < 4 || length > (1 + kMaxCollectorCalls * 2) * 4 || length % 4 != 0) return false;
    std::vector<std::uint32_t> words(length / 4);
    if (serialization->ReadRecordData(words.data(), length) != length) return false;
    if (!integrations::DecodeCollectorContinuations(words, calls)) return false;
    return true;
}
void RestoreProviderCollectorCalls(std::vector<Continuation> calls) {
    std::lock_guard guard(providerMutex);
    restored = std::move(calls);
}
void ResetProviderCollectorCalls() {
    std::lock_guard guard(providerMutex);
    pending.clear();
    restored.clear();
}
void ResumeProviderCollectorCalls(bool loaded) {
    std::vector<Continuation> calls;
    {
        std::lock_guard guard(providerMutex);
        calls.swap(restored);
    }
    if (!loaded) return;
    auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
    for (const auto& call : calls) {
        if (call.actors) vm->ReturnLatentResult(call.stack, std::vector<RE::Actor*>{});
        else vm->ReturnLatentResult(call.stack, std::string{});
    }
}
void InitializeProviderCollectors(std::function<Epoch()> epoch, std::function<bool(const Epoch&)> isCurrent) {
    { std::lock_guard guard(providerMutex); currentEpoch = std::move(epoch); epochIsCurrent = std::move(isCurrent); }
    InitializeAppearanceProviders();
    if (ddng) return;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN, L"DeviousDevices.dll", &ddngModule);
    ddngAvailability = ddngModule ? "Incompatible" : "Missing";
    if (!ddngModule) return;
    using GetApi = DeviousDevicesAPI::DeviousDevicesAPI* (*)();
    auto get = reinterpret_cast<GetApi>(GetProcAddress(ddngModule, "GetAPI"));
    if (!get) return;
    auto* api = get();
    if (!api || api->GetVersion() != 2) return;
    ddng = api;
    ddngAvailability = "Available";
}
bool RegisterProviderCollectors(RE::BSScript::IVirtualMachine* vm) {
    if (!vm) return false;
    vm->BindNativeMethod(new CollectorFunction<std::string, decltype(IdentityAsync)>("Identity", "SLSF_Reloaded_ProbeNative", IdentityAsync));
    vm->RegisterFunction("Escape", "SLSF_Reloaded_ProbeNative", Escape);
    vm->RegisterFunction("Hex", "SLSF_Reloaded_ProbeNative", Hex);
    vm->RegisterFunction("RequestEpoch", "SLSF_Reloaded_ProbeNative", RequestEpoch);
    vm->BindNativeMethod(new CollectorFunction<std::vector<RE::Actor*>, decltype(RequestActorsAsync)>("RequestActors", "SLSF_Reloaded_ProbeNative", RequestActorsAsync));
    vm->BindNativeMethod(new CollectorFunction<std::string, decltype(ActorRaceAsync)>("ActorRace", "SLSF_Reloaded_ProbeNative", ActorRaceAsync));
    vm->BindNativeMethod(new CollectorFunction<std::string, decltype(CollectDdngAsync)>("CollectScene", "SLSF_Reloaded_DDProbe", CollectDdngAsync));
    vm->BindNativeMethod(new CollectorFunction<std::string, decltype(CollectRaceMenuAsync)>("CollectRaceMenu", "SLSF_Reloaded_ProbeNative", CollectRaceMenuAsync));
    vm->BindNativeMethod(new CollectorFunction<std::string, decltype(CollectSlaveTatsAsync)>("CollectSlaveTats", "SLSF_Reloaded_ProbeNative", CollectSlaveTatsAsync));
    return true;
}
}
