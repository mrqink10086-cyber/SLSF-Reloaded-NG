#include "pch.h"

#include "slsf/core/Snapshots.h"
#include "slsf/core/location/DefaultRegions.h"
#include "slsf/core/location/RegionRegistry.h"
#include "slsf/core/reputation/DiffusionKernel.h"
#include "slsf/core/reputation/Settlement.h"
#include "slsf/engine/EventBridge.h"
#include "slsf/integrations/CapabilityRegistry.h"
#include "slsf/persistence/Serialization.h"
#include "slsf/persistence/StableKeys.h"
#include "slsf/runtime/Lifecycle.h"
#include "slsf/runtime/ControlExecutor.h"
#include "slsf/boundary/ProviderCollectors.h"
#include "slsf/boundary/AppearanceProviders.h"
#include "slsf/boundary/Gameplay.h"

#include <chrono>
#include <thread>
#include "slsf/compat/VmNatives.h"
#include "slsf/compat/ModEventBridge.h"
#include "slsf/config/ConfigurationStore.h"
#include "slsf/ui/Menu.h"
#include "slsf/ui/MenuText.h"
#include "slsf/api/PublicApi.h"
#include "slsf/io/JsonTransfer.h"

#include <Windows.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

using slsf::ConfigurationRevision;
using slsf::ConfigurationSnapshot;
using slsf::ConfigurationUpdateResult;
using slsf::ConfigurationUpdateStatus;

// Minimal ABI surface: DDNG publishes a C++ object through
// DeviousDevices.dll!GetAPI; GetVersion() is the first virtual.
class DDNG_API_Probe {
public:
    virtual std::size_t GetVersion() const = 0;
};

using DDNG_GetAPI = DDNG_API_Probe* (*)();

std::string VersionText(const REL::Version& version) {
    return std::to_string(version[0]) + "." + std::to_string(version[1]) + "." +
           std::to_string(version[2]) + "." + std::to_string(version[3]);
}

REL::Version DecodePackedVersion(std::uint32_t packed) {
    // SKSE's packed form is major.minor.revision.build in 8/8/12/4 bits.
    return REL::Version{
        static_cast<std::uint16_t>((packed >> 24u) & 0xFFu),
        static_cast<std::uint16_t>((packed >> 16u) & 0xFFu),
        static_cast<std::uint16_t>((packed >> 4u) & 0xFFFu),
        static_cast<std::uint16_t>(packed & 0xFu),
    };
}

bool LogHostVersions(const SKSE::LoadInterface* a_skse) {
    if (a_skse == nullptr) {
        logs::error("host: SKSE load interface is null");
        return false;
    }
    const REL::Version runtime = a_skse->RuntimeVersion();
    const REL::Version skse = DecodePackedVersion(a_skse->SKSEVersion());
    logs::info("host: runtime={} skse={} commonlib={}", VersionText(runtime),
               VersionText(skse),
#ifdef COMMONLIB_VERSION
               COMMONLIB_VERSION
#else
               "unknown"
#endif
    );
    return true;
}

std::unique_ptr<slsf::config::ConfigurationStore> g_configuration;
std::unique_ptr<slsf::Lifecycle> g_lifecycle;
std::atomic<slsf::Lifecycle*> g_lifecycleView{nullptr};
auto g_hostGate = std::make_shared<slsf::runtime::SessionGate>();
slsf::runtime::ControlExecutor g_controls;
std::mutex g_controlIngress;
std::unique_ptr<slsf::engine::EventBridge> g_events;
std::unique_ptr<slsf::boundary::Gameplay> g_gameplay;
std::atomic<bool> g_pumpQueued{false};
std::jthread g_pump;
std::unique_ptr<slsf::integrations::CapabilityRegistry> g_capabilities;
slsf::RegionSnapshot g_regions;
std::map<slsf::StableFormKey, slsf::RegionId> g_locationOwners;
bool g_worldBuilt{false};
bool g_dataLoaded{false};

slsf::Lifecycle* Runtime() { return g_lifecycleView.load(); }

slsf::SessionContext NewContext() {
    slsf::SessionContext context;
    context.scene.status = slsf::Status::NotReady;
    return context;
}

slsf::integrations::CapabilityProbeResult ProbeDDNG(
    const slsf::integrations::CapabilityRequirement&) {
    slsf::integrations::CapabilityProbeResult probe;
    const HMODULE module = ::GetModuleHandleW(L"DeviousDevices.dll");
    if (module == nullptr) {
        // Before kDataLoaded the plugin may still be initializing; after it,
        // absence is final for this process.
        probe.pending = !g_dataLoaded;
        return probe;
    }
    probe.loaded = true;
    const auto getApi = reinterpret_cast<DDNG_GetAPI>(
        reinterpret_cast<void*>(::GetProcAddress(module, "GetAPI")));
    if (getApi == nullptr) {
        probe.version = 0;
        return probe;
    }
    const DDNG_API_Probe* api = getApi();
    if (api == nullptr) {
        // DDNG publishes its API object from kDataLoaded, after its services
        // are initialized.  Before that point a loaded DLL with a null API is
        // still pending, not permanently incompatible.
        probe.loaded = g_dataLoaded;
        probe.pending = !g_dataLoaded;
        probe.apiVersion = 0;
        probe.version = 0;
        return probe;
    }
    probe.version = static_cast<std::uint32_t>(api->GetVersion());
    // DDNG publishes its interface generation through GetVersion(); the
    // locked DD_APIVERSION is 2 and there is no separate API selector.
    probe.apiVersion = probe.version;
    probe.structSize = sizeof(void*);
    return probe;
}

void LogCapabilities(const char* when) {
    if (!g_capabilities) {
        return;
    }
    for (const auto& record : g_capabilities->Snapshot()) {
        logs::info("[capability:{}] {} state={} version={} api={} reason={} business={}",
                   when, record.requirement.id,
                   slsf::integrations::CapabilityStateName(record.state), record.version,
                   record.apiVersion, record.reason, record.requirement.business);
    }
}

bool BuildStaticWorld() {
    auto* dataHandler = RE::TESDataHandler::GetSingleton();
    if (dataHandler == nullptr) {
        logs::error("lifecycle: TESDataHandler unavailable at DataLoaded");
        return false;
    }

    std::vector<slsf::LocationRecord> records;
    std::map<const RE::BGSLocation*, slsf::StableRegionKey> keysByForm;
    std::map<const RE::BGSLocation*, std::size_t> recordIndices;
    const auto& locations = dataHandler->GetFormArray<RE::BGSLocation>();
    records.reserve(locations.size());
    for (RE::BGSLocation* location : locations) {
        if (location == nullptr) {
            continue;
        }
        const RE::TESFile* file = location->GetFile(0);
        if (file == nullptr) {
            continue;
        }
        const std::string_view filename = file->GetFilename();
        if (filename.empty()) {
            continue;
        }
        slsf::StableRegionKey key{
            .plugin = std::string(filename),
            .localFormId = location->GetLocalFormID(),
        };
        keysByForm.emplace(location, key);
        slsf::LocationRecord record;
        record.formId = slsf::StableFormKey{.plugin = key.plugin, .localFormId = key.localFormId};
        record.stableKey = key;
        record.displayName =
            location->GetFullName() != nullptr ? std::string(location->GetFullName()) : std::string{};
        recordIndices.emplace(location, records.size());
        records.push_back(std::move(record));
    }

    // BuildLocationIndex only derives the parent forest from parentFormId.
    // The engine exposes parentLoc separately, so project it onto the stable
    // form key before binding; otherwise every location becomes a root and
    // child locations cannot inherit their owner region.
    for (const auto& [location, key] : keysByForm) {
        (void)key;
        if (location == nullptr || location->parentLoc == nullptr) {
            continue;
        }
        const auto self = recordIndices.find(location);
        const auto parent = keysByForm.find(location->parentLoc);
        if (self != recordIndices.end() && parent != keysByForm.end()) {
            records[self->second].parentFormId =
                slsf::StableFormKey{parent->second.plugin, parent->second.localFormId};
        }
    }

    std::map<slsf::StableRegionKey, std::vector<slsf::StableRegionKey>> childrenByOwner;
    for (const RE::BGSLocation* location : locations) {
        if (location == nullptr || location->parentLoc == nullptr) {
            continue;
        }
        const auto self = keysByForm.find(location);
        const auto parent = keysByForm.find(location->parentLoc);
        if (self == keysByForm.end() || parent == keysByForm.end() ||
            self->second == parent->second) {
            continue;
        }
        childrenByOwner[parent->second].push_back(self->second);
    }
    for (auto& [owner, children] : childrenByOwner) {
        (void)owner;
        std::sort(children.begin(), children.end());
        children.erase(std::unique(children.begin(), children.end()), children.end());
    }

    slsf::RegionDefinitions definitions;
    definitions.keys.reserve(keysByForm.size());
    for (const auto& [form, key] : keysByForm) {
        (void)form;
        definitions.keys.push_back(key);
    }
    std::sort(definitions.keys.begin(), definitions.keys.end());
    for (const auto& entry : slsf::kDefaultRegions) {
        const slsf::StableRegionKey key{std::string(entry.plugin), entry.form};
        if (std::binary_search(definitions.keys.begin(), definitions.keys.end(), key)) {
            definitions.customOrder.push_back(key);
            definitions.apiAliases.push_back({std::string(entry.name), key});
        }
    }
    for (const auto& [alias, name] : std::array<std::pair<const char*, const char*>, 6>{{
        {"Eastmarch", "Windhelm"}, {"Haafingar", "Solitude"}, {"Hjaalmarch", "Morthal"},
        {"the Reach", "Markarth"}, {"the Pale", "Dawnstar"}, {"the Rift", "Riften"}}}) {
        const auto found = std::find_if(definitions.apiAliases.begin(), definitions.apiAliases.end(),
            [name](const auto& entry) { return entry.alias == name; });
        if (found != definitions.apiAliases.end()) definitions.apiAliases.push_back({alias, found->target});
    }

    auto index = slsf::BuildLocationIndex(records);
    if (!index) {
        logs::error("lifecycle: location index failed: {}", index.error().message);
        return false;
    }
    auto regions = slsf::BindRegions(*index, definitions);
    if (!regions) {
        logs::error("lifecycle: region binding failed: {}", regions.error().message);
        return false;
    }
    g_regions = std::move(*regions);
    for (std::size_t i = 0; i < index->records.size(); ++i)
        if (const auto owner = slsf::ResolveRegion(g_regions, slsf::LocationId(static_cast<std::uint32_t>(i))))
            g_locationOwners.emplace(index->records[i].formId, *owner);
    logs::info("lifecycle: static table built locations={} regions={} ownershipRules={}",
               g_regions.locationOwners.size(), g_regions.regions.size(),
               g_regions.ownership.size());
    return true;
}

void EnsureLifecycle() {
    if (!g_worldBuilt || !g_configuration) {
        return;
    }
    if (!g_lifecycle) {
        g_lifecycle = std::make_unique<slsf::Lifecycle>(
            slsf::StaticWorld{.regions = &g_regions}, *g_configuration, g_hostGate);
        g_lifecycleView.store(g_lifecycle.get());
        slsf::compat::VmNatives::BindEpochProbe([] { return Runtime()->CurrentEpoch(); },
            [](slsf::Epoch epoch) { return Runtime()->IsEpochCurrent(epoch); });
        g_events = std::make_unique<slsf::engine::EventBridge>(
            *g_lifecycle, [](const slsf::engine::AsyncMessage& message) {
                logs::info("event: {} payload={} epoch={}.{}.{}", message.topic, message.payload,
                           message.epoch.session, message.epoch.index, message.epoch.scene);
                g_lifecycle->Executor().Post(message.epoch, [topic = message.topic, payload = message.payload] {
                    if (g_gameplay) g_gameplay->OnEvent(topic, payload);
                });
            });
    }
}

bool Activate(const char* reason, bool hostSucceeded = true,
              std::optional<std::uint64_t> generation = {}) {
    if (!g_lifecycle) {
        logs::error("lifecycle: {} requested before composition root exists", reason);
        return false;
    }
    if (!g_lifecycle->ActivatePrepared(hostSucceeded, generation)) {
        const auto status = g_lifecycle->Status();
        logs::error("lifecycle: {} activation failed: {}", reason, status.message);
        return false;
    }
    const auto status = g_lifecycle->Status();
    logs::info("lifecycle: {} active epoch={}.{}.{} configRevision={} restored={} unresolved={} "
               "droppedEdges={} business={}",
               reason, status.epoch.session, status.epoch.index, status.epoch.scene,
               status.configurationRevision.value, status.restoredAccounts,
               status.unresolvedAccounts, status.droppedConnectionEdges, status.businessOpen);
    return true;
}

// SKSE's serialization and messaging callbacks cross a C ABI boundary;
// an exception escaping here would terminate the process through SKSE's
// dispatch.  Every callback body is guarded and failures are routed to the log.
template <typename Fn>
void GuardedHostCallback(const char* where, Fn&& body) {
    try {
        body();
    } catch (const std::exception& error) {
        logs::error("lifecycle: exception in {}: {}", where, error.what());
    } catch (...) {
        logs::error("lifecycle: unknown exception in {}", where);
    }
}

std::uint64_t QueueControl(const char* name, bool closesBusiness,
                           std::function<void(std::uint64_t)> body) {
    logs::info("control: received name={} closesBusiness={} pid={} tid={}", name,
               closesBusiness, GetCurrentProcessId(), GetCurrentThreadId());
    std::uint64_t generation;
    std::uint64_t sequence;
    {
        std::lock_guard guard(g_controlIngress);
        generation = closesBusiness ? g_hostGate->Close() : g_hostGate->Generation();
        sequence = g_controls.Post([name, generation, body = std::move(body)](std::uint64_t order) {
            logs::info("control: started name={} order={} load={} pid={} tid={}", name,
                       order, generation, GetCurrentProcessId(), GetCurrentThreadId());
            GuardedHostCallback(name, [&] { body(generation); });
            logs::info("control: completed name={} order={} load={} tid={}", name,
                       order, generation, GetCurrentThreadId());
        });
    }
    logs::info("control: enqueued name={} order={} load={} pid={} tid={}", name,
               sequence, generation, GetCurrentProcessId(), GetCurrentThreadId());
    SKSE::GetTaskInterface()->AddTask([] {
        GuardedHostCallback("control drain", [] {
            if (!g_controls.Drain()) logs::error("control: owner/reentry rejected tid={}", GetCurrentThreadId());
        });
    });
    return generation;
}

void OnSerializationSaveBody(SKSE::SerializationInterface* serialization) {
    auto* lifecycle = Runtime();
    if (serialization == nullptr || !lifecycle) {
        return;
    }
    if (!slsf::boundary::SaveProviderCollectorCalls(serialization)) {
        logs::error("collectors: failed to save pending VM continuations");
        return;
    }
    auto captured = lifecycle->Capture();
    if (!captured) {
        logs::warn("lifecycle: save skipped: {}", captured.error().message);
        return;
    }
    auto encoded = slsf::persistence::EncodeSaveRecord(*captured);
    if (!encoded) {
        logs::error("lifecycle: save encode failed: {}", encoded.error().message);
        return;
    }
    if (encoded->size() > (std::numeric_limits<std::uint32_t>::max)()) {
        logs::error("lifecycle: save record too large");
        return;
    }
    const bool written = serialization->WriteRecord(
        slsf::persistence::kSaveRecordType, slsf::persistence::kSaveRecordVersion,
        encoded->data(), static_cast<std::uint32_t>(encoded->size()));
    logs::info("lifecycle: native save record bytes={} written={}", encoded->size(), written);
}

void OnSerializationSave(SKSE::SerializationInterface* serialization) {
    logs::info("serialization: save received pid={} tid={}", GetCurrentProcessId(), GetCurrentThreadId());
    GuardedHostCallback("OnSerializationSave",
                        [&] { OnSerializationSaveBody(serialization); });
}

void OnSerializationLoadBody(SKSE::SerializationInterface* serialization) {
    const auto loadGeneration = QueueControl("serialization reset", true, [](auto) {
        slsf::boundary::ResetProviderCollectorCalls();
        if (g_lifecycle) g_lifecycle->ReturnToMenu();
    });
    if (serialization == nullptr) return;
    std::vector<std::byte> bytes;
    std::vector<slsf::integrations::CollectorContinuation> calls;
    bool foundCalls = false;
    bool found = false;
    bool malformed = false;
    std::uint32_t type = 0;
    std::uint32_t version = 0;
    std::uint32_t length = 0;
    while (serialization->GetNextRecordInfo(type, version, length)) {
        if (type == slsf::boundary::kCollectorCallsRecord) {
            if (foundCalls || !slsf::boundary::LoadProviderCollectorCalls(serialization, version, length, calls)) {
                malformed = true;
                break;
            }
            foundCalls = true;
            continue;
        }
        if (type != slsf::persistence::kSaveRecordType) {
            continue;
        }
        if (found) {
            malformed = true;
            break;
        }
        found = true;
        if (length == 0 || length > slsf::persistence::kSaveRecordMaxBytes ||
            version != slsf::persistence::kSaveRecordVersion) {
            malformed = true;
            break;
        }
        bytes.resize(length);
        if (serialization->ReadRecordData(bytes.data(), length) != length) {
            malformed = true;
            break;
        }
    }

    std::string error;
    std::optional<slsf::PersistentState> candidate;
    if (malformed) error = "save record is malformed or unsupported";
    else if (!found) error = "save has no SLSF native record";
    else {
        auto decoded = slsf::persistence::DecodeSaveRecord(bytes);
        if (!decoded) error = "save record decode failed: " + decoded.error().message;
        else candidate = std::move(*decoded);
    }
    QueueControl("serialization stage", false,
        [loadGeneration, candidate = std::move(candidate), calls = std::move(calls),
         error = std::move(error)](auto generation) mutable {
            if (!g_lifecycle || generation != loadGeneration) return;
            if (!candidate) {
                g_lifecycle->RejectPreparation(std::move(error));
                return;
            }
            slsf::persistence::NormalizeLightPluginKeys(*candidate, [](std::string_view plugin) {
                auto* data = RE::TESDataHandler::GetSingleton();
                const auto* file = data ? data->LookupModByName(plugin) : nullptr;
                return file && file->IsLight();
            });
            if (!g_lifecycle->PrepareLoad(*candidate, NewContext())) {
                logs::error("lifecycle: load bind failed: {}", g_lifecycle->Status().message);
                return;
            }
            slsf::boundary::RestoreProviderCollectorCalls(std::move(calls));
            logs::info("lifecycle: load staged epoch={} tid={}",
                       g_lifecycle->Status().epoch.session, GetCurrentThreadId());
        });
}

void OnSerializationLoad(SKSE::SerializationInterface* serialization) {
    logs::info("serialization: load received pid={} tid={}", GetCurrentProcessId(), GetCurrentThreadId());
    GuardedHostCallback("OnSerializationLoad",
                        [&] { OnSerializationLoadBody(serialization); });
}

void OnSerializationRevertBody(SKSE::SerializationInterface*) {
    QueueControl("serialization revert", true, [](auto) {
        slsf::boundary::ResetProviderCollectorCalls();
        if (g_lifecycle) g_lifecycle->ReturnToMenu();
    });
}

void OnSerializationRevert(SKSE::SerializationInterface* serialization) {
    logs::info("serialization: revert received pid={} tid={}", GetCurrentProcessId(), GetCurrentThreadId());
    GuardedHostCallback("OnSerializationRevert",
                        [&] { OnSerializationRevertBody(serialization); });
}

slsf::api::PublicApi& MenuPublicApi() {
    static slsf::api::PublicApi api{g_lifecycle->Executor(), g_regions};
    return api;
}

void WireCompatBridge() {
    const auto* lifecycle = g_lifecycle.get();
    auto* bridge = slsf::compat::VmNatives::Bridge();
    if (bridge == nullptr) {
        static std::unique_ptr<slsf::compat::ModEventBridge> compatBridge =
            std::make_unique<slsf::compat::ModEventBridge>();
        bridge = compatBridge.get();
        slsf::compat::VmNatives::BindBridge(bridge);
        slsf::compat::VmNatives::BindApiProvider([] {
            return g_lifecycle ? &MenuPublicApi() : nullptr;
        });
        logs::info("compat: bridge created");
    }
    auto& api = MenuPublicApi();
    if (!g_gameplay) {
        g_gameplay = std::make_unique<slsf::boundary::Gameplay>(api, g_regions, g_locationOwners);
        api.SetGameplayCall([](const auto& name, const auto& args, auto& out) { return g_gameplay->Invoke(name, args, out); });
        api.SetFormRegionResolver([](const auto& key) { return g_gameplay->ResolveForm(key); });
        api.SetStrictLocationResolver([]() -> std::optional<std::string> {
            auto* player = RE::PlayerCharacter::GetSingleton();
            auto* location = player ? player->GetCurrentLocation() : nullptr;
            return location && location->GetName()[0] ? location->GetName() : "$NoneText";
        });
        g_pump = std::jthread([](std::stop_token stop) {
            while (!stop.stop_requested()) {
                if (!g_pumpQueued.exchange(true)) {
                    SKSE::GetTaskInterface()->AddTask([] {
                        GuardedHostCallback("gameplay pump", [] {
                            if (!g_controls.Drain()) return;
                            if (g_events) g_events->Flush();
                            if (g_gameplay) g_gameplay->Tick();
                        });
                        g_pumpQueued.store(false);
                    });
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        });
    }
    api.AttachToBridge(*bridge);
    bridge->SetReplySink([](const std::string& eventName,
                            const std::vector<slsf::compat::ModEventValue>& values) {
        slsf::compat::VmNatives::DeliverReply(eventName, values);
    });
    (void)lifecycle;
}

std::string OutcomeText(slsf::Status status, const std::string& message) {
    std::string text(slsf::ui::MenuStatusText(status));
    if (!message.empty()) {
        text += ": ";
        text += slsf::ui::MenuMessageText(message);
    }
    return text;
}

std::string OutcomeText(const slsf::CommandResult& result) {
    return OutcomeText(result.status, result.message);
}

void InstallSlsfMenu() {
    slsf::ui::Menu::Ports ports;
    ports.snapshot = [] {
        return g_configuration ? g_configuration->Current() : slsf::ConfigurationSnapshot{};
    };
    ports.reload = [] {
        return g_configuration ? g_configuration->Reload() : slsf::ConfigurationUpdateResult{};
    };
    ports.commit = [](slsf::ConfigurationData edit) {
        return g_configuration ? g_configuration->ApplyEdit(std::move(edit))
                               : slsf::ConfigurationUpdateResult{};
    };
    ports.regions = [] { return &g_regions; };
    ports.status = [] {
        if (const auto* lifecycle = Runtime()) {
            const slsf::LifecycleStatus status = lifecycle->Status();
            std::string label = "运行阶段：";
            label += status.phase == slsf::LifecyclePhase::Active
                         ? "运行中"
                         : (status.phase == slsf::LifecyclePhase::Preparing ? "准备中" : "未载入存档");
            return label;
        }
        return std::string("运行阶段：未载入存档");
    };
    ports.fameCommand = [](slsf::ui::Menu::FameCommand command, const std::string& location,
                           const std::string& target, const std::string& category, int amount,
                           int extra) {
        if (g_lifecycle == nullptr) {
            return std::string("业务执行尚未就绪");
        }
        slsf::api::PublicApi& api = MenuPublicApi();
        switch (command) {
        case slsf::ui::Menu::FameCommand::SetFame:
            return OutcomeText(api.SetFameValue(location, category, amount));
        case slsf::ui::Menu::FameCommand::ClearFame:
            return OutcomeText(api.ClearFame(location, category));
        case slsf::ui::Menu::FameCommand::ClearAllFame:
            return OutcomeText(api.ClearAllFame(location));
        case slsf::ui::Menu::FameCommand::ManualSpread: {
            const auto outcome = api.ManualSpread(location, target, category, amount);
            return OutcomeText(outcome.status, outcome.message);
        }
        case slsf::ui::Menu::FameCommand::ManualDecay: {
            const auto outcome =
                api.ManualDecay(location, category, amount <= extra ? amount : extra,
                                amount <= extra ? extra : amount);
            return OutcomeText(outcome.status, outcome.message);
        }
        case slsf::ui::Menu::FameCommand::ManualGain: {
            const auto outcome =
                api.ManualGain(location, category, amount <= extra ? amount : extra,
                               amount <= extra ? extra : amount);
            return OutcomeText(outcome.status, outcome.message);
        }
        case slsf::ui::Menu::FameCommand::SpreadPulse:
            return OutcomeText(api.SpreadPulse({location}));
        }
        return std::string("未知操作");
    };
    ports.legacyFiles = [] {
        std::vector<std::string> files;
        std::error_code ec;
        std::filesystem::directory_iterator it(
            "Data/SKSE/Plugins/StorageUtilData/SLSF_Reloaded", ec);
        if (ec) {
            return files;
        }
        for (const auto& entry : it) {
            std::error_code fileError;
            if (!entry.is_regular_file(fileError) || fileError) {
                continue;
            }
            if (entry.path().extension() == ".json") {
                files.push_back(entry.path().filename().string());
            }
        }
        std::sort(files.begin(), files.end());
        return files;
    };
    ports.legacyImport = [](const std::string& file) {
        std::string path = "Data/SKSE/Plugins/StorageUtilData/SLSF_Reloaded/" + file;
        if (path.size() < 5 || path.compare(path.size() - 5, 5, ".json") != 0) {
            path += ".json";
        }
        std::ifstream stream(path, std::ios::binary);
        if (!stream) {
            return "旧版数据导入：无法打开文件 " + file;
        }
        const std::string bytes((std::istreambuf_iterator<char>(stream)),
                                std::istreambuf_iterator<char>());
        std::string parseError;
        const slsf::io::JsonPtr parsed =
            slsf::io::JsonValue::Parse(bytes, 4u << 20, &parseError);
        if (parsed == nullptr) {
            return "旧版数据导入：JSON 格式无效，文件 " + file + "：" + slsf::ui::MenuMessageText(parseError);
        }
        const auto import =
            slsf::io::ExtractLegacyImport(parsed, slsf::api::PublicApi::FlagNames());
        if (!import.has_value()) {
            return std::string("旧版数据导入：") + slsf::ui::MenuMessageText(import.error().message);
        }

        if (g_lifecycle == nullptr) {
            return std::string("旧版数据导入：业务执行尚未就绪");
        }
        slsf::api::PublicApi& api = MenuPublicApi();

        const auto result = api.ImportLegacy(*import);
        logs::info("legacy import: file={} status={} message={}", file,
                   static_cast<int>(result.status), result.message);
        return std::string("旧版数据导入「") + file + "」：" + OutcomeText(result);
    };
    const bool installed = slsf::ui::Menu::Instance().Install(std::move(ports));
    logs::info("menu: framework binding installed={}", installed);
}

void OnMessageBody(SKSE::MessagingInterface::Message* message) {
    if (message == nullptr) {
        return;
    }
    switch (message->type) {
    case SKSE::MessagingInterface::kPostLoad:
        slsf::boundary::SubscribeAppearanceProviders();
        if (g_capabilities) {
            g_capabilities->EnsureAll();
            LogCapabilities("postload");
        }
        break;
    case SKSE::MessagingInterface::kPostPostLoad:
        if (g_capabilities) {
            g_capabilities->EnsureAll();
            LogCapabilities("postpostload");
        }
        break;
    case SKSE::MessagingInterface::kPreLoadGame:
        QueueControl("pre-load", true, [](auto) {
            slsf::boundary::ResetProviderCollectorCalls();
            if (g_lifecycle) g_lifecycle->ReturnToMenu();
        });
        break;
    case SKSE::MessagingInterface::kPostLoadGame:
        // SKSE encodes this bool in the pointer value, not in pointed-to data.
        QueueControl("post-load", false, [succeeded = message->data != nullptr](auto generation) {
            const bool activated = Activate("post-load", succeeded, generation);
            slsf::boundary::ResumeProviderCollectorCalls(activated && !g_hostGate->Closed());
        });
        break;
    case SKSE::MessagingInterface::kNewGame:
        QueueControl("new-game", true, [](auto generation) {
            slsf::boundary::ResetProviderCollectorCalls();
            if (!g_lifecycle) return;
            if (!g_lifecycle->PrepareNewGame(NewContext())) {
                logs::error("lifecycle: new game preparation failed: {}",
                            g_lifecycle->Status().message);
                return;
            }
            Activate("new-game", true, generation);
        });
        break;
    case SKSE::MessagingInterface::kDataLoaded:
        QueueControl("data-loaded", false, [](auto) {
            g_dataLoaded = true;
            if (!g_worldBuilt) {
                g_worldBuilt = BuildStaticWorld();
                EnsureLifecycle();
            }
            if (g_lifecycle) {
                WireCompatBridge();
                const auto update = g_lifecycle->ReloadConfiguration();
                logs::info("lifecycle: startup configuration status={} message={}",
                           static_cast<int>(update.status), update.message);
            }
            InstallSlsfMenu();
            if (g_capabilities) {
                g_capabilities->EnsureAll();
                LogCapabilities("dataloaded");
            }
            slsf::boundary::InitializeProviderCollectors([] {
                const auto* lifecycle = Runtime();
                return lifecycle ? lifecycle->CurrentEpoch() : slsf::Epoch{};
            }, [](const slsf::Epoch& epoch) {
                const auto* lifecycle = Runtime();
                return lifecycle && lifecycle->IsEpochCurrent(epoch);
            });
        });
        break;
    case SKSE::MessagingInterface::kSaveGame:
        break;
    default:
        break;
    }
}

void OnMessage(SKSE::MessagingInterface::Message* message) {
    GuardedHostCallback("OnMessage", [&] { OnMessageBody(message); });
}

}

SKSE_PLUGIN_LOAD(const SKSE::LoadInterface* a_skse)
{
    SKSE::Init(a_skse);
    logs::info("SLSF Reloaded NG lifecycle bootstrap");
    if (!LogHostVersions(a_skse)) {
        return false;
    }
    // Baseline-TU CPUID/OSXSAVE/XGETBV gate; the result selects the AVX2 leaf
    // for the first diffusion round and stays stable for this process.
    logs::info("lifecycle: baseline ISA check avx2={}", slsf::KernelSupportsAvx2());

    g_configuration = slsf::config::ConfigurationStore::Open(
        std::make_unique<slsf::config::DiskConfigurationFile>(
            "Data/SKSE/Plugins/SLSFReloadedNG.json"));
    if (g_configuration != nullptr) {
        g_configuration->BindNotify([](const slsf::ConfigurationSnapshot& snapshot) {
            logs::info("lifecycle: configuration published revision={} status={} message={}",
                       snapshot.revision.value, static_cast<int>(snapshot.lastUpdate.status),
                       snapshot.lastUpdate.message);
            const auto* lifecycle = Runtime();
            if (lifecycle && lifecycle->BusinessOpen()) {
                const auto epoch = lifecycle->CurrentEpoch();
                QueueControl("configuration apply", false, [epoch](auto) {
                    if (!g_lifecycle || !g_lifecycle->IsEpochCurrent(epoch)) return;
                    const auto applied = g_lifecycle->ApplyCurrentConfiguration();
                    if (!applied.Succeeded()) logs::error("configuration apply failed: {}", applied.message);
                });
            }
        });
    }
    auto* papyrus = SKSE::GetPapyrusInterface();
    if (!papyrus || !papyrus->Register(slsf::boundary::RegisterProviderCollectors, slsf::compat::VmNatives::RegisterPapyrusNatives)) return false;

    auto capabilities = std::make_unique<slsf::integrations::CapabilityRegistry>(ProbeDDNG);
    const bool ddngRequired = capabilities->Require(slsf::integrations::CapabilityRequirement{
        .id = "ddng",
        .displayName = "Devious Devices NG",
        .business = "equipment visibility and device facts",
        .minimumVersion = 2,
        .maximumVersion = 2,
        .requiredApiVersion = 2,
        .minimumStructSize = 0,
        .requiredFunctionCount = 0,
    });
    if (!ddngRequired) {
        logs::error("capability: failed to register DDNG requirement");
        return false;
    }
    g_capabilities = std::move(capabilities);

    auto* serialization = SKSE::GetSerializationInterface();
    if (serialization == nullptr) {
        logs::error("lifecycle: serialization interface unavailable");
        return false;
    }
    serialization->SetUniqueID('SLSF');
    serialization->SetSaveCallback(OnSerializationSave);
    serialization->SetLoadCallback(OnSerializationLoad);
    serialization->SetRevertCallback(OnSerializationRevert);

    // Register before any kPostLoad broadcast so one-shot provider messages
    // cannot be missed; capability queries happen in OnMessage.
    auto* messaging = SKSE::GetMessagingInterface();
    if (messaging == nullptr) {
        logs::error("lifecycle: messaging interface unavailable");
        return false;
    }
    if (!messaging->RegisterListener(OnMessage)) {
        logs::error("lifecycle: messaging listener registration failed");
        return false;
    }

    logs::info("lifecycle: serialization and messaging listeners registered");
    return true;
}
