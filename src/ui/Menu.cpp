#include "slsf/ui/Menu.h"
#include "slsf/ui/MenuText.h"

#include "slsf/core/Types.h"

#include <windows.h>

#include <algorithm>
#include <charconv>
#include <initializer_list>
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
    struct Vec4 {
        float x;
        float y;
        float z;
        float w;
    };

    using TextFn = void (*)(const char*, ...);
    using TextUnformattedFn = void (*)(const char*, const char*);
    using SameLineFn = void (*)(float, float);
    using ButtonFn = bool (*)(const char*, Vec2);
    using CheckboxFn = bool (*)(const char*, bool*);
    using SliderFloatFn = bool (*)(const char*, float*, float, float, const char*, int);
    using SliderScalarFn = bool (*)(const char*, int, void*, const void*, const void*, const char*, int);
    using BeginComboFn = bool (*)(const char*, const char*, int);
    using EndComboFn = void (*)();
    using SelectableFn = bool (*)(const char*, bool, int, Vec2);
    using PushIdFn = void (*)(const char*);
    using PopIdFn = void (*)();
    using CollapsingHeaderFn = bool (*)(const char*, int);
    using LayoutFn = void (*)();
    using FloatFn = void (*)(float);
    using LabelFn = void (*)(const char*);
    using SectionFn = bool (*)(const char*, int);
    using BeginTabItemFn = bool (*)(const char*, bool*, int);
    using BeginChildFn = bool (*)(const char*, Vec2, int, int);
    using BeginTableFn = bool (*)(const char*, int, int, Vec2, float);
    using SetupColumnFn = void (*)(const char*, int, float, unsigned int);
    using NextColumnFn = bool (*)();
    using BeginDisabledFn = void (*)(bool);
    using KeyPressedFn = bool (*)(int, bool);
    using OpenPopupFn = void (*)(const char*, int);
    using PushStyleColorFn = void (*)(int, Vec4);
    using PopStyleColorFn = void (*)(int);

    TextFn text{nullptr};
    TextUnformattedFn textUnformatted{nullptr};
    SameLineFn sameLine{nullptr};
    ButtonFn button{nullptr};
    CheckboxFn checkbox{nullptr};
    SliderFloatFn sliderFloat{nullptr};
    SliderScalarFn sliderScalar{nullptr};
    BeginComboFn beginCombo{nullptr};
    EndComboFn endCombo{nullptr};
    SelectableFn selectable{nullptr};
    PushIdFn pushId{nullptr};
    PopIdFn popId{nullptr};
    CollapsingHeaderFn collapsingHeader{nullptr};
    FloatFn setNextItemWidth{};
    FloatFn pushTextWrapPos{};
    LayoutFn popTextWrapPos{};
    LayoutFn setItemDefaultFocus{};
    LabelFn separatorText{};
    SectionFn beginTabBar{};
    BeginTabItemFn beginTabItem{};
    LayoutFn endTabBar{};
    LayoutFn endTabItem{};
    BeginChildFn beginChild{};
    LayoutFn endChild{};
    BeginTableFn beginTable{};
    SetupColumnFn tableSetupColumn{};
    LayoutFn tableHeadersRow{};
    NextColumnFn tableNextColumn{};
    LayoutFn endTable{};
    BeginDisabledFn beginDisabled{};
    LayoutFn endDisabled{};
    KeyPressedFn isKeyPressed{};
    OpenPopupFn openPopup{};
    BeginTabItemFn beginPopupModal{};
    LayoutFn closeCurrentPopup{};
    LayoutFn endPopup{};
    PushStyleColorFn pushStyleColor{};
    PopStyleColorFn popStyleColor{};

    [[nodiscard]] bool Usable() const noexcept {
        return text != nullptr && textUnformatted != nullptr && button != nullptr &&
               checkbox != nullptr && sliderFloat != nullptr &&
               sliderScalar != nullptr && beginCombo != nullptr && endCombo != nullptr &&
               selectable != nullptr && pushId != nullptr && popId != nullptr &&
               sameLine && collapsingHeader && setNextItemWidth && pushTextWrapPos &&
               popTextWrapPos && setItemDefaultFocus && separatorText && beginTabBar && beginTabItem &&
               endTabBar && endTabItem && beginChild && endChild && beginTable && tableSetupColumn &&
               tableHeadersRow && tableNextColumn && endTable && beginDisabled && endDisabled &&
               isKeyPressed && openPopup && beginPopupModal && closeCurrentPopup && endPopup;
    }

    [[nodiscard]] static const ImGuiApi& Fetch();
};

const ImGuiApi& ImGuiApi::Fetch() {
    static const ImGuiApi cached = [] {
        ImGuiApi api;
        api.text = FrameworkFunction<ImGuiApi::TextFn>("igText");
        api.textUnformatted = FrameworkFunction<ImGuiApi::TextUnformattedFn>("igTextUnformatted");
        api.sameLine = FrameworkFunction<ImGuiApi::SameLineFn>("igSameLine");
        api.button = FrameworkFunction<ImGuiApi::ButtonFn>("igButton");
        api.checkbox = FrameworkFunction<ImGuiApi::CheckboxFn>("igCheckbox");
        api.sliderFloat = FrameworkFunction<ImGuiApi::SliderFloatFn>("igSliderFloat");
        api.sliderScalar = FrameworkFunction<ImGuiApi::SliderScalarFn>("igSliderScalar");
        api.beginCombo = FrameworkFunction<ImGuiApi::BeginComboFn>("igBeginCombo");
        api.endCombo = FrameworkFunction<ImGuiApi::EndComboFn>("igEndCombo");
        api.selectable = FrameworkFunction<ImGuiApi::SelectableFn>("igSelectable_Bool");
        api.pushId = FrameworkFunction<ImGuiApi::PushIdFn>("igPushID_Str");
        api.popId = FrameworkFunction<ImGuiApi::PopIdFn>("igPopID");
        api.collapsingHeader = FrameworkFunction<ImGuiApi::CollapsingHeaderFn>("igCollapsingHeader_TreeNodeFlags");
        api.setNextItemWidth = FrameworkFunction<ImGuiApi::FloatFn>("igSetNextItemWidth");
        api.pushTextWrapPos = FrameworkFunction<ImGuiApi::FloatFn>("igPushTextWrapPos");
        api.popTextWrapPos = FrameworkFunction<ImGuiApi::LayoutFn>("igPopTextWrapPos");
        api.setItemDefaultFocus = FrameworkFunction<ImGuiApi::LayoutFn>("igSetItemDefaultFocus");
        api.separatorText = FrameworkFunction<ImGuiApi::LabelFn>("igSeparatorText");
        api.beginTabBar = FrameworkFunction<ImGuiApi::SectionFn>("igBeginTabBar");
        api.beginTabItem = FrameworkFunction<ImGuiApi::BeginTabItemFn>("igBeginTabItem");
        api.endTabBar = FrameworkFunction<ImGuiApi::LayoutFn>("igEndTabBar");
        api.endTabItem = FrameworkFunction<ImGuiApi::LayoutFn>("igEndTabItem");
        api.beginChild = FrameworkFunction<ImGuiApi::BeginChildFn>("igBeginChild_Str");
        api.endChild = FrameworkFunction<ImGuiApi::LayoutFn>("igEndChild");
        api.beginTable = FrameworkFunction<ImGuiApi::BeginTableFn>("igBeginTable");
        api.tableSetupColumn = FrameworkFunction<ImGuiApi::SetupColumnFn>("igTableSetupColumn");
        api.tableHeadersRow = FrameworkFunction<ImGuiApi::LayoutFn>("igTableHeadersRow");
        api.tableNextColumn = FrameworkFunction<ImGuiApi::NextColumnFn>("igTableNextColumn");
        api.endTable = FrameworkFunction<ImGuiApi::LayoutFn>("igEndTable");
        api.beginDisabled = FrameworkFunction<ImGuiApi::BeginDisabledFn>("igBeginDisabled");
        api.endDisabled = FrameworkFunction<ImGuiApi::LayoutFn>("igEndDisabled");
        api.isKeyPressed = FrameworkFunction<ImGuiApi::KeyPressedFn>("igIsKeyPressed_Bool");
        api.openPopup = FrameworkFunction<ImGuiApi::OpenPopupFn>("igOpenPopup_Str");
        api.beginPopupModal = FrameworkFunction<ImGuiApi::BeginTabItemFn>("igBeginPopupModal");
        api.closeCurrentPopup = FrameworkFunction<ImGuiApi::LayoutFn>("igCloseCurrentPopup");
        api.endPopup = FrameworkFunction<ImGuiApi::LayoutFn>("igEndPopup");
        api.pushStyleColor = FrameworkFunction<ImGuiApi::PushStyleColorFn>("igPushStyleColor_Vec4");
        api.popStyleColor = FrameworkFunction<ImGuiApi::PopStyleColorFn>("igPopStyleColor");
        return api;
    }();
    return cached;
}

constexpr int kImGuiDataTypeS32 = 4;
constexpr int kTextColor = 0;
constexpr int kClampSlider = 1 << 4;
constexpr int kTabScroll = 1 << 7;
constexpr int kTableRowBg = 1 << 6;
constexpr int kTableInnerVerticalBorders = 1 << 9;
constexpr int kTableStretchSame = 4 << 13;
constexpr int kAlwaysAutoResize = 1 << 6;
constexpr int kEscapeKey = 526;
constexpr double kConfigMaxValue = 1e6;
constexpr float kLastDisplayableHour = 23.99f;

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

void Wrapped(std::string_view value) {
    const ImGuiApi& api = ImGuiApi::Fetch();
    api.pushTextWrapPos(0.0f);
    TextUnformatted(value);
    api.popTextWrapPos();
}

void StatusLine(std::string_view value, ImGuiApi::Vec4 color) {
    const auto& ui = ImGuiApi::Fetch();
    const bool themed = ui.pushStyleColor && ui.popStyleColor;
    if (themed) ui.pushStyleColor(kTextColor, color);
    Wrapped(value);
    if (themed) ui.popStyleColor(1);
}

bool Button(std::string_view label, bool enabled = true) {
    const ImGuiApi& api = ImGuiApi::Fetch();
    api.beginDisabled(!enabled);
    const bool clicked = api.button(label.data(), ImGuiApi::Vec2{0.0f, 0.0f});
    api.endDisabled();
    return enabled && clicked;
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
    if (label.starts_with("##")) api.setNextItemWidth(-1.0f);
    return api.sliderFloat(label.data(), value, minimum, maximum, "%.2f", kClampSlider);
}

bool SliderInt(std::string_view label, int* value, int minimum, int maximum) {
    const ImGuiApi& api = ImGuiApi::Fetch();
    if (api.sliderScalar == nullptr) {
        return false;
    }
    return api.sliderScalar(label.data(), kImGuiDataTypeS32, value, &minimum, &maximum, "%d", kClampSlider);
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
    const bool clicked = api.selectable(label.data(), selected, 0, ImGuiApi::Vec2{0.0f, 0.0f});
    if (selected) api.setItemDefaultFocus();
    return clicked;
}

void SameLine() {
    const ImGuiApi& api = ImGuiApi::Fetch();
    if (api.sameLine != nullptr) {
        api.sameLine(0.0f, 12.0f);
    }
}

void PushId(const char* id) {
    const ImGuiApi& api = ImGuiApi::Fetch();
    api.pushId(id);
}

void PushId(int index) {
    PushId(std::to_string(index).c_str());
}

void PopId() {
    const ImGuiApi& api = ImGuiApi::Fetch();
    if (api.popId != nullptr) {
        api.popId();
    }
}

bool Group(const char* label, bool open = true) {
    return ImGuiApi::Fetch().collapsingHeader(label, open ? 1 << 5 : 0);
}

void Heading(const char* label) {
    ImGuiApi::Fetch().separatorText(label);
}

template <typename Render>
void Tab(const char* label, Render render) {
    const auto& ui = ImGuiApi::Fetch();
    if (!ui.beginTabItem(label, nullptr, 0)) return;
    if (ui.beginChild(label, {0.0f, 0.0f}, 0, 0)) render();
    ui.endChild();
    ui.endTabItem();
}

bool BeginTable(const char* id, std::initializer_list<const char*> columns) {
    const auto& ui = ImGuiApi::Fetch();
    if (!ui.beginTable(id, static_cast<int>(columns.size()), kTableRowBg | kTableInnerVerticalBorders | kTableStretchSame, {}, 0.0f)) return false;
    for (const char* column : columns) ui.tableSetupColumn(column, 0, 0.0f, 0);
    ui.tableHeadersRow();
    return true;
}

template <typename Action, typename Cancel>
void Confirmation(const char* title, std::string_view prompt, const char* confirm, bool ready,
                  Action action, Cancel cancel) {
    const auto& ui = ImGuiApi::Fetch();
    if (!ui.beginPopupModal(title, nullptr, kAlwaysAutoResize)) return;
    Wrapped(prompt);
    const bool cancelled = Button("取消") || ui.isKeyPressed(kEscapeKey, false);
    ui.setItemDefaultFocus();
    SameLine();
    const bool confirmed = Button(confirm, ready);
    if (cancelled || !ready || confirmed) {
        if (!cancelled && confirmed) action();
        else cancel();
        ui.closeCurrentPopup();
    }
    ui.endPopup();
}

void LocationPicker(const char* label, std::string& selected, const FameReadout& readout, bool allowCurrent = false) {
    const auto emptyLabel = allowCurrent ? "跟随玩家当前地区" : "（未选择）";
    const auto preview = selected.empty() ? std::string(emptyLabel) : LocationLabel(selected);
    if (!BeginCombo(label, preview)) return;
    if (allowCurrent && Selectable(emptyLabel, selected.empty())) selected.clear();
    for (const auto& location : readout.locations) {
        PushId(location.c_str());
        if (Selectable(LocationLabel(location), selected == location)) selected = location;
        PopId();
    }
    EndCombo();
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
    self.draftBase_.reset();
    self.draftRevision_ = {};
    self.lastAction_.clear();
    self.rejectedDraft_.reset();
    self.lastCommandResult_.clear();
    self.fameLocation_.clear();
    self.legacyImportFile_.reset();
    self.lastImportResult_.clear();
    self.commandLocations_ = {};
    self.commandIndex_ = 0;
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

    addSection((std::string(kSectionRoot) + "/设置").c_str(),
               []() { Instance().Render(); });

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
                if (self.DraftDirty() && self.ports_.snapshot && result.Succeeded() &&
                    result.revision != self.cached_.revision) {
                    self.cached_ = self.ports_.snapshot();
                }
            }
            break;
        case MenuRequestQueue::Kind::CloseCommit:
            self.legacyImportFile_.reset();
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

bool Menu::DraftDirty() const {
    return draft_ && draftBase_ && *draft_ != *draftBase_;
}

void Menu::RefreshDraft() {
    const ConfigurationData* data = CurrentData();
    if (data && (!draft_ || (draftRevision_ != cached_.revision && !DraftDirty()))) {
        draft_ = *data;
        draftBase_ = cached_.data;
        draftRevision_ = cached_.revision;
        rejectedDraft_.reset();
        sourceIndex_ = -1;
        targetIndex_ = -1;
    }
}

Menu::CommitOutcome Menu::ApplyDraft() {
    if (!DraftDirty()) {
        rejectedDraft_.reset();
        lastAction_ = "没有待应用的修改";
        return CommitOutcome::Nothing;
    }
    if (!CurrentData() || !ports_.commit || draftRevision_ != cached_.revision) {
        rejectedDraft_ = draftRevision_ != cached_.revision
                             ? "磁盘上的配置已更改。请放弃此草稿后，再根据新版本重新编辑。"
                             : "配置尚未就绪，未保存的修改已保留。";
        lastAction_ = *rejectedDraft_;
        return CommitOutcome::Failed;
    }
    const ConfigurationUpdateResult result = ports_.commit(*draft_, draftRevision_);
    lastAction_ = result.message;
    if (ports_.snapshot) cached_ = ports_.snapshot();
    if (result.Succeeded()) {
        draft_.reset();
        draftBase_.reset();
        draftRevision_ = {};
        rejectedDraft_.reset();
        return CommitOutcome::Committed;
    }
    rejectedDraft_ = result.message.empty() ? StatusText(result.status) : result.message;
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
    if (DraftDirty() && rejectedDraft_) return *rejectedDraft_;
    if (!lastAction_.empty()) {
        return lastAction_;
    }
    return cached_.lastUpdate.message;
}

void Menu::Render() {
    if (!UiAvailable()) {
        TextUnformatted("菜单框架的界面功能不可用。");
        return;
    }
    const auto& ui = ImGuiApi::Fetch();
    PushId("SLSFReloadedNG");
    RefreshDraft();
    Heading("SLSF 声望重制");
    Wrapped("设置跨存档共用，可手动保存，关闭菜单时也会尝试保存。声望操作立即修改当前存档的声望数据。");
    const bool dirty = DraftDirty();
    const bool conflict = dirty && draftRevision_ != cached_.revision;
    constexpr ImGuiApi::Vec4 warning{0.96f, 0.77f, 0.39f, 1.0f};
    constexpr ImGuiApi::Vec4 failure{1.0f, 0.58f, 0.53f, 1.0f};
    if (conflict) StatusLine("配置版本已更改，未保存的草稿已保留。请先放弃草稿，再根据新版本重新编辑。", warning);
    else if (dirty && rejectedDraft_) StatusLine("保存失败，修改仍未保存：" + MenuMessageText(*rejectedDraft_), failure);
    else if (dirty) StatusLine("有未保存的修改。", warning);
    else if (CurrentData()) Text("当前配置版本：" + std::to_string(cached_.revision.value) + " · 无未保存的修改");
    else TextUnformatted("配置尚未就绪。");
    if (Button("应用并保存", dirty && !conflict && CurrentData() && ports_.commit)) (void)ApplyDraft();
    SameLine();
    if (Button("放弃未保存的修改", dirty)) ui.openPopup("放弃修改？", 0);
    Confirmation("放弃修改？", "将放弃本次未保存的设置修改，并显示当前已保存的配置。", "放弃修改", dirty, [this] {
        draft_.reset();
        rejectedDraft_.reset();
        lastAction_ = "已放弃未保存的修改。";
    }, [] {});
    RefreshDraft();
    if (!DraftDirty() && !LastUpdateMessage().empty()) Wrapped(MenuMessageText(LastUpdateMessage()));
    if (ui.beginTabBar("##Sections", kTabScroll)) {
        const auto settings = [this](void (Menu::*render)()) {
            if (draft_) (this->*render)();
            else TextUnformatted("配置尚未就绪。");
        };
        Tab("基础设置", [&] { settings(&Menu::RenderOverview); });
        Tab("声望规则", [&] { settings(&Menu::RenderCategories); settings(&Menu::RenderBalance); });
        Tab("地区连接", [&] { settings(&Menu::RenderConnections); });
        Tab("实时声望", [&] { RenderFame(); });
        Tab("声望操作", [&] { RenderCommands(); });
        Tab("维护", [&] {
            if (Group("旧版数据导入")) RenderLegacy();
            Heading("运行状态");
            RenderDiagnostics();
        });
        ui.endTabBar();
    }
    PopId();
}

void Menu::RenderOverview() {
    Heading("通用规则");
    Checkbox("夜间减少声望增长", &draft_->features.reduceFameAtNight);
    Checkbox("启用声望传播限制", &draft_->features.fameSpreadRestrictions);
    Checkbox("动态匿名", &draft_->features.dynamicAnonymity);
    Heading("通知");
    Checkbox("声望增长时通知", &draft_->menu.notifications.fameIncrease);
    Checkbox("声望衰减时通知", &draft_->menu.notifications.fameDecay);
    Checkbox("声望传播时通知", &draft_->menu.notifications.fameSpread);
    if (Group("玩法开关")) {
        for (const auto& setting : kGameplayBoolSettings) Checkbox(GameplayLabel(setting.name), &(draft_->gameplay.*setting.member));
    }
    if (Group("目击、评论与声望数值", false)) {
        for (const auto& setting : kGameplayNumberSettings) {
            float value = static_cast<float>(draft_->gameplay.*setting.member);
            if (SliderFloat(GameplayLabel(setting.name), &value, static_cast<float>(setting.minimum), static_cast<float>(setting.maximum))) draft_->gameplay.*setting.member = value;
        }
    }
}

void Menu::RenderCategories() {
    if (Group("声望增长倍率", false)) {
        Wrapped("以下为增长倍率，不是当前声望；1.00 为默认倍率，调高可增加每次符合条件的增长量。");
        if (BeginTable("##Multipliers", {"类别", "倍率"})) {
            const auto& ui = ImGuiApi::Fetch();
            for (std::size_t index = 0; index < kFameCategoryCount; ++index) {
                PushId(static_cast<int>(index));
                ui.tableNextColumn();
                TextUnformatted(kCategoryLabels[index]);
                ui.tableNextColumn();
                float multiplier = static_cast<float>(draft_->categories.multipliers[index]);
                if (SliderFloat("##Multiplier", &multiplier, 0.0f, kMultiplierLimit)) {
                    draft_->categories.multipliers[index] = multiplier;
                }
                PopId();
            }
            ui.endTable();
        }
    }
    Heading("传播限制");
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
}

void Menu::RenderConnections() {
    const RegionSnapshot* regions =
        ports_.regions != nullptr ? ports_.regions() : nullptr;

    Heading("已设置的连接");
    Wrapped("添加、启用或删除连接只会修改设置草稿，保存后才生效。箭头表示传播方向。");
    const auto& ui = ImGuiApi::Fetch();
    std::size_t orphanCount = 0;
    if (!draft_->connections.empty() && BeginTable("##Connections", {"连接", "启用", "操作"})) {
        for (std::size_t index = 0; index < draft_->connections.size(); ++index) {
            auto& setting = draft_->connections[index];
            const auto id = RegionKeyText(setting.from) + "->" + RegionKeyText(setting.to);
            PushId(id.c_str());
            ui.tableNextColumn();
            std::string row = RegionLabel(regions, setting.from) + " → " + RegionLabel(regions, setting.to);
            if (!IsKnownRegion(regions, setting.from) || !IsKnownRegion(regions, setting.to)) {
                row += "（地点暂不可用）";
                ++orphanCount;
            }
            Wrapped(row);
            ui.tableNextColumn();
            Checkbox("##Enabled", &setting.enabled);
            ui.tableNextColumn();
            const bool remove = Button("删除");
            PopId();
            if (remove) {
                draft_->connections.erase(draft_->connections.begin() + static_cast<std::ptrdiff_t>(index));
                break;
            }
        }
        ui.endTable();
    }
    if (draft_->connections.empty()) {
        TextUnformatted("尚未设置地区连接。");
    }
    if (orphanCount != 0) {
        Text("地点暂不可用的连接数：" + std::to_string(orphanCount));
    }
    Heading("添加连接");
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
            PushId(RegionKeyText(regions->regions[index]).c_str());
            const bool selected = static_cast<std::size_t>(sourceIndex_) == index;
            if (Selectable(RegionLabel(regions, regions->regions[index]), selected)) {
                sourceIndex_ = static_cast<int>(index);
            }
            PopId();
        }
        EndCombo();
    }
    std::string targetPreview =
        validIndex(targetIndex_)
            ? RegionLabel(regions, regions->regions[static_cast<std::size_t>(targetIndex_)])
            : std::string("（未选择）");
    if (BeginCombo("目标地区", targetPreview)) {
        for (std::size_t index = 0; index < regionCount; ++index) {
            PushId(RegionKeyText(regions->regions[index]).c_str());
            const bool selected = static_cast<std::size_t>(targetIndex_) == index;
            if (Selectable(RegionLabel(regions, regions->regions[index]), selected)) {
                targetIndex_ = static_cast<int>(index);
            }
            PopId();
        }
        EndCombo();
    }
    const bool picked = validIndex(sourceIndex_) && validIndex(targetIndex_);
    RegionConnectionSetting setting{};
    if (picked) {
        setting.from = regions->regions[static_cast<std::size_t>(sourceIndex_)];
        setting.to = regions->regions[static_cast<std::size_t>(targetIndex_)];
    }
    const bool duplicate = picked && std::any_of(draft_->connections.begin(), draft_->connections.end(),
        [&](const auto& entry) { return entry.from == setting.from && entry.to == setting.to; });
    const bool same = picked && setting.from == setting.to;
    const bool reversed = picked && setting.to < setting.from;
    if (!picked) TextUnformatted("请选择来源地区和目标地区。");
    else if (same) TextUnformatted("来源和目标必须是不同地区。");
    else if (reversed) Wrapped("配置文件要求连接按地区编号顺序保存；当前方向不受支持。");
    else if (duplicate) TextUnformatted("此连接已存在；可在上方启用或删除。");
    if (Button("添加连接", picked && !same && !reversed && !duplicate)) draft_->connections.push_back(std::move(setting));
}

void Menu::RenderBalance() {
    if (Group("按类别限制朋友与恋人传播", false)) {
        Wrapped("按类别限制朋友和恋人的声望传播；泄密概率与高声望规则仍然生效。");
        const auto& ui = ImGuiApi::Fetch();
        if (BeginTable("##RelationshipRestrictions", {"类别", "限制朋友", "限制恋人"})) {
            for (std::size_t index = 0; index < kFameCategoryCount; ++index) {
                PushId(static_cast<int>(index));
                const auto category = static_cast<FameCategory>(index);
                ui.tableNextColumn();
                TextUnformatted(kCategoryLabels[index]);
                ui.tableNextColumn();
                bool friendForbidden = draft_->relationships.forbiddenByFriend.Test(category);
                if (Checkbox("##Friend", &friendForbidden)) {
                    if (friendForbidden) (void)draft_->relationships.forbiddenByFriend.Set(category);
                    else (void)draft_->relationships.forbiddenByFriend.Reset(category);
                }
                ui.tableNextColumn();
                bool loverForbidden = draft_->relationships.forbiddenByLover.Test(category);
                if (Checkbox("##Lover", &loverForbidden)) {
                    if (loverForbidden) (void)draft_->relationships.forbiddenByLover.Set(category);
                    else (void)draft_->relationships.forbiddenByLover.Reset(category);
                }
                PopId();
            }
            ui.endTable();
        }
    }
    if (Group("朋友与恋人规则", false)) {
        float friendThreshold = static_cast<float>(draft_->relationships.friendHighFameThreshold);
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
    }
    if (Group("时间与传播", false)) {
        float decayPeriod = static_cast<float>(draft_->timing.decayPeriodDays);
        if (SliderFloat("衰减周期（游戏天）", &decayPeriod, 0.0f, 30.0f)) {
            draft_->timing.decayPeriodDays = decayPeriod;
        }
        float spreadPeriod = static_cast<float>(draft_->timing.spreadPeriodDays);
        if (SliderFloat("传播周期（游戏天）", &spreadPeriod, 0.0f, 30.0f)) {
            draft_->timing.spreadPeriodDays = spreadPeriod;
        }
        float nightStart = static_cast<float>(draft_->timing.nightStart);
        if (SliderFloat("夜间开始时刻", &nightStart, 0.0f, kLastDisplayableHour)) {
            draft_->timing.nightStart = nightStart;
        }
        float nightEnd = static_cast<float>(draft_->timing.nightEnd);
        if (SliderFloat("夜间结束时刻", &nightEnd, 0.0f, kLastDisplayableHour)) {
            draft_->timing.nightEnd = nightEnd;
        }
    }
    if (Group("声望模型参数", false)) {
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
    }
}

void Menu::RenderFame() {
    if (!ports_.fameReadout) {
        TextUnformatted("实时声望尚未就绪。");
        return;
    }
    auto readout = ports_.fameReadout(fameLocation_);
    if (!fameLocation_.empty() && !FameCommandLocations::Contains(readout, fameLocation_)) {
        fameLocation_.clear();
        readout = ports_.fameReadout(fameLocation_);
    }
    const auto current = readout.currentLocation.empty() ? std::string("无声望地区") : LocationLabel(readout.currentLocation);
    Text("玩家当前地区：" + current);
    LocationPicker("查看地区", fameLocation_, readout, true);
    Wrapped("自动显示当前存档中已提交的数值。修改数值请打开“声望操作”。");
    if (!readout.values) {
        Wrapped("该地区暂无可用声望；请载入存档并进入声望地区，或选择其他地区。");
        return;
    }
    Text("显示地区：" + LocationLabel(readout.location));
    if (BeginTable("##RegionalFame", {"类别", "声望", "类别", "声望"})) {
        const auto& ui = ImGuiApi::Fetch();
        for (std::size_t i = 0; i < kFameCategoryCount; ++i) {
            ui.tableNextColumn();
            TextUnformatted(kCategoryLabels[i]);
            ui.tableNextColumn();
            Text(std::to_string((*readout.values)[i]));
        }
        ui.endTable();
    }
}

void Menu::RenderCommands() {
    if (!ports_.fameCommand || !ports_.fameReadout) {
        TextUnformatted("声望操作尚未就绪。");
        return;
    }
    const auto readout = ports_.fameReadout(commandLocations_.source);
    commandLocations_.Refresh(readout);
    Heading("修改当前存档");
    Wrapped("此处的操作立即生效，不使用上方的“应用并保存”。");
    if (readout.locations.empty()) {
        TextUnformatted("请先载入一个存档。没有可操作的声望地区。");
        return;
    }
    static constexpr std::array operations{
        std::pair{FameCommand::SetFame, "设置声望"},
        std::pair{FameCommand::ManualGain, "手动增加"},
        std::pair{FameCommand::ManualDecay, "手动衰减"},
        std::pair{FameCommand::ManualSpread, "手动传播"},
        std::pair{FameCommand::SpreadPulse, "执行一次自动传播"},
        std::pair{FameCommand::ClearFame, "清除当前类别"},
        std::pair{FameCommand::ClearAllFame, "清除该地区全部声望"},
    };
    if (BeginCombo("操作类型", operations[commandIndex_].second)) {
        for (std::size_t i = 0; i < operations.size(); ++i) {
            if (Selectable(operations[i].second, i == static_cast<std::size_t>(commandIndex_))) commandIndex_ = static_cast<int>(i);
        }
        EndCombo();
    }
    const auto [command, actionLabel] = operations[commandIndex_];
    LocationPicker("操作地点", commandLocations_.source, readout);
    const bool usesCategory = command != FameCommand::ClearAllFame && command != FameCommand::SpreadPulse;
    if (usesCategory && BeginCombo("声望类别", kCategoryLabels[commandCategoryIndex_])) {
        for (std::size_t i = 0; i < kFameCategoryCount; ++i) {
            if (Selectable(kCategoryLabels[i], i == static_cast<std::size_t>(commandCategoryIndex_))) commandCategoryIndex_ = static_cast<int>(i);
        }
        EndCombo();
    }
    const auto& source = commandLocations_.source;
    const auto& target = commandLocations_.target;
    const std::string category = usesCategory ? std::string(kFameCategoryNames[commandCategoryIndex_]) : std::string{};
    if (usesCategory && !source.empty()) {
        if (readout.values && readout.location == source) {
            Text("所选地区当前声望：" + std::to_string((*readout.values)[commandCategoryIndex_]));
        }
        if (category == "Cum Dump" || category == "Cuck" || category == "Unfaithful" || category == "Airhead") {
            Wrapped("此类别需要对应来源模组或外部启用；全部来源缺失时，周期规则会清理一次。");
        }
    }
    const bool spreading = command == FameCommand::ManualSpread;
    int amount = 0;
    int extra = 0;
    switch (command) {
    case FameCommand::SetFame:
        SliderInt("数值", &commandValue_, 0, static_cast<int>(kConfigMaxValue));
        amount = commandValue_;
        break;
    case FameCommand::ManualGain:
    case FameCommand::ManualDecay:
        SliderInt("最小值", &commandMinimum_, 0, static_cast<int>(kConfigMaxValue));
        SliderInt("最大值", &commandMaximum_, 0, static_cast<int>(kConfigMaxValue));
        amount = commandMinimum_;
        extra = commandMaximum_;
        break;
    case FameCommand::ManualSpread:
        LocationPicker("传播目标地区", commandLocations_.target, readout);
        SliderInt("传播比例（%）", &commandPercent_, 0, 100);
        amount = commandPercent_;
        break;
    default:
        break;
    }
    const bool same = spreading && !target.empty() && source == target;
    const bool canRun = !source.empty() && (!spreading || (!target.empty() && !same));
    if (source.empty()) TextUnformatted("请先选择操作地点。");
    else if (same) TextUnformatted("来源和目标必须是不同地区。");
    else if (spreading && target.empty()) TextUnformatted("请选择传播目标地区。");
    const auto run = [&] {
        lastCommandResult_ = ports_.fameCommand(command, source, spreading ? target : std::string{}, category, amount, extra);
    };
    const bool clears = command == FameCommand::ClearFame || command == FameCommand::ClearAllFame;
    const auto& ui = ImGuiApi::Fetch();
    if (Button(actionLabel, canRun)) {
        if (clears) ui.openPopup("确认清除声望？", 0);
        else run();
    }
    if (clears) {
        const auto location = LocationLabel(source);
        const auto what = command == FameCommand::ClearAllFame ? std::string("全部类别") : std::string(kCategoryLabels[commandCategoryIndex_]);
        Confirmation("确认清除声望？", "将清除“" + location + "”的“" + what + "”声望。此操作立即生效。", "确认清除", canRun, run, [] {});
    }
    Heading("上次操作结果");
    Wrapped(lastCommandResult_.empty() ? "（暂无）" : lastCommandResult_);
}

void Menu::RenderLegacy() {
    Wrapped("可使用旧版 SLSF 数据文件导入声望。导入可能覆盖当前存档中已有的数据。");
    Wrapped("导入目录：Data/SKSE/Plugins/StorageUtilData/SLSF_Reloaded/");
    Wrapped("跨存档设置与地区连接：Data/SKSE/Plugins/SLSFReloadedNG.json");
    const auto files = ports_.legacyFiles ? ports_.legacyFiles() : std::vector<std::string>{};
    const auto& ui = ImGuiApi::Fetch();
    bool openImport = false;
    if (files.empty()) TextUnformatted("未找到旧版数据文件。");
    if (!files.empty() && BeginTable("##Legacy", {"文件", "操作"})) {
        for (const auto& file : files) {
            PushId(file.c_str());
            ui.tableNextColumn();
            Wrapped(file);
            ui.tableNextColumn();
            const bool requestImport = Button("导入", static_cast<bool>(ports_.legacyImport));
            PopId();
            if (requestImport) {
                legacyImportFile_ = file;
                openImport = true;
            }
        }
        ui.endTable();
    }
    if (openImport) ui.openPopup("确认导入旧版数据？", 0);
    const auto file = legacyImportFile_.value_or("");
    const bool ready = ports_.legacyImport && legacyImportFile_ && std::find(files.begin(), files.end(), file) != files.end();
    Confirmation("确认导入旧版数据？", "将从“" + file + "”导入数据。同名数据可能被覆盖。", "确认导入", ready, [&] {
        lastImportResult_ = ports_.legacyImport(file);
        legacyImportFile_.reset();
    }, [this] { legacyImportFile_.reset(); });
    if (!lastImportResult_.empty()) Wrapped(lastImportResult_);
}

void Menu::RenderDiagnostics() {
    if (ports_.status != nullptr) {
        Wrapped(ports_.status());
    }
    Wrapped(UpdateStatusLine(cached_));
    Heading("上次菜单操作");
    Wrapped(MenuMessageText(LastUpdateMessage()));
    std::string counters = "请求处理轮次：" + std::to_string(queue_.DrainCount());
    counters += "，已处理请求数：" + std::to_string(queue_.HandledCount());
    Text(counters);
}

}
