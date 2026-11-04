#include "slsf/ui/Menu.h"

#include "slsf/core/Types.h"

#include <windows.h>

#include <algorithm>
#include <charconv>
#include <string>
#include <utility>
#include <vector>

namespace slsf::ui {
namespace {

constexpr char kSectionRoot[] = "SLSF Fame Reloaded";
constexpr wchar_t kMenuFrameworkModule[] = L"SKSEMenuFramework";
constexpr std::uint32_t kRequiredFrameworkApiVersion = 1;
constexpr float kMultiplierLimit = 100.0f;

using EventCallback = void(__stdcall*)(std::uint32_t);
using RegisterEventFunction = std::int64_t (*)(EventCallback, float);
using UnregisterEventFunction = void (*)(std::int64_t);
using RenderFunction = void(__stdcall*)();
using AddSectionItemFunction = void (*)(const char*, RenderFunction);
using GetFrameworkVersionFunction = float (*)();
using GetFrameworkApiVersionFunction = std::uint32_t (*)();

enum FrameworkEvent : std::uint32_t {
    kFrameworkOpenMenu = 1u,
    kFrameworkCloseMenu = 2u,
    kFrameworkBeforeRender = 3u,
};

template <typename T>
T FrameworkFunction(const char* name) {
    const HMODULE module = GetModuleHandleW(kMenuFrameworkModule);
    if (module == nullptr) {
        return nullptr;
    }
    return reinterpret_cast<T>(GetProcAddress(module, name));
}

struct ImGuiApi {
    struct Vec2 {
        float x;
        float y;
    };

    using TextFn = void (*)(const char*, ...);
    using TextUnformattedFn = void (*)(const char*, const char*);
    using SameLineFn = void (*)(float, float);
    using SeparatorFn = void (*)();
    using ButtonFn = bool (*)(const char*, Vec2);
    using SmallButtonFn = bool (*)(const char*);
    using CheckboxFn = bool (*)(const char*, bool*);
    using SliderFloatFn = bool (*)(const char*, float*, float, float, const char*, int);
    using SliderScalarFn = bool (*)(const char*, int, void*, const void*, const void*, const char*, int);
    using BeginComboFn = bool (*)(const char*, const char*, int);
    using EndComboFn = void (*)();
    using SelectableFn = bool (*)(const char*, bool, int, Vec2);
    using PushIdFn = void (*)(const char*);
    using PopIdFn = void (*)();

    TextFn text{nullptr};
    TextUnformattedFn textUnformatted{nullptr};
    SameLineFn sameLine{nullptr};
    SeparatorFn separator{nullptr};
    ButtonFn button{nullptr};
    SmallButtonFn smallButton{nullptr};
    CheckboxFn checkbox{nullptr};
    SliderFloatFn sliderFloat{nullptr};
    SliderScalarFn sliderScalar{nullptr};
    BeginComboFn beginCombo{nullptr};
    EndComboFn endCombo{nullptr};
    SelectableFn selectable{nullptr};
    PushIdFn pushId{nullptr};
    PopIdFn popId{nullptr};

    [[nodiscard]] bool Usable() const noexcept {
        return text != nullptr && textUnformatted != nullptr && button != nullptr &&
               smallButton != nullptr && checkbox != nullptr && sliderFloat != nullptr &&
               sliderScalar != nullptr && beginCombo != nullptr && endCombo != nullptr &&
               selectable != nullptr && pushId != nullptr && popId != nullptr &&
               sameLine != nullptr && separator != nullptr;
    }

    [[nodiscard]] static const ImGuiApi& Fetch();
};

const ImGuiApi& ImGuiApi::Fetch() {
    static const ImGuiApi cached = [] {
        ImGuiApi api;
        api.text = FrameworkFunction<ImGuiApi::TextFn>("igText");
        api.textUnformatted = FrameworkFunction<ImGuiApi::TextUnformattedFn>("igTextUnformatted");
        api.sameLine = FrameworkFunction<ImGuiApi::SameLineFn>("igSameLine");
        api.separator = FrameworkFunction<ImGuiApi::SeparatorFn>("igSeparator");
        api.button = FrameworkFunction<ImGuiApi::ButtonFn>("igButton");
        api.smallButton = FrameworkFunction<ImGuiApi::SmallButtonFn>("igSmallButton");
        api.checkbox = FrameworkFunction<ImGuiApi::CheckboxFn>("igCheckbox");
        api.sliderFloat = FrameworkFunction<ImGuiApi::SliderFloatFn>("igSliderFloat");
        api.sliderScalar = FrameworkFunction<ImGuiApi::SliderScalarFn>("igSliderScalar");
        api.beginCombo = FrameworkFunction<ImGuiApi::BeginComboFn>("igBeginCombo");
        api.endCombo = FrameworkFunction<ImGuiApi::EndComboFn>("igEndCombo");
        api.selectable = FrameworkFunction<ImGuiApi::SelectableFn>("igSelectable_Bool");
        api.pushId = FrameworkFunction<ImGuiApi::PushIdFn>("igPushID_Str");
        api.popId = FrameworkFunction<ImGuiApi::PopIdFn>("igPopID");
        return api;
    }();
    return cached;
}

constexpr int kImGuiDataTypeS32 = 4;
constexpr double kConfigMaxValue = 1e6;

void Text(std::string_view value) {
    const char* data = value.data();
    const int size = static_cast<int>(value.size());

    const ImGuiApi& api = ImGuiApi::Fetch();
    if (api.text != nullptr) {
        api.text("%.*s", size, data);
        return;
    }
    if (api.textUnformatted != nullptr) {
        api.textUnformatted(data, data + value.size());
    }
}

void TextUnformatted(std::string_view value) {
    const char* data = value.data();
    const int size = static_cast<int>(value.size());

    const ImGuiApi& api = ImGuiApi::Fetch();
    if (api.textUnformatted != nullptr) {
        api.textUnformatted(data, data + size);
        return;
    }
    if (api.text != nullptr) {
        api.text("%.*s", size, data);
    }
}

bool Button(std::string_view label) {
    const ImGuiApi& api = ImGuiApi::Fetch();
    return api.button != nullptr && api.button(label.data(), ImGuiApi::Vec2{0.0f, 0.0f});
}

bool SmallButton(std::string_view label) {
    const ImGuiApi& api = ImGuiApi::Fetch();
    return api.smallButton != nullptr && api.smallButton(label.data());
}

bool Checkbox(std::string_view label, bool* value) {
    const ImGuiApi& api = ImGuiApi::Fetch();
    return api.checkbox != nullptr && api.checkbox(label.data(), value);
}

bool SliderFloat(std::string_view label, float* value, float minimum, float maximum) {
    const ImGuiApi& api = ImGuiApi::Fetch();
    if (api.sliderFloat == nullptr) {
        return false;
    }
    return api.sliderFloat(label.data(), value, minimum, maximum, "%.2f", 0);
}

bool SliderInt(std::string_view label, int* value, int minimum, int maximum) {
    const ImGuiApi& api = ImGuiApi::Fetch();
    if (api.sliderScalar == nullptr) {
        return false;
    }
    return api.sliderScalar(label.data(), kImGuiDataTypeS32, value, &minimum, &maximum, "%d", 0);
}

bool BeginCombo(std::string_view label, std::string_view preview) {
    const ImGuiApi& api = ImGuiApi::Fetch();
    if (api.beginCombo == nullptr) {
        return false;
    }
    return api.beginCombo(label.data(), preview.data(), 0);
}

void EndCombo() {
    const ImGuiApi& api = ImGuiApi::Fetch();
    if (api.endCombo != nullptr) {
        api.endCombo();
    }
}

bool Selectable(std::string_view label, bool selected) {
    const ImGuiApi& api = ImGuiApi::Fetch();
    if (api.selectable == nullptr) {
        return false;
    }
    return api.selectable(label.data(), selected, 0, ImGuiApi::Vec2{0.0f, 0.0f});
}

void Separator() {
    const ImGuiApi& api = ImGuiApi::Fetch();
    if (api.separator != nullptr) {
        api.separator();
    }
}

void SameLine() {
    const ImGuiApi& api = ImGuiApi::Fetch();
    if (api.sameLine != nullptr) {
        api.sameLine(0.0f, 12.0f);
    }
}

void PushId(int index) {
    const ImGuiApi& api = ImGuiApi::Fetch();
    if (api.pushId != nullptr) {
        const std::string id = std::to_string(index);
        api.pushId(id.c_str());
    }
}

void PopId() {
    const ImGuiApi& api = ImGuiApi::Fetch();
    if (api.popId != nullptr) {
        api.popId();
    }
}

std::string FormatFormId(std::uint32_t value) {
    char buffer[16];
    const std::size_t digits = std::to_chars(buffer, buffer + sizeof(buffer), value, 16)
                                   .ptr -
                               buffer;
    return std::string("0x") + std::string(buffer, digits);
}

std::string RegionKeyText(const StableRegionKey& key) {
    return key.plugin + ":" + FormatFormId(key.localFormId);
}

[[nodiscard]] bool IsKnownRegion(const RegionSnapshot* regions,
                                 const StableRegionKey& key) {
    if (regions == nullptr) {
        return false;
    }
    return std::find(regions->regions.begin(), regions->regions.end(), key) !=
               regions->regions.end() ||
           std::find(regions->customOrder.begin(), regions->customOrder.end(), key) !=
               regions->customOrder.end();
}

const char* StatusText(ConfigurationUpdateStatus status) {
    switch (status) {
    case ConfigurationUpdateStatus::Applied:
        return "applied";
    case ConfigurationUpdateStatus::Unchanged:
        return "unchanged";
    case ConfigurationUpdateStatus::Invalid:
        return "invalid";
    case ConfigurationUpdateStatus::IoError:
        return "io error";
    }
    return "unknown";
}

std::string UpdateStatusLine(const ConfigurationSnapshot& snapshot) {
    std::string line = "revision ";
    line += std::to_string(snapshot.revision.value);
    line += ", status ";
    line += StatusText(snapshot.lastUpdate.status);
    if (!snapshot.lastUpdate.message.empty()) {
        line += ": ";
        line += snapshot.lastUpdate.message;
    }
    return line;
}

bool UiAvailable() {
    return ImGuiApi::Fetch().Usable();
}

}

void MenuRequestQueue::Drain(const std::function<void(Kind)>& handler) {
    ++drainCount_;
    for (std::size_t index = 0; index < 2; ++index) {
        if (pending_[index]) {
            pending_[index] = false;
            ++handledCount_;
            if (handler != nullptr) {
                handler(static_cast<Kind>(index));
            }
        }
    }
}

Menu& Menu::Instance() noexcept {
    static Menu instance;
    return instance;
}

void Menu::Unregister() noexcept {
    if (eventToken_ != 0) {
        const auto unregister = FrameworkFunction<UnregisterEventFunction>("UnregisterEvent");
        if (unregister != nullptr) {
            unregister(eventToken_);
        }
        eventToken_ = 0;
    }
    installed_ = false;
}

void Menu::Bind(Ports ports) {
    ports_ = std::move(ports);
}

void Menu::ResetForTest() noexcept {
    Menu& self = Instance();
    self.Unregister();
    self.ports_ = Ports{};
    self.queue_ = MenuRequestQueue{};
    self.cached_ = ConfigurationSnapshot{};
    self.draft_.reset();
    self.draftRevision_ = 0;
    self.lastAction_.clear();
    self.lastCommandResult_.clear();
    self.legacyFiles_.clear();
    self.commandLocationIndex_ = -1;
    self.commandTargetIndex_ = -1;
    self.commandCategoryIndex_ = 0;
    self.commandValue_ = 30;
    self.commandMinimum_ = 0;
    self.commandMaximum_ = 50;
    self.commandPercent_ = 30;
    self.sourceIndex_ = -1;
    self.targetIndex_ = -1;
}

bool Menu::Install(Ports ports) {
    if (installed_) {
        Bind(std::move(ports));
        return true;
    }
    if (GetModuleHandleW(kMenuFrameworkModule) == nullptr) {
        return false;
    }
    const auto registerEvent =
        FrameworkFunction<RegisterEventFunction>("RegisterEventPriority");
    const auto addSection = FrameworkFunction<AddSectionItemFunction>("AddSectionItem");
    const auto apiVersion =
        FrameworkFunction<GetFrameworkApiVersionFunction>("GetMenuFrameworkAPIVersion");
    const auto menuVersion =
        FrameworkFunction<GetFrameworkVersionFunction>("GetMenuFrameworkVersion");
    if (registerEvent == nullptr || addSection == nullptr || apiVersion == nullptr) {
        return false;
    }
    if (menuVersion != nullptr) {
        const float version = menuVersion();
        if (version < 3.0f) {
            return false;
        }
    }
    if (apiVersion() != kRequiredFrameworkApiVersion) {
        return false;
    }

    Bind(std::move(ports));
    eventToken_ = registerEvent(&Menu::HandleFrameworkEvent, 0.0f);
    if (eventToken_ == 0) {
        ports_ = Ports{};
        return false;
    }

    addSection((std::string(kSectionRoot) + "/Overview").c_str(),
               []() { Instance().RenderOverview(); });
    addSection((std::string(kSectionRoot) + "/Categories").c_str(),
               []() { Instance().RenderCategories(); });
    addSection((std::string(kSectionRoot) + "/Connections").c_str(),
               []() { Instance().RenderConnections(); });
    addSection((std::string(kSectionRoot) + "/Balance").c_str(),
               []() { Instance().RenderBalance(); });
    addSection((std::string(kSectionRoot) + "/Commands").c_str(),
               []() { Instance().RenderCommands(); });
    addSection((std::string(kSectionRoot) + "/Legacy").c_str(),
               []() { Instance().RenderLegacy(); });
    addSection((std::string(kSectionRoot) + "/Diagnostics").c_str(),
               []() { Instance().RenderDiagnostics(); });

    installed_ = true;
    return true;
}

void __stdcall Menu::HandleFrameworkEvent(std::uint32_t type) {
    Menu& self = Instance();
    switch (type) {
    case kFrameworkOpenMenu:
        self.queue_.Post(MenuRequestQueue::Kind::Configure);
        break;
    case kFrameworkCloseMenu:
        self.queue_.Post(MenuRequestQueue::Kind::CloseCommit);
        break;
    case kFrameworkBeforeRender:
        self.Frame();
        break;
    default:
        break;
    }
}

void Menu::Frame() {
    if (ports_.snapshot != nullptr) {
        cached_ = ports_.snapshot();
    }
    queue_.Drain([](MenuRequestQueue::Kind kind) {
        Menu& self = Instance();
        switch (kind) {
        case MenuRequestQueue::Kind::Configure:
            if (self.ports_.reload != nullptr) {
                const ConfigurationUpdateResult result = self.ports_.reload();
                self.lastAction_ = result.message;
            }
            break;
        case MenuRequestQueue::Kind::CloseCommit:
            self.RefreshDraft();
            {
                const CommitOutcome outcome = self.ApplyDraft();
                if (outcome == Menu::CommitOutcome::Nothing &&
                    self.ports_.reload != nullptr) {
                    const ConfigurationUpdateResult result = self.ports_.reload();
                    self.lastAction_ = result.message;
                }
            }
            break;
        }
    });
}

const ConfigurationData* Menu::CurrentData() const {
    return cached_.data != nullptr ? cached_.data.get() : nullptr;
}

void Menu::RefreshDraft() {
    const ConfigurationData* data = CurrentData();
    if (data == nullptr) {
        draft_.reset();
        return;
    }
    if (!draft_.has_value() || draftRevision_ != cached_.revision.value) {
        draft_ = *data;
        draftRevision_ = cached_.revision.value;
        sourceIndex_ = -1;
        targetIndex_ = -1;
    }
}

Menu::CommitOutcome Menu::ApplyDraft() {
    if (draft_ == std::nullopt || ports_.commit == nullptr) {
        lastAction_ = "no pending edits";
        return CommitOutcome::Nothing;
    }
    const ConfigurationData* data = CurrentData();
    if (data != nullptr && *draft_ == *data) {
        lastAction_ = "no pending edits";
        return CommitOutcome::Nothing;
    }
    const ConfigurationUpdateResult result = ports_.commit(*draft_);
    lastAction_ = result.message;
    if (result.Succeeded()) {
        draft_.reset();
        draftRevision_ = 0;
        return CommitOutcome::Committed;
    }
    return CommitOutcome::Failed;
}

bool Menu::EditDraftForTest(const std::function<void(ConfigurationData&)>& mutate) {
    if (draft_ == std::nullopt || mutate == nullptr) {
        return false;
    }
    mutate(*draft_);
    return true;
}

std::string Menu::LastUpdateMessage() const {
    if (!lastAction_.empty()) {
        return lastAction_;
    }
    return cached_.lastUpdate.message;
}

void Menu::RenderOverview() {
    if (!UiAvailable()) {
        TextUnformatted("Framework UI functions unavailable.");
        return;
    }
    RefreshDraft();
    if (draft_ == std::nullopt) {
        TextUnformatted("Configuration snapshot unavailable.");
        return;
    }
    const ConfigurationSnapshot& snapshot = cached_;
    Text(UpdateStatusLine(snapshot));
    Separator();

    std::string counts = "connections: " + std::to_string(draft_->connections.size());
    counts += ", tracked regions: " + std::to_string(draft_->menu.selectedRegions.size());
    if (const RegionSnapshot* regions = ports_.regions != nullptr ? ports_.regions() : nullptr) {
        counts += ", world regions: " + std::to_string(regions->regions.size());
    }
    Text(counts);
    Separator();

    Checkbox("Reduce fame at night", &draft_->features.reduceFameAtNight);
    Checkbox("Fame spread restrictions", &draft_->features.fameSpreadRestrictions);
    Checkbox("Dynamic anonymity", &draft_->features.dynamicAnonymity);
    for (const auto& setting : kGameplayBoolSettings) Checkbox(setting.name, &(draft_->gameplay.*setting.member));
    for (const auto& setting : kGameplayNumberSettings) {
        float value = static_cast<float>(draft_->gameplay.*setting.member);
        if (SliderFloat(setting.name, &value, static_cast<float>(setting.minimum), static_cast<float>(setting.maximum))) draft_->gameplay.*setting.member = value;
    }
    Separator();
    Checkbox("Notify fame increases", &draft_->menu.notifications.fameIncrease);
    Checkbox("Notify fame decay", &draft_->menu.notifications.fameDecay);
    Checkbox("Notify fame spread", &draft_->menu.notifications.fameSpread);
    Separator();
    if (Button("Apply")) {
        (void)ApplyDraft();
    }
}

void Menu::RenderCategories() {
    if (!UiAvailable()) {
        TextUnformatted("Framework UI functions unavailable.");
        return;
    }
    RefreshDraft();
    if (draft_ == std::nullopt) {
        TextUnformatted("Configuration snapshot unavailable.");
        return;
    }
    for (std::size_t index = 0; index < kFameCategoryCount; ++index) {
        PushId(static_cast<int>(index));
        float multiplier = static_cast<float>(draft_->categories.multipliers[index]);
        if (SliderFloat(kFameCategoryNames[index], &multiplier, 0.0f, kMultiplierLimit)) {
            draft_->categories.multipliers[index] =
                std::clamp(static_cast<double>(multiplier), 0.0, kConfigMaxValue);
        }
        PopId();
    }
    Separator();
    int minimumFame = static_cast<int>(draft_->categories.minimumFameToSpread);
    if (SliderInt("Minimum fame to spread", &minimumFame, 0,
                  static_cast<int>(kConfigMaxValue))) {
        draft_->categories.minimumFameToSpread = static_cast<double>(minimumFame);
    }
    int maximumCategories = static_cast<int>(draft_->categories.maximumSpreadCategories);
    if (SliderInt("Maximum spread categories", &maximumCategories, 0,
                  static_cast<int>(kFameCategoryCount))) {
        draft_->categories.maximumSpreadCategories = static_cast<std::uint32_t>(maximumCategories);
    }
    int maximumPercentage = static_cast<int>(draft_->categories.maximumSpreadPercentage);
    if (SliderInt("Maximum spread percentage", &maximumPercentage, 0, 100)) {
        draft_->categories.maximumSpreadPercentage = static_cast<double>(maximumPercentage);
    }
    Separator();
    if (Button("Apply")) {
        (void)ApplyDraft();
    }
}

void Menu::RenderConnections() {
    if (!UiAvailable()) {
        TextUnformatted("Framework UI functions unavailable.");
        return;
    }
    RefreshDraft();
    if (draft_ == std::nullopt) {
        TextUnformatted("Configuration snapshot unavailable.");
        return;
    }
    const RegionSnapshot* regions =
        ports_.regions != nullptr ? ports_.regions() : nullptr;

    std::size_t orphanCount = 0;
    for (std::size_t index = 0; index < draft_->connections.size(); ++index) {
        const RegionConnectionSetting& setting = draft_->connections[index];
        PushId(static_cast<int>(index));
        std::string row = RegionKeyText(setting.from) + " -> " + RegionKeyText(setting.to);
        if (!IsKnownRegion(regions, setting.from) || !IsKnownRegion(regions, setting.to)) {
            row += " (unresolved)";
            ++orphanCount;
        }
        Text(row);
        bool enabled = setting.enabled;
        SameLine();
        if (Checkbox("enabled", &enabled)) {
            draft_->connections[index].enabled = enabled;
        }
        if (SmallButton("remove")) {
            draft_->connections.erase(draft_->connections.begin() +
                                      static_cast<std::ptrdiff_t>(index));
            PopId();
            --index;
            continue;
        }
        PopId();
    }
    if (draft_->connections.empty()) {
        TextUnformatted("No connections configured.");
    }
    if (orphanCount != 0) {
        Text("orphan connections: " + std::to_string(orphanCount));
    }
    Separator();
    if (regions == nullptr || regions->regions.empty()) {
        TextUnformatted("Region world not loaded; cannot add connections.");
        return;
    }
    const std::size_t regionCount = regions->regions.size();
    const auto validIndex = [regionCount](int index) {
        return index >= 0 && static_cast<std::size_t>(index) < regionCount;
    };
    std::string sourcePreview =
        validIndex(sourceIndex_)
            ? RegionKeyText(regions->regions[static_cast<std::size_t>(sourceIndex_)])
            : std::string("(none)");
    if (BeginCombo("Source", sourcePreview)) {
        for (std::size_t index = 0; index < regionCount; ++index) {
            if (Selectable(RegionKeyText(regions->regions[index]),
                           static_cast<std::size_t>(sourceIndex_) == index)) {
                sourceIndex_ = static_cast<int>(index);
            }
        }
        EndCombo();
    }
    std::string targetPreview =
        validIndex(targetIndex_)
            ? RegionKeyText(regions->regions[static_cast<std::size_t>(targetIndex_)])
            : std::string("(none)");
    if (BeginCombo("Target", targetPreview)) {
        for (std::size_t index = 0; index < regionCount; ++index) {
            if (Selectable(RegionKeyText(regions->regions[index]),
                           static_cast<std::size_t>(targetIndex_) == index)) {
                targetIndex_ = static_cast<int>(index);
            }
        }
        EndCombo();
    }
    if (Button("Add connection") && validIndex(sourceIndex_) && validIndex(targetIndex_)) {
        RegionConnectionSetting setting{};
        setting.from = regions->regions[static_cast<std::size_t>(sourceIndex_)];
        setting.to = regions->regions[static_cast<std::size_t>(targetIndex_)];
        setting.enabled = true;
        if (std::find(draft_->connections.begin(), draft_->connections.end(), setting) ==
            draft_->connections.end()) {
            draft_->connections.push_back(std::move(setting));
        }
    }
    Separator();
    if (Button("Apply")) {
        (void)ApplyDraft();
    }
}

void Menu::RenderBalance() {
    if (!UiAvailable()) {
        TextUnformatted("Framework UI functions unavailable.");
        return;
    }
    RefreshDraft();
    if (draft_ == std::nullopt) {
        TextUnformatted("Configuration snapshot unavailable.");
        return;
    }
    TextUnformatted("Relationship rules");
    float friendThreshold =
        static_cast<float>(draft_->relationships.friendHighFameThreshold);
    if (SliderFloat("Friend high fame threshold", &friendThreshold, 0.0f,
                    static_cast<float>(kConfigMaxValue))) {
        draft_->relationships.friendHighFameThreshold = friendThreshold;
    }
    float loverThreshold = static_cast<float>(draft_->relationships.loverHighFameThreshold);
    if (SliderFloat("Lover high fame threshold", &loverThreshold, 0.0f,
                    static_cast<float>(kConfigMaxValue))) {
        draft_->relationships.loverHighFameThreshold = loverThreshold;
    }
    float friendChance = static_cast<float>(draft_->relationships.friendBetrayChance);
    if (SliderFloat("Friend betrayal chance", &friendChance, 0.0f, 100.0f)) {
        draft_->relationships.friendBetrayChance = friendChance;
    }
    float loverChance = static_cast<float>(draft_->relationships.loverBetrayChance);
    if (SliderFloat("Lover betrayal chance", &loverChance, 0.0f, 100.0f)) {
        draft_->relationships.loverBetrayChance = loverChance;
    }
    Separator();
    TextUnformatted("Timing and spread");
    float decayPeriod = static_cast<float>(draft_->timing.decayPeriodDays);
    if (SliderFloat("Decay period (days)", &decayPeriod, 0.0f, 30.0f)) {
        draft_->timing.decayPeriodDays = decayPeriod;
    }
    float spreadPeriod = static_cast<float>(draft_->timing.spreadPeriodDays);
    if (SliderFloat("Spread period (days)", &spreadPeriod, 0.0f, 30.0f)) {
        draft_->timing.spreadPeriodDays = spreadPeriod;
    }
    float nightStart = static_cast<float>(draft_->timing.nightStart);
    if (SliderFloat("Night start hour", &nightStart, 0.0f, 24.0f)) {
        draft_->timing.nightStart = nightStart;
    }
    float nightEnd = static_cast<float>(draft_->timing.nightEnd);
    if (SliderFloat("Night end hour", &nightEnd, 0.0f, 24.0f)) {
        draft_->timing.nightEnd = nightEnd;
    }
    Separator();
    TextUnformatted("World parameters");
    float decayPerDay = static_cast<float>(draft_->parameters.decayPerDay);
    if (SliderFloat("Decay per day", &decayPerDay, 0.0f,
                    static_cast<float>(kConfigMaxValue))) {
        draft_->parameters.decayPerDay = decayPerDay;
    }
    float relaxationPerDay = static_cast<float>(draft_->parameters.relaxationPerDay);
    if (SliderFloat("Relaxation per day", &relaxationPerDay, 0.0f,
                    static_cast<float>(kConfigMaxValue))) {
        draft_->parameters.relaxationPerDay = relaxationPerDay;
    }
    float spreadThreshold = static_cast<float>(draft_->parameters.spreadThreshold);
    if (SliderFloat("Spread threshold", &spreadThreshold, 0.0f,
                    static_cast<float>(kConfigMaxValue))) {
        draft_->parameters.spreadThreshold = spreadThreshold;
    }
    float spreadRetention = static_cast<float>(draft_->parameters.spreadRetention);
    if (SliderFloat("Spread retention", &spreadRetention, 0.0f, 1.0f)) {
        draft_->parameters.spreadRetention = spreadRetention;
    }
    float maxFame = static_cast<float>(draft_->parameters.maxFame);
    if (SliderFloat("Max fame", &maxFame, 1.0f, static_cast<float>(kConfigMaxValue))) {
        draft_->parameters.maxFame = maxFame;
    }
    Separator();
    if (Button("Apply")) {
        (void)ApplyDraft();
    }
}

void Menu::RenderCommands() {
    if (!UiAvailable()) {
        TextUnformatted("Framework UI functions unavailable.");
        return;
    }
    if (ports_.fameCommand == nullptr) {
        TextUnformatted("No command executor is bound.");
        return;
    }
    const RegionSnapshot* regions =
        ports_.regions != nullptr ? ports_.regions() : nullptr;
    if (regions == nullptr || regions->apiAliases.empty()) {
        TextUnformatted("Region world not loaded; commands need known locations.");
        return;
    }

    std::vector<std::string> locations;
    std::vector<StableRegionKey> targets;
    for (const auto& alias : regions->apiAliases) {
        if (std::find(targets.begin(), targets.end(), alias.target) != targets.end()) {
            continue;
        }
        targets.push_back(alias.target);
        locations.push_back(alias.alias);
    }

    const auto validLocation = [&locations](int index) {
        return index >= 0 && static_cast<std::size_t>(index) < locations.size();
    };
    std::string locationPreview = validLocation(commandLocationIndex_)
                                      ? locations[static_cast<std::size_t>(commandLocationIndex_)]
                                      : std::string("(none)");
    if (BeginCombo("Location", locationPreview)) {
        for (std::size_t index = 0; index < locations.size(); ++index) {
            if (Selectable(locations[index], static_cast<std::size_t>(commandLocationIndex_) == index)) {
                commandLocationIndex_ = static_cast<int>(index);
            }
        }
        EndCombo();
    }

    std::string categoryPreview = commandCategoryIndex_ >= 0 &&
                                          commandCategoryIndex_ < static_cast<int>(kFameCategoryCount)
                                      ? std::string(
                                            kFameCategoryNames[static_cast<std::size_t>(commandCategoryIndex_)])
                                      : std::string("(none)");
    if (BeginCombo("Category", categoryPreview)) {
        for (std::size_t index = 0; index < kFameCategoryCount; ++index) {
            if (Selectable(kFameCategoryNames[index],
                           static_cast<std::size_t>(commandCategoryIndex_) == index)) {
                commandCategoryIndex_ = static_cast<int>(index);
            }
        }
        EndCombo();
    }

    std::string targetPreview = validLocation(commandTargetIndex_)
                                    ? locations[static_cast<std::size_t>(commandTargetIndex_)]
                                    : std::string("(none)");
    if (BeginCombo("Target", targetPreview)) {
        for (std::size_t index = 0; index < locations.size(); ++index) {
            if (Selectable(locations[index],
                           static_cast<std::size_t>(commandTargetIndex_) == index)) {
                commandTargetIndex_ = static_cast<int>(index);
            }
        }
        EndCombo();
    }

    SliderInt("Value", &commandValue_, 0, static_cast<int>(kConfigMaxValue));
    SliderInt("Minimum", &commandMinimum_, 0, static_cast<int>(kConfigMaxValue));
    SliderInt("Maximum", &commandMaximum_, 0, static_cast<int>(kConfigMaxValue));
    SliderInt("Spread percent", &commandPercent_, 0, 100);
    Separator();

    TextUnformatted("Last command result:");
    Text(lastCommandResult_.empty() ? std::string("(none)") : lastCommandResult_);
    Separator();

    const std::string location =
        validLocation(commandLocationIndex_)
            ? locations[static_cast<std::size_t>(commandLocationIndex_)]
            : std::string();
    const std::string category =
        commandCategoryIndex_ >= 0 && commandCategoryIndex_ < static_cast<int>(kFameCategoryCount)
            ? std::string(kFameCategoryNames[static_cast<std::size_t>(commandCategoryIndex_)])
            : std::string();
    const std::string target =
        validLocation(commandTargetIndex_) && commandTargetIndex_ != commandLocationIndex_
            ? locations[static_cast<std::size_t>(commandTargetIndex_)]
            : std::string();
    const auto run = [this](FameCommand command, const std::string& loc,
                            const std::string& target, const std::string& cat, int amount,
                            int extra) {
        lastCommandResult_ = ports_.fameCommand(command, loc, target, cat, amount, extra);
    };

    const bool locationReady = !location.empty();
    if (Button("Set fame")) {
        if (!locationReady || category.empty()) {
            lastCommandResult_ = "select a location and category first";
        } else {
            run(FameCommand::SetFame, location, target, category, commandValue_, commandValue_);
        }
    }
    SameLine();
    if (Button("Clear fame")) {
        if (!locationReady || category.empty()) {
            lastCommandResult_ = "select a location and category first";
        } else {
            run(FameCommand::ClearFame, location, target, category, 0, 0);
        }
    }
    SameLine();
    if (Button("Clear all")) {
        if (!locationReady) {
            lastCommandResult_ = "select a location first";
        } else {
            run(FameCommand::ClearAllFame, location, target, category, 0, 0);
        }
    }
    if (Button("Manual gain") && locationReady) {
        run(FameCommand::ManualGain, location, target, category, commandMinimum_,
            commandMaximum_);
    }
    SameLine();
    if (Button("Manual decay") && locationReady) {
        run(FameCommand::ManualDecay, location, target, category, commandMinimum_,
            commandMaximum_);
    }
    SameLine();
    if (Button("Manual spread") && locationReady && !target.empty()) {
        run(FameCommand::ManualSpread, location, target, category, commandPercent_, 0);
    }
    SameLine();
    if (Button("Spread pulse") && locationReady) {
        run(FameCommand::SpreadPulse, location, target, category, 0, 0);
    }
}

void Menu::RenderLegacy() {
    if (!UiAvailable()) {
        TextUnformatted("Framework UI functions unavailable.");
        return;
    }
    TextUnformatted("Legacy SLSF data files remain supported for transfer only.");
    Separator();
    TextUnformatted(
        "Importer: Data/SKSE/Plugins/StorageUtilData/SLSF_Reloaded/<file> SLSF Reloaded Data.json");
    TextUnformatted(
        "NG config: Data/SKSE/Plugins/SLSFReloadedNG.json (cross-save settings and connections).");
    Separator();
    if (ports_.legacyFiles != nullptr) {
        legacyFiles_ = ports_.legacyFiles();
    }
    if (legacyFiles_.empty()) {
        TextUnformatted("No legacy data files found.");
    }
    for (std::size_t index = 0; index < legacyFiles_.size(); ++index) {
        PushId(static_cast<int>(index));
        const bool runImport =
            ports_.legacyImport != nullptr && SmallButton("import");
        SameLine();
        Text(legacyFiles_[index]);
        if (runImport) {
            Text(ports_.legacyImport(legacyFiles_[index]));
        }
        PopId();
    }
    Separator();
    Text(UpdateStatusLine(cached_));
}

void Menu::RenderDiagnostics() {
    if (ports_.status != nullptr) {
        Text(ports_.status());
    }
    Text(UpdateStatusLine(cached_));
    Separator();
    TextUnformatted("Last menu action:");
    Text(LastUpdateMessage());
    Separator();
    std::string counters = "drain rounds " + std::to_string(queue_.DrainCount());
    counters += ", handled requests " + std::to_string(queue_.HandledCount());
    Text(counters);
}

}
