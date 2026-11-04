#include "slsf/config/ConfigurationStore.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <system_error>
#include <utility>
#include <vector>

namespace slsf::config {
namespace {


constexpr std::size_t kJsonConfigMaxBytes = 1u << 20;
constexpr std::size_t kConfigMaxConnections = 1024;
constexpr std::size_t kConfigMaxSelectedRegions = 1024;
constexpr std::uint32_t kConfigurationFirstRevision = 1;
constexpr double kConfigMaxValue = 1e6;

[[nodiscard]] bool IsIntegral(double value) noexcept {
    return std::isfinite(value) && std::fabs(value) <= 9007199254740992.0 &&
           value == std::floor(value);
}

[[nodiscard]] io::JsonPtr Number(double value) { return io::JsonValue::MakeNumber(value); }

void AttachNumber(io::JsonValue& parent, std::string_view name, double value) {
    parent.AttachMember(std::string(name), Number(value));
}

void AttachRounded(io::JsonValue& parent, std::string_view name, double value) {
    parent.AttachMember(std::string(name), Number(std::llround(value)));
}

void AttachBool(io::JsonValue& parent, std::string_view name, bool value) {
    parent.AttachMember(std::string(name), io::JsonValue::MakeBool(value));
}

void AttachString(io::JsonValue& parent, std::string_view name, const std::string& value) {
    parent.AttachMember(std::string(name), io::JsonValue::MakeString(value));
}

void AttachKey(io::JsonValue& parent, std::string_view name, const StableRegionKey& key) {
    auto entry = std::make_shared<io::JsonValue>(io::JsonKind::Object);
    AttachString(*entry, "plugin", key.plugin);
    AttachRounded(*entry, "formId", static_cast<double>(key.localFormId));
    parent.AttachMember(std::string(name), std::move(entry));
}

void AttachNotifications(io::JsonValue& parent, std::string_view name,
                         const NotificationSettings& settings) {
    auto entry = std::make_shared<io::JsonValue>(io::JsonKind::Object);
    AttachBool(*entry, "fameIncrease", settings.fameIncrease);
    AttachBool(*entry, "fameDecay", settings.fameDecay);
    AttachBool(*entry, "fameSpread", settings.fameSpread);
    parent.AttachMember(std::string(name), std::move(entry));
}

[[nodiscard]] std::string CategoryKey(FameCategory category) {
    return io::ToLowerAscii(kFameCategoryNames[FameCategoryIndex(category)]);
}

void AttachMask(io::JsonValue& parent, std::string_view name, CategoryMask mask) {
    auto array = std::make_shared<io::JsonValue>(io::JsonKind::Array);
    mask.ForEach([&array](FameCategory category) {
        auto entry = io::JsonValue::MakeString(CategoryKey(category));
        array->AppendArrayItem(std::move(entry));
    });
    parent.AttachMember(std::string(name), std::move(array));
}

[[nodiscard]] bool ReadNumberField(const io::JsonValue& object, std::string_view name, double& target,
                                   double min, double max, bool integral,
                                   std::string_view groupName, std::string* error) {
    const io::JsonPtr member = object.MemberAt(name);
    if (member == nullptr) {
        return true;
    }
    if (!member->IsNumber() || member->IsBool()) {
        if (error != nullptr) {
            *error = std::string(groupName) + "/" + std::string(name) + " is not a number";
        }
        return false;
    }
    const double value = member->NumberAt(0.0);
    if (!std::isfinite(value)) {
        if (error != nullptr) {
            *error = std::string(groupName) + "/" + std::string(name) + " is not finite";
        }
        return false;
    }
    if (integral && !IsIntegral(value)) {
        if (error != nullptr) {
            *error = std::string(groupName) + "/" + std::string(name) + " must be an integer";
        }
        return false;
    }
    if (value < min || value > max) {
        if (error != nullptr) {
            *error = std::string(groupName) + "/" + std::string(name) + " is out of range";
        }
        return false;
    }
    target = value;
    return true;
}

[[nodiscard]] bool ReadBoolField(const io::JsonValue& object, std::string_view name, bool& target,
                                 std::string_view groupName, std::string* error) {
    const io::JsonPtr member = object.MemberAt(name);
    if (member == nullptr) {
        return true;
    }
    if (!member->IsBool()) {
        if (error != nullptr) {
            *error = std::string(groupName) + "/" + std::string(name) + " is not a boolean";
        }
        return false;
    }
    target = member->BoolAt(false);
    return true;
}

void NormalizeConnections(std::vector<RegionConnectionSetting>& connections) {
    if (connections.empty()) {
        return;
    }
    std::sort(connections.begin(), connections.end(),
              [](const RegionConnectionSetting& lhs,
                 const RegionConnectionSetting& rhs) noexcept {
                  if (const auto fromOrder = lhs.from <=> rhs.from; fromOrder != 0) {
                      return fromOrder < 0;
                  }
                  return lhs.to < rhs.to;
              });
    connections.erase(std::unique(connections.begin(), connections.end()),
                      connections.end());
}

void NormalizeSelectedRegions(std::vector<StableRegionKey>& regions) {
    if (regions.empty()) {
        return;
    }
    std::sort(regions.begin(), regions.end());
    regions.erase(std::unique(regions.begin(), regions.end()), regions.end());
}

[[nodiscard]] bool ReadKeyField(const io::JsonValue& object, std::string_view name,
                                StableRegionKey& key, std::string_view groupName,
                                std::string* error) {
    const io::JsonPtr member = object.MemberAt(name);
    if (member == nullptr) {
        if (error != nullptr) {
            *error = std::string(groupName) + "/" + std::string(name) + " is missing";
        }
        return false;
    }
    if (!member->IsObject()) {
        if (error != nullptr) {
            *error = std::string(groupName) + "/" + std::string(name) + " is not an object";
        }
        return false;
    }
    const io::JsonPtr plugin = member->MemberAt("plugin");
    if (plugin == nullptr || !plugin->IsString() || plugin->StringAt("").empty()) {
        if (error != nullptr) {
            *error = std::string(groupName) + "/" + std::string(name) + "/plugin is invalid";
        }
        return false;
    }
    double formId = 0;
    if (!ReadNumberField(*member, "formId", formId, 0.0, 0xFFFFFF, true,
                         groupName, error)) {
        return false;
    }
    key.plugin = plugin->StringAt("");
    key.localFormId = static_cast<std::uint32_t>(formId);
    return true;
}

[[nodiscard]] bool ReadKeyFields(const io::JsonValue& object, StableRegionKey& key,
                                 std::string_view groupName, std::string* error) {
    const io::JsonPtr plugin = object.MemberAt("plugin");
    if (plugin == nullptr || !plugin->IsString() || plugin->StringAt("").empty()) {
        if (error != nullptr) {
            *error = std::string(groupName) + "/plugin is invalid";
        }
        return false;
    }
    double formId = 0;
    if (!ReadNumberField(object, "formId", formId, 0.0, 0xFFFFFF, true, groupName, error)) {
        return false;
    }
    key.plugin = plugin->StringAt("");
    key.localFormId = static_cast<std::uint32_t>(formId);
    return true;
}

[[nodiscard]] bool ReadMaskField(const io::JsonValue& object, std::string_view name,
                                 CategoryMask& mask, std::string* error) {
    const io::JsonPtr member = object.MemberAt(name);
    if (member == nullptr) {
        return true;
    }
    if (!member->IsArray()) {
        if (error != nullptr) {
            *error = std::string(name) + " must be an array of category names";
        }
        return false;
    }
    for (std::size_t index = 0; index < member->ArraySize(); ++index) {
        const io::JsonPtr entry = member->ArrayAt(index);
        if (entry == nullptr || !entry->IsString()) {
            if (error != nullptr) {
                *error = std::string(name) + "[" + std::to_string(index) + "] is not a string";
            }
            return false;
        }
        const auto category = ParseFameCategory(entry->StringAt(""));
        if (!category.has_value()) {
            if (error != nullptr) {
                *error = std::string(name) + "[" + std::to_string(index) +
                         "] names an unknown category: " + entry->StringAt("");
            }
            return false;
        }
        mask.Set(*category);
    }
    return true;
}

[[nodiscard]] bool ValidateData(const ConfigurationData& data, std::string* error) {
    for (std::size_t index = 0; index < kFameCategoryCount; ++index) {
        const double multiplier = data.categories.multipliers[index];
        if (!std::isfinite(multiplier) || multiplier < 0.0 || multiplier > kConfigMaxValue) {
            if (error != nullptr) {
                *error = std::string("categories/multipliers invalid for category ") +
                         std::string(kFameCategoryNames[index]);
            }
            return false;
        }
    }
    if (!std::isfinite(data.categories.minimumFameToSpread) ||
        data.categories.minimumFameToSpread < 0.0 ||
        data.categories.minimumFameToSpread > kConfigMaxValue) {
        if (error != nullptr) {
            *error = "categories/minimumFameToSpread is out of range";
        }
        return false;
    }
    if (data.categories.maximumSpreadCategories > kFameCategoryCount) {
        if (error != nullptr) {
            *error = "categories/maximumSpreadCategories is out of range";
        }
        return false;
    }
    if (!std::isfinite(data.categories.maximumSpreadPercentage) ||
        data.categories.maximumSpreadPercentage < 0.0 ||
        data.categories.maximumSpreadPercentage > 100.0) {
        if (error != nullptr) {
            *error = "categories/maximumSpreadPercentage is out of range";
        }
        return false;
    }
    const auto validateChance = [](const char* name, double chance, std::string* message) {
        if (!std::isfinite(chance) || chance < 0.0 || chance > 100.0) {
            if (message != nullptr) {
                *message = name;
            }
            return false;
        }
        return true;
    };
    if (!validateChance("relationships/friendBetrayChance is out of range",
                        data.relationships.friendBetrayChance, error) ||
        !validateChance("relationships/loverBetrayChance is out of range",
                        data.relationships.loverBetrayChance, error)) {
        return false;
    }
    const auto validateThreshold = [](const char* name, double threshold, std::string* message) {
        if (!std::isfinite(threshold) || threshold < 0.0 || threshold > kConfigMaxValue) {
            if (message != nullptr) {
                *message = name;
            }
            return false;
        }
        return true;
    };
    if (!validateThreshold("relationships/friendHighFameThreshold is out of range",
                           data.relationships.friendHighFameThreshold, error) ||
        !validateThreshold("relationships/loverHighFameThreshold is out of range",
                           data.relationships.loverHighFameThreshold, error)) {
        return false;
    }
    const auto validateDay = [](double days) {
        return std::isfinite(days) && days >= 0.0 && days <= 3650.0;
    };
    if (!validateDay(data.timing.decayPeriodDays) ||
        !validateDay(data.timing.spreadPeriodDays)) {
        if (error != nullptr) {
            *error = "timing period days are out of range";
        }
        return false;
    }
    if (data.timing.nightStart < 0.0 || data.timing.nightStart >= 24.0 ||
        data.timing.nightEnd < 0.0 || data.timing.nightEnd >= 24.0) {
        if (error != nullptr) {
            *error = "timing night hours are out of range";
        }
        return false;
    }
    if (data.timing.failedSpreadIncrease < 0.0 || data.timing.failedSpreadIncrease > 100.0 ||
        data.timing.successfulSpreadReduction < 0.0 ||
        data.timing.successfulSpreadReduction > 100.0) {
        if (error != nullptr) {
            *error = "timing spread adjustments are out of range";
        }
        return false;
    }
    if (!std::isfinite(data.parameters.maxFame) || data.parameters.maxFame < 1.0 ||
        data.parameters.maxFame > kConfigMaxValue) {
        if (error != nullptr) {
            *error = "parameters/maxFame is out of range";
        }
        return false;
    }
    if (!std::isfinite(data.parameters.decayPerDay) ||
        data.parameters.decayPerDay < 0.0 || data.parameters.decayPerDay > kConfigMaxValue ||
        !std::isfinite(data.parameters.relaxationPerDay) ||
        data.parameters.relaxationPerDay < 0.0 ||
        data.parameters.relaxationPerDay > kConfigMaxValue ||
        !std::isfinite(data.parameters.spreadThreshold) ||
        data.parameters.spreadThreshold < 0.0 ||
        data.parameters.spreadThreshold > kConfigMaxValue ||
        !std::isfinite(data.parameters.spreadRetention) ||
        data.parameters.spreadRetention < 0.0 || data.parameters.spreadRetention > 1.0) {
        if (error != nullptr) {
            *error = "parameters values are out of range";
        }
        return false;
    }
    if (data.connections.size() > kConfigMaxConnections) {
        if (error != nullptr) {
            *error = "connections exceed limits";
        } return false;
    }
    for (const auto& connection : data.connections) {
        if (connection.from.plugin.empty() || connection.to.plugin.empty()) {
            if (error != nullptr) {
                *error = "connections entry has an empty plugin";
            }
            return false;
        }
        if (connection.from.localFormId > 0xFFFFFF || connection.to.localFormId > 0xFFFFFF) {
            if (error != nullptr) {
                *error = "connections entry has an out-of-range formId";
            }
            return false;
        }
        if (!(connection.from < connection.to)) {
            if (error != nullptr) {
                *error = "connections entries must be ordered from < to";
            }
            return false;
        }
    }
    if (data.menu.selectedRegions.size() > kConfigMaxSelectedRegions) {
        if (error != nullptr) {
            *error = "menu selectedRegions exceed limits";
        }
        return false;
    }
    for (std::size_t index = 0; index < data.menu.selectedRegions.size(); ++index) {
        const auto& key = data.menu.selectedRegions[index];
        if (key.plugin.empty() || key.localFormId > 0xFFFFFF) {
            if (error != nullptr) {
                *error = "menu selectedRegions[" + std::to_string(index) + "] is invalid";
            }
            return false;
        }
    }
    if (std::adjacent_find(data.menu.selectedRegions.begin(),
                           data.menu.selectedRegions.end()) !=
        data.menu.selectedRegions.end()) {
        if (error != nullptr) {
            *error = "menu selectedRegions contain duplicates";
        }
        return false;
    }
    return true;
}

[[nodiscard]] bool DecodeConnections(const io::JsonPtr& member,
                                     std::vector<RegionConnectionSetting>& connections,
                                     std::string* error) {
    if (member == nullptr) {
        return true;
    }
    if (!member->IsArray()) {
        if (error != nullptr) {
            *error = "connections is not an array";
        }
        return false;
    }
    if (member->ArraySize() > kConfigMaxConnections) {
        if (error != nullptr) {
            *error = "connections exceed limits";
        }
        return false;
    }
    for (std::size_t index = 0; index < member->ArraySize(); ++index) {
        const io::JsonPtr entry = member->ArrayAt(index);
        if (entry == nullptr || !entry->IsObject()) {
            if (error != nullptr) {
                *error = "connections[" + std::to_string(index) + "] is not an object";
            }
            return false;
        }
        RegionConnectionSetting connection;
        std::string entryGroup = "connections[" + std::to_string(index) + "]";
        if (!ReadKeyField(*entry, "from", connection.from, entryGroup, error) ||
            !ReadKeyField(*entry, "to", connection.to, entryGroup, error) ||
            !ReadBoolField(*entry, "enabled", connection.enabled, entryGroup, error)) {
            return false;
        }
        connections.push_back(std::move(connection));
    }
    NormalizeConnections(connections);
    return true;
}

[[nodiscard]] bool DecodeSelectedRegions(const io::JsonPtr& member,
                                         std::vector<StableRegionKey>& selected,
                                         std::string* error) {
    if (member == nullptr) {
        return true;
    }
    if (!member->IsArray()) {
        if (error != nullptr) {
            *error = "menu/selectedRegions is not an array";
        }
        return false;
    }
    if (member->ArraySize() > kConfigMaxSelectedRegions) {
        if (error != nullptr) {
            *error = "menu/selectedRegions exceeds limits";
        }
        return false;
    }
    for (std::size_t index = 0; index < member->ArraySize(); ++index) {
        const io::JsonPtr entry = member->ArrayAt(index);
        StableRegionKey key;
        const std::string entryGroup = "menu/selectedRegions[" + std::to_string(index) + "]";
        if (entry == nullptr || !entry->IsObject()) {
            if (error != nullptr) {
                *error = entryGroup + " is not an object";
            }
            return false;
        }
        if (!ReadKeyFields(*entry, key, entryGroup, error)) {
            return false;
        }
        selected.push_back(std::move(key));
    }
    NormalizeSelectedRegions(selected);
    return true;
}

} // namespace

ConfigurationStore::ConfigurationStore(std::unique_ptr<ConfigurationFile> file)
    : file_(std::move(file)) {}

std::unique_ptr<ConfigurationStore> ConfigurationStore::Open(
    std::unique_ptr<ConfigurationFile> file) {
    if (file == nullptr) {
        return nullptr;
    }
    return std::unique_ptr<ConfigurationStore>(new ConfigurationStore(std::move(file)));
}

ConfigurationStore::~ConfigurationStore() = default;

void ConfigurationStore::BindNotify(ConfigurationNotify notify) {
    std::lock_guard lock(mutex_);
    notify_ = std::move(notify);
}

ConfigurationData ConfigurationStore::LegacyDefaults() {
    ConfigurationData data;
    data.schemaVersion = kConfigurationSchemaVersion;
    data.categories.minimumFameToSpread = 30.0;
    data.categories.maximumSpreadCategories = 5;
    data.categories.maximumSpreadPercentage = 30.0;
    data.relationships.friendHighFameThreshold = 0.0;
    data.relationships.loverHighFameThreshold = 50.0;
    data.relationships.friendBetrayChance = 50.0;
    data.relationships.loverBetrayChance = 25.0;
    data.timing.decayPeriodDays = 0.5;
    data.timing.spreadPeriodDays = 1.0;
    data.timing.nightStart = 22.0;
    data.timing.nightEnd = 6.0;
    data.timing.failedSpreadIncrease = 10.0;
    data.timing.successfulSpreadReduction = 10.0;
    data.parameters = Parameters{2.0, 1.0, 30.0, 0.6, 150.0};
    return data;
}

io::JsonPtr ConfigurationStore::Encode(const ConfigurationData& data) {
    auto root = std::make_shared<io::JsonValue>(io::JsonKind::Object);
    AttachRounded(*root, "schemaVersion", static_cast<double>(kConfigurationSchemaVersion));

    auto features = std::make_shared<io::JsonValue>(io::JsonKind::Object);
    AttachBool(*features, "reduceFameAtNight", data.features.reduceFameAtNight);
    AttachBool(*features, "fameSpreadRestrictions", data.features.fameSpreadRestrictions);
    AttachBool(*features, "dynamicAnonymity", data.features.dynamicAnonymity);
    root->AttachMember("features", std::move(features));

    auto categories = std::make_shared<io::JsonValue>(io::JsonKind::Object);
    auto multipliers = std::make_shared<io::JsonValue>(io::JsonKind::Object);
    for (std::size_t index = 0; index < kFameCategoryCount; ++index) {
        auto entry = io::JsonValue::MakeNumber(data.categories.multipliers[index]);
        multipliers->AttachMember(CategoryKey(FameCategoryFromIndex(index)), std::move(entry));
    }
    categories->AttachMember("multipliers", std::move(multipliers));
    AttachRounded(*categories, "minimumFameToSpread", data.categories.minimumFameToSpread);
    AttachRounded(*categories, "maximumSpreadCategories",
                  static_cast<double>(data.categories.maximumSpreadCategories));
    AttachRounded(*categories, "maximumSpreadPercentage",
                  data.categories.maximumSpreadPercentage);
    root->AttachMember("categories", std::move(categories));

    auto relationships = std::make_shared<io::JsonValue>(io::JsonKind::Object);
    AttachMask(*relationships, "forbiddenByFriend", data.relationships.forbiddenByFriend);
    AttachMask(*relationships, "forbiddenByLover", data.relationships.forbiddenByLover);
    AttachRounded(*relationships, "friendHighFameThreshold",
                  data.relationships.friendHighFameThreshold);
    AttachRounded(*relationships, "loverHighFameThreshold",
                  data.relationships.loverHighFameThreshold);
    AttachRounded(*relationships, "friendBetrayChance", data.relationships.friendBetrayChance);
    AttachRounded(*relationships, "loverBetrayChance", data.relationships.loverBetrayChance);
    root->AttachMember("relationships", std::move(relationships));

    auto timing = std::make_shared<io::JsonValue>(io::JsonKind::Object);
    AttachNumber(*timing, "decayPeriodDays", data.timing.decayPeriodDays);
    AttachNumber(*timing, "spreadPeriodDays", data.timing.spreadPeriodDays);
    AttachNumber(*timing, "nightStart", data.timing.nightStart);
    AttachNumber(*timing, "nightEnd", data.timing.nightEnd);
    AttachRounded(*timing, "failedSpreadIncrease", data.timing.failedSpreadIncrease);
    AttachRounded(*timing, "successfulSpreadReduction",
                  data.timing.successfulSpreadReduction);
    root->AttachMember("timing", std::move(timing));

    auto parameters = std::make_shared<io::JsonValue>(io::JsonKind::Object);
    AttachNumber(*parameters, "decayPerDay", data.parameters.decayPerDay);
    AttachNumber(*parameters, "relaxationPerDay", data.parameters.relaxationPerDay);
    AttachNumber(*parameters, "spreadThreshold", data.parameters.spreadThreshold);
    AttachNumber(*parameters, "spreadRetention", data.parameters.spreadRetention);
    AttachNumber(*parameters, "maxFame", data.parameters.maxFame);
    root->AttachMember("parameters", std::move(parameters));

    auto connections = std::make_shared<io::JsonValue>(io::JsonKind::Array);
    for (const auto& connection : data.connections) {
        auto entry = std::make_shared<io::JsonValue>(io::JsonKind::Object);
        AttachKey(*entry, "from", connection.from);
        AttachKey(*entry, "to", connection.to);
        AttachBool(*entry, "enabled", connection.enabled);
        connections->AppendArrayItem(std::move(entry));
    }
    root->AttachMember("connections", std::move(connections));

    auto menu = std::make_shared<io::JsonValue>(io::JsonKind::Object);
    AttachNotifications(*menu, "notifications", data.menu.notifications);
    auto selected = std::make_shared<io::JsonValue>(io::JsonKind::Array);
    for (const auto& key : data.menu.selectedRegions) {
        auto entry = std::make_shared<io::JsonValue>(io::JsonKind::Object);
        AttachString(*entry, "plugin", key.plugin);
        AttachRounded(*entry, "formId", static_cast<double>(key.localFormId));
        selected->AppendArrayItem(std::move(entry));
    }
    menu->AttachMember("selectedRegions", std::move(selected));
    root->AttachMember("menu", std::move(menu));

    return root;
}

Result<ConfigurationData> ConfigurationStore::Decode(const io::JsonPtr& root,
                                                           const ConfigurationData& base) {
    if (root == nullptr) {
        return std::unexpected{Error{Error::Code::InvalidArgument,
                                           "document is null"}};
    }
    if (!root->IsObject()) {
        return std::unexpected{Error{Error::Code::InvalidArgument,
                                           "root is not an object"}};
    }
    ConfigurationData data = base;

    double schemaVersion = static_cast<double>(data.schemaVersion);
    std::string error;
    if (!ReadNumberField(*root, "schemaVersion", schemaVersion, 0.0, 65535.0, true, "root",
                         &error)) {
        return std::unexpected{Error{Error::Code::InvalidArgument, error}};
    }
    if (static_cast<std::uint32_t>(schemaVersion) != kConfigurationSchemaVersion) {
        return std::unexpected{
            Error{Error::Code::Unsupported, "unsupported schema version " +
                                                std::to_string(schemaVersion)}};
    }

    const io::JsonPtr features = root->MemberAt("features");
    if (features != nullptr) {
        if (!features->IsObject()) {
            return std::unexpected{Error{Error::Code::InvalidArgument,
                                         "features is not an object"}};
        }
        if (!ReadBoolField(*features, "reduceFameAtNight", data.features.reduceFameAtNight,
                           "features", &error) ||
            !ReadBoolField(*features, "fameSpreadRestrictions",
                           data.features.fameSpreadRestrictions, "features", &error) ||
            !ReadBoolField(*features, "dynamicAnonymity", data.features.dynamicAnonymity,
                           "features", &error)) {
            return std::unexpected{Error{Error::Code::InvalidArgument, error}};
        }
    }

    const io::JsonPtr categories = root->MemberAt("categories");
    if (categories != nullptr) {
        if (!categories->IsObject()) {
            return std::unexpected{Error{Error::Code::InvalidArgument,
                                         "categories is not an object"}};
        }
        const io::JsonPtr multipliers = categories->MemberAt("multipliers");
        if (multipliers != nullptr) {
            if (!multipliers->IsObject()) {
                return std::unexpected{Error{Error::Code::InvalidArgument,
                                             "multipliers is not an object"}};
            }
            for (const auto& entry : multipliers->ObjectMembers()) {
                double value = 0;
                if (!ReadNumberField(*multipliers, entry.first, value, 0.0, kConfigMaxValue,
                                     false, "categories/multipliers", &error)) {
                    return std::unexpected{Error{Error::Code::InvalidArgument, error}};
                }
                const auto category = ParseFameCategory(entry.first);
                if (!category.has_value()) {
                    return std::unexpected{
                        Error{Error::Code::InvalidArgument,
                              "unknown multiplier category '" + entry.first + "'"}};
                }
                data.categories.multipliers[FameCategoryIndex(*category)] = value;
            }
        }
        double maximumSpreadCategories = static_cast<double>(data.categories.maximumSpreadCategories);
        if (!ReadNumberField(*categories, "minimumFameToSpread",
                             data.categories.minimumFameToSpread, 0.0, kConfigMaxValue, false,
                             "categories", &error) ||
            !ReadNumberField(*categories, "maximumSpreadCategories", maximumSpreadCategories,
                             0.0, kFameCategoryCount, true, "categories", &error) ||
            !ReadNumberField(*categories, "maximumSpreadPercentage",
                             data.categories.maximumSpreadPercentage, 0.0, 100.0, false,
                             "categories", &error)) {
            return std::unexpected{Error{Error::Code::InvalidArgument, error}};
        }
        data.categories.maximumSpreadCategories =
            static_cast<std::uint32_t>(maximumSpreadCategories);
    }

    const io::JsonPtr relationships = root->MemberAt("relationships");
    if (relationships != nullptr) {
        if (!relationships->IsObject()) {
            return std::unexpected{Error{Error::Code::InvalidArgument,
                                         "relationships is not an object"}};
        }
        if (!ReadMaskField(*relationships, "forbiddenByFriend",
                           data.relationships.forbiddenByFriend, &error) ||
            !ReadMaskField(*relationships, "forbiddenByLover",
                           data.relationships.forbiddenByLover, &error) ||
            !ReadNumberField(*relationships, "friendHighFameThreshold",
                             data.relationships.friendHighFameThreshold, 0.0, kConfigMaxValue,
                             false, "relationships", &error) ||
            !ReadNumberField(*relationships, "loverHighFameThreshold",
                             data.relationships.loverHighFameThreshold, 0.0, kConfigMaxValue,
                             false, "relationships", &error) ||
            !ReadNumberField(*relationships, "friendBetrayChance",
                             data.relationships.friendBetrayChance, 0.0, 100.0, false,
                             "relationships", &error) ||
            !ReadNumberField(*relationships, "loverBetrayChance",
                             data.relationships.loverBetrayChance, 0.0, 100.0, false,
                             "relationships", &error)) {
            return std::unexpected{Error{Error::Code::InvalidArgument, error}};
        }
    }

    const io::JsonPtr timing = root->MemberAt("timing");
    if (timing != nullptr) {
        if (!timing->IsObject()) {
            return std::unexpected{Error{Error::Code::InvalidArgument,
                                         "timing is not an object"}};
        }
        if (!ReadNumberField(*timing, "decayPeriodDays", data.timing.decayPeriodDays, 0.0,
                             3650.0, false, "timing", &error) ||
            !ReadNumberField(*timing, "spreadPeriodDays", data.timing.spreadPeriodDays, 0.0,
                             3650.0, false, "timing", &error) ||
            !ReadNumberField(*timing, "nightStart", data.timing.nightStart, 0.0, 24.0, false,
                             "timing", &error) ||
            !ReadNumberField(*timing, "nightEnd", data.timing.nightEnd, 0.0, 24.0, false,
                             "timing", &error) ||
            !ReadNumberField(*timing, "failedSpreadIncrease",
                             data.timing.failedSpreadIncrease, 0.0, 100.0, false, "timing",
                             &error) ||
            !ReadNumberField(*timing, "successfulSpreadReduction",
                             data.timing.successfulSpreadReduction, 0.0, 100.0, false,
                             "timing", &error)) {
            return std::unexpected{Error{Error::Code::InvalidArgument, error}};
        }
    }

    const io::JsonPtr parameters = root->MemberAt("parameters");
    if (parameters != nullptr) {
        if (!parameters->IsObject()) {
            return std::unexpected{Error{Error::Code::InvalidArgument,
                                         "parameters is not an object"}};
        }
        if (!ReadNumberField(*parameters, "decayPerDay", data.parameters.decayPerDay, 0.0,
                             kConfigMaxValue, false, "parameters", &error) ||
            !ReadNumberField(*parameters, "relaxationPerDay", data.parameters.relaxationPerDay,
                             0.0, kConfigMaxValue, false, "parameters", &error) ||
            !ReadNumberField(*parameters, "spreadThreshold", data.parameters.spreadThreshold,
                             0.0, kConfigMaxValue, false, "parameters", &error) ||
            !ReadNumberField(*parameters, "spreadRetention", data.parameters.spreadRetention,
                             0.0, 1.0, false, "parameters", &error) ||
            !ReadNumberField(*parameters, "maxFame", data.parameters.maxFame, 1.0,
                             kConfigMaxValue, false, "parameters", &error)) {
            return std::unexpected{Error{Error::Code::InvalidArgument, error}};
        }
    }

    error.clear();
    if (!DecodeConnections(root->MemberAt("connections"), data.connections, &error)) {
        return std::unexpected{Error{Error::Code::InvalidArgument, error}};
    }

    const io::JsonPtr menu = root->MemberAt("menu");
    if (menu != nullptr) {
        if (!menu->IsObject()) {
            return std::unexpected{Error{Error::Code::InvalidArgument, "menu is not an object"}};
        }
        const io::JsonPtr menuNotifications = menu->MemberAt("notifications");
        if (menuNotifications != nullptr) {
            if (!menuNotifications->IsObject()) {
                return std::unexpected{Error{Error::Code::InvalidArgument,
                                             "menu/notifications is not an object"}};
            }
            if (!ReadBoolField(*menuNotifications, "fameIncrease",
                               data.menu.notifications.fameIncrease, "menu/notifications",
                               &error) ||
                !ReadBoolField(*menuNotifications, "fameDecay",
                               data.menu.notifications.fameDecay, "menu/notifications",
                               &error) ||
                !ReadBoolField(*menuNotifications, "fameSpread",
                               data.menu.notifications.fameSpread, "menu/notifications",
                               &error)) {
                return std::unexpected{Error{Error::Code::InvalidArgument, error}};
            }
        }
        if (!DecodeSelectedRegions(menu->MemberAt("selectedRegions"),
                                   data.menu.selectedRegions, &error)) {
            return std::unexpected{Error{Error::Code::InvalidArgument, error}};
        }
    }

    if (!ValidateData(data, &error)) {
        return std::unexpected{Error{Error::Code::InvalidArgument, error}};
    }
    return data;
}

std::string ConfigurationStore::Validate(const ConfigurationData& data) {
    std::string error;
    if (!ValidateData(data, &error)) {
        return error;
    }
    return {};
}

ConfigurationSnapshot ConfigurationStore::Current() const {
    std::lock_guard lock(mutex_);
    return snapshot_;
}

ConfigurationUpdateResult ConfigurationStore::Reload() {
    ConfigurationSnapshot notify;
    ConfigurationUpdateResult result;
    {
        std::lock_guard lock(mutex_);
        result = ReloadLocked(&notify);
    }
    EmitNotification(notify);
    return result;
}

ConfigurationUpdateResult ConfigurationStore::ApplyEdit(ConfigurationData edit) {
    ConfigurationSnapshot notify;
    ConfigurationUpdateResult result;
    {
        std::lock_guard lock(mutex_);
        NormalizeConnections(edit.connections);
        NormalizeSelectedRegions(edit.menu.selectedRegions);

        std::string error;
        if (!ValidateData(edit, &error)) {
            result = FinishWithCurrentLocked(ConfigurationUpdateStatus::Invalid,
                                             "configuration edit rejected: " + error);
        } else {
            const std::string text = io::JsonValue::Write(*Encode(edit), 2);
            const auto written = file_->Write(text);
            if (!written.has_value()) {
                result = FinishWithCurrentLocked(ConfigurationUpdateStatus::IoError,
                                                 "configuration write failed: " +
                                                     written.error().message);
            } else {
                result = ReloadLocked(&notify);
            }
        }
    }
    EmitNotification(notify);
    return result;
}

std::string UpdateMessage(ConfigurationUpdateResult result) {
    return result.message;
}

void ConfigurationStore::EmitNotification(const ConfigurationSnapshot& snapshot) {
    if (snapshot.revision.value == 0) {
        return;
    }
    ConfigurationNotify notify;
    {
        std::lock_guard lock(mutex_);
        notify = notify_;
    }
    if (notify != nullptr) {
        ++notifyCount_;
        notify(snapshot);
    }
}

ConfigurationUpdateResult ConfigurationStore::FinishWithCurrentLocked(
    ConfigurationUpdateStatus status, std::string message) {
    snapshot_.lastUpdate =
        ConfigurationUpdateResult{status, snapshot_.revision, std::move(message)};
    return snapshot_.lastUpdate;
}

ConfigurationUpdateResult ConfigurationStore::ReloadLocked(ConfigurationSnapshot* notify) {
    if (!file_->Exists()) {
        if (!synced_) {
            auto data = std::make_shared<const ConfigurationData>(LegacyDefaults());
            const std::string text = io::JsonValue::Write(*Encode(*data), 2);
            const auto written = file_->Write(text);
            ConfigurationUpdateStatus status = ConfigurationUpdateStatus::Applied;
            std::string message;
            if (written.has_value()) {
                message = "configuration initialized from defaults";
            } else {
                status = ConfigurationUpdateStatus::IoError;
                message = "configuration file missing and could not be generated (" +
                          written.error().message + ")";
            }
            lastText_ = text;
            synced_ = true;
            snapshot_ = ConfigurationSnapshot{
                ConfigurationRevision{kConfigurationFirstRevision}, std::move(data),
                ConfigurationUpdateResult{status, ConfigurationRevision{kConfigurationFirstRevision},
                                          std::move(message)}};
            if (notify != nullptr) {
                *notify = snapshot_;
            }
            return snapshot_.lastUpdate;
        }
        return FinishWithCurrentLocked(ConfigurationUpdateStatus::IoError,
                                       "configuration file is missing");
    }

    const auto read = file_->Read();
    if (!read.has_value()) {
        return FinishWithCurrentLocked(ConfigurationUpdateStatus::IoError,
                                       "configuration file read failed: " +
                                           read.error().message);
    }

    std::string parseError;
    const io::JsonPtr parsed = io::JsonValue::Parse(*read, kJsonConfigMaxBytes, &parseError);
    if (parsed == nullptr) {
        return FinishWithCurrentLocked(ConfigurationUpdateStatus::Invalid,
                                       "configuration JSON is invalid: " + parseError);
    }

    const ConfigurationData base =
        synced_ && snapshot_.data != nullptr ? *snapshot_.data : LegacyDefaults();
    auto decoded = Decode(parsed, base);
    if (!decoded.has_value()) {
        return FinishWithCurrentLocked(ConfigurationUpdateStatus::Invalid,
                                       "configuration invalid: " + decoded.error().message);
    }

    const std::string canonical = io::JsonValue::Write(*Encode(*decoded), 2);
    const bool changed = !synced_ || canonical != lastText_;
    const std::uint64_t revision =
        changed ? (synced_ ? snapshot_.revision.value + 1 : kConfigurationFirstRevision)
                : snapshot_.revision.value;

    lastText_ = canonical;
    synced_ = true;
    auto data = std::make_shared<const ConfigurationData>(std::move(*decoded));
    snapshot_ = ConfigurationSnapshot{
        ConfigurationRevision{revision}, std::move(data),
        ConfigurationUpdateResult{
            changed ? ConfigurationUpdateStatus::Applied
                    : ConfigurationUpdateStatus::Unchanged,
            ConfigurationRevision{revision},
            changed ? "configuration updated" : "configuration unchanged"}};
    if (changed && notify != nullptr) {
        *notify = snapshot_;
    }
    return snapshot_.lastUpdate;
}

DiskConfigurationFile::DiskConfigurationFile(std::string path) : path_(std::move(path)) {}

bool DiskConfigurationFile::Exists() {
    std::error_code ec;
    return std::filesystem::exists(path_, ec);
}

Result<std::string> DiskConfigurationFile::Read() {
    std::ifstream file(path_, std::ios::binary);
    if (!file) {
        return std::unexpected{Error{Error::Code::Io, "cannot open file"}};
    }
    const std::string bytes((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
    if (file.bad()) {
        return std::unexpected{Error{Error::Code::Io, "read failed mid-stream"}};
    }
    return bytes;
}

Result<std::monostate> DiskConfigurationFile::Write(const std::string& bytes) {
    const std::filesystem::path target(path_);
    std::filesystem::path temp = target;
    temp += ".tmp";
    try {
        std::ofstream file(temp, std::ios::binary | std::ios::trunc);
        if (!file) {
            return std::unexpected{Error{Error::Code::Io, "cannot create temp file"}};
        }
        file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        file.flush();
        if (!file) {
            file.close();
            std::error_code cleanup;
            std::filesystem::remove(temp, cleanup);
            return std::unexpected{Error{Error::Code::Io, "temp file write failed"}};
        }
        file.close();
        std::error_code ec;
        std::filesystem::rename(temp, target, ec);
        if (ec) {
            std::error_code cleanup;
            std::filesystem::remove(temp, cleanup);
            return std::unexpected{Error{Error::Code::Io, "replace failed: " + ec.message()}};
        }
    } catch (const std::filesystem::filesystem_error& error) {
        return std::unexpected{Error{Error::Code::Io, error.what()}};
    }
    return std::monostate{};
}

MemoryConfigurationFile::MemoryConfigurationFile(std::string initialBytes)
    : bytes_(std::move(initialBytes)), failNextWrite_(false) {}

bool MemoryConfigurationFile::Exists() {
    return !bytes_.empty();
}

Result<std::string> MemoryConfigurationFile::Read() {
    if (bytes_.empty()) {
        return std::unexpected{Error{Error::Code::Io, "no bytes stored"}};
    }
    return bytes_;
}

Result<std::monostate> MemoryConfigurationFile::Write(const std::string& bytes) {
    if (failNextWrite_) {
        failNextWrite_ = false;
        return std::unexpected{Error{Error::Code::Io, "injected write failure"}};
    }
    bytes_.assign(bytes);
    return std::monostate{};
}

} // namespace slsf::config
