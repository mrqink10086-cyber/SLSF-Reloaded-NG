#include "slsf/ui/Menu.h"
#include "slsf/ui/MenuText.h"

#include "slsf/core/Types.h"

#include <windows.h>

#include <algorithm>
#include <charconv>
#include <string>
#include <utility>
#include <vector>

namespace slsf::ui {
namespace {

constexpr char kSectionRoot[] = "SLSF 声望重制";
constexpr wchar_t kMenuFrameworkModule[] = L"SKSEMenuFramework";
constexpr std::uint32_t kRequiredFrameworkApiVersion = 1;
constexpr float kMultiplierLimit = 100.0f;

constexpr std::array<const char*, kFameCategoryCount> kCategoryLabels{
    "卖淫", "淫荡", "暴露", "口交", "肛交", "放荡", "怀孕", "支配", "顺从",
    "施虐", "受虐", "温柔", "喜欢男性", "喜欢女性", "喜欢兽人", "喜欢虎人",
    "喜欢亚龙人", "兽交", "群交", "束缚", "纹身", "精液容器", "不忠", "戴绿帽", "空脑"
};

const char* GameplayLabel(std::string_view name) {
    static constexpr std::pair<std::string_view, const char*> labels[]{
        {"anonymityEnabled", "允许玩家匿名"},
        {"allowCollarBoundFame", "项圈可增加束缚声望"},
        {"allowSLSCursedCollarBoundFame", "SLS 诅咒项圈可增加束缚声望"},
        {"thanesCantBeAnonymous", "男爵无法匿名"},
        {"allowForeplayFame", "前戏可增加声望"},
        {"npcNeedsLOS", "目击需要视线可达"},
        {"friendsBetrayFromHighFame", "高声望可使朋友泄密"},
        {"loversBetrayFromHighFame", "高声望可使恋人泄密"},
        {"disableNakedCommentsWhilePW", "公共妓女任务期间禁用裸体评论"},
        {"submissiveDefault", "默认视为顺从"},
        {"dominantDefault", "默认视为支配"},
        {"victimsAreMasochist", "受害者可获得受虐声望"},
        {"allowLikeFameWhenRaped", "受侵犯时可增加性偏好声望"},
        {"allowBestialityWhenRaped", "受侵犯时可增加兽交声望"},
        {"dynamicAnonymityFameCutoff", "动态匿名声望上限"},
        {"fameChanceByEnemy", "敌人目击传播概率（%）"},
        {"fameChanceByNeutral", "陌生人目击传播概率（%）"},
        {"fameChanceByFriend", "朋友目击传播概率（%）"},
        {"fameChanceByLover", "恋人目击传播概率（%）"},
        {"minimumNPCLOSDistance", "目击视线距离阈值"},
        {"sexFameChanceByFriend", "与朋友发生关系的泄密概率（%）"},
        {"sexFameChanceByLover", "与恋人发生关系的泄密概率（%）"},
        {"cumDumpFHUReq", "精液容器声望所需 FHU 膨胀值"},
        {"commentFrequency", "声望评论概率（%）"},
        {"scanPeriodDays", "目击扫描间隔（游戏天）"},
    };
    for (const auto& [key, label] : labels) if (key == name) return label;
    return "未知设置";
}

std::string LocationLabel(std::string_view name) {
    static constexpr std::pair<std::string_view, std::string_view> labels[]{
        {"Whiterun", "雪漫城"}, {"Winterhold", "冬堡"}, {"Windhelm", "风盔城"},
        {"Solitude", "独孤城"}, {"Riften", "裂谷城"}, {"Markarth", "马卡斯城"},
        {"Morthal", "莫萨尔"}, {"Dawnstar", "晨星城"}, {"Falkreath", "佛克瑞斯"},
        {"Raven Rock", "乌鸦岩"}, {"Riverwood", "溪木镇"}, {"Rorikstead", "洛里斯泰德"},
        {"Ivarstead", "紫杉镇"}, {"Shor's Stone", "舒尔之石"}, {"Dragon Bridge", "龙桥镇"},
        {"Karthwasten", "卡斯威斯腾"}, {"Largashbur", "拉加什布尔"},
        {"Dushnikh Yal", "杜什尼克·亚尔"}, {"Mor Khazgur", "莫·卡兹古尔"},
        {"Narzulbur", "纳祖尔布尔"}, {"Skaal Village", "斯卡尔村"},
        {"Eastmarch", "风盔城"}, {"Haafingar", "独孤城"}, {"Hjaalmarch", "莫萨尔"},
        {"the Reach", "马卡斯城"}, {"the Pale", "晨星城"}, {"the Rift", "裂谷城"},
    };
    for (const auto& [key, label] : labels) if (key == name) return std::string(label);
    return std::string(name);
}

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

std::string RegionLabel(const RegionSnapshot* regions, const StableRegionKey& key) {
    if (regions != nullptr) {
        for (const auto& alias : regions->apiAliases) {
            if (alias.target == key) return LocationLabel(alias.alias);
        }
    }
    return RegionKeyText(key);
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
        return "已应用";
    case ConfigurationUpdateStatus::Unchanged:
        return "未更改";
    case ConfigurationUpdateStatus::Invalid:
        return "配置无效";
    case ConfigurationUpdateStatus::IoError:
        return "文件读写失败";
    }
    return "未知";
}

std::string UpdateStatusLine(const ConfigurationSnapshot& snapshot) {
    std::string line = "配置版本：";
    line += std::to_string(snapshot.revision.value);
    line += "，状态：";
    line += StatusText(snapshot.lastUpdate.status);
    if (!snapshot.lastUpdate.message.empty()) {
        line += ": ";
        line += MenuMessageText(snapshot.lastUpdate.message);
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

    addSection((std::string(kSectionRoot) + "/总览").c_str(),
               []() { Instance().RenderOverview(); });
    addSection((std::string(kSectionRoot) + "/声望类别").c_str(),
               []() { Instance().RenderCategories(); });
    addSection((std::string(kSectionRoot) + "/地区连接").c_str(),
               []() { Instance().RenderConnections(); });
    addSection((std::string(kSectionRoot) + "/规则与平衡").c_str(),
               []() { Instance().RenderBalance(); });
    addSection((std::string(kSectionRoot) + "/声望操作").c_str(),
               []() { Instance().RenderCommands(); });
    addSection((std::string(kSectionRoot) + "/旧版数据").c_str(),
               []() { Instance().RenderLegacy(); });
    addSection((std::string(kSectionRoot) + "/运行状态").c_str(),
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
        lastAction_ = "没有待应用的修改";
        return CommitOutcome::Nothing;
    }
    const ConfigurationData* data = CurrentData();
    if (data != nullptr && *draft_ == *data) {
        lastAction_ = "没有待应用的修改";
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
        TextUnformatted("菜单框架的界面功能不可用。");
        return;
    }
    RefreshDraft();
    if (draft_ == std::nullopt) {
        TextUnformatted("配置尚未就绪。");
        return;
    }
    const ConfigurationSnapshot& snapshot = cached_;
    Text(UpdateStatusLine(snapshot));
    Separator();

    std::string counts = "地区连接数：" + std::to_string(draft_->connections.size());
    counts += "，关注地区数：" + std::to_string(draft_->menu.selectedRegions.size());
    if (const RegionSnapshot* regions = ports_.regions != nullptr ? ports_.regions() : nullptr) {
        counts += "，世界地区数：" + std::to_string(regions->regions.size());
    }
    Text(counts);
    Separator();

    Checkbox("夜间减少声望增长", &draft_->features.reduceFameAtNight);
    Checkbox("启用声望传播限制", &draft_->features.fameSpreadRestrictions);
    Checkbox("动态匿名", &draft_->features.dynamicAnonymity);
    for (const auto& setting : kGameplayBoolSettings) Checkbox(GameplayLabel(setting.name), &(draft_->gameplay.*setting.member));
    for (const auto& setting : kGameplayNumberSettings) {
        float value = static_cast<float>(draft_->gameplay.*setting.member);
        if (SliderFloat(GameplayLabel(setting.name), &value, static_cast<float>(setting.minimum), static_cast<float>(setting.maximum))) draft_->gameplay.*setting.member = value;
    }
    Separator();
    Checkbox("声望增长时通知", &draft_->menu.notifications.fameIncrease);
    Checkbox("声望衰减时通知", &draft_->menu.notifications.fameDecay);
    Checkbox("声望传播时通知", &draft_->menu.notifications.fameSpread);
    Separator();
    if (Button("应用设置")) {
        (void)ApplyDraft();
    }
}

void Menu::RenderCategories() {
    if (!UiAvailable()) {
        TextUnformatted("菜单框架的界面功能不可用。");
        return;
    }
    RefreshDraft();
    if (draft_ == std::nullopt) {
        TextUnformatted("配置尚未就绪。");
        return;
    }
    for (std::size_t index = 0; index < kFameCategoryCount; ++index) {
        PushId(static_cast<int>(index));
        float multiplier = static_cast<float>(draft_->categories.multipliers[index]);
        if (SliderFloat(kCategoryLabels[index], &multiplier, 0.0f, kMultiplierLimit)) {
            draft_->categories.multipliers[index] =
                std::clamp(static_cast<double>(multiplier), 0.0, kConfigMaxValue);
        }
        PopId();
    }
    Separator();
    int minimumFame = static_cast<int>(draft_->categories.minimumFameToSpread);
    if (SliderInt("传播所需最低声望", &minimumFame, 0,
                  static_cast<int>(kConfigMaxValue))) {
        draft_->categories.minimumFameToSpread = static_cast<double>(minimumFame);
    }
    int maximumCategories = static_cast<int>(draft_->categories.maximumSpreadCategories);
    if (SliderInt("单次传播类别上限", &maximumCategories, 0,
                  static_cast<int>(kFameCategoryCount))) {
        draft_->categories.maximumSpreadCategories = static_cast<std::uint32_t>(maximumCategories);
    }
    int maximumPercentage = static_cast<int>(draft_->categories.maximumSpreadPercentage);
    if (SliderInt("传播百分比上限", &maximumPercentage, 0, 100)) {
        draft_->categories.maximumSpreadPercentage = static_cast<double>(maximumPercentage);
    }
    Separator();
    if (Button("应用设置")) {
        (void)ApplyDraft();
    }
}

void Menu::RenderConnections() {
    if (!UiAvailable()) {
        TextUnformatted("菜单框架的界面功能不可用。");
        return;
    }
    RefreshDraft();
    if (draft_ == std::nullopt) {
        TextUnformatted("配置尚未就绪。");
        return;
    }
    const RegionSnapshot* regions =
        ports_.regions != nullptr ? ports_.regions() : nullptr;

    std::size_t orphanCount = 0;
    for (std::size_t index = 0; index < draft_->connections.size(); ++index) {
        const RegionConnectionSetting& setting = draft_->connections[index];
        PushId(static_cast<int>(index));
        std::string row = RegionLabel(regions, setting.from) + " → " + RegionLabel(regions, setting.to);
        if (!IsKnownRegion(regions, setting.from) || !IsKnownRegion(regions, setting.to)) {
            row += "（地点暂不可用）";
            ++orphanCount;
        }
        Text(row);
        bool enabled = setting.enabled;
        SameLine();
        if (Checkbox("启用", &enabled)) {
            draft_->connections[index].enabled = enabled;
        }
        if (SmallButton("删除连接")) {
            draft_->connections.erase(draft_->connections.begin() +
                                      static_cast<std::ptrdiff_t>(index));
            PopId();
            --index;
            continue;
        }
        PopId();
    }
    if (draft_->connections.empty()) {
        TextUnformatted("尚未设置地区连接。");
    }
    if (orphanCount != 0) {
        Text("地点暂不可用的连接数：" + std::to_string(orphanCount));
    }
    Separator();
    if (regions == nullptr || regions->regions.empty()) {
        TextUnformatted("地区数据尚未加载，暂时无法添加连接。");
        return;
    }
    const std::size_t regionCount = regions->regions.size();
    const auto validIndex = [regionCount](int index) {
        return index >= 0 && static_cast<std::size_t>(index) < regionCount;
    };
    std::string sourcePreview =
        validIndex(sourceIndex_)
            ? RegionLabel(regions, regions->regions[static_cast<std::size_t>(sourceIndex_)])
            : std::string("（未选择）");
    if (BeginCombo("来源地区", sourcePreview)) {
        for (std::size_t index = 0; index < regionCount; ++index) {
            if (Selectable(RegionLabel(regions, regions->regions[index]),
                           static_cast<std::size_t>(sourceIndex_) == index)) {
                sourceIndex_ = static_cast<int>(index);
            }
        }
        EndCombo();
    }
    std::string targetPreview =
        validIndex(targetIndex_)
            ? RegionLabel(regions, regions->regions[static_cast<std::size_t>(targetIndex_)])
            : std::string("（未选择）");
    if (BeginCombo("目标地区", targetPreview)) {
        for (std::size_t index = 0; index < regionCount; ++index) {
            if (Selectable(RegionLabel(regions, regions->regions[index]),
                           static_cast<std::size_t>(targetIndex_) == index)) {
                targetIndex_ = static_cast<int>(index);
            }
        }
        EndCombo();
    }
    if (Button("添加连接") && validIndex(sourceIndex_) && validIndex(targetIndex_)) {
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
    if (Button("应用设置")) {
        (void)ApplyDraft();
    }
}

void Menu::RenderBalance() {
    if (!UiAvailable()) {
        TextUnformatted("菜单框架的界面功能不可用。");
        return;
    }
    RefreshDraft();
    if (draft_ == std::nullopt) {
        TextUnformatted("配置尚未就绪。");
        return;
    }
    TextUnformatted("朋友与恋人规则");
    float friendThreshold =
        static_cast<float>(draft_->relationships.friendHighFameThreshold);
    if (SliderFloat("朋友的高声望阈值", &friendThreshold, 0.0f,
                    static_cast<float>(kConfigMaxValue))) {
        draft_->relationships.friendHighFameThreshold = friendThreshold;
    }
    float loverThreshold = static_cast<float>(draft_->relationships.loverHighFameThreshold);
    if (SliderFloat("恋人的高声望阈值", &loverThreshold, 0.0f,
                    static_cast<float>(kConfigMaxValue))) {
        draft_->relationships.loverHighFameThreshold = loverThreshold;
    }
    float friendChance = static_cast<float>(draft_->relationships.friendBetrayChance);
    if (SliderFloat("朋友泄密概率（%）", &friendChance, 0.0f, 100.0f)) {
        draft_->relationships.friendBetrayChance = friendChance;
    }
    float loverChance = static_cast<float>(draft_->relationships.loverBetrayChance);
    if (SliderFloat("恋人泄密概率（%）", &loverChance, 0.0f, 100.0f)) {
        draft_->relationships.loverBetrayChance = loverChance;
    }
    Separator();
    TextUnformatted("时间与传播");
    float decayPeriod = static_cast<float>(draft_->timing.decayPeriodDays);
    if (SliderFloat("衰减周期（游戏天）", &decayPeriod, 0.0f, 30.0f)) {
        draft_->timing.decayPeriodDays = decayPeriod;
    }
    float spreadPeriod = static_cast<float>(draft_->timing.spreadPeriodDays);
    if (SliderFloat("传播周期（游戏天）", &spreadPeriod, 0.0f, 30.0f)) {
        draft_->timing.spreadPeriodDays = spreadPeriod;
    }
    float nightStart = static_cast<float>(draft_->timing.nightStart);
    if (SliderFloat("夜间开始时刻", &nightStart, 0.0f, 24.0f)) {
        draft_->timing.nightStart = nightStart;
    }
    float nightEnd = static_cast<float>(draft_->timing.nightEnd);
    if (SliderFloat("夜间结束时刻", &nightEnd, 0.0f, 24.0f)) {
        draft_->timing.nightEnd = nightEnd;
    }
    Separator();
    TextUnformatted("声望模型参数");
    float decayPerDay = static_cast<float>(draft_->parameters.decayPerDay);
    if (SliderFloat("每日衰减量", &decayPerDay, 0.0f,
                    static_cast<float>(kConfigMaxValue))) {
        draft_->parameters.decayPerDay = decayPerDay;
    }
    float relaxationPerDay = static_cast<float>(draft_->parameters.relaxationPerDay);
    if (SliderFloat("每日传闻回落速率", &relaxationPerDay, 0.0f,
                    static_cast<float>(kConfigMaxValue))) {
        draft_->parameters.relaxationPerDay = relaxationPerDay;
    }
    float spreadThreshold = static_cast<float>(draft_->parameters.spreadThreshold);
    if (SliderFloat("传播阈值", &spreadThreshold, 0.0f,
                    static_cast<float>(kConfigMaxValue))) {
        draft_->parameters.spreadThreshold = spreadThreshold;
    }
    float spreadRetention = static_cast<float>(draft_->parameters.spreadRetention);
    if (SliderFloat("跨区传播保留比例", &spreadRetention, 0.0f, 1.0f)) {
        draft_->parameters.spreadRetention = spreadRetention;
    }
    float maxFame = static_cast<float>(draft_->parameters.maxFame);
    if (SliderFloat("声望上限", &maxFame, 1.0f, static_cast<float>(kConfigMaxValue))) {
        draft_->parameters.maxFame = maxFame;
    }
    Separator();
    if (Button("应用设置")) {
        (void)ApplyDraft();
    }
}

void Menu::RenderCommands() {
    if (!UiAvailable()) {
        TextUnformatted("菜单框架的界面功能不可用。");
        return;
    }
    if (ports_.fameCommand == nullptr) {
        TextUnformatted("声望操作尚未就绪。");
        return;
    }
    const RegionSnapshot* regions =
        ports_.regions != nullptr ? ports_.regions() : nullptr;
    if (regions == nullptr || regions->apiAliases.empty()) {
        TextUnformatted("地区数据尚未加载，暂时无法执行声望操作。");
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
                                      ? LocationLabel(locations[static_cast<std::size_t>(commandLocationIndex_)])
                                      : std::string("（未选择）");
    if (BeginCombo("操作地点", locationPreview)) {
        for (std::size_t index = 0; index < locations.size(); ++index) {
            if (Selectable(LocationLabel(locations[index]), static_cast<std::size_t>(commandLocationIndex_) == index)) {
                commandLocationIndex_ = static_cast<int>(index);
            }
        }
        EndCombo();
    }

    std::string categoryPreview = commandCategoryIndex_ >= 0 &&
                                          commandCategoryIndex_ < static_cast<int>(kFameCategoryCount)
                                      ? std::string(
                                            kCategoryLabels[static_cast<std::size_t>(commandCategoryIndex_)])
                                      : std::string("（未选择）");
    if (BeginCombo("声望类别", categoryPreview)) {
        for (std::size_t index = 0; index < kFameCategoryCount; ++index) {
            if (Selectable(kCategoryLabels[index],
                           static_cast<std::size_t>(commandCategoryIndex_) == index)) {
                commandCategoryIndex_ = static_cast<int>(index);
            }
        }
        EndCombo();
    }

    std::string targetPreview = validLocation(commandTargetIndex_)
                                    ? LocationLabel(locations[static_cast<std::size_t>(commandTargetIndex_)])
                                    : std::string("（未选择）");
    if (BeginCombo("目标地区", targetPreview)) {
        for (std::size_t index = 0; index < locations.size(); ++index) {
            if (Selectable(LocationLabel(locations[index]),
                           static_cast<std::size_t>(commandTargetIndex_) == index)) {
                commandTargetIndex_ = static_cast<int>(index);
            }
        }
        EndCombo();
    }

    SliderInt("数值", &commandValue_, 0, static_cast<int>(kConfigMaxValue));
    SliderInt("最小值", &commandMinimum_, 0, static_cast<int>(kConfigMaxValue));
    SliderInt("最大值", &commandMaximum_, 0, static_cast<int>(kConfigMaxValue));
    SliderInt("传播比例（%）", &commandPercent_, 0, 100);
    Separator();

    TextUnformatted("上次操作结果：");
    Text(lastCommandResult_.empty() ? std::string("（暂无）") : lastCommandResult_);
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
    if (Button("设置声望")) {
        if (!locationReady || category.empty()) {
            lastCommandResult_ = "请先选择地点和声望类别";
        } else {
            run(FameCommand::SetFame, location, target, category, commandValue_, commandValue_);
        }
    }
    SameLine();
    if (Button("清除当前类别")) {
        if (!locationReady || category.empty()) {
            lastCommandResult_ = "请先选择地点和声望类别";
        } else {
            run(FameCommand::ClearFame, location, target, category, 0, 0);
        }
    }
    SameLine();
    if (Button("清除该地区全部声望")) {
        if (!locationReady) {
            lastCommandResult_ = "请先选择地点";
        } else {
            run(FameCommand::ClearAllFame, location, target, category, 0, 0);
        }
    }
    if (Button("手动增加") && locationReady) {
        run(FameCommand::ManualGain, location, target, category, commandMinimum_,
            commandMaximum_);
    }
    SameLine();
    if (Button("手动衰减") && locationReady) {
        run(FameCommand::ManualDecay, location, target, category, commandMinimum_,
            commandMaximum_);
    }
    SameLine();
    if (Button("手动传播") && locationReady && !target.empty()) {
        run(FameCommand::ManualSpread, location, target, category, commandPercent_, 0);
    }
    SameLine();
    if (Button("执行一次自动传播") && locationReady) {
        run(FameCommand::SpreadPulse, location, target, category, 0, 0);
    }
}

void Menu::RenderLegacy() {
    if (!UiAvailable()) {
        TextUnformatted("菜单框架的界面功能不可用。");
        return;
    }
    TextUnformatted("可使用旧版 SLSF 数据文件导入声望。");
    Separator();
    TextUnformatted(
        "导入目录：Data/SKSE/Plugins/StorageUtilData/SLSF_Reloaded/");
    TextUnformatted(
        "跨存档设置与地区连接：Data/SKSE/Plugins/SLSFReloadedNG.json");
    Separator();
    if (ports_.legacyFiles != nullptr) {
        legacyFiles_ = ports_.legacyFiles();
    }
    if (legacyFiles_.empty()) {
        TextUnformatted("未找到旧版数据文件。");
    }
    for (std::size_t index = 0; index < legacyFiles_.size(); ++index) {
        PushId(static_cast<int>(index));
        const bool runImport =
            ports_.legacyImport != nullptr && SmallButton("导入");
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
    TextUnformatted("上次菜单操作：");
    Text(MenuMessageText(LastUpdateMessage()));
    Separator();
    std::string counters = "请求处理轮次：" + std::to_string(queue_.DrainCount());
    counters += "，已处理请求数：" + std::to_string(queue_.HandledCount());
    Text(counters);
}

}
