#include "pch.h"
#include "slsf/boundary/Gameplay.h"
#include "slsf/boundary/GameplayState.h"
#include "slsf/boundary/EngineVm.h"
#include "slsf/boundary/AppearanceProviders.h"
#include "slsf/compat/PapyrusBindings.h"
#include "slsf/compat/VmNatives.h"
#include "slsf/core/gameplay/Anonymity.h"
#include "slsf/core/gameplay/Visibility.h"
#include "slsf/core/gameplay/Witness.h"
#include "slsf/core/gameplay/AnimationClassifier.h"
#include "slsf/core/gameplay/Comments.h"
#include "slsf/core/location/RegionRegistry.h"
#include "slsf/core/reputation/ReputationState.h"
#include "slsf/engine/RecordLookup.h"
#include "slsf/integrations/ExternalMods.h"
#include "slsf/integrations/SexLabPPlus.h"
#include <chrono>
#include <random>
#include <sstream>
#include <cctype>
#include <RE/P/PackUnpack.h>
#undef GetObject

namespace slsf::boundary {
namespace {
using Value = compat::ModEventValue;
using namespace gameplay;
Value Boolean(bool value) { Value out; out.type = compat::ModEventType::Bool; out.boolValue = value; return out; }
Value Integer(int value) { Value out; out.type = compat::ModEventType::Int; out.intValue = value; return out; }
Value Number(double value) { Value out; out.type = compat::ModEventType::Float; out.floatValue = value; return out; }
Value Text(std::string value) { Value out; out.type = compat::ModEventType::String; out.stringValue = std::move(value); return out; }
StableFormKey Key(RE::TESForm* form) {
    auto* file = form ? form->GetFile(0) : nullptr;
    return file ? StableFormKey{std::string(file->GetFilename()), form->GetLocalFormID()} : StableFormKey{};
}
template<class T> T* Form(const StableFormKey& key) {
    return engine::LookupLoadedRecord<T>(RE::TESDataHandler::GetSingleton(), key);
}
RE::BSTSmartPointer<RE::BSScript::Object> Object(std::string_view script) {
    static const compat::PapyrusBindings bindings;
    auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
    if (!vm) return {};
    for (const auto& fn : bindings.Functions()) {
        if (fn.script != script || fn.quest.plugin.empty()) continue;
        auto* quest = Form<RE::TESQuest>(fn.quest);
        if (!quest) return {};
        auto* policy = vm->GetObjectHandlePolicy();
        auto handle = policy->GetHandleForObject(RE::FormType::Quest, quest);
        if (fn.aliasIndex >= 0) {
            for (auto* alias : quest->aliases) {
                if (alias && alias->aliasID == static_cast<std::uint32_t>(fn.aliasIndex))
                    handle = policy->GetHandleForObject(alias->GetVMTypeID(), alias);
            }
        }
        RE::BSTSmartPointer<RE::BSScript::Object> object;
        vm->FindBoundObject(handle, fn.script.c_str(), object);
        return object;
    }
    return {};
}
const RE::BSScript::Variable* Property(const RE::BSTSmartPointer<RE::BSScript::Object>& object, const char* name) {
    return object ? object->GetProperty(name) : nullptr;
}
RE::BSTSmartPointer<RE::BSScript::Object> Linked(std::string_view script, const char* property) {
    const auto object = Object(script);
    const auto* value = Property(object, property);
    return value && value->IsObject() ? value->GetObject() : RE::BSTSmartPointer<RE::BSScript::Object>{};
}
bool Installed(const char* file) { return RE::TESDataHandler::GetSingleton()->LookupModByName(file) != nullptr; }
bool WornKeyword(RE::Actor* actor, const char* keyword) {
    for (std::uint32_t slot = 30; slot < 61; ++slot) {
        auto* armor = actor->GetWornArmor(static_cast<RE::BIPED_MODEL::BipedObjectSlot>(1u << (slot - 30)));
        if (armor && armor->HasKeywordString(keyword)) return true;
    }
    return false;
}
void Global(std::string_view script, const char* name, double value) {
    const auto object = Object(script);
    const auto* variable = Property(object, name);
    auto* form = variable && variable->IsObject() ? variable->Unpack<RE::TESGlobal*>() : nullptr;
    if (form) form->value = static_cast<float>(value);
}
void BoolProperty(std::string_view script, const char* name, bool value) {
    auto object = Object(script);
    if (auto* property = object ? object->GetProperty(name) : nullptr) property->SetBool(value);
}
template<class T> T ArrayValue(const char* property, std::size_t slot, T fallback) {
    const auto object = Object("SLSF_Reloaded_VisibilityManager");
    const auto* value = Property(object, property);
    if (!value || !value->IsArray() || !value->GetArray() || slot >= value->GetArray()->size()) return fallback;
    return (*value->GetArray())[static_cast<RE::BSScript::Array::size_type>(slot)].Unpack<T>();
}
template<class T> void SetProperty(const char* name, T value) {
    auto object = Object("SLSF_Reloaded_VisibilityManager");
    if (auto* property = object ? object->GetProperty(name) : nullptr) property->Pack(std::move(value));
}
class Random final : public IRandom {
public:
    std::int32_t UniformInt(std::int32_t low, std::int32_t high) override { return std::uniform_int_distribution<std::int32_t>(low, high)(rng_); }
private:
    std::mt19937 rng_{std::random_device{}()};
};
std::int64_t Milliseconds() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
}

struct Gameplay::Impl {
    api::PublicApi& api;
    const RegionSnapshot& regions;
    std::map<StableFormKey, RegionId> owners;
    EngineVm vm;
    PapyrusCalls calls;
    integrations::ExternalMods external;
    integrations::SexLabPPlus scene;
    AnonymityRules anonymity;
    VisibilityRules visibility;
    Random random;
    Epoch epoch{};
    std::optional<RegionId> current;
    bool stopped{};
    double nextScan{};
    double nextComment{};
    std::int64_t sceneStarted{};
    bool analyzePending{};
    std::string consumedStage;
    std::shared_ptr<const ConfigurationData> settings;
    std::optional<std::array<int, 3>> cumCounts;
    bool cumPending{};
    std::uint64_t cumGeneration{};
    double nextGameScan{};
    double lastSkoomaDay{};
    std::array<bool, 4> missingCleared{};

    void CleanupMissingCategories() {
        const auto object = Object("SLSF_Reloaded_DataManager");
        const auto* flags = Property(object, "DisplayFameFlags");
        const std::array<const char*, 4> plugins{"sr_FillHerUp.esp", "SLSFFameComments.esp", "SLSFFameComments.esp", "CustomComments.esp"};
        const std::array<const char*, 4> names{"Cum Dump", "Cuck", "Unfaithful", "Airhead"};
        const std::array<const char*, 4> enableNames{"EnableCumDump", "EnableCuck", "EnableUnfaithful", "EnableAirhead"};
        for (std::uint32_t i = 0; i < 4; ++i) {
            const bool displayed = flags && flags->IsArray() && flags->GetArray() && flags->GetArray()->size() > i && (*flags->GetArray())[i].GetBool();
            if (Installed(plugins[i]) || api.GetAggregatedFlag(enableNames[i]) || api.GetAggregatedFlag(names[i]) || displayed) { missingCleared[i] = false; continue; }
            if (missingCleared[i]) continue;
            for (const auto& location : api.RegisteredLocations()) (void)api.ClearFame(location, names[i]);
            missingCleared[i] = true;
        }
    }

    double Setting(const char* raw, double fallback = 0) const {
        if (!settings) return fallback;
        const std::string_view original(raw);
        if (original == "DynamicAnonymity") return settings->features.dynamicAnonymity;
        if (original == "FriendBetrayChance") return settings->relationships.friendBetrayChance;
        if (original == "LoverBetrayChance") return settings->relationships.loverBetrayChance;
        if (original == "FriendHighFameThreshold") return settings->relationships.friendHighFameThreshold;
        if (original == "LoverHighFameThreshold") return settings->relationships.loverHighFameThreshold;
        std::string key(raw); key[0] = static_cast<char>(std::tolower(static_cast<unsigned char>(key[0])));
        if (original == "NPCNeedsLOS") key = "npcNeedsLOS";
        for (const auto& s : kGameplayBoolSettings) if (key == s.name) return settings->gameplay.*s.member;
        for (const auto& s : kGameplayNumberSettings) if (key == s.name) return settings->gameplay.*s.member;
        return fallback;
    }
    void RequestCum() {
        if (cumPending) return;
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) return;
        const bool slacs = RE::TESDataHandler::GetSingleton()->LookupModByName("SLACS.esp") != nullptr;
        const StableFormKey instance = slacs ? StableFormKey{"SLACS.esp", 0x5557} : StableFormKey{"SexLab.esm", 0xD62};
        struct Batch { std::array<int, 3> values{}; std::size_t remaining{3}; bool valid{true}; };
        const auto batch = std::make_shared<Batch>();
        const auto generation = ++cumGeneration;
        const std::array functions{"CountCumAnal", "CountCumVaginal", "CountCumOral"};
        VmValue actor; actor.type = VmValueType::Form; actor.formValue = Key(player);
        cumPending = true;
        for (std::size_t i = 0; i < functions.size(); ++i) {
            auto complete = [this, batch, generation, i](const PapyrusCallResult& result) {
                    if (result.kind == PapyrusCallKind::Cancelled) return;
                    if (generation != cumGeneration) return;
                    if (result.kind == PapyrusCallKind::Ready && result.values.size() == 1 &&
                        result.values.front().type == VmValueType::Int && result.values.front().intValue >= 0 &&
                        result.values.front().intValue <= (std::numeric_limits<int>::max)())
                        batch->values[i] = static_cast<int>(result.values.front().intValue);
                    else batch->valid = false;
                    if (--batch->remaining == 0) {
                        cumPending = false;
                        if (batch->valid) cumCounts = batch->values;
                        else { cumCounts.reset(); logs::warn("gameplay: cum count interface unavailable; dependent facts remain Unknown"); }
                    }
                };
            if (!slacs) calls.Submit(VmTargetKind::Instance, instance, "SexLabFramework", functions[i], {actor}, std::move(complete));
            else {
                VmValue part; part.type = VmValueType::String; part.stringValue = std::array{"Anal", "Vaginal", "Oral"}[i];
                VmValue area; area.type = VmValueType::String; area.stringValue = i == 2 ? "Face" : "Body";
                calls.Submit(VmTargetKind::Instance, instance, "SLACS_main", "GetStorageKey", {actor, part, area},
                    [this, generation, actor, complete = std::move(complete)](const PapyrusCallResult& result) {
                        if (result.kind == PapyrusCallKind::Cancelled) return;
                        if (generation != cumGeneration) return;
                        if (result.kind != PapyrusCallKind::Ready || result.values.size() != 1 ||
                            result.values.front().type != VmValueType::String || result.values.front().stringValue.empty()) {
                            complete({PapyrusCallKind::Failed, Status::NotReady, {}, "SLACS storage key unavailable"});
                            return;
                        }
                        calls.SubmitGlobal("StorageUtil", "IntListCount", {actor, result.values.front()}, complete);
                    });
            }
        }
    }

    Impl(api::PublicApi& a, const RegionSnapshot& r, std::map<StableFormKey, RegionId> o)
        : api(a), regions(r), owners(std::move(o)), vm([this](Epoch e) { return api.IsEpochCurrent(e); }),
          calls(vm, [this] { return api.CurrentEpoch(); }, {5000, 256}), external(calls), scene(calls) {}

    std::optional<RegionId> Resolve(RE::BGSLocation* location) const {
        const auto custom = api.CustomLocations();
        for (auto* cursor = location; cursor; cursor = cursor->parentLoc) {
            const auto key = Key(cursor);
            for (const auto& entry : custom) {
                if (entry.key.plugin == key.plugin && entry.key.localFormId == key.localFormId)
                    return FindRegion(regions, entry.key);
            }
        }
        const auto it = owners.find(Key(location));
        return it == owners.end() || api.RegionName(it->second).empty() ? std::nullopt : std::optional{it->second};
    }
    bool Excluded(RE::BGSLocation* location) const {
        if (!location) return true;
        const auto object = Object("SLSF_Reloaded_LocationManager");
        for (const auto* property : {"OverrideExcludedLocation", "ExcludedLocations"}) {
            const auto* value = Property(object, property);
            if (!value || !value->IsArray() || !value->GetArray()) continue;
            for (const auto& row : *value->GetArray()) {
                auto* keyword = row.Unpack<RE::BGSKeyword*>();
                if (keyword && location->HasKeyword(keyword)) return std::string_view(property) != "OverrideExcludedLocation";
            }
        }
        return false;
    }
    void Restore() {
        anonymity.Clear(); current.reset();
        const auto* mainQuest = Property(Object("SLSF_Reloaded_Uninstall"), "MainQuest");
        const auto* quest = mainQuest ? mainQuest->Unpack<RE::TESQuest*>() : nullptr;
        stopped = quest && quest->IsStopped(); nextScan = nextComment = 0;
        scene.Reset(); external.Reset(); analyzePending = false; consumedStage.clear(); cumCounts.reset(); cumPending = false; ++cumGeneration; nextGameScan = 0;
        lastSkoomaDay = api.CurrentGameTime().days; missingCleared.fill(false);
        const auto capture = api.Executor().Capture();
        if (capture) RestoreRecognition(anonymity, current, capture->recognition, regions);
    }
    void Persist() {
        auto rows = CaptureRecognition(anonymity, current, regions);
        const auto captured = api.Executor().Capture();
        if (captured && captured->recognition == rows) return;
        (void)api.Executor().EditState([rows = std::move(rows)](State& state) -> Result<void> {
            state.recognition = rows; return {};
        });
    }
    bool Thane(const std::string& name) const {
        const auto object = Object("SLSF_Reloaded_DynamicAnonymity");
        if (name == "Raven Rock" || name == "Skaal Village") {
            const auto* value = Property(object, "DLC2StoneCleanse");
            auto* quest = value ? value->Unpack<RE::TESQuest*>() : nullptr;
            return quest && quest->IsCompleted();
        }
        if (name == "Largashbur" || name == "Dushnikh Yal" || name == "Mor Khazgur" || name == "Narzulbur") {
            const auto* value = Property(object, "OrcFriends");
            auto* faction = value ? value->Unpack<RE::TESFaction*>() : nullptr;
            return faction && RE::PlayerCharacter::GetSingleton()->IsInFaction(faction);
        }
        const std::map<std::string, std::string> holds{{"Whiterun", "Whiterun"}, {"Riverwood", "Whiterun"}, {"Rorikstead", "Whiterun"}, {"Dragon Bridge", "Haafingar"},
            {"Ivarstead", "Rift"}, {"Shor's Stone", "Rift"}, {"Karthwasten", "Reach"}, {"Windhelm", "Eastmarch"},
            {"Solitude", "Haafingar"}, {"Morthal", "Hjaalmarch"}, {"Markarth", "Reach"},
            {"Dawnstar", "Pale"}, {"Riften", "Rift"}, {"Winterhold", "Winterhold"}, {"Falkreath", "Falkreath"}};
        const auto found = holds.find(name);
        std::string hold = found == holds.end() ? "" : found->second;
        const auto locations = Object("SLSF_Reloaded_LocationManager");
        for (const auto* prefix : {"Whiterun", "Winterhold", "Eastmarch", "Haafingar", "Hjaalmarch", "Pale", "Rift", "Reach", "Falkreath", "Solstheim"}) {
            const auto* list = Property(locations, (std::string(prefix) + "LocationList").c_str());
            if (list && list->IsArray() && list->GetArray()) for (const auto& entry : *list->GetArray())
                if (entry.IsString() && name == entry.GetString()) hold = prefix;
        }
        if (hold == "Solstheim") {
            const auto* questValue = Property(object, "DLC2StoneCleanse");
            auto* quest = questValue ? questValue->Unpack<RE::TESQuest*>() : nullptr;
            return quest && quest->IsCompleted();
        }
        if (hold.empty()) return false;
        const auto* value = Property(object, "ThaneScript");
        const auto thane = value && value->IsObject() ? value->GetObject() : RE::BSTSmartPointer<RE::BSScript::Object>{};
        for (const auto suffix : {"ImpGetOutofJail", "SonsGetOutofJail"}) {
            const auto* flag = Property(thane, (hold + suffix).c_str());
            if (flag && ((flag->IsBool() && flag->GetBool()) || (flag->IsInt() && flag->GetSInt() > 0))) return true;
        }
        return false;
    }
    VisibilityFacts Facts() {
        VisibilityFacts f;
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) { f.status = Status::NotReady; return f; }
        f.anonymous = Fact::Known(false);
        f.playerIsMale = Fact::Known(player->GetActorBase()->GetSex() == RE::SEX::kMale);
        f.mods.deviousDevices = Fact::Known(Installed("Devious Devices - Integration.esm"));
        f.mods.armorKeywords = Fact::Known(Installed("ArmorKeywords.esm"));
        f.mods.sls = Fact::Known(Installed("SL Survival.esp"));
        f.mods.slacs = Fact::Known(Installed("SLACS.esp"));
        f.mods.slaveTats = Fact::Known(Installed("SlaveTats.esp"));
        f.settings.anonymityEnabled = Fact::Known(Setting("AnonymityEnabled", 1) != 0);
        f.settings.dynamicAnonymity = Fact::Known(Setting("DynamicAnonymity", 1) != 0);
        f.settings.allowCollarBoundFame = Fact::Known(Setting("AllowCollarBoundFame") != 0);
        f.settings.allowSlscCursedCollarBoundFame = Fact::Known(Setting("AllowSLSCursedCollarBoundFame") != 0);
        for (std::uint32_t slot = 30; slot < kArmorSlotCount; ++slot) {
            auto* armor = player->GetWornArmor(static_cast<RE::BIPED_MODEL::BipedObjectSlot>(1u << (slot - 30)));
            auto& item = f.items[slot];
            const auto keyword = [armor](const char* key) { return Fact::Known(armor && armor->HasKeywordString(key)); };
            item.present = Fact::Known(armor != nullptr);
            item.bikiniArmor = keyword("SLA_ArmorBikini");
            item.doesNotHideIdentity = keyword("SLSF_Reloaded_DoesNotHideIdentity");
            item.doesNotCoverBack = keyword("SLSF_Reloaded_DoesNotCoverBack");
            item.doesNotCoverHands = keyword("SLSF_Reloaded_DoesNotCoverHands");
            item.doesNotCoverFeet = keyword("SLSF_Reloaded_DoesNotCoverFeet");
            item.deviousCollar = keyword("zad_DeviousCollar"); item.deviousBra = keyword("zad_DeviousBra");
            item.deviousBelt = keyword("zad_DeviousBelt"); item.deviousHarness = keyword("zad_DeviousHarness");
        }
        const auto worn = [player](const char* key) { return Fact::Known(WornKeyword(player, key)); };
        f.worn.hidesIdentity = worn("SLSF_Reloaded_HidesIdentity");
        f.worn.coversBack = worn("SLSF_Reloaded_CoversBack"); f.worn.coversHands = worn("SLSF_Reloaded_CoversHands");
        f.worn.coversFeet = worn("SLSF_Reloaded_CoversFeet"); f.worn.deviousHood = worn("zad_DeviousHood");
        f.worn.deviousHeavyBondage = worn("zad_DeviousHeavyBondage"); f.worn.deviousLockable = worn("zad_Lockable");
        f.worn.deviousBeltWorn = worn("zad_DeviousBelt"); f.worn.deviousCollarWorn = worn("zad_DeviousCollar");
        f.worn.slsCollarCurse = worn("_SLS_CollarCurse");
        f.worn.devious.hood = worn("zad_DeviousHood"); f.worn.devious.gag = worn("zad_DeviousGag");
        f.worn.devious.gagPanel = worn("zad_DeviousGagPanel"); f.worn.devious.armCuffs = worn("zad_DeviousArmCuffs");
        f.worn.devious.armCuffsFront = worn("zad_DeviousArmbinderFront"); f.worn.devious.armbinder = worn("zad_DeviousArmbinder");
        f.worn.devious.armbinderElbow = worn("zad_DeviousArmbinderElbow"); f.worn.devious.gloves = worn("zad_DeviousGloves");
        f.worn.devious.legCuffs = worn("zad_DeviousLegCuffs"); f.worn.devious.boots = worn("zad_DeviousBoots");
        f.worn.devious.suit = worn("zad_DeviousSuit"); f.worn.devious.corset = worn("zad_DeviousCorset");
        f.worn.devious.blindfold = worn("zad_DeviousBlindfold");
        f.andRanks.installed = Fact::Known(Installed("Advanced Nudity Detection.esp"));
        const std::array<std::uint32_t, 5> ranks{2095, 2096, 2100, 2101, 2094};
        for (std::size_t i = 0; i < ranks.size(); ++i) {
            auto* faction = Form<RE::TESFaction>({"Advanced Nudity Detection.esp", ranks[i]});
            f.andRanks.rankIsOne[i] = faction ? Fact::Known(player->GetFactionRank(faction, false) == 1) :
                (f.andRanks.installed.IsKnownFalse() ? Fact::Known(false) : Fact{FactState::Unknown});
        }
        const auto tattoos = QuerySlaveTats(player, api.CurrentEpoch());
        const std::array<TattooAreaFacts*, 4> areas{&f.tattoos.body, &f.tattoos.face, &f.tattoos.hands, &f.tattoos.feet};
        for (std::size_t i = 0; i < areas.size(); ++i) {
            auto& area = *areas[i]; area.installed = f.mods.slaveTats;
            area.appliedCountKnown = Fact::Known(tattoos.availability == integrations::ExternalAvailability::Available || f.mods.slaveTats.IsKnownFalse());
            area.appliedCount = static_cast<int>(tattoos.areas[i].placements.size());
            area.slots = integrations::SlaveTatsSlotFacts(tattoos.areas[i], i == 0 ? 40 : 20);
            const std::string prefix = i == 0 ? "Body" : i == 1 ? "Face" : i == 2 ? "Hand" : "Foot";
            for (std::size_t slot = 0; slot < area.slots.size(); ++slot) {
                if (ArrayValue<bool>((prefix + "TattooExcluded").c_str(), slot, false)) area.slots[slot].applied = Fact::Known(false);
                if (i == 0) {
                    const auto name = ArrayValue<std::string>("BodyTattooSubcategory", slot, "-NONE-");
                    const std::map<std::string, TattooSubcategory> categories{{"-NONE-", TattooSubcategory::None}, {"Chest", TattooSubcategory::Chest},
                        {"Pelvis", TattooSubcategory::Pelvis}, {"Ass", TattooSubcategory::Ass}, {"Back", TattooSubcategory::Back},
                        {"Thigh", TattooSubcategory::Thigh}, {"Calves", TattooSubcategory::Calves}};
                    const auto found = categories.find(name);
                    area.slots[slot].subcategory = found == categories.end() ? TattooSubcategory::Unknown : found->second;
                }
            }
        }
        const std::array<CountFacts*, 3> counts{&f.cum.anal, &f.cum.vaginal, &f.cum.oral};
        for (std::size_t i = 0; i < counts.size(); ++i) {
            counts[i]->known = cumCounts ? Fact::Known(true) : Fact{FactState::Unknown};
            counts[i]->value = cumCounts ? (*cumCounts)[i] : 0;
        }
        return f;
    }
    RuleDecision Anonymous(const VisibilityFacts& facts, std::optional<RegionId> target = std::nullopt) {
        const auto region = target ? target : current;
        if (!region) return RuleDecision::Known(false);
        AnonymityConditions conditions;
        conditions.anonymityEnabled = facts.settings.anonymityEnabled;
        conditions.dynamicAnonymity = facts.settings.dynamicAnonymity;
        bool low = true;
        for (const auto name : kFameCategoryNames) if (api.GetFameValue(api.RegionName(*region), std::string(name)) >= Setting("DynamicAnonymityFameCutoff", 50)) low = false;
        conditions.lowFameRegion = Fact::Known(low);
        conditions.thanesCantBeAnonymous = Fact::Known(Setting("ThanesCantBeAnonymous", 1) != 0);
        conditions.isThane = Fact::Known(Thane(api.RegionName(*region)));
        conditions.hidesIdentity = facts.worn.hidesIdentity;
        conditions.deviousDevicesInstalled = facts.mods.deviousDevices;
        conditions.deviousHoodWorn = facts.worn.deviousHood;
        conditions.headSlotOccupied = facts.items[31].present;
        conditions.headSlotBikiniArmor = facts.items[31].bikiniArmor;
        conditions.headSlotDoesNotHideIdentity = facts.items[31].doesNotHideIdentity;
        return anonymity.Evaluate(*region, conditions);
    }
    std::array<int, 4> TattooCounts(const VisibilityFacts& facts) {
        std::array<int, 4> counts{};
        const std::array<const TattooAreaFacts*,4> areas{&facts.tattoos.body, &facts.tattoos.face, &facts.tattoos.hands, &facts.tattoos.feet};
        for (std::size_t a = 0; a < areas.size(); ++a) for (std::size_t slot = 0; slot < areas[a]->slots.size(); ++slot) {
            const auto result = a == 0 ? visibility.BodyTattooVisible(facts, slot) : a == 1 ? visibility.FaceTattooVisible(facts, slot) :
                a == 2 ? visibility.HandTattooVisible(facts, slot) : visibility.FootTattooVisible(facts, slot);
            if (result.IsReady() && result.value) ++counts[a];
        }
        return counts;
    }
    WitnessSettings Witness() const {
        WitnessSettings s;
        if (settings) { s.forbiddenByFriend = settings->relationships.forbiddenByFriend; s.forbiddenByLover = settings->relationships.forbiddenByLover; }
        s.enemyChance = static_cast<int>(Setting("FameChanceByEnemy", 100));
        s.neutralChance = static_cast<int>(Setting("FameChanceByNeutral", 50));
        s.friendChance = static_cast<int>(Setting("FameChanceByFriend", 25));
        s.loverChance = static_cast<int>(Setting("FameChanceByLover", 10));
        s.npcNeedsLineOfSight = Setting("NPCNeedsLOS", 1) != 0;
        s.minimumNpcLosDistance = Setting("MinimumNPCLOSDistance", 2048);
        s.friendBetrayChance = static_cast<int>(Setting("FriendBetrayChance"));
        s.loverBetrayChance = static_cast<int>(Setting("LoverBetrayChance"));
        s.friendsBetrayFromHighFame = Setting("FriendsBetrayFromHighFame", 1) != 0;
        s.loversBetrayFromHighFame = Setting("LoversBetrayFromHighFame", 1) != 0;
        s.friendHighFameThreshold = static_cast<int>(Setting("FriendHighFameThreshold", 100));
        s.loverHighFameThreshold = static_cast<int>(Setting("LoverHighFameThreshold", 100));
        return s;
    }
    void Roll(const std::string& location, bool friendOfPlayer = false, bool loverOfPlayer = false, bool foreplay = false, double multiplier = 1) {
        auto f = Facts();
        const auto anon = Anonymous(f);
        FameGainRollFacts input;
        input.validLocation = Fact::Known(api.ResolveRegionByName(location).has_value());
        input.anonymous = anon.IsReady() ? Fact::Known(anon.value) : Fact{FactState::Unknown};
        input.relationship = loverOfPlayer ? RelationshipGroup::Lover : friendOfPlayer ? RelationshipGroup::Friend : RelationshipGroup::Neutral;
        input.settings = Witness();
        const auto state = external.Snapshot();
        const auto* actor = state.FindActor(Key(RE::PlayerCharacter::GetSingleton()));
        const auto tats = TattooCounts(f);
        const auto anal = visibility.VisibleAssCum(f), vaginal = visibility.VisibleVaginalCum(f), oral = visibility.VisibleOralCum(f);
        const auto bondage = visibility.EvaluateBondage(f);
        auto* player = RE::PlayerCharacter::GetSingleton();
        const auto worn = [player](RE::BGSKeyword* keyword) {
            if (!keyword || !player) return false;
            for (int slot = 30; slot <= 60; ++slot) if (auto* armor = player->GetWornArmor(static_cast<RE::BGSBipedObjectForm::BipedObjectSlot>(1u << (slot - 30))); armor && armor->HasKeyword(keyword)) return true;
            return false;
        };
        const auto localKeyword = [](const char* property) {
            auto object = Object("SLSF_Reloaded_FameManager");
            auto* value = object ? object->GetProperty(property) : nullptr;
            return value ? value->Unpack<RE::BGSKeyword*>() : nullptr;
        };
        const bool dd = f.mods.deviousDevices.IsKnownTrue();
        const bool hasAnd = f.andRanks.installed.IsKnownTrue();
        const bool naked = f.items[32].present.IsKnownFalse();
        const bool nipple = worn(localKeyword("SLSF_Reloaded_NipplePiercing")) || dd && worn(Form<RE::BGSKeyword>({"Devious Devices - Assets.esm", 51769}));
        const bool genital = worn(localKeyword("SLSF_Reloaded_VaginalPiercing")) || dd && worn(Form<RE::BGSKeyword>({"Devious Devices - Assets.esm", 147056}));
        const bool vaginalPlug = dd && worn(Form<RE::BGSKeyword>({"Devious Devices - Assets.esm", 122236}));
        const bool analPlug = dd && worn(Form<RE::BGSKeyword>({"Devious Devices - Assets.esm", 122237}));
        const auto rank = [&](AndLocation area) { return f.andRanks.rankIsOne[static_cast<std::size_t>(area)].IsKnownTrue(); };
        const auto extraRank = [player](std::uint32_t id) { auto* faction = Form<RE::TESFaction>({"Advanced Nudity Detection.esp", id}); return faction && player && player->GetFactionRank(faction, false) == 1; };
        const auto& d = f.worn.devious;
        int points = bondage.collarVisible.value && Setting("AllowCollarBoundFame") != 0 ? 1 : 0;
        if (bondage.braVisible.value || d.corset.IsKnownTrue()) points += 2;
        if (bondage.beltVisible.value) points += 2;
        if (bondage.harnessVisible.value || d.suit.IsKnownTrue()) points += 3;
        if (d.armCuffs.IsKnownTrue() || d.armCuffsFront.IsKnownTrue() || d.armbinder.IsKnownTrue() || d.armbinderElbow.IsKnownTrue() || d.gloves.IsKnownTrue()) points += 2;
        if (d.legCuffs.IsKnownTrue() || d.boots.IsKnownTrue()) points += 2;
        if (d.blindfold.IsKnownTrue()) ++points;
        if (d.gag.IsKnownTrue() || d.gagPanel.IsKnownTrue()) ++points;
        if (d.hood.IsKnownTrue()) ++points;
        std::array<Tri, kFameCategoryCount> tattooExtras{};
        tattooExtras.fill(Tri::False);
        const std::array<const char*, 4> areas{"Body", "Face", "Hand", "Foot"};
        const std::array<const TattooAreaFacts*, 4> tattooAreas{&f.tattoos.body, &f.tattoos.face, &f.tattoos.hands, &f.tattoos.feet};
        for (std::size_t area = 0; area < areas.size(); ++area) for (std::size_t slot = 0; slot < tattooAreas[area]->slots.size(); ++slot) {
            const auto extra = ArrayValue<std::string>((std::string(areas[area]) + "TattooExtraFameType").c_str(), slot, "");
            const auto category = api.ResolveCategory(extra);
            if (!category) continue;
            const auto visible = area == 0 ? visibility.BodyTattooVisible(f, slot) : area == 1 ? visibility.FaceTattooVisible(f, slot) : area == 2 ? visibility.HandTattooVisible(f, slot) : visibility.FootTattooVisible(f, slot);
            const auto index = static_cast<std::size_t>(*category);
            tattooExtras[index] = TriOr(tattooExtras[index], visible.IsReady() ? (visible.value ? Tri::True : Tri::False) : Tri::Unknown);
        }
        for (std::size_t i = 0; i < kFameCategoryCount; ++i) {
            const auto category = static_cast<FameCategory>(i);
            const auto name = std::string(kFameCategoryNames[i]);
            const int fame = api.GetFameValue(location, name).value_or(0);
            input.currentFame[i] = FameValue::Known(fame);
            Tri eligible = TriOr(api.GetAggregatedFlag(name) ? Tri::True : Tri::False, tattooExtras[i]);
            const auto threshold = [fame](int count) { return count > 2 || (count == 2 && fame < 100) || (count == 1 && fame < 50); };
            switch (category) {
            case FameCategory::Whore: eligible = TriOr(eligible, TriOf(integrations::ExternalPublicWhoreFact(state))); break;
            case FameCategory::Slut: {
                const bool exposed = hasAnd ? nipple ? rank(AndLocation::Chest) && fame < 50 : genital && rank(AndLocation::Genitals) && fame < 75 : dd && naked && (nipple || genital || vaginalPlug);
                eligible = TriOr(eligible, exposed ? Tri::True : vaginal.IsReady() ? (threshold(vaginal.value) ? Tri::True : Tri::False) : Tri::Unknown); break;
            }
            case FameCategory::Exhibitionist: {
                const bool exposed = !hasAnd ? naked : extraRank(0x831) ? true : extraRank(0x832) || extraRank(0x833) ? fame < 100 : rank(AndLocation::Chest) || rank(AndLocation::Genitals) ? fame < 75 : rank(AndLocation::Ass) ? fame < 50 : (rank(AndLocation::Bra) || rank(AndLocation::Underwear)) && fame < 25;
                eligible = TriOr(eligible, scene.Active() || exposed ? Tri::True : Tri::False); break;
            }
            case FameCategory::Oral: eligible = TriOr(eligible, oral.IsReady() ? (threshold(oral.value) ? Tri::True : Tri::False) : Tri::Unknown); break;
            case FameCategory::Anal: eligible = TriOr(eligible, analPlug && (hasAnd ? rank(AndLocation::Ass) : naked) ? Tri::True : anal.IsReady() ? (threshold(anal.value) ? Tri::True : Tri::False) : Tri::Unknown); break;
            case FameCategory::Nasty:
                eligible = TriOr(eligible, anal.IsReady() && vaginal.IsReady() && oral.IsReady() ?
                    ((static_cast<std::int64_t>(anal.value) + vaginal.value + oral.value > 0 && cumCounts && (static_cast<std::int64_t>((*cumCounts)[0]) + (*cumCounts)[1] + (*cumCounts)[2] > 3 || (static_cast<std::int64_t>((*cumCounts)[0]) + (*cumCounts)[1] + (*cumCounts)[2] > 1 && fame < 75))) ? Tri::True : Tri::False) : Tri::Unknown); break;
            case FameCategory::Pregnant:
                eligible = TriOr(eligible, actor ? ((actor->estrusChaurusBreeder || actor->estrusSpiderBreeder ||
                    actor->fertilityModeRank.value_or(0) > 60 || actor->hentaiPregnancyRank.value_or(0) >= 3 ||
                    (fame < 50 && (actor->fertilityModeRank.value_or(0) > 30 || actor->hentaiPregnancyRank.value_or(0) == 2))) ? Tri::True : Tri::False) : Tri::Unknown); break;
            case FameCategory::Bound:
                eligible = TriOr(eligible, bondage.visiblyBound.IsReady() && bondage.heavilyBound.IsReady() ?
                    (bondage.visiblyBound.value && (bondage.heavilyBound.value || points > 8 || bondage.lightlyBound.value && fame < points * 15) ? Tri::True : Tri::False) : Tri::Unknown); break;
            case FameCategory::Tattoo: {
                const int total = tats[0] + tats[1] + tats[2] + tats[3];
                const bool can = total > 10 || tats[0] > 5 || (total > 8 || tats[0] == 5) && fame < 125 ||
                    (total > 6 || tats[0] == 4) && fame < 100 || (total > 4 || tats[0] == 3) && fame < 75 ||
                    (total > 2 || tats[0] == 2) && fame < 50 || total > 0 && fame < 25;
                eligible = TriOr(eligible, f.tattoos.body.appliedCountKnown.IsKnownTrue() ? (can ? Tri::True : Tri::False) : Tri::Unknown); break;
            }
            case FameCategory::CumDump: eligible = TriOr(eligible, actor && actor->inflationKnown ?
                (actor->inflation >= Setting("CumDumpFHUReq", 5) ? Tri::True : Tri::False) : Tri::Unknown); break;
            default: break;
            }
            input.eligible[i] = eligible == Tri::Unknown ? Fact{FactState::Unknown} : Fact::Known(eligible == Tri::True);
        }
        FameGainRoller roller(random);
        const auto result = roller.Run(input);
        if (result.IsReady()) for (const auto category : result.selection.selected) {
            GainFameInputs gain; gain.foreplay = Fact::Known(foreplay); gain.iterationMultiplier = multiplier;
            (void)api.GainFame(location, std::string(kFameCategoryNames[static_cast<std::size_t>(category)]), gain);
        }
    }
    void Scan(RE::Actor* target = nullptr, double multiplier = 1) {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player || !current) return;
        std::vector<RE::Actor*> actors;
        if (target) actors.push_back(target);
        else if (auto* processes = RE::ProcessLists::GetSingleton()) for (const auto& handle : processes->highActorHandles) {
            auto actor = handle.get(); if (actor && actor.get() != player) actors.push_back(actor.get());
        }
        std::vector<NpcScanTargetFacts> rows;
        for (auto* actor : actors) {
            NpcScanTargetFacts row;
            row.actor = Key(actor); row.validLocation = Fact::Known(!Excluded(actor->GetCurrentLocation()));
            row.dead = Fact::Known(actor->IsDead()); row.inCombat = Fact::Known(actor->IsInCombat());
            const auto snapshot = scene.Snapshot();
            row.sexLabActive = Fact::Known(scene.Active() && std::ranges::any_of(snapshot.actors, [&](const auto& entry) { return entry.actor == row.actor; }));
            bool unused{}; row.lineOfSight = Fact::Known(actor->HasLineOfSight(player, unused));
            row.distance = ObservedDouble::Known(actor->GetDistance(player));
            row.playerDetected = Fact::Known(player->RequestDetectionLevel(actor, RE::DETECTION_PRIORITY::kNormal) > 0);
            auto* relation = RE::BGSRelationship::GetRelationship(player->GetActorBase(), actor->GetActorBase());
            row.relationshipKnown = Fact::Known(true); row.relationshipRank = relation ? 4 - static_cast<int>(relation->level.underlying()) : 0;
            rows.push_back(std::move(row));
        }
        NpcScanner scanner(Witness(), random);
        const auto result = scanner.Run(rows);
        if (const auto* entry = result.SuccessEntry()) {
            Global("SLSF_Reloaded_PlayerScript", "SLSF_Reloaded_NPCScanSucess", 1);
            Roll(api.RegionName(*current), entry->friendOfPlayer, entry->loverOfPlayer, false, multiplier);
        }
    }
    bool Analyze(const std::vector<Value>* supplied = nullptr) {
        const auto snapshot = scene.Snapshot();
        AnimationFacts input;
        if (!integrations::ApplyPPlusToAnimation(snapshot, input).applied) return false;
        auto destination = current;
        if (supplied) {
            if (supplied->size() != 7 || (*supplied)[2].intValue != scene.ThreadId() || !(*supplied)[1].scriptHandle) return false;
            RE::BSTSmartPointer<RE::BSScript::Object> controller;
            auto* machine = RE::BSScript::Internal::VirtualMachine::GetSingleton();
            if (!machine || !machine->FindBoundObject((*supplied)[1].scriptHandle, "sslThreadController", controller) || !controller) return false;
            const auto* thread = controller->GetVariable("thread_id");
            if (!thread || !thread->IsInt() || thread->GetSInt() != (*supplied)[2].intValue) return false;
            const auto& actors = (*supplied)[0].formArray;
            if (actors.size() != snapshot.actors.size()) return false;
            for (std::size_t i = 0; i < actors.size(); ++i) if (actors[i] != snapshot.actors[i].actor) return false;
            destination = api.ResolveRegionByName((*supplied)[4].stringValue);
            if (!destination) return false;
        }
        if (!supplied && !consumedStage.empty() && consumedStage == snapshot.sceneId) { analyzePending = false; return true; }
        for (std::size_t i = 0; i < input.actors.size() && i < snapshot.actors.size(); ++i) {
            auto* actor = Form<RE::Actor>(snapshot.actors[i].actor);
            if (!actor) continue;
            auto& target = input.actors[i];
            auto* relation = RE::BGSRelationship::GetRelationship(RE::PlayerCharacter::GetSingleton()->GetActorBase(), actor->GetActorBase());
            target.relationshipKnown = Fact::Known(true); target.relationshipRank = relation ? 4 - static_cast<int>(relation->level.underlying()) : 0;
            const auto worn = [actor](const char* key) { return Fact::Known(WornKeyword(actor, key)); };
            target.devious.lockable = worn("zad_Lockable"); target.devious.hood = worn("zad_DeviousHood");
            target.devious.harness = worn("zad_DeviousHarness"); target.devious.belt = worn("zad_DeviousBelt");
            target.devious.bra = worn("zad_DeviousBra"); target.devious.heavyBondage = worn("zad_DeviousHeavyBondage");
            target.devious.armCuffs = worn("zad_DeviousArmCuffs"); target.devious.armCuffsFront = worn("zad_DeviousArmbinderFront");
            target.devious.armbinder = worn("zad_DeviousArmbinder"); target.devious.armbinderElbow = worn("zad_DeviousArmbinderElbow");
            target.devious.gloves = worn("zad_DeviousGloves"); target.devious.legCuffs = worn("zad_DeviousLegCuffs");
            target.devious.boots = worn("zad_DeviousBoots"); target.devious.gag = worn("zad_DeviousGag"); target.devious.gagPanel = worn("zad_DeviousGagPanel");
            target.devious.suit = worn("zad_DeviousSuit"); target.devious.corset = worn("zad_DeviousCorset"); target.devious.blindfold = worn("zad_DeviousBlindfold");
        }
        auto facts = Facts(); const auto anon = Anonymous(facts);
        input.anonymous = anon.IsReady() ? Fact::Known(anon.value) : Fact{FactState::Unknown};
        input.validLocation = Fact::Known(current.has_value());
        input.startGraceElapsed = Fact::Known(Milliseconds() - sceneStarted >= 10000);
        input.sceneEnded = Fact::Known(!scene.Active());
        input.allowForeplayFame = Fact::Known(Setting("AllowForeplayFame", 1) != 0);
        input.bodySearch = Fact::Known(false);
        input.sexWorker = Fact::Known(api.GetAggregatedFlag("Sex Worker"));
        input.publicWhore = integrations::ExternalPublicWhoreFact(external.Snapshot());
        input.submissiveDefault = Fact::Known(Setting("SubmissiveDefault") != 0);
        input.dominantDefault = Fact::Known(Setting("DominantDefault") != 0);
        const auto bound = visibility.EvaluateBondage(facts);
        input.canGainBoundFame = bound.visiblyBound.IsReady() && bound.heavilyBound.IsReady() ?
            Fact::Known(bound.visiblyBound.value && (bound.heavilyBound.value || destination && api.GetFameValue(api.RegionName(*destination), "Bound").value_or(0) < 75)) : Fact{FactState::Unknown};
        input.victimsAreMasochist = Fact::Known(Setting("VictimsAreMasochist") != 0);
        input.allowLikeFameWhenRaped = Fact::Known(Setting("AllowLikeFameWhenRaped") != 0);
        input.allowBestialityWhenRaped = Fact::Known(Setting("AllowBestialityWhenRaped") != 0);
        input.deviousDevicesInstalled = facts.mods.deviousDevices; input.settings = Witness();
        input.friendKissAndTellChance = static_cast<int>(Setting("SexFameChanceByFriend", 50));
        input.loverKissAndTellChance = static_cast<int>(Setting("SexFameChanceByLover", 25));
        const auto result = supplied ? AnimationClassifier{}.ClassifySubAnalyze(input, (*supplied)[3].boolValue, (*supplied)[5].boolValue, (*supplied)[6].boolValue) : AnimationClassifier{}.Classify(input, random);
        if (!result.IsReady() || result.kind == AnimationResultKind::Deferred) return false;
        if (result.triggerNpcScan) Scan();
        if (destination) for (const auto category : result.categories) {
            GainFameInputs gain; gain.foreplay = Fact::Known(result.foreplay);
            (void)api.GainFame(api.RegionName(*destination), std::string(kFameCategoryNames[static_cast<std::size_t>(category)]), gain);
        }
        analyzePending = false;
        consumedStage = snapshot.sceneId;
        return true;
    }
};

Gameplay::Gameplay(api::PublicApi& api, const RegionSnapshot& regions, std::map<StableFormKey, RegionId> owners)
    : impl_(std::make_unique<Impl>(api, regions, std::move(owners))) {}
Gameplay::~Gameplay() = default;

std::optional<RegionId> Gameplay::ResolveForm(const StableFormKey& key) const {
    auto* location = Form<RE::BGSLocation>(key);
    return impl_->Excluded(location) ? std::nullopt : impl_->Resolve(location);
}

bool Gameplay::Invoke(const std::string& fullName, const std::vector<Value>& args, Value& out) {
    auto& x = *impl_;
    if (!x.api.BusinessOpen()) return false;
    if (x.epoch != x.api.CurrentEpoch()) { x.epoch = x.api.CurrentEpoch(); x.Restore(); }
    x.settings = x.api.Executor().Settings();
    const auto separator = fullName.rfind('.');
    const auto name = separator == std::string::npos ? fullName : fullName.substr(separator + 1);
    const auto text = [&args](std::size_t i) { return i < args.size() ? args[i].stringValue : std::string{}; };
    const auto integer = [&args](std::size_t i) { return i < args.size() ? static_cast<int>(args[i].intValue) : 0; };
    const auto boolean = [&args](std::size_t i) { return i < args.size() && args[i].boolValue; };
    const auto number = [&args](std::size_t i) { return i < args.size() ? args[i].floatValue : 0.0; };
    const auto location = [&args](std::size_t i) { return i < args.size() && args[i].hasForm ? Form<RE::BGSLocation>(args[i].formValue) : nullptr; };
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (name == "SceneEvent") { OnEvent(text(0), std::to_string(integer(1))); return true; }
    if (name == "Equipped") {
        auto* form = args.empty() || !args[0].hasForm ? nullptr : Form<RE::TESForm>(args[0].formValue);
        const std::string item = form ? form->GetName() : "";
        if (item == "Skooma" || item == "Kordir's Skooma" || item == "Redwater Skooma" || item == "Double-Distilled Skooma") {
            const auto object = Object("SLSF_Reloaded_PlayerScript");
            const auto* property = Property(object, "SLSF_Reloaded_Skooma");
            auto* global = property ? property->Unpack<RE::TESGlobal*>() : nullptr;
            if (global) global->value = (std::min)(150.0f, global->value + 1);
        }
        return true;
    }
    if (name == "RunUninstall") {
        x.stopped = true; x.external.Cancel(); x.scene.Cancel();
        const auto integration = Object("SLSF_Reloaded_ModIntegration");
        for (const auto* name : {"FHU", "PW_Tracker", "LegacyConfig"})
            if (auto* property = integration ? integration->GetProperty(name) : nullptr) property->SetNone();
        const auto uninstall = Object("SLSF_Reloaded_Uninstall");
        for (const auto* name : {"MainQuest", "ManagerQuest", "ConfigQuest"}) {
            const auto* property = Property(uninstall, name);
            if (auto* quest = property ? property->Unpack<RE::TESQuest*>() : nullptr) quest->Stop();
        }
        Global("SLSF_Reloaded_PlayerScript", "SLSF_AllowComment", 0);
        return true;
    }
    if (x.stopped) return false;
    if (name == "LocationCanBeRegistered") {
        auto* loc = player->GetCurrentLocation();
        out = Boolean(!text(0).empty() && text(0) != "Wilderness" && !x.api.ResolveRegionByName(text(0)).has_value() && (boolean(1) || loc && !x.Excluded(loc))); return true;
    }
    if (name == "FetchLocationName") { auto* loc = location(0); out = Text(loc && loc->GetName()[0] ? loc->GetName() : "$NoneText"); return true; }
    if (name == "GetLocalizedFameLocation" || name == "GetFameLocation") {
        auto* requested = location(0);
        const auto region = x.Excluded(requested) ? std::nullopt : x.Resolve(requested);
        if (!region) out = Text(name == "GetFameLocation" ? "Null" : "$NoneText");
        else if (name == "GetLocalizedFameLocation") {
            auto* loc = Form<RE::BGSLocation>({x.regions.regions[region->value].plugin, x.regions.regions[region->value].localFormId});
            out = Text(loc ? loc->GetName() : x.api.RegionName(*region));
        } else out = Text(x.api.RegionName(*region));
        return true;
    }
    if (name == "IsLocationExcluded") { out = Boolean(x.Excluded(location(0))); return true; }
    if (name == "RegisterCustomLocation" || name == "AddToLocationList" || name == "RegisterCustomLocationExternal" || name == "OnExternalLocationRegister") {
        auto* loc = name == "RegisterCustomLocation" ? player->GetCurrentLocation() : location(name == "RegisterCustomLocationExternal" ? 1 : 0);
        if (loc && !x.Excluded(loc)) {
            const auto key = Key(loc);
            const auto display = name == "RegisterCustomLocationExternal" ? text(0) : std::string(loc->GetName());
            (void)x.api.RegisterCustomLocation(display, {key.plugin, key.localFormId});
            Global("SLSF_Reloaded_LocationManager", "SLSF_Reloaded_CustomLocationCount", static_cast<double>(x.api.CustomLocations().size()));
        }
        return true;
    }
    if (name == "RemoveFromLocationList" || name == "UnregisterCustomLocationExternal" || name == "UnregisterCustomLocation" || name == "OnExternalLocationUnregister") {
        auto display = text(0); const auto custom = x.api.CustomLocations();
        if (name == "UnregisterCustomLocation" && integer(0) >= 0 && static_cast<std::size_t>(integer(0)) < custom.size()) display = custom[integer(0)].name;
        if (name == "OnExternalLocationUnregister") for (const auto& item : custom) {
            const auto key = Key(location(0));
            if (item.key.plugin == key.plugin && item.key.localFormId == key.localFormId) display = item.name;
        }
        (void)x.api.UnregisterCustomLocation(display);
        Global("SLSF_Reloaded_LocationManager", "SLSF_Reloaded_CustomLocationCount", static_cast<double>(x.api.CustomLocations().size()));
        return true;
    }
    if (name == "UpdateCustomLocationCount") {
        Global("SLSF_Reloaded_LocationManager", "SLSF_Reloaded_CustomLocationCount", static_cast<double>(x.api.CustomLocations().size())); return true;
    }
    if (name == "CompareLocations") {
        const auto next = x.Resolve(location(1));
        if (!x.current) x.current = x.Resolve(location(0));
        TransitionRecognition(x.anonymity, x.current, next, x.api.CurrentGameTime()); x.Persist(); return true;
    }
    if (name == "SetLastEnterTime" || name == "SetLastExitTime" || name == "CheckRecognitionTimeIncrease" || name == "CheckRecognitionTimeDecay" || name == "RecognitionTime") {
        const auto region = x.api.ResolveRegionByName(text(0));
        if (!region) { out = Number(0); return true; }
        auto& memory = x.anonymity.Memory(*region);
        if (name == "SetLastEnterTime" && std::isfinite(number(1)) && number(1) >= 0) memory.lastEnter = GameTime{number(1)};
        else if (name == "SetLastExitTime" && std::isfinite(number(1)) && number(1) >= 0) memory.lastExit = GameTime{number(1)};
        else if (name == "CheckRecognitionTimeIncrease") x.anonymity.CalculateStoredIncrease(*region);
        else if (name == "CheckRecognitionTimeDecay") x.anonymity.CalculateStoredDecay(*region);
        out = Number(memory.recognitionDays); x.Persist(); return true;
    }
    if (name == "ThaneStatus") { out = Boolean(x.Thane(text(0))); return true; }
    if (name == "IsFMPregnant" || name == "IsECPregnant" || name == "IsESPregnant" || name == "IsHentaiPregnant") {
        auto* target = !args.empty() && args[0].hasForm ? Form<RE::Actor>(args[0].formValue) : nullptr;
        if (!target || !target->GetActorBase()) { out = Boolean(false); return true; }
        const char* plugin = name == "IsFMPregnant" ? "Fertility Mode 3 Fixes and Updates.esp" : name == "IsECPregnant" ? "EstrusChaurus.esp" : name == "IsESPregnant" ? "EstrusSpider.esp" : "HentaiPregnancy.esm";
        const std::uint32_t id = name == "IsFMPregnant" ? 0x862 : name == "IsECPregnant" ? 0x160A9 : name == "IsESPregnant" ? 0x4E258 : 0x12085;
        auto* faction = Form<RE::TESFaction>({plugin, id});
        const int rank = faction ? target->GetFactionRank(faction, false) : -1;
        out = Boolean(target->GetActorBase()->GetSex() == RE::SEX::kFemale && (name == "IsFMPregnant" ? rank > 30 : name == "IsHentaiPregnant" ? rank > 1 : rank >= 0));
        return true;
    }
    if (name == "IsPublicWhore" || name == "GetFHUInflation") {
        const auto snapshot = x.external.Snapshot();
        if (name == "IsPublicWhore") { out = Boolean(snapshot.publicWhoreActive); return snapshot.availability == integrations::ExternalAvailability::Available; }
        const auto* actor = !args.empty() && args[0].hasForm ? snapshot.FindActor(args[0].formValue) : nullptr;
        if (!actor) return false;
        if (name == "GetFHUInflation") { out = Number(actor->inflation); return actor->inflationKnown; }
        out = Boolean(name == "IsFMPregnant" ? actor->fertilityModeBreeder : name == "IsECPregnant" ? actor->estrusChaurusBreeder : name == "IsESPregnant" ? actor->estrusSpiderBreeder : actor->hentaiPregnant); return true;
    }
    if (name == "FameGainRoll") { x.Roll(text(0), boolean(2), boolean(3)); return true; }
    if (name == "CheckExternalFlags") {
        for (const auto category : kFameCategoryNames) {
            std::string property(category); std::erase(property, ' ');
            BoolProperty("SLSF_Reloaded_FameManager", ("External" + property + "Flag").c_str(), x.api.GetAggregatedFlag(std::string(category)));
        }
        return true;
    }
    if (name == "RunNPCDetect" || name == "ObserveNpc") { x.Scan(!args.empty() && args[0].hasForm ? Form<RE::Actor>(args[0].formValue) : nullptr); return true; }
    if (name == "UpdateFame") {
        (void)x.api.Executor().AdvanceTime(GameTime{RE::Calendar::GetSingleton()->GetCurrentGameTime()});
        x.CleanupMissingCategories();
        x.Scan(nullptr, args.empty() ? 1 : (std::max)(1, integer(0))); compat::VmNatives::PublishOutputs(); return true;
    }
    if (name == "DecayFame") {
        const auto multiplier = args.empty() ? 1 : integer(0);
        if (multiplier < 1) return false;
        return x.api.Executor().DecayBatch(multiplier).status == Status::Ready;
    }
    if (name == "AnimationSubAnalyze") return x.Analyze(&args);
    if (name == "AnimationAnalyze") {
        x.scene.SetThreadId(integer(0));
        x.sceneStarted = Milliseconds(); x.analyzePending = true;
        x.scene.RequestScene(x.api.CurrentEpoch(), Milliseconds()); return true;
    }
    auto facts = x.Facts();
    if (name == "GetAnonymity") {
        const auto region = x.Resolve(location(0));
        if (!region) return true;
        const auto value = x.Anonymous(facts, region);
        if (value.IsReady()) BoolProperty("SLSF_Reloaded_DynamicAnonymity", "IsAnonymous", value.value);
        out = Boolean(value.value); return value.IsReady();
    }
    const auto anonymous = x.Anonymous(facts);
    facts.anonymous = anonymous.IsReady() ? Fact::Known(anonymous.value) : Fact{FactState::Unknown};
    if (name == "GetAnonymity" || name == "IsPlayerAnonymous") {
        out = Boolean(anonymous.value);
        BoolProperty("SLSF_Reloaded_DynamicAnonymity", "IsAnonymous", anonymous.value); return anonymous.IsReady();
    }
    const auto tats = x.TattooCounts(facts);
    if (name == "CountVisibleTattoos" || name == "CountBodyTattoos" || name == "CountFaceTattoos" || name == "CountHandTattoos" || name == "CountFootTattoos") {
        out = Integer(name == "CountVisibleTattoos" ? tats[0] + tats[1] + tats[2] + tats[3] :
            name == "CountBodyTattoos" ? tats[0] : name == "CountFaceTattoos" ? tats[1] : name == "CountHandTattoos" ? tats[2] : tats[3]); return true;
    }
    if (name == "CountAppliedTattoos") {
        const auto area = text(0);
        if (area != "Body" && area != "Face" && area != "Hands" && area != "Feet") { out = Integer(0); return true; }
        const auto& count = area == "Body" ? facts.tattoos.body : area == "Face" ? facts.tattoos.face : area == "Hands" ? facts.tattoos.hands : facts.tattoos.feet;
        out = Integer(count.appliedCount); return count.appliedCountKnown.IsKnownTrue();
    }
    if (name == "IsBodyTattooVisible" || name == "IsFaceTattooVisible" || name == "IsHandTattooVisible" || name == "IsFootTattooVisible") {
        const auto slot = static_cast<std::size_t>(integer(0));
        const auto result = name == "IsBodyTattooVisible" ? x.visibility.BodyTattooVisible(facts, slot) : name == "IsFaceTattooVisible" ?
            x.visibility.FaceTattooVisible(facts, slot) : name == "IsHandTattooVisible" ? x.visibility.HandTattooVisible(facts, slot) : x.visibility.FootTattooVisible(facts, slot);
        out = Boolean(result.value); return result.IsReady();
    }
    if (name == "CumVisibility" || name == "IsAssCumVisible" || name == "CountVisibleAssCum" || name == "IsVaginalCumVisible" || name == "CountVisibleVaginalCum" || name == "IsOralCumVisible" || name == "CountVisibleOralCum") {
        if (name == "CumVisibility" && text(0) == "Any") {
            const std::array values{x.visibility.VisibleAssCum(facts), x.visibility.VisibleVaginalCum(facts), x.visibility.VisibleOralCum(facts)};
            if (std::any_of(values.begin(), values.end(), [](const auto& value) { return value.IsReady() && value.value > 0; })) { out = Boolean(true); return true; }
            out = Boolean(false); return std::all_of(values.begin(), values.end(), [](const auto& value) { return value.IsReady(); });
        }
        if (name == "CumVisibility" && text(0) != "Anal" && text(0) != "Ass" && text(0) != "Vaginal" && text(0) != "Oral") { out = Boolean(false); return true; }
        const bool ass = name.find("Ass") != std::string::npos || text(0) == "Anal" || text(0) == "Ass";
        const bool vaginal = name.find("Vaginal") != std::string::npos || text(0) == "Vaginal";
        const auto value = ass ? x.visibility.VisibleAssCum(facts) : vaginal ? x.visibility.VisibleVaginalCum(facts) : x.visibility.VisibleOralCum(facts);
        out = name.starts_with("Count") ? Integer(value.value) : Boolean(value.value > 0); return value.IsReady();
    }
    if (name == "CheckBondage" || name == "FullUpdate" || name == "UpdateTattooSlots" || name == "CheckAppliedTattoos" || name == "Startup") {
        const auto bondage = x.visibility.EvaluateBondage(facts);
        const std::array<const TattooAreaFacts*, 4> areas{&facts.tattoos.body, &facts.tattoos.face, &facts.tattoos.hands, &facts.tattoos.feet};
        for (std::size_t a = 0; a < areas.size(); ++a) {
            std::vector<bool> applied, visible;
            for (std::size_t slot = 0; slot < areas[a]->slots.size(); ++slot) {
                applied.push_back(areas[a]->slots[slot].applied.IsKnownTrue());
                const auto decision = a == 0 ? x.visibility.BodyTattooVisible(facts, slot) : a == 1 ? x.visibility.FaceTattooVisible(facts, slot) : a == 2 ? x.visibility.HandTattooVisible(facts, slot) : x.visibility.FootTattooVisible(facts, slot);
                visible.push_back(decision.IsReady() && decision.value);
            }
            const std::string prefix = a == 0 ? "Body" : a == 1 ? "Face" : a == 2 ? "Hand" : "Foot";
            if (areas[a]->appliedCountKnown.IsKnownTrue()) { SetProperty((prefix + "TattooApplied").c_str(), applied); SetProperty((prefix + "TattooVisible").c_str(), visible); }
        }
        SetProperty("VisibleBodyTats", tats[0]);
        if (bondage.collarVisible.IsReady()) SetProperty("DD_CollarVisible", bondage.collarVisible.value);
        if (bondage.braVisible.IsReady()) SetProperty("DD_BraVisible", bondage.braVisible.value);
        if (bondage.beltVisible.IsReady()) SetProperty("DD_BeltVisible", bondage.beltVisible.value);
        if (bondage.harnessVisible.IsReady()) SetProperty("DD_HarnessVisible", bondage.harnessVisible.value);
        const auto set = [](const char* property, const RuleDecision& value) { if (value.IsReady()) Global("SLSF_Reloaded_VisibilityManager", property, value.value ? 1 : 0); };
        set("IsLightlyBound", bondage.lightlyBound); set("IsHeavilyBound", bondage.heavilyBound);
        set("IsVisiblyBound", bondage.visiblyBound); set("IsBelted", bondage.beltWorn); set("IsCollared", bondage.collarWorn);
        const std::array<const char*, 3> countNames{"AnalCumCountGlobal", "VaginalCumCountGlobal", "OralCumCountGlobal"};
        const std::array<const char*, 3> visibilityNames{"AnalCumGlobal", "VaginalCumGlobal", "OralCumGlobal"};
        const std::array counts{x.visibility.VisibleAssCum(facts), x.visibility.VisibleVaginalCum(facts), x.visibility.VisibleOralCum(facts)};
        std::int64_t total{};
        for (std::size_t i = 0; i < counts.size(); ++i) if (counts[i].IsReady()) {
            Global("SLSF_Reloaded_VisibilityManager", countNames[i], counts[i].value);
            Global("SLSF_Reloaded_VisibilityManager", visibilityNames[i], counts[i].value > 0 ? 1 : 0); total += counts[i].value;
        }
        Global("SLSF_Reloaded_VisibilityManager", "TotalCumCountGlobal", total);
        return true;
    }
    return false;
}

void Gameplay::OnEvent(std::string_view topic, std::string_view payload) {
    auto& x = *impl_;
    if (!x.api.BusinessOpen() || x.stopped) return;
    x.external.OnModEvent(topic, Milliseconds());
    x.scene.OnSceneNotification(topic, payload, x.api.CurrentEpoch(), Milliseconds());
    if (topic.ends_with("AnimationEnd") || topic.ends_with("SceneEnd")) {
        if (!x.scene.Active()) x.analyzePending = false;
        return;
    }
    if (topic.ends_with("AnimationStart") || topic.ends_with("SceneStart") || topic.ends_with("LeadInEnd")) {
        x.sceneStarted = Milliseconds(); x.consumedStage.clear();
        if (x.scene.Active()) x.analyzePending = true;
    }
}

void Gameplay::Tick() {
    auto& x = *impl_;
    if (!x.api.BusinessOpen()) return;
    const auto epoch = x.api.CurrentEpoch();
    if (x.epoch != epoch) { x.epoch = epoch; x.Restore(); }
    x.settings = x.api.Executor().Settings();
    if (x.stopped) return;
    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) return;
    auto* calendar = RE::Calendar::GetSingleton();
    if (!calendar) return;
    const auto now = GameTime{calendar->GetCurrentGameTime()};
    auto& dispatcher = x.api.Executor();
    dispatcher.Clocks().SetMenuOpen(RE::UI::GetSingleton()->GameIsPaused());
    dispatcher.WakeTick(now);
    dispatcher.Step();
    (void)dispatcher.AdvanceTime(now);
    x.calls.Advance(Milliseconds()); x.external.Advance(Milliseconds()); x.scene.Advance(Milliseconds());
    const auto region = x.Excluded(player->GetCurrentLocation()) ? std::nullopt : x.Resolve(player->GetCurrentLocation());
    if (region != x.current) {
        TransitionRecognition(x.anonymity, x.current, region, now); x.Persist();
    }
    x.api.SetCurrentLocation(region ? x.api.RegionName(*region) : "");
    if (region) { x.anonymity.AdvanceHourly(*region, now); x.Persist(); }
    const auto real = dispatcher.Clocks().Active().value;
    if (real >= x.nextScan) {
        x.nextScan = real + 5;
        if (x.external.Gate() != integrations::ExternalGate::Waiting) x.external.OnStateChanged(epoch);
        x.external.RequestState(epoch, {Key(player)}, Milliseconds());
        x.RequestCum();
        Value unused; Invoke("FullUpdate", {}, unused);
        compat::VmNatives::PublishOutputs();
    }
    if (now.days >= x.nextGameScan) { x.nextGameScan = now.days + x.Setting("ScanPeriodDays", 1.0 / 24); x.CleanupMissingCategories(); x.Scan(); }
    if (now.days - x.lastSkoomaDay >= 1.0 / 48) {
        const auto elapsed = std::floor((now.days - x.lastSkoomaDay) * 48);
        const auto object = Object("SLSF_Reloaded_PlayerScript"); const auto* property = Property(object, "SLSF_Reloaded_Skooma");
        auto* global = property ? property->Unpack<RE::TESGlobal*>() : nullptr;
        if (global) global->value = static_cast<float>((std::max)(0.0, global->value - elapsed));
        x.lastSkoomaDay += elapsed / 48;
    }
    if (x.analyzePending) x.Analyze();
    if (real >= x.nextComment) {
        CommentFacts input; const auto anon = x.Anonymous(x.Facts());
        input.anonymous = anon.IsReady() ? Fact::Known(anon.value) : Fact{FactState::Unknown};
        input.combatState = ObservedNumber::Known(player->IsInCombat() ? 1 : 0);
        input.commentFrequency = ObservedNumber::Known(static_cast<int>(x.Setting("CommentFrequency", 50)));
        input.publicWhore = integrations::ExternalPublicWhoreFact(x.external.Snapshot());
        input.disableNakedCommentsWhilePW = Fact::Known(x.Setting("DisableNakedCommentsWhilePW") != 0);
        input.andInstalled = Fact::Known(Installed("Advanced Nudity Detection.esp"));
        const auto result = CommentRules{}.Evaluate(input, x.random);
        if (result.status == Status::Ready) Global("SLSF_Reloaded_PlayerScript", "SLSF_AllowComment", result.allowed ? 1 : 0);
        if (result.naked.changes && result.naked.status == Status::Ready) Global("SLSF_Reloaded_CommentManager", "WICommentChanceNaked", result.naked.chance);
        x.nextComment = real + result.nextDelaySeconds;
    }
}
}
