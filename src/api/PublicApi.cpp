#include "slsf/api/PublicApi.h"

#include "slsf/core/location/RegionRegistry.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>

namespace slsf::api {

namespace {

std::string NormalizeName(const std::string& name) {
    std::string out;
    out.reserve(name.size());
    bool pendingSpace = false;
    for (const char ch : name) {
        const auto value = static_cast<unsigned char>(ch);
        if (std::isspace(value) != 0) {
            pendingSpace = !out.empty();
            continue;
        }
        if (pendingSpace) {
            out.push_back(' ');
            pendingSpace = false;
        }
        if (value >= 'A' && value <= 'Z') {
            out.push_back(static_cast<char>(value - 'A' + 'a'));
        } else {
            out.push_back(static_cast<char>(value));
        }
    }
    return out;
}
bool EqualNormalized(const std::string& lhs, const std::string& rhs) {
    return NormalizeName(lhs) == NormalizeName(rhs);
}
compat::ModEventValue BoolValue(bool v) {
    compat::ModEventValue out;
    out.type = compat::ModEventType::Bool;
    out.boolValue = v;
    return out;
}
compat::ModEventValue IntValue(std::int64_t v) {
    compat::ModEventValue out;
    out.type = compat::ModEventType::Int;
    out.intValue = v;
    return out;
}
compat::ModEventValue FloatValue(double v) {
    compat::ModEventValue out;
    out.type = compat::ModEventType::Float;
    out.floatValue = v;
    return out;
}
compat::ModEventValue StringValue(const std::string& v) {
    compat::ModEventValue out;
    out.type = compat::ModEventType::String;
    out.stringValue = v;
    return out;
}
}
PublicApi::PublicApi(Dispatcher& executor, const slsf::RegionSnapshot& regions)
    : executor_(executor), regions_(regions) {}

Revision PublicApi::LastPublishedRevision() const {
    const auto snapshot = Snapshot();
    return snapshot ? snapshot->revision : Revision{};
}
std::optional<RegionId> PublicApi::ResolveRegionByName(const std::string& name) const {
    const auto apiIndex = slsf::ResolveLegacyLocation(regions_, name);
    if (!apiIndex) {
        return std::nullopt;
    }
    const auto& order = regions_.customOrder;
    if (apiIndex->value >= order.size()) {
        return std::nullopt;
    }
    return slsf::FindRegion(regions_, order[apiIndex->value]);
}
std::optional<FameCategory> PublicApi::ResolveCategory(const std::string& name) {
    return slsf::ParseFameCategory(name);
}
void PublicApi::SetFormRegionResolver(FormRegionResolver resolver) {
    std::lock_guard lock(formMutex_);
    formResolver_ = std::move(resolver);
}
std::optional<RegionId> PublicApi::ResolveForm(const slsf::StableFormKey& form) const {
    FormRegionResolver resolver;
    {
        std::lock_guard lock(formMutex_);
        resolver = formResolver_;
    }
    if (!resolver) {
        return std::nullopt;
    }
    return resolver(form);
}
std::optional<RegionId> PublicApi::ResolveParamLocation(const compat::ModEvent& event,
                                                       const std::string& paramName) const {
    if (const auto* form = event.FormValue(paramName)) {
        return ResolveForm(*form);
    }
    return ResolveRegionByName(event.StringValue(paramName));
}
CommandResult PublicApi::SetFameValueRegion(RegionId region, FameCategory category,
                                           double value) {
    ExplicitCommand command;
    command.kind = ExplicitCommandKind::Set;
    command.target = region;
    command.category = category;
    command.value = value;
    return executor_.Execute(command);
}
CommandResult PublicApi::ClearFameRegion(RegionId region, FameCategory category) {
    ExplicitCommand command;
    command.kind = ExplicitCommandKind::Clear;
    command.target = region;
    command.category = category;
    return executor_.Execute(command);
}
CommandResult PublicApi::ClearAllFameRegion(RegionId region) {
    ExplicitCommand command;
    command.kind = ExplicitCommandKind::ClearAll;
    command.target = region;
    return executor_.Execute(command);
}
bool PublicApi::Resolve(const std::string& locationName, const std::string& categoryName,
                       RegionId& region, FameCategory& category) const {
    const auto resolvedRegion = ResolveRegionByName(locationName);
    const auto resolvedCategory = ResolveCategory(categoryName);
    if (!resolvedRegion || !resolvedCategory) {
        return false;
    }
    region = *resolvedRegion;
    category = *resolvedCategory;
    return true;
}
CommandResult PublicApi::SetFameValue(const std::string& locationName,
                                     const std::string& categoryName, int value) {
    RegionId region{};
    FameCategory category{FameCategory::Count};
    CommandResult result;
    if (!Resolve(locationName, categoryName, region, category)) {
        result.status = Status::Invalid;
        result.message = "unresolvable location or category";
        return result;
    }
    return SetFameValueRegion(region, category, static_cast<double>(value));
}
CommandResult PublicApi::ClearFame(const std::string& locationName,
                                  const std::string& categoryName) {
    RegionId region{};
    FameCategory category{FameCategory::Count};
    CommandResult result;
    if (!Resolve(locationName, categoryName, region, category)) {
        result.status = Status::Invalid;
        result.message = "unresolvable location or category";
        return result;
    }
    return ClearFameRegion(region, category);
}
CommandResult PublicApi::ClearAllFame(const std::string& locationName) {
    const auto region = ResolveRegionByName(locationName);
    if (!region) {
        CommandResult result;
        result.status = Status::Invalid;
        result.message = "unresolvable location";
        return result;
    }
    return ClearAllFameRegion(*region);
}
std::optional<int> PublicApi::GetFameValueRegion(RegionId region,
                                                FameCategory category) const {
    const auto snapshot = Snapshot();
    if (!snapshot) {
        return std::nullopt;
    }
    std::size_t row = std::numeric_limits<std::size_t>::max();
    for (std::size_t i = 0; i < snapshot->regionIds.size(); ++i) {
        if (snapshot->regionIds[i] == region) {
            row = i;
            break;
        }
    }
    if (row == std::numeric_limits<std::size_t>::max()) {
        return std::nullopt;
    }
    const auto slot = row * slsf::kFameSlotCount + slsf::FameCategoryIndex(category);
    if (slot >= snapshot->fame.size()) {
        return std::nullopt;
    }
    return static_cast<int>(std::lround(snapshot->fame[slot]));
}
std::optional<int> PublicApi::GetFameValue(const std::string& locationName,
                                          const std::string& categoryName) const {
    RegionId region{};
    FameCategory category{FameCategory::Count};
    if (!Resolve(locationName, categoryName, region, category)) {
        return std::nullopt;
    }
    return GetFameValueRegion(region, category);
}
Dispatcher::GainOutcome PublicApi::GainFame(const std::string& locationName,
                                            const std::string& categoryName,
                                            const GainFameInputs& ruleInputs) {
    RegionId region{};
    FameCategory category{FameCategory::Count};
    Dispatcher::GainOutcome outcome;
    if (!Resolve(locationName, categoryName, region, category)) {
        outcome.status = Status::Invalid;
        outcome.message = "unresolvable location or category";
        return outcome;
    }
    GainFameInputs inputs = ruleInputs;
    if (const auto current = GetFameValueRegion(region, category)) {
        inputs.previousFame = *current;
    }
    return executor_.GainFame(CurrentGameTime(), region, category, inputs);
}
Dispatcher::ApplyOutcome PublicApi::ManualSpread(const std::string& fromName,
                                                 const std::string& toName,
                                                 const std::string& categoryName,
                                                 int percent) {
    Dispatcher::ApplyOutcome outcome;
    const auto category = ResolveCategory(categoryName);
    const auto from = ResolveRegionByName(fromName);
    const auto to = ResolveRegionByName(toName);
    if (!category || !from || !to) {
        outcome.status = Status::Invalid;
        outcome.message = "unresolvable location or category";
        return outcome;
    }
    return executor_.ApplyManualSpread(*from, *to, *category, percent);
}
Dispatcher::ApplyOutcome PublicApi::ManualDecay(const std::string& locationName,
                                                const std::string& categoryName,
                                                int minimum, int maximum) {
    Dispatcher::ApplyOutcome outcome;
    RegionId region{};
    FameCategory category{FameCategory::Count};
    if (!Resolve(locationName, categoryName, region, category)) {
        outcome.status = Status::Invalid;
        outcome.message = "unresolvable location or category";
        return outcome;
    }
    return executor_.ApplyManualDecay(region, category, minimum, maximum);
}
Dispatcher::ApplyOutcome PublicApi::ManualGain(const std::string& locationName,
                                              const std::string& categoryName,
                                              int minimum, int maximum) {
    Dispatcher::ApplyOutcome outcome;
    RegionId region{};
    FameCategory category{FameCategory::Count};
    if (!Resolve(locationName, categoryName, region, category)) {
        outcome.status = Status::Invalid;
        outcome.message = "unresolvable location or category";
        return outcome;
    }
    return executor_.ApplyManualGain(region, category, minimum, maximum);
}
CommandResult PublicApi::SpreadPulse(const std::vector<std::string>& sourceNames) {
    std::vector<RegionId> sources;
    sources.reserve(sourceNames.size());
    for (const auto& name : sourceNames) {
        const auto region = ResolveRegionByName(name);
        if (!region) {
            CommandResult result;
            result.status = Status::Invalid;
            result.message = "unresolvable source location " + name;
            return result;
        }
        sources.push_back(*region);
    }
    return executor_.ApplySpreadPulse(CurrentGameTime(), sources);
}
const std::vector<std::string>& PublicApi::FlagNames() {
    static const std::vector<std::string> names = [] {
        std::vector<std::string> result;
        result.reserve(slsf::kFameCategoryCount + 1);
        for (std::size_t i = 0; i < slsf::kFameCategoryCount; ++i) {
            result.emplace_back(slsf::kFameCategoryNames[i]);
        }
        result.emplace_back("Sex Worker");
        return result;
    }();
    return names;
}
bool PublicApi::IsKnownFlag(const std::string& name) {
    const auto normalized = NormalizeName(name);
    if (normalized == "whore event") {
        return true;
    }
    for (const auto& flag : FlagNames()) {
        if (EqualNormalized(flag, name)) {
            return true;
        }
    }
    return false;
}
std::string PublicApi::NormalizeFlag(const std::string& name) {
    const auto normalized = NormalizeName(name);
    if (normalized == "whore event") {
        return "Sex Worker";
    }
    for (const auto& flag : FlagNames()) {
        if (EqualNormalized(flag, name)) {
            return flag;
        }
    }
    return name;
}
bool PublicApi::RegisterMod(const std::string& modName) {
    if (modName.empty() || modName == "-EMPTY-") {
        return false;
    }
    std::lock_guard lock(modMutex_);
    auto& state = mods_[modName];
    if (state.registered) {
        return false;
    }
    state.registered = true;
    return true;
}
bool PublicApi::UnregisterMod(const std::string& modName) {
    std::lock_guard lock(modMutex_);
    const auto it = mods_.find(modName);
    if (it == mods_.end() || !it->second.registered) {
        return false;
    }
    it->second.registered = false;
    it->second.flags.clear();
    return true;
}
bool PublicApi::IsModRegistered(const std::string& modName) const {
    std::lock_guard lock(modMutex_);
    const auto it = mods_.find(modName);
    return it != mods_.end() && it->second.registered;
}
std::size_t PublicApi::CountExternalMods() const {
    std::lock_guard lock(modMutex_);
    std::size_t count = 0;
    for (const auto& [name, state] : mods_) {
        if (state.registered) {
            ++count;
        }
    }
    return count;
}
void PublicApi::SetExternalFlag(const std::string& modName, const std::string& flagName,
                               bool flag) {
    if (modName.empty()) {
        return;
    }
    const auto canonical = NormalizeFlag(flagName);
    std::lock_guard lock(modMutex_);
    mods_[modName].flags[canonical] = flag;
}
bool PublicApi::GetModFlag(const std::string& modName, const std::string& flagName) const {
    const auto canonical = NormalizeFlag(flagName);
    std::lock_guard lock(modMutex_);
    const auto it = mods_.find(modName);
    if (it == mods_.end()) {
        return false;
    }
    const auto flag = it->second.flags.find(canonical);
    return flag != it->second.flags.end() && flag->second;
}
bool PublicApi::GetAggregatedFlag(const std::string& flagName) const {
    const auto canonical = NormalizeFlag(flagName);
    std::lock_guard lock(modMutex_);
    for (const auto& [name, state] : mods_) {
        if (!state.registered) {
            continue;
        }
        const auto flag = state.flags.find(canonical);
        if (flag != state.flags.end() && flag->second) {
            return true;
        }
    }
    return false;
}
void PublicApi::SetCurrentLocation(const std::string& name) {
    std::lock_guard lock(locationMutex_);
    currentLocation_ = name;
}
std::string PublicApi::CurrentLocation() const {
    std::lock_guard lock(locationMutex_);
    return currentLocation_;
}
void PublicApi::SetStrictLocationResolver(StrictLocationResolver resolver) {
    std::lock_guard lock(strictMutex_);
    strictResolver_ = std::move(resolver);
}
compat::FameValueResolver PublicApi::CommittedResolver() const {
    return [this](const std::string& locationName, FameCategory category) -> int {
        const auto region = ResolveRegionByName(locationName);
        if (!region) {
            return 0;
        }
        const auto value = GetFameValueRegion(*region, category);
        return value ? *value : 0;
    };
}
std::size_t PublicApi::PublishCurrentFameGlobals(compat::LegacyOutputSink& sink) const {
    return outputs_.PublishCurrentFameGlobals(sink, CommittedResolver(), CurrentLocation());
}
std::size_t PublicApi::PublishStorageUtilLists(compat::LegacyOutputSink& sink) const {
    return outputs_.PublishStorageUtilLists(sink, CommittedResolver());
}
std::size_t PublicApi::PublishLegacyBridge(compat::LegacyOutputSink& sink) const {
    return outputs_.PublishLegacyBridge(sink, CommittedResolver(), CurrentLocation());
}
void PublicApi::AttachToBridge(compat::ModEventBridge& bridge) {
    for (const auto& contract : bridge.InputContracts()) {
        bridge.RegisterHandler(contract.event,
                               [this](const compat::ModEvent& event) { OnEvent(event); });
    }
    SetReplySender([&bridge](const std::string& eventName,
                            const std::vector<compat::ModEventValue>& values) {
        compat::ModEvent reply;
        reply.eventName = eventName;
        reply.values = values;
        return bridge.SendReply(reply);
    });
}
void PublicApi::OnEvent(const compat::ModEvent& event) {
    const auto& handler = event.handler;
    if (handler == "OnExternalFameGainRoll" || handler == "OnExternalFameGain") {
        HandleGain(event);
        return;
    }
    if (handler == "OnExternalManualFameGain" ||
        handler == "OnExternalManualFameGainAllInLocation" ||
        handler == "OnExternalManualFameGainAll") {
        HandleManualGain(event);
        return;
    }
    if (handler == "OnExternalFameDecay" || handler == "OnExternalManualFameDecay" ||
        handler == "OnExternalManualFameDecayAllInLocation" ||
        handler == "OnExternalManualFameDecayAll") {
        HandleDecay(event);
        return;
    }
    if (handler == "OnExternalFameSpreadRoll" || handler == "OnExternalFameSpread" ||
        handler == "OnExternalFameSpreadByName" || handler == "OnExternalManualFameSpread") {
        HandleSpread(event);
        return;
    }
    if (handler == "OnExternalLocationRegister" ||
        handler == "OnExternalLocationUnregister") {
        return;
    }
    if (handler == "OnExternalModRegister") {
        (void)RegisterMod(event.StringValue("ModName"));
        return;
    }
    if (handler == "OnExternalModUnregister") {
        (void)UnregisterMod(event.StringValue("ModName"));
        return;
    }
    if (handler == "OnExternalWhoreEventFlag" || handler == "OnExternalSexWorkerFlag") {
        HandleFlag(event, "Sex Worker");
        return;
    }
    if (handler == "OnEnableFameType") {
        HandleEnableFameType(event);
        return;
    }
    if (handler.rfind("OnExternal", 0) == 0 && handler.size() > 4 &&
         handler.compare(handler.size() - 4, 4, "Flag") == 0) {
        if (const auto category = CategoryFromFlagHandler(handler)) {
            HandleFlag(event,
                       std::string(slsf::kFameCategoryNames[static_cast<std::size_t>(*category)]));
            return;
        }
    }
    if (handler == "OnRequestLocation") {
        HandleRequestLocation(event);
        return;
    }
    if (handler == "OnRequestRecognitionTime") {
        HandleRequestRecognitionTime(event);
        return;
    }
    if (handler == "OnRequestFame") {
        HandleRequestFame(event);
        return;
    }
    if (handler == "OnRequestCumVisibility") {
        HandleRequestCumVisibility(event);
        return;
    }
    if (handler == "OnRequestFlagState") {
        HandleRequestFlagState(event);
        return;
    }
    if (handler == "OnRequestModFlagState") {
        HandleRequestModFlagState(event);
        return;
    }
    if (handler == "OnRequestModRegisterState") {
        HandleRequestModRegisterState(event);
        return;
    }
}
std::optional<FameCategory> PublicApi::CategoryFromFlagHandler(const std::string& handler) {
    constexpr const char* kPrefix = "OnExternal";
    constexpr std::size_t kPrefixLen = 12;
    constexpr std::size_t kSuffixLen = 4;
    if (handler.rfind(kPrefix, 0) != 0 || handler.size() <= kPrefixLen + kSuffixLen) {
        return std::nullopt;
    }
    const std::string suffix =
        handler.substr(kPrefixLen, handler.size() - kPrefixLen - kSuffixLen);
    static const std::unordered_map<std::string, FameCategory> table = {
        {"Slut", FameCategory::Slut},
        {"Whore", FameCategory::Whore},
        {"Exhibitionist", FameCategory::Exhibitionist},
        {"Oral", FameCategory::Oral},
        {"Anal", FameCategory::Anal},
        {"Nasty", FameCategory::Nasty},
        {"Pregnant", FameCategory::Pregnant},
        {"Dominant", FameCategory::Dominant},
        {"Submissive", FameCategory::Submissive},
        {"Sadist", FameCategory::Sadist},
        {"Masochist", FameCategory::Masochist},
        {"Gentle", FameCategory::Gentle},
        {"LikesMen", FameCategory::LikesMen},
        {"LikesWomen", FameCategory::LikesWomen},
        {"LikesOrc", FameCategory::LikesOrc},
        {"LikesKhajiit", FameCategory::LikesKhajiit},
        {"LikesArgonian", FameCategory::LikesArgonian},
        {"Bestiality", FameCategory::Bestiality},
        {"Group", FameCategory::Group},
        {"Bound", FameCategory::Bound},
        {"Tattoo", FameCategory::Tattoo},
        {"CumDump", FameCategory::CumDump},
        {"Unfaithful", FameCategory::Unfaithful},
        {"Cuck", FameCategory::Cuck},
        {"Airhead", FameCategory::Airhead},
    };
    const auto it = table.find(suffix);
    if (it == table.end()) {
        return std::nullopt;
    }
    return it->second;
}
void PublicApi::HandleGain(const compat::ModEvent& event) {
    const auto category = ResolveCategory(event.StringValue("Category"));
    const auto region = ResolveParamLocation(event, "EventLocation");
    if (!category || !region) {
        return;
    }
    GainFameInputs inputs;
    if (const auto current = GetFameValueRegion(*region, *category)) {
        inputs.previousFame = *current;
    }
    executor_.GainFame(CurrentGameTime(), *region, *category, inputs);
}
void PublicApi::HandleManualGain(const compat::ModEvent& event) {
    const int min = static_cast<int>(event.IntValue("MinIncrease", 0));
    const int max = static_cast<int>(event.IntValue("MaxIncrease", 0));

    std::optional<RegionId> region;
    if (const auto location = ResolveParamLocation(event, "EventLocation")) {
        region = location;
    } else if (event.handler == "OnExternalManualFameGainAll") {
        region = ResolveRegionByName(CurrentLocation());
    }
    if (!region) {
        return;
    }

    if (event.handler == "OnExternalManualFameGain") {
        const auto category = ResolveCategory(event.StringValue("Category"));
        if (category) {
            executor_.ApplyManualGain(*region, *category, min, max);
        }
        return;
    }
    for (std::size_t i = 0; i < slsf::kFameCategoryCount; ++i) {
        const auto category = slsf::FameCategoryFromIndex(i);
        if (category != slsf::FameCategory::Count) {
            executor_.ApplyManualGain(*region, category, min, max);
        }
    }
}
void PublicApi::HandleDecay(const compat::ModEvent& event) {
    const auto category = ResolveCategory(event.StringValue("Category"));
    const auto region = ResolveParamLocation(event, "EventLocation");
    if (!category || !region) {
        return;
    }
    const int min = static_cast<int>(event.IntValue("MinDecay", 0));
    const int max = static_cast<int>(event.IntValue("MaxDecay", 0));
    executor_.ApplyManualDecay(*region, *category, min, max);
}
void PublicApi::HandleSpread(const compat::ModEvent& event) {
    if (event.handler == "OnExternalManualFameSpread") {
        const auto category = ResolveCategory(event.StringValue("Category"));
        const auto from = ResolveParamLocation(event, "SpreadFromLocation");
        const auto to = ResolveParamLocation(event, "SpreadToLocation");
        if (category && from && to) {
            executor_.ApplyManualSpread(*from, *to, *category,
                                       static_cast<int>(event.IntValue("PercentToSpread", 0)));
        }
        return;
    }
    const auto source = ResolveParamLocation(event, "SpreadFromLocation");
    if (source) {
        std::vector<RegionId> sources{*source};
        executor_.ApplySpreadPulse(CurrentGameTime(), std::span(sources));
    }
}
void PublicApi::HandleFlag(const compat::ModEvent& event, const std::string& flagName) {
    const auto mod = event.StringValue("ModName");
    const bool flag = event.BoolValue("Flag");
    SetExternalFlag(mod, flagName, flag);
}
void PublicApi::HandleEnableFameType(const compat::ModEvent& event) {
    const auto fameType = event.StringValue("FameType");
    const auto mod = event.StringValue("ModName");
    const bool flag = event.BoolValue("Flag");
    SetExternalFlag(mod, NormalizeFlag(fameType), flag);
}
bool PublicApi::SendReply(const std::string& eventName,
                          const std::vector<compat::ModEventValue>& values) const {
    ReplySender sender;
    {
        std::lock_guard lock(replyMutex_);
        sender = replySender_;
    }
    if (!sender) {
        return false;
    }
    return sender(eventName, values);
}
void PublicApi::HandleRequestLocation(const compat::ModEvent& event) {
    const bool strict = event.BoolValue("Strict");
    std::string name;
    if (strict) {
        StrictLocationResolver resolver;
        {
            std::lock_guard lock(strictMutex_);
            resolver = strictResolver_;
        }
        if (resolver) {
            const auto resolved = resolver();
            name = resolved ? *resolved : std::string{};
            SendReply("SLSF_Reloaded_ReturnRequestedLocation", {MakeString(name)});
            return;
        }
    }
    SendReply("SLSF_Reloaded_ReturnRequestedLocation", {MakeString(CurrentLocation())});
}
std::string PublicApi::ResolveParamLocationName(const compat::ModEvent& event,
                                                const char* param) const {
    std::string name = event.StringValue(param);
    if (name == "Current") {
        name = CurrentLocation();
    }
    return name;
}
void PublicApi::HandleRequestRecognitionTime(const compat::ModEvent& event) {
    const auto locationName = ResolveParamLocationName(event, "FameLocation");
    if (!ResolveRegionByName(locationName)) {
        return;
    }
    SendReply("SLSF_Reloaded_ReturnRecognitionTime", {MakeFloat(0.0)});
}
void PublicApi::HandleRequestFame(const compat::ModEvent& event) {
    const auto locationName = ResolveParamLocationName(event, "LocationName");
    const auto categoryName = event.StringValue("Category");
    const auto value = GetFameValue(locationName, categoryName);
    if (!value) {
        return;
    }
    SendReply("SLSF_Reloaded_ReturnRequestedFame",
              {MakeString(locationName), MakeString(categoryName), MakeInt(*value)});
}
void PublicApi::HandleRequestCumVisibility(const compat::ModEvent&) {
    SendReply("SLSF_Reloaded_ReturnRequestedCum", {MakeBool(false)});
}
void PublicApi::HandleRequestFlagState(const compat::ModEvent& event) {
    const auto flagName = event.StringValue("FlagName");
    const bool known = IsKnownFlag(flagName);
    const bool state = known && GetAggregatedFlag(flagName);
    SendReply("SLSF_Reloaded_ReturnFlagState",
              {MakeString(flagName), MakeString(known ? "Yes" : "No"), MakeBool(state)});
}
void PublicApi::HandleRequestModFlagState(const compat::ModEvent& event) {
    const auto modName = event.StringValue("ModName");
    const auto flagName = event.StringValue("FlagName");
    const bool registered = IsModRegistered(modName);
    const bool known = IsKnownFlag(flagName);
    const bool state = registered && known && GetModFlag(modName, flagName);
    SendReply("SLSF_Reloaded_ReturnModFlagState",
              {MakeString(modName), MakeBool(registered), MakeString(flagName),
               MakeString(known ? "Yes" : "No"), MakeBool(state)});
}
void PublicApi::HandleRequestModRegisterState(const compat::ModEvent& event) {
    const auto modName = event.StringValue("ModName");
    SendReply("SLSF_Reloaded_ReturnModRegisteredState",
              {MakeString(modName), MakeBool(IsModRegistered(modName))});
}
compat::ModEventValue PublicApi::MakeBool(bool v) { return BoolValue(v); }
compat::ModEventValue PublicApi::MakeInt(std::int64_t v) { return IntValue(v); }
compat::ModEventValue PublicApi::MakeFloat(double v) { return FloatValue(v); }
compat::ModEventValue PublicApi::MakeString(const std::string& v) { return StringValue(v); }

void PublicApi::SetReplySender(ReplySender sender) {
    std::lock_guard lock(replyMutex_);
    replySender_ = std::move(sender);
}
}