#include "slsf/api/PublicApi.h"

#include "slsf/core/location/RegionRegistry.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <charconv>
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

std::optional<std::size_t> FlagIndex(const std::string& name) {
    const auto canonical = PublicApi::NormalizeFlag(name);
    const auto& names = PublicApi::FlagNames();
    const auto found = std::find(names.begin(), names.end(), canonical);
    if (found == names.end()) return std::nullopt;
    return static_cast<std::size_t>(found - names.begin());
}

std::uint64_t RegistrationId(std::string_view name) {
    std::uint64_t hash = 14695981039346656037ull;
    for (const auto ch : name) {
        hash ^= static_cast<unsigned char>(ch);
        hash *= 1099511628211ull;
    }
    return hash == 0 ? 1 : hash;
}

void SyncProvider(State& state, const NamedProviderState& provider) {
    ProviderCommand command;
    command.registrationId = RegistrationId(provider.name);
    command.phase = ProviderPhase::Unregistered;
    (void)UpdateProvider(state.providers, command);
    command.phase = ProviderPhase::Enabled;
    for (std::size_t i = 0; i < kFameCategoryCount; ++i) {
        if ((provider.flags & (1u << i)) != 0) command.categories.push_back(static_cast<FameCategory>(i));
    }
    (void)UpdateProvider(state.providers, command);
}
}
PublicApi::PublicApi(Dispatcher& executor, const slsf::RegionSnapshot& regions)
    : executor_(executor), regions_(regions) {}

Revision PublicApi::LastPublishedRevision() const {
    const auto snapshot = Snapshot();
    return snapshot ? snapshot->revision : Revision{};
}
std::optional<RegionId> PublicApi::ResolveRegionByName(const std::string& name) const {
    return ResolveRegionByName(name, Snapshot());
}
std::optional<RegionId> PublicApi::ResolveRegionByName(
    const std::string& name, const PublishedStatePtr& snapshot) const {
    if (snapshot) {
        for (const auto& custom : snapshot->customRegions) {
            if (EqualNormalized(custom.name, name)) return FindRegion(regions_, custom.key);
        }
    }
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
RegionalFameView PublicApi::ReadRegionalFame(const std::string& location) const {
    if (!BusinessOpen() || executor_.Suspended()) return {};
    const auto snapshot = Snapshot();
    if (!snapshot) return {};
    RegionalFameView view;
    view.epoch = snapshot->epoch;
    view.revision = snapshot->revision;
    view.currentLocation = CurrentLocation();
    view.location = location.empty() ? view.currentLocation : location;
    for (std::size_t i = 0; i < compat::LegacyOutputs::kLegacyLocationIndexCount; ++i) {
        const auto name = compat::LegacyOutputs::LegacyLocationName(i);
        if (name && ResolveRegionByName(*name, snapshot)) view.locations.push_back(*name);
    }
    for (const auto& custom : snapshot->customRegions) view.locations.push_back(custom.name);
    if (const auto region = ResolveRegionByName(view.location, snapshot)) {
        const auto row = std::find(snapshot->regionIds.begin(), snapshot->regionIds.end(), *region);
        if (row != snapshot->regionIds.end()) {
            const auto offset = static_cast<std::size_t>(row - snapshot->regionIds.begin()) * kFameSlotCount;
            if (offset + kFameCategoryCount <= snapshot->fame.size()) {
                view.values.emplace();
                for (std::size_t i = 0; i < kFameCategoryCount; ++i) {
                    (*view.values)[i] = static_cast<int>(std::lround(snapshot->fame[offset + i]));
                }
            }
        }
    }
    if (!BusinessOpen() || executor_.Suspended() || CurrentEpoch() != snapshot->epoch) return {};
    return view;
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
        result.reserve(slsf::kFameCategoryCount + 5);
        for (std::size_t i = 0; i < slsf::kFameCategoryCount; ++i) {
            result.emplace_back(slsf::kFameCategoryNames[i]);
        }
        result.emplace_back("Sex Worker");
        for (const auto* name : {"EnableCumDump", "EnableCuck", "EnableUnfaithful", "EnableAirhead"}) result.emplace_back(name);
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
    if (modName.empty() || modName == "-EMPTY-" || IsModRegistered(modName)) return false;
    return executor_.EditState([&](State& state) -> Result<void> {
        if (std::any_of(state.namedProviders.begin(), state.namedProviders.end(), [&](const auto& entry) { return EqualNormalized(entry.name, modName); })) return Fail(Error::Code::InvalidArgument, "mod is already registered");
        if (state.namedProviders.size() >= kMaxProviderRegistrations) return Fail(Error::Code::OutOfRange, "mod registry is full");
        state.namedProviders.push_back({modName, 0});
        return {};
    }).status == Status::Ready;
}
bool PublicApi::UnregisterMod(const std::string& modName) {
    if (!IsModRegistered(modName)) return false;
    return executor_.EditState([&](State& state) -> Result<void> {
        for (const auto& provider : state.namedProviders) {
            if (!EqualNormalized(provider.name, modName)) continue;
            ProviderCommand command{RegistrationId(provider.name), ProviderPhase::Unregistered, {}};
            (void)UpdateProvider(state.providers, command);
        }
        std::erase_if(state.namedProviders, [&](const auto& provider) { return EqualNormalized(provider.name, modName); });
        RebuildDerivedSets(state);
        return {};
    }).status == Status::Ready;
}
bool PublicApi::IsModRegistered(const std::string& modName) const {
    const auto names = RegisteredMods();
    return std::any_of(names.begin(), names.end(), [&](const auto& name) { return EqualNormalized(name, modName); });
}
std::size_t PublicApi::CountExternalMods() const {
    const auto snapshot = Snapshot();
    return snapshot ? snapshot->namedProviders.size() : 0;
}
void PublicApi::SetExternalFlag(const std::string& modName, const std::string& flagName,
                               bool flag) {
    const auto index = FlagIndex(flagName);
    if (!index || !IsModRegistered(modName)) return;
    (void)executor_.EditState([&](State& state) -> Result<void> {
        for (auto& provider : state.namedProviders) {
            if (!EqualNormalized(provider.name, modName)) continue;
            if (flag) provider.flags |= 1u << *index;
            else provider.flags &= ~(1u << *index);
            SyncProvider(state, provider);
        }
        RebuildDerivedSets(state);
        return {};
    });
}
bool PublicApi::GetModFlag(const std::string& modName, const std::string& flagName) const {
    const auto index = FlagIndex(flagName);
    const auto snapshot = Snapshot();
    if (!index || !snapshot) return false;
    for (const auto& provider : snapshot->namedProviders) {
        if (EqualNormalized(provider.name, modName)) return (provider.flags & (1u << *index)) != 0;
    }
    return false;
}
bool PublicApi::GetAggregatedFlag(const std::string& flagName) const {
    const auto index = FlagIndex(flagName);
    const auto snapshot = Snapshot();
    if (!index || !snapshot) return false;
    for (const auto& provider : snapshot->namedProviders) {
        if ((provider.flags & (1u << *index)) != 0) return true;
    }
    return false;
}

std::vector<std::string> PublicApi::RegisteredMods() const {
    std::vector<std::string> names;
    if (const auto snapshot = Snapshot()) {
        for (const auto& provider : snapshot->namedProviders) names.push_back(provider.name);
    }
    return names;
}

std::vector<CustomRegionState> PublicApi::CustomLocations() const {
    const auto snapshot = Snapshot();
    return snapshot ? snapshot->customRegions : std::vector<CustomRegionState>{};
}

bool PublicApi::RegisterCustomLocation(const std::string& name, const StableRegionKey& key) {
    if (name.empty() || ResolveRegionByName(name) || !FindRegion(regions_, key)) return false;
    io::LegacyImport input;
    input.customRegions.push_back({name, key});
    return ImportLegacy(input).status == Status::Ready;
}

bool PublicApi::UnregisterCustomLocation(const std::string& name) {
    const auto custom = CustomLocations();
    if (std::none_of(custom.begin(), custom.end(), [&](const auto& entry) { return EqualNormalized(entry.name, name); })) return false;
    return executor_.EditState([&](State& state) -> Result<void> {
        std::erase_if(state.customRegions, [&](const auto& entry) { return EqualNormalized(entry.name, name); });
        return {};
    }).status == Status::Ready;
}

std::string PublicApi::RegionName(RegionId region) const {
    for (const auto& custom : CustomLocations()) {
        if (FindRegion(regions_, custom.key) == region) return custom.name;
    }
    for (std::size_t i = 0; i < compat::LegacyOutputs::kLegacyLocationIndexCount; ++i) {
        const auto name = compat::LegacyOutputs::LegacyLocationName(i);
        if (name && ResolveRegionByName(*name) == region) return *name;
    }
    return {};
}

int PublicApi::SpreadChance(const std::string& name) const {
    const auto region = ResolveRegionByName(name);
    const auto snapshot = Snapshot();
    if (!region || !snapshot || region->value >= regions_.regions.size()) return 0;
    for (const auto& entry : snapshot->spreadChances) {
        if (entry.key == regions_.regions[region->value]) return entry.chance;
    }
    return 0;
}

bool PublicApi::SetSpreadChance(const std::string& name, int chance) {
    io::LegacyImport input;
    input.spread.push_back({name, chance});
    return ImportLegacy(input).status == Status::Ready;
}

CommandResult PublicApi::ImportLegacy(const io::LegacyImport& input) {
    const auto now = CurrentGameTime();
    return executor_.EditState([&](State& state) -> Result<void> {
        for (const auto& custom : input.customRegions) {
            if (custom.name.empty() || !FindRegion(regions_, custom.key)) return Fail(Error::Code::InvalidArgument, "unknown imported location form");
            const auto named = std::find_if(state.customRegions.begin(), state.customRegions.end(), [&](const auto& entry) { return EqualNormalized(entry.name, custom.name); });
            if (named != state.customRegions.end()) {
                if (named->key != custom.key) return Fail(Error::Code::InvalidArgument, "custom location name conflict");
            } else {
                if (std::any_of(state.customRegions.begin(), state.customRegions.end(), [&](const auto& entry) { return entry.key == custom.key; })) return Fail(Error::Code::InvalidArgument, "custom location form conflict");
                state.customRegions.push_back(custom);
            }
        }
        const auto resolve = [&](const std::string& name) -> std::optional<RegionId> {
            for (const auto& custom : state.customRegions) {
                if (EqualNormalized(custom.name, name)) return FindRegion(regions_, custom.key);
            }
            return ResolveRegionByName(name);
        };
        for (const auto& entry : input.fame) {
            const auto region = resolve(entry.location);
            if (!region || !IsValidFameCategory(entry.category)) return Fail(Error::Code::InvalidArgument, "unknown imported fame location or category: " + entry.location);
            ExplicitCommand command{ExplicitCommandKind::Set, *region, entry.category, static_cast<double>(entry.value)};
            const auto result = ApplyExplicit(state, command, RuleContext{now});
            if (result.status != Status::Ready) return Fail(Error::Code::InvalidArgument, result.message);
        }
        for (const auto& entry : input.spread) {
            const auto region = resolve(entry.location);
            if (!region || entry.chance < 0 || entry.chance > 100) return Fail(Error::Code::InvalidArgument, "invalid imported spread chance");
            const auto& key = regions_.regions[region->value];
            const auto found = std::find_if(state.spreadChances.begin(), state.spreadChances.end(), [&](const auto& saved) { return saved.key == key; });
            if (found == state.spreadChances.end()) state.spreadChances.push_back({key, entry.chance});
            else found->chance = entry.chance;
        }
        for (const auto& name : input.mods) {
            if (name.empty() || name == "-EMPTY-") return Fail(Error::Code::InvalidArgument, "invalid imported mod name");
            if (std::none_of(state.namedProviders.begin(), state.namedProviders.end(), [&](const auto& entry) { return EqualNormalized(entry.name, name); })) state.namedProviders.push_back({name, 0});
        }
        if (state.namedProviders.size() > kMaxProviderRegistrations) return Fail(Error::Code::OutOfRange, "too many imported mods");
        for (const auto& flags : input.modFlags) {
            const auto provider = std::find_if(state.namedProviders.begin(), state.namedProviders.end(), [&](const auto& entry) { return EqualNormalized(entry.name, flags.mod); });
            if (provider == state.namedProviders.end()) return Fail(Error::Code::InvalidArgument, "flags for unregistered imported mod");
            for (const auto& [flag, enabled] : flags.flags) {
                const auto index = FlagIndex(flag);
                if (!index) return Fail(Error::Code::InvalidArgument, "unknown imported mod flag");
                if (enabled) provider->flags |= 1u << *index;
                else provider->flags &= ~(1u << *index);
            }
            SyncProvider(state, *provider);
        }
        RebuildDerivedSets(state);
        return {};
    });
}

io::LegacyDocument PublicApi::ExportLegacy() const {
    io::LegacyDocument document;
    const auto captured = executor_.Capture();
    if (!captured) return document;
    const auto exportLocation = [&](const std::string& name) {
        std::optional<StableRegionKey> key;
        for (const auto& custom : captured->customRegions) {
            if (EqualNormalized(custom.name, name)) key = custom.key;
        }
        if (!key) {
            const auto index = ResolveLegacyLocation(regions_, name);
            if (index && index->value < regions_.customOrder.size()) key = regions_.customOrder[index->value];
        }
        if (!key) return;
        const auto account = std::find_if(captured->locations.begin(), captured->locations.end(), [&](const auto& saved) { return saved.key == *key; });
        for (std::size_t i = 0; i < kFameCategoryCount; ++i) {
            const auto value = account == captured->locations.end() ? 0 : static_cast<int>(std::max(account->local[i], account->remote[i]));
            document.SetInt(name + "." + std::string(kFameCategoryNames[i]), value);
        }
        const auto spread = std::find_if(captured->spreadChances.begin(), captured->spreadChances.end(), [&](const auto& saved) { return saved.key == *key; });
        document.SetInt(name + ".famespreadchance", spread == captured->spreadChances.end() ? 0 : spread->chance);
    };
    for (std::size_t i = 0; i < compat::LegacyOutputs::kLegacyLocationIndexCount; ++i) {
        if (const auto name = compat::LegacyOutputs::LegacyLocationName(i)) exportLocation(*name);
    }
    document.SetInt("LocationTotal", static_cast<double>(captured->customRegions.size()));
    for (std::size_t i = 0; i < captured->customRegions.size(); ++i) {
        const auto& custom = captured->customRegions[i];
        exportLocation(custom.name);
        const auto suffix = std::to_string(i);
        document.SetString("Location_String_" + suffix, custom.name);
        char id[16]{};
        const auto formatted = std::to_chars(id, id + sizeof(id), custom.key.localFormId, 16);
        document.SetForm("Location_Form_" + suffix, "0x" + std::string(id, formatted.ptr) + "|" + custom.key.plugin);
    }
    document.SetInt("ModTotal", static_cast<double>(captured->namedProviders.size()));
    for (std::size_t i = 0; i < captured->namedProviders.size(); ++i) {
        const auto& provider = captured->namedProviders[i];
        document.SetString("ModIndex[" + std::to_string(i) + "]", provider.name);
        for (std::size_t bit = 0; bit < FlagNames().size(); ++bit) document.SetInt(provider.name + "." + FlagNames()[bit], (provider.flags & (1u << bit)) ? 1 : 0);
    }
    return document;
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
    const auto snapshot = Snapshot();
    return [this, snapshot](const std::string& locationName, FameCategory category) -> int {
        if (!snapshot) return 0;
        std::optional<RegionId> region;
        for (const auto& custom : snapshot->customRegions) if (EqualNormalized(custom.name, locationName)) region = FindRegion(regions_, custom.key);
        if (!region) {
            const auto index = ResolveLegacyLocation(regions_, locationName);
            if (index && index->value < regions_.customOrder.size()) region = FindRegion(regions_, regions_.customOrder[index->value]);
        }
        if (!region) {
            return 0;
        }
        const auto row = std::find(snapshot->regionIds.begin(), snapshot->regionIds.end(), *region);
        if (row == snapshot->regionIds.end()) return 0;
        const auto offset = static_cast<std::size_t>(row - snapshot->regionIds.begin()) * 32 + static_cast<std::size_t>(category);
        return offset < snapshot->fame.size() ? static_cast<int>(snapshot->fame[offset]) : 0;
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
std::size_t PublicApi::PublishAllOutputs(compat::LegacyOutputSink& sink) const {
    const auto resolve = CommittedResolver();
    const auto current = CurrentLocation();
    return outputs_.PublishCurrentFameGlobals(sink, resolve, current) + outputs_.PublishStorageUtilLists(sink, resolve) + outputs_.PublishLegacyBridge(sink, resolve, current);
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
std::vector<std::string> PublicApi::RegisteredLocations() const {
    std::vector<std::string> names;
    for (std::size_t i = 0; i < compat::LegacyOutputs::kLegacyLocationIndexCount; ++i) {
        const auto name = compat::LegacyOutputs::LegacyLocationName(i);
        if (name && ResolveRegionByName(*name)) names.push_back(*name);
    }
    for (const auto& region : CustomLocations()) names.push_back(region.name);
    return names;
}
void PublicApi::SetGameplayCall(GameplayCall call) {
    std::lock_guard lock(gameplayMutex_);
    gameplayCall_ = std::move(call);
}
bool PublicApi::CallGameplay(const std::string& name, const std::vector<compat::ModEventValue>& arguments, compat::ModEventValue& result) const {
    GameplayCall call;
    { std::lock_guard lock(gameplayMutex_); call = gameplayCall_; }
    return call && call(name, arguments, result);
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
        compat::ModEventValue result;
        (void)CallGameplay(handler, event.values, result);
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
    if (event.handler == "OnExternalFameGainRoll") {
        compat::ModEventValue result;
        (void)CallGameplay("FameGainRoll", event.values, result);
        return;
    }
    const auto category = ResolveCategory(event.StringValue("Category"));
    const auto region = ResolveParamLocation(event, "EventLocation");
    if (!category || !region) {
        return;
    }
    GainFameInputs inputs;
    if (const auto current = GetFameValueRegion(*region, *category)) {
        if (*current < event.IntValue("MinFame", 0) || *current >= event.IntValue("MaxFame", 150)) return;
        inputs.previousFame = *current;
    }
    executor_.GainFame(CurrentGameTime(), *region, *category, inputs);
}
void PublicApi::HandleManualGain(const compat::ModEvent& event) {
    const int min = static_cast<int>(event.IntValue("MinIncrease", 0));
    const int max = static_cast<int>(event.IntValue("MaxIncrease", 0));
    if (event.handler == "OnExternalManualFameGainAll") {
        for (const auto& name : RegisteredLocations())
            for (const auto category : slsf::kFameCategoryNames)
                (void)ManualGain(name, std::string(category), min, max);
        return;
    }

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
    if (event.handler == "OnExternalFameDecay") {
        compat::ModEventValue result;
        (void)CallGameplay("DecayFame", {}, result);
        return;
    }
    const int min = static_cast<int>(event.IntValue("MinDecay", 0));
    const int max = static_cast<int>(event.IntValue("MaxDecay", 0));
    std::vector<std::string> names;
    if (event.handler == "OnExternalManualFameDecayAll") names = RegisteredLocations();
    else if (const auto region = ResolveParamLocation(event, "EventLocation")) names.push_back(RegionName(*region));
    for (const auto& name : names) {
        if (event.handler == "OnExternalManualFameDecay")
            (void)ManualDecay(name, event.StringValue("Category"), min, max);
        else for (const auto category : slsf::kFameCategoryNames)
            (void)ManualDecay(name, std::string(category), min, max);
    }
}
void PublicApi::HandleSpread(const compat::ModEvent& event) {
    if (event.handler == "OnExternalFameSpreadRoll") {
        (void)SpreadPulse(RegisteredLocations());
        return;
    }
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
    const auto category = NormalizeFlag(fameType);
    if (category == "Cum Dump") SetExternalFlag(mod, "EnableCumDump", flag);
    else if (category == "Cuck" || category == "Unfaithful" || category == "Airhead") SetExternalFlag(mod, "Enable" + category, flag);
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
    compat::ModEventValue result;
    if (CallGameplay("RecognitionTime", {MakeString(locationName)}, result))
        SendReply("SLSF_Reloaded_ReturnRecognitionTime", {result});
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
void PublicApi::HandleRequestCumVisibility(const compat::ModEvent& event) {
    compat::ModEventValue result;
    if (CallGameplay("CumVisibility", event.values, result))
        SendReply("SLSF_Reloaded_ReturnRequestedCum", {result});
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
