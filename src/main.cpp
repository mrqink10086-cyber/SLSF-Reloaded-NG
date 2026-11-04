#include "pch.h"

#include "slsf/core/Snapshots.h"
#include "slsf/core/reputation/DiffusionKernel.h"
#include "slsf/core/reputation/Settlement.h"
#include "slsf/engine/EventBridge.h"
#include "slsf/integrations/CapabilityRegistry.h"
#include "slsf/persistence/Serialization.h"
#include "slsf/persistence/StableKeys.h"
#include "slsf/runtime/Lifecycle.h"
#include "slsf/boundary/ProviderCollectors.h"
#include "slsf/boundary/AppearanceProviders.h"

#include <chrono>
#include <thread>
#include "slsf/compat/VmNatives.h"
#include "slsf/compat/ModEventBridge.h"
#include "slsf/config/ConfigurationStore.h"
#include "slsf/ui/Menu.h"
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
std::unique_ptr<slsf::engine::EventBridge> g_events;
std::unique_ptr<slsf::integrations::CapabilityRegistry> g_capabilities;
slsf::RegionSnapshot g_regions;
bool g_worldBuilt{false};
bool g_dataLoaded{false};

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
    for (const auto& [owner, children] : childrenByOwner) {
        definitions.ownerRules.push_back(slsf::OwnerRule{owner, children});
    }
    std::sort(definitions.ownerRules.begin(), definitions.ownerRules.end(),
              [](const slsf::OwnerRule& left, const slsf::OwnerRule& right) {
                  return left.owner < right.owner;
              });
    definitions.customOrder = definitions.keys;

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
            slsf::StaticWorld{.regions = &g_regions}, *g_configuration);
        g_events = std::make_unique<slsf::engine::EventBridge>(
            *g_lifecycle, [](const slsf::engine::AsyncMessage& message) {
                logs::info("event: {} payload={} epoch={}.{}.{}", message.topic, message.payload,
                           message.epoch.session, message.epoch.index, message.epoch.scene);
                // Dispatcher revalidates the epoch when the round consumes the
                // event, so a menu transition between Flush and Step still wins.
                g_lifecycle->Executor().Post(message.epoch, [] {});
            });
    }
}

bool Activate(const char* reason, bool hostSucceeded = true) {
    if (!g_lifecycle) {
        logs::error("lifecycle: {} requested before composition root exists", reason);
        return false;
    }
    if (!g_lifecycle->ActivatePrepared(hostSucceeded)) {
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

void OnSerializationSaveBody(SKSE::SerializationInterface* serialization) {
    if (serialization == nullptr || !g_lifecycle) {
        return;
    }
    if (!slsf::boundary::SaveProviderCollectorCalls(serialization)) {
        logs::error("collectors: failed to save pending VM continuations");
        return;
    }
    auto captured = g_lifecycle->Capture();
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
    GuardedHostCallback("OnSerializationSave",
                        [&] { OnSerializationSaveBody(serialization); });
}

void OnSerializationLoadBody(SKSE::SerializationInterface* serialization) {
    if (serialization == nullptr || !g_lifecycle) {
        return;
    }
    slsf::boundary::ResetProviderCollectorCalls();
    g_lifecycle->ReturnToMenu();
    std::vector<std::byte> bytes;
    bool found = false;
    bool malformed = false;
    std::uint32_t type = 0;
    std::uint32_t version = 0;
    std::uint32_t length = 0;
    while (serialization->GetNextRecordInfo(type, version, length)) {
        if (type == slsf::boundary::kCollectorCallsRecord) {
            if (!slsf::boundary::LoadProviderCollectorCalls(serialization, version, length)) {
                malformed = true;
                break;
            }
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

    if (malformed) {
        g_lifecycle->RejectPreparation("save record is malformed or unsupported");
        logs::error("lifecycle: load rejected before binding; business stays closed");
        return;
    }
    if (!found) {
        // A save authored without our co-save record cannot be distinguished
        // from a truncated/foreign save.  Do not synthesize state and open
        // business on top of it; report the failure and stay closed.
        g_lifecycle->RejectPreparation("save has no SLSF native record");
        logs::error("lifecycle: save has no SLSF native record; business stays closed");
        return;
    }

    auto decoded = slsf::persistence::DecodeSaveRecord(bytes);
    if (!decoded) {
        g_lifecycle->RejectPreparation("save record decode failed: " + decoded.error().message);
        logs::error("lifecycle: load decode failed: {}; business stays closed",
                    decoded.error().message);
        return;
    }
    const auto inspected = slsf::persistence::InspectSaveRecord(bytes);
    logs::info("lifecycle: save record decoded bytes={} codec={} locations={}",
               bytes.size(), inspected ? inspected->codecVersion : 0,
               decoded->locations.size());
    slsf::persistence::NormalizeLightPluginKeys(*decoded, [](std::string_view plugin) {
        auto* data = RE::TESDataHandler::GetSingleton();
        const auto* file = data ? data->LookupModByName(plugin) : nullptr;
        return file && file->IsLight();
    });
    if (!g_lifecycle->PrepareLoad(*decoded, NewContext())) {
        logs::error("lifecycle: load bind failed: {}; business stays closed",
                    g_lifecycle->Status().message);
        return;
    }
    // Activation is deferred to kPostLoadGame so business handling cannot
    // start before the load has completely bound its location ownership.
    const auto status = g_lifecycle->Status();
    logs::info("lifecycle: load prepared epoch={} restored={} unresolved={} business={}",
               status.epoch.session, status.restoredAccounts, status.unresolvedAccounts,
               status.businessOpen);
}

void OnSerializationLoad(SKSE::SerializationInterface* serialization) {
    GuardedHostCallback("OnSerializationLoad",
                        [&] { OnSerializationLoadBody(serialization); });
}

void OnSerializationRevertBody(SKSE::SerializationInterface*) {
    slsf::boundary::ResetProviderCollectorCalls();
    if (g_lifecycle) {
        g_lifecycle->ReturnToMenu();
        logs::info("lifecycle: revert closed the business switch");
    }
}

void OnSerializationRevert(SKSE::SerializationInterface* serialization) {
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
    api.AttachToBridge(*bridge);
    bridge->SetReplySink([](const std::string& eventName,
                            const std::vector<slsf::compat::ModEventValue>& values) {
        slsf::compat::VmNatives::DeliverReply(eventName, values);
    });
    (void)lifecycle;
}

std::string OutcomeText(slsf::Status status, const std::string& message) {
    std::string text(slsf::ToString(status));
    if (!message.empty()) {
        text += ": ";
        text += message;
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
        if (g_lifecycle != nullptr) {
            const slsf::LifecycleStatus status = g_lifecycle->Status();
            std::string label = "phase=";
            label += status.phase == slsf::LifecyclePhase::Active
                         ? "active"
                         : (status.phase == slsf::LifecyclePhase::Preparing ? "preparing" : "detached");
            return label;
        }
        return std::string("phase=detached");
    };
    ports.fameCommand = [](slsf::ui::Menu::FameCommand command, const std::string& location,
                           const std::string& target, const std::string& category, int amount,
                           int extra) {
        if (g_lifecycle == nullptr) {
            return std::string("executor not ready");
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
        return std::string("unknown command");
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
            return "legacy import: cannot open " + file;
        }
        const std::string bytes((std::istreambuf_iterator<char>(stream)),
                                std::istreambuf_iterator<char>());
        std::string parseError;
        const slsf::io::JsonPtr parsed =
            slsf::io::JsonValue::Parse(bytes, 4u << 20, &parseError);
        if (parsed == nullptr) {
            return "legacy import: invalid JSON in " + file + ": " + parseError;
        }
        const auto import =
            slsf::io::ExtractLegacyImport(parsed, slsf::api::PublicApi::FlagNames());
        if (!import.has_value()) {
            return std::string("legacy import: ") + import.error().message;
        }

        if (g_lifecycle == nullptr) {
            return std::string("legacy import: executor not ready");
        }
        slsf::api::PublicApi& api = MenuPublicApi();

        std::size_t applied = 0;
        std::size_t rejected = 0;
        std::string firstError;
        for (const auto& entry : import->fame) {
            const slsf::CommandResult result = api.SetFameValue(
                entry.location, std::string(slsf::ToString(entry.category)), entry.value);
            if (result.status == slsf::Status::Ready) {
                ++applied;
            } else {
                ++rejected;
                if (firstError.empty()) {
                    firstError = entry.location + " (" + result.message + ")";
                }
            }
        }
        std::size_t modsRegistered = 0;
        std::size_t flagsImported = 0;
        for (const auto& mod : import->modFlags) {
            if (!api.RegisterMod(mod.mod)) {
                continue;
            }
            ++modsRegistered;
            for (const auto& [flag, enabled] : mod.flags) {
                if (enabled) {
                    api.SetExternalFlag(mod.mod, flag, true);
                    ++flagsImported;
                }
            }
        }
        std::string summary = "legacy import '" + file + "': fame " +
                              std::to_string(applied) + " applied, " +
                              std::to_string(rejected) + " rejected";
        summary += ", spread " + std::to_string(import->spread.size()) +
                   " extracted, mods " + std::to_string(modsRegistered) + " registered, " +
                   std::to_string(flagsImported) + " flags";
        if (!firstError.empty()) {
            summary += "; first rejection: " + firstError;
        }
        return summary;
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
        slsf::boundary::ResetProviderCollectorCalls();
        if (g_lifecycle) {
            g_lifecycle->ReturnToMenu();
            logs::info("lifecycle: pre-load closed the business switch");
        }
        break;
    case SKSE::MessagingInterface::kPostLoadGame:
        // SKSE encodes this bool in the pointer value, not in pointed-to data.
        Activate("post-load", message->data != nullptr);
        slsf::boundary::ResumeProviderCollectorCalls(message->data != nullptr);
        break;
    case SKSE::MessagingInterface::kNewGame:
        if (g_lifecycle) {
            if (!g_lifecycle->PrepareNewGame(NewContext())) {
                logs::error("lifecycle: new game preparation failed: {}",
                            g_lifecycle->Status().message);
                break;
            }
            Activate("new-game");
        }
        break;
    case SKSE::MessagingInterface::kDataLoaded:
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
            return g_lifecycle ? g_lifecycle->CurrentEpoch() : slsf::Epoch{};
        }, [](const slsf::Epoch& epoch) {
            return g_lifecycle && g_lifecycle->IsEpochCurrent(epoch);
        });
        if (auto* tasks = SKSE::GetTaskInterface()) {
            tasks->AddTask([] { slsf::compat::VmNatives::KickFrozenProbe(); });
            std::thread delayed{[] {
                std::this_thread::sleep_for(std::chrono::seconds(6));
                if (auto* late = SKSE::GetTaskInterface()) {
                    late->AddTask([] {
                        const slsf::compat::VmNatives::Counters counters{
                            slsf::compat::VmNatives::Stats()};
                        logs::info("compat: counters+6s nativeCalls={} sinkEvents={} repliesScheduled={} repliesSent={} fallbacks={}",
                                   counters.nativeCalls, counters.sinkEvents,
                                   counters.repliesScheduled, counters.repliesSent,
                                   counters.implFallbacks);
                        slsf::compat::VmNatives::KickFrozenProbe();
                    });
                }
            }};
            delayed.detach();
        }
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
