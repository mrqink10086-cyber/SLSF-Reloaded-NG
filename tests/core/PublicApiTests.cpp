#include <slsf/api/PublicApi.h>
#include <slsf/core/Commands.h>
#include <slsf/core/Configuration.h>
#include <slsf/core/Snapshots.h>
#include <slsf/core/Types.h>
#include <slsf/core/reputation/DiffusionGraph.h>
#include <slsf/core/reputation/ReputationState.h>
#include <slsf/core/reputation/Settlement.h>
#include <slsf/runtime/Dispatcher.h>
#include <slsf/ui/FameMenu.h>

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

using slsf::ExplicitCommandKind;
using slsf::FameCategory;
using slsf::LocationId;
using slsf::RegionId;
using slsf::StableRegionKey;

namespace {

std::uint32_t gNextRegion = 1;

StableRegionKey Key(std::uint32_t i) {
    return StableRegionKey{"location.esp", i + 1};
}
slsf::RegionSnapshot MakeRegions() {
    slsf::RegionSnapshot snapshot;
    snapshot.regions = {Key(0), Key(1), Key(2), Key(3), Key(4)};
    snapshot.customOrder = snapshot.regions;
    snapshot.regionApiIndex = {
        slsf::ApiLocationIndex{0}, slsf::ApiLocationIndex{1}, slsf::ApiLocationIndex{2},
        slsf::ApiLocationIndex{3}, slsf::ApiLocationIndex{4}};
    snapshot.apiAliases = {{"Whiterun", Key(0)}, {"Winterhold", Key(1)}, {"Solitude", Key(2)}};
    snapshot.aliasIndex = {
        {"solitude", slsf::ApiLocationIndex{2}},
        {"whiterun", slsf::ApiLocationIndex{0}},
        {"winterhold", slsf::ApiLocationIndex{1}},
    };
    return snapshot;
}
slsf::State MakeState(std::size_t count = 5) {
    slsf::State state;
    state.accounts.resize(count);
    state.accountKeys.resize(count);
    state.locationAccounts.resize(count);
    for (std::size_t i = 0; i < count; ++i) {
        state.accountKeys[i] = Key(static_cast<std::uint32_t>(i));
        state.locationAccounts[i] = RegionId(static_cast<std::uint32_t>(i));
    }
    return state;
}
slsf::compat::ModEventValue EventString(std::string value) {
    return {.type = slsf::compat::ModEventType::String, .stringValue = std::move(value)};
}
slsf::compat::ModEventValue EventBool(bool value) {
    return {.type = slsf::compat::ModEventType::Bool, .boolValue = value};
}
slsf::compat::ModEventValue EventInt(int value) {
    return {.type = slsf::compat::ModEventType::Int, .intValue = value};
}

class DeterministicQueue final : public slsf::IRandom {
public:
    std::int32_t UniformInt(std::int32_t lo, std::int32_t) override { return lo; }
};

struct ApiFixture {
    slsf::DiffusionGraph graph;
    slsf::Parameters parameters = slsf::LegacyDefaultParameters();
    slsf::Configuration configuration;
    slsf::runtime::RuntimeBindings bindings;
    slsf::runtime::Dispatcher dispatcher;
    slsf::RegionSnapshot regions = MakeRegions();
    slsf::api::PublicApi api;

    ApiFixture(std::shared_ptr<slsf::runtime::SessionGate> hostGate = {})
        : bindings(MakeBindings())
        , dispatcher(std::move(hostGate))
        , api(dispatcher, regions) {
        static DeterministicQueue queue;
        bindings.rng = &queue;
        CHECK(dispatcher.Attach(slsf::Epoch{2, 0, 0}, bindings, MakeState()));
    }

    slsf::runtime::RuntimeBindings MakeBindings() {
        slsf::runtime::RuntimeBindings b;
        b.graph = &graph;
        b.parameters = &parameters;
        b.configuration = &configuration;
        b.regions = nullptr;
        b.rng = nullptr;
        return b;
    }
};

TEST_CASE("legacy import commits fame and save-owned metadata together", "[public-api][game-ready]") {
    ApiFixture fx;
    REQUIRE(fx.api.SetFameValue("Whiterun", "Whore", 20).status == slsf::Status::Ready);
    const auto revision = fx.api.LastPublishedRevision();
    slsf::io::LegacyImport invalid;
    invalid.fame = {{"Whiterun", FameCategory::Whore, 12}, {"Unknown", FameCategory::Whore, 7}};
    CHECK(fx.api.ImportLegacy(invalid).status == slsf::Status::Invalid);
    CHECK(fx.api.GetFameValue("Whiterun", "Whore") == 20);
    CHECK(fx.api.LastPublishedRevision() == revision);

    slsf::io::LegacyImport input;
    input.customRegions = {{"Favorite House", Key(3)}};
    input.fame = {{"Favorite House", FameCategory::Whore, 37}};
    input.spread = {{"Favorite House", 73}};
    input.mods = {"example.esp"};
    input.modFlags = {{"example.esp", {{"Whore", true}, {"Sex Worker", true}}}};
    REQUIRE(fx.api.ImportLegacy(input).status == slsf::Status::Ready);
    CHECK(fx.api.GetFameValue("Favorite House", "Whore") == 37);
    CHECK(fx.api.GetModFlag("example.esp", "Whore"));
    CHECK(fx.api.GetAggregatedFlag("Sex Worker"));
    CHECK(fx.api.SpreadChance("Favorite House") == 73);
    REQUIRE(fx.dispatcher.Capture());
    CHECK(fx.dispatcher.Capture()->namedProviders.size() == 1);
    CHECK(fx.dispatcher.Capture()->customRegions.size() == 1);
    const auto exported = fx.api.ExportLegacy();
    const auto extracted = slsf::io::ExtractLegacyImport(slsf::io::LegacyDocument::ToJsonPtr(exported), slsf::api::PublicApi::FlagNames());
    REQUIRE(extracted);
    ApiFixture restored;
    REQUIRE(restored.api.ImportLegacy(*extracted).status == slsf::Status::Ready);
    CHECK(restored.api.GetFameValue("Favorite House", "Whore") == 37);
    CHECK(restored.api.SpreadChance("Favorite House") == 73);
    CHECK(restored.api.GetModFlag("example.esp", "Whore"));
    CHECK(restored.api.GetAggregatedFlag("Sex Worker"));
    CHECK(restored.api.UnregisterMod("example.esp"));
    CHECK_FALSE(restored.api.GetAggregatedFlag("Whore"));
}

TEST_CASE("built-in form keys cannot be registered or imported as custom locations", "[public-api][game-ready]") {
    ApiFixture fx;
    REQUIRE(fx.api.SetFameValue("Whiterun", "Whore", 20).status == slsf::Status::Ready);
    CHECK_FALSE(fx.api.RegisterCustomLocation("New Whiterun", Key(0)));

    slsf::io::LegacyImport input;
    input.customRegions = {{"Favorite House", Key(3)}, {"New Whiterun", Key(0)}};
    input.fame = {{"New Whiterun", FameCategory::Whore, 37}};
    CHECK(fx.api.ImportLegacy(input).status == slsf::Status::Invalid);
    CHECK(fx.api.CustomLocations().empty());
    CHECK(fx.api.GetFameValue("Whiterun", "Whore") == 20);
}

TEST_CASE("unregistering a saved alias preserves the built-in location state", "[public-api][game-ready]") {
    ApiFixture fx;
    REQUIRE(fx.api.SetFameValue("Whiterun", "Whore", 37).status == slsf::Status::Ready);
    REQUIRE(fx.api.SetSpreadChance("Whiterun", 73));
    const slsf::RegionRecognitionState recognition{Key(0), 3.0, 2.0, 1.0, true, true, true};
    REQUIRE(fx.dispatcher.EditState([&](slsf::State& state) -> slsf::Result<void> {
        state.customRegions = {{"Old Whiterun", Key(0)}};
        state.recognition = {recognition};
        return {};
    }).status == slsf::Status::Ready);

    std::vector<StableRegionKey> removed;
    fx.api.SetCustomLocationRemoved([&](const StableRegionKey& key) { removed.push_back(key); });
    REQUIRE(fx.api.UnregisterCustomLocation("Old Whiterun"));
    CHECK(removed.empty());
    CHECK_FALSE(fx.api.ResolveRegionByName("Old Whiterun"));
    CHECK(fx.api.GetFameValue("Whiterun", "Whore") == 37);
    CHECK(fx.api.SpreadChance("Whiterun") == 73);
    const auto captured = fx.dispatcher.Capture();
    REQUIRE(captured);
    CHECK(captured->recognition == std::vector{recognition});
}

TEST_CASE("unregistering a custom location clears its save-owned state", "[public-api][game-ready]") {
    ApiFixture fx;
    REQUIRE(fx.api.RegisterCustomLocation("Favorite House", Key(3)));
    REQUIRE(fx.api.RegisterCustomLocation("Other House", Key(4)));
    REQUIRE(fx.api.SetFameValue("Favorite House", "Whore", 37).status == slsf::Status::Ready);
    REQUIRE(fx.api.SetFameValue("Other House", "Slut", 20).status == slsf::Status::Ready);
    REQUIRE(fx.api.SetSpreadChance("Favorite House", 73));
    REQUIRE(fx.api.SetSpreadChance("Other House", 54));
    REQUIRE(fx.dispatcher.EditState([](slsf::State& state) -> slsf::Result<void> {
        auto& account = state.accounts[3];
        account.remote[FameCategory::Slut] = 45;
        account.flags[static_cast<std::size_t>(FameCategory::Whore)] = 1;
        account.protectionUntilDay[static_cast<std::size_t>(FameCategory::Whore)] = 12;
        account.anonymity = 1;
        account.auxiliaryState[0] = 1;
        slsf::RecomputeAccount(account);
        slsf::RebuildDerivedSets(state);
        state.recognition = {{Key(3), 3.0, 2.0, 1.0, true, true, true}, {Key(4), 1.0}};
        return {};
    }).status == slsf::Status::Ready);

    std::vector<StableRegionKey> removed;
    fx.api.SetCustomLocationRemoved([&](const StableRegionKey& key) {
        removed.push_back(key);
        CHECK_FALSE(fx.api.ResolveRegionByName("Favorite House"));
        const auto saved = fx.dispatcher.Capture();
        REQUIRE(saved);
        CHECK(saved->recognition == std::vector<slsf::RegionRecognitionState>{{Key(4), 1.0}});
    });
    fx.dispatcher.Suspend();
    CHECK_FALSE(fx.api.UnregisterCustomLocation("Favorite House"));
    CHECK(removed.empty());
    fx.dispatcher.Resume();
    REQUIRE(fx.api.UnregisterCustomLocation("favorite house"));
    CHECK_FALSE(fx.api.ResolveRegionByName("Favorite House"));
    CHECK_FALSE(fx.api.UnregisterCustomLocation("Favorite House"));
    CHECK(removed == std::vector{Key(3)});
    const auto captured = fx.dispatcher.Capture();
    REQUIRE(captured);
    REQUIRE(captured->locations.size() == 5);
    const auto& cleared = captured->locations[3];
    CHECK(cleared.key == Key(3));
    CHECK(cleared.local[static_cast<std::size_t>(FameCategory::Whore)] == 0);
    CHECK(cleared.remote[static_cast<std::size_t>(FameCategory::Slut)] == 0);
    CHECK(cleared.flags[static_cast<std::size_t>(FameCategory::Whore)] == 0);
    CHECK(cleared.protectionUntilDay[static_cast<std::size_t>(FameCategory::Whore)] == 0);
    CHECK(cleared.anonymity == 0);
    CHECK(cleared.auxiliaryState[0] == 0);
    CHECK(captured->recognition == std::vector<slsf::RegionRecognitionState>{{Key(4), 1.0}});
    CHECK(captured->spreadChances == std::vector<slsf::RegionSpreadState>{{Key(4), 54}});
    CHECK(fx.api.GetFameValue("Other House", "Slut") == 20);

    REQUIRE(fx.api.RegisterCustomLocation("New House", Key(3)));
    CHECK(fx.api.GetFameValue("New House", "Whore") == 0);
    CHECK(fx.api.GetFameValue("New House", "Slut") == 0);
    CHECK(fx.api.SpreadChance("New House") == 0);
}

struct RecordingSink : slsf::compat::LegacyOutputSink {
    std::vector<std::pair<slsf::StableFormKey, int>> globals;
    std::vector<std::tuple<std::string, std::size_t, int>> storage;
    std::vector<std::pair<slsf::StableFormKey, int>> bridge;

    void SetCurrentFameGlobal(const slsf::StableFormKey& formId, int value) override {
        globals.emplace_back(formId, value);
    }
    void SetStorageIntList(const std::string& key, std::size_t locationIndex,
                          int value) override {
        storage.emplace_back(key, locationIndex, value);
    }
    void SetLegacyBridgeGlobal(const slsf::StableFormKey& formId, int value) override {
        bridge.emplace_back(formId, value);
    }
};

TEST_CASE("menu writes publish compatibility values while active time is paused", "[menu-fame]") {
    ApiFixture fx;
    fx.api.SetCurrentLocation("Whiterun");
    fx.dispatcher.Clocks().SetMenuOpen(true);
    RecordingSink sink;
    int publications = 0;
    const auto result = slsf::ui::ExecuteFameCommand(fx.api, slsf::ui::FameCommand::SetFame,
        "Whiterun", "", "Cum Dump", 100, 100, [&] {
            ++publications;
            (void)fx.api.PublishAllOutputs(sink);
        });
    REQUIRE(result.status == slsf::Status::Ready);
    REQUIRE(fx.api.GetFameValue("Whiterun", "Cum Dump") == 100);
    REQUIRE(publications == 1);
    const auto key = slsf::compat::LegacyOutputs::CurrentFameGlobalKey(FameCategory::CumDump);
    REQUIRE(std::find(sink.globals.begin(), sink.globals.end(), std::pair{key, 100}) != sink.globals.end());
}

TEST_CASE("menu readout follows committed regional fame independently of multipliers", "[menu-fame]") {
    ApiFixture fx;
    fx.api.SetCurrentLocation("Winterhold");
    REQUIRE(fx.api.SetFameValue("Whiterun", "Cum Dump", 100).status == slsf::Status::Ready);
    REQUIRE(fx.api.SetFameValue("Winterhold", "Whore", 37).status == slsf::Status::Ready);
    const auto selected = slsf::ui::ReadFameMenu(fx.api, "Whiterun");
    REQUIRE(selected.values);
    CHECK((*selected.values)[slsf::FameCategoryIndex(FameCategory::CumDump)] == 100);
    CHECK(selected.currentLocation == "Winterhold");
    const auto current = slsf::ui::ReadFameMenu(fx.api, "");
    REQUIRE(current.values);
    CHECK((*current.values)[slsf::FameCategoryIndex(FameCategory::Whore)] == 37);
    CHECK_FALSE(slsf::ui::ReadFameMenu(fx.api, "Unknown").values);
    int publications = 0;
    const auto invalid = slsf::ui::ExecuteFameCommand(fx.api, slsf::ui::FameCommand::SetFame,
        "Unknown", "", "Whore", 100, 0, [&] { ++publications; });
    CHECK(invalid.status == slsf::Status::Invalid);
    CHECK(publications == 0);
    const auto gain = slsf::ui::ExecuteFameCommand(fx.api, slsf::ui::FameCommand::ManualGain,
        "Whiterun", "", "Whore", 3, 3, [&] { ++publications; });
    REQUIRE(gain.status == slsf::Status::Ready);
    CHECK(gain.revision == fx.api.LastPublishedRevision());
    CHECK(publications == 1);
    fx.dispatcher.Detach();
    CHECK_FALSE(slsf::ui::ReadFameMenu(fx.api, "Whiterun").values);
}

TEST_CASE("menu readout rounds public values and hides suspended sessions", "[menu-fame]") {
    auto gate = std::make_shared<slsf::runtime::SessionGate>();
    ApiFixture fx(gate);
    REQUIRE(fx.api.SetFameValueRegion(RegionId{0}, FameCategory::Whore, 37.6).status == slsf::Status::Ready);
    REQUIRE(fx.api.RegisterCustomLocation("Favorite House", Key(3)));
    REQUIRE(fx.api.SetFameValue("Favorite House", "Whore", 19).status == slsf::Status::Ready);
    const auto view = slsf::ui::ReadFameMenu(fx.api, "Whiterun");
    REQUIRE(view.values);
    CHECK((*view.values)[slsf::FameCategoryIndex(FameCategory::Whore)] == 38);
    CHECK((*view.values)[slsf::FameCategoryIndex(FameCategory::Whore)] == *fx.api.GetFameValue("Whiterun", "Whore"));
    CHECK(view.revision == fx.api.LastPublishedRevision());
    const auto custom = slsf::ui::ReadFameMenu(fx.api, "Favorite House");
    REQUIRE(custom.values);
    CHECK((*custom.values)[slsf::FameCategoryIndex(FameCategory::Whore)] == 19);
    REQUIRE(fx.api.UnregisterCustomLocation("Favorite House"));
    CHECK_FALSE(slsf::ui::ReadFameMenu(fx.api, "Favorite House").values);
    CHECK(std::find(view.locations.begin(), view.locations.end(), "Favorite House") != view.locations.end());
    fx.dispatcher.Suspend();
    CHECK_FALSE(slsf::ui::ReadFameMenu(fx.api, "Whiterun").values);
    fx.dispatcher.Resume();
    REQUIRE(slsf::ui::ReadFameMenu(fx.api, "Whiterun").values);
    gate->Close();
    CHECK(fx.dispatcher.Attached());
    CHECK_FALSE(slsf::ui::ReadFameMenu(fx.api, "Whiterun").values);
}

TEST_CASE("manual fame locations follow live custom registrations by name", "[menu-fame]") {
    ApiFixture fx;
    slsf::ui::FameCommandLocations pickers;
    auto view = slsf::ui::ReadFameMenu(fx.api, "");
    pickers.Refresh(view);
    CHECK_FALSE(pickers.Contains(view, "Favorite House"));
    REQUIRE(fx.api.RegisterCustomLocation("Earlier House", Key(4)));
    REQUIRE(fx.api.RegisterCustomLocation("Favorite House", Key(3)));
    view = slsf::ui::ReadFameMenu(fx.api, "");
    REQUIRE(pickers.Contains(view, "Favorite House"));
    pickers.source = "Favorite House";
    pickers.target = "Whiterun";
    REQUIRE(fx.api.UnregisterCustomLocation("Earlier House"));
    pickers.Refresh(slsf::ui::ReadFameMenu(fx.api, ""));
    CHECK(pickers.source == "Favorite House");
    CHECK(pickers.target == "Whiterun");
    REQUIRE(slsf::ui::ExecuteFameCommand(fx.api, slsf::ui::FameCommand::SetFame,
        pickers.source, "", "Whore", 40, 0, {}).status == slsf::Status::Ready);
    std::swap(pickers.source, pickers.target);
    REQUIRE(slsf::ui::ExecuteFameCommand(fx.api, slsf::ui::FameCommand::SetFame,
        pickers.source, "", "Whore", 50, 0, {}).status == slsf::Status::Ready);
    const auto spread = slsf::ui::ExecuteFameCommand(fx.api, slsf::ui::FameCommand::ManualSpread,
        pickers.source, pickers.target, "Whore", 100, 0, {});
    CHECK(spread.status == slsf::Status::Ready);
    REQUIRE(fx.api.UnregisterCustomLocation("Favorite House"));
    pickers.Refresh(slsf::ui::ReadFameMenu(fx.api, ""));
    CHECK(pickers.source == "Whiterun");
    CHECK(pickers.target.empty());
    fx.dispatcher.Suspend();
    pickers.Refresh(slsf::ui::ReadFameMenu(fx.api, ""));
    CHECK(pickers.source.empty());
}

TEST_CASE("PublicApi synchronous set commits before return",
         "[slsf][api][public-api]") {
    ApiFixture fx;
    auto& api = fx.api;

    const auto set = api.SetFameValue("Whiterun", "Whore", 20);
    REQUIRE(set.status == slsf::Status::Ready);

    const auto value = api.GetFameValue("Whiterun", "Whore");
    REQUIRE(value.has_value());
    REQUIRE(*value == 20);

    const auto other = api.GetFameValue("Whiterun", "Slut");
    REQUIRE(other.has_value());
    REQUIRE(*other == 0);
}
TEST_CASE("PublicApi rejects unresolvable names and categories",
         "[slsf][api][public-api]") {
    ApiFixture fx;
    auto& api = fx.api;

    REQUIRE(api.SetFameValue("Nowhere", "Whore", 20).status == slsf::Status::Invalid);
    REQUIRE(api.SetFameValue("Whiterun", "NotACategory", 20).status ==
            slsf::Status::Invalid);
    REQUIRE(!api.GetFameValue("Nowhere", "Whore").has_value());
    REQUIRE(!api.GetFameValue("Whiterun", "NotACategory").has_value());

    REQUIRE(*api.ResolveCategory("whore") == FameCategory::Whore);
    REQUIRE(*api.ResolveCategory("CUM DUMP") == FameCategory::CumDump);
    REQUIRE(!api.ResolveCategory("definitely-not-a-category").has_value());
}
TEST_CASE("PublicApi reads only published regions", "[slsf][api][public-api]") {
    ApiFixture fx;
    auto& api = fx.api;

    REQUIRE(!api.GetFameValueRegion(RegionId{7}, FameCategory::Whore).has_value());

    REQUIRE(api.SetFameValueRegion(RegionId{0}, FameCategory::Anal, 5).status ==
            slsf::Status::Ready);
    const auto value = api.GetFameValueRegion(RegionId{0}, FameCategory::Anal);
    REQUIRE(value.has_value());
    REQUIRE(*value == 5);
}
TEST_CASE("PublicApi one-shot operations run through the executor",
         "[slsf][api][public-api]") {
    ApiFixture fx;
    auto& api = fx.api;

    const auto gain = api.ManualGain("Winterhold", "Whore", 10, 20);
    REQUIRE(gain.status == slsf::Status::Ready);
    const auto value = api.GetFameValue("Winterhold", "Whore");
    REQUIRE(value.has_value());
    REQUIRE(*value >= 10);
    REQUIRE(*value <= 20);
}
TEST_CASE("PublicApi external-mod flag registry", "[slsf][api][public-api]") {
    ApiFixture fx;
    auto& api = fx.api;

    REQUIRE(api.RegisterMod("ModA"));
    REQUIRE(!api.RegisterMod("ModA"));
    REQUIRE(api.RegisterMod("ModB"));
    REQUIRE(api.CountExternalMods() == 2);
    REQUIRE(api.IsModRegistered("ModA"));
    REQUIRE(api.IsModRegistered("ModB"));
    REQUIRE(!api.IsModRegistered("ModC"));

    api.SetExternalFlag("ModA", "Whore", true);
    REQUIRE(api.GetAggregatedFlag("Whore"));
    api.SetExternalFlag("ModA", "Whore", false);
    REQUIRE(!api.GetAggregatedFlag("Whore"));
    api.SetExternalFlag("ModB", "Whore", true);
    REQUIRE(api.GetAggregatedFlag("Whore"));

    REQUIRE(api.UnregisterMod("ModB"));
    REQUIRE(!api.GetAggregatedFlag("Whore"));
    REQUIRE(api.CountExternalMods() == 1);

    REQUIRE(!api.IsKnownFlag("NotAFlag"));
    REQUIRE(api.IsKnownFlag("Whore"));
    REQUIRE(api.IsKnownFlag("Whore Event"));
    REQUIRE(api.NormalizeFlag("Whore Event") == "Sex Worker");
    REQUIRE(api.IsKnownFlag("Sex Worker"));
    REQUIRE(!api.GetAggregatedFlag("NotAFlag"));
}
TEST_CASE("PublicApi legacy outputs read the committed revision",
         "[slsf][api][public-api]") {
    ApiFixture fx;
    auto& api = fx.api;

    (void)api.SetFameValue("Whiterun", "Whore", 20);
    api.SetCurrentLocation("Whiterun");

    RecordingSink sink;
    REQUIRE(api.PublishCurrentFameGlobals(sink) == 25);
    int whoreGlobal = -1;
    for (const auto& [formId, value] : sink.globals) {
        if (formId == slsf::StableFormKey{"SLSF Reloaded.esp", 0x80Du}) {
            whoreGlobal = value;
        }
    }
    REQUIRE(whoreGlobal == 20);

    sink = RecordingSink{};
    REQUIRE(api.PublishStorageUtilLists(sink) == 20 * 24);
    int stored = -1;
    for (const auto& [key, locationIndex, value] : sink.storage) {
        if (key == "SLSF.LocationsFame.PC.Whore" && locationIndex == 6) {
            stored = value;
        }
    }
    REQUIRE(stored == 20);

    sink = RecordingSink{};
    REQUIRE(api.PublishLegacyBridge(sink) == 20);
    int bridged = -1;
    for (const auto& [formId, value] : sink.bridge) {
        if (formId == slsf::StableFormKey{"SexLab - Sexual Fame [SLSF].esm", 0x24268u}) {
            bridged = value;
        }
    }
    REQUIRE(bridged == 20);
}
TEST_CASE("typed external flag events update all 25 category flags", "[public-api][events]") {
    ApiFixture fx;
    slsf::compat::ModEventBridge bridge;
    fx.api.AttachToBridge(bridge);
    REQUIRE(bridge.Dispatch("SLSF_Reloaded_RegisterMod", {EventString("Provider")}));

    for (const auto category : slsf::kFameCategoryNames) {
        const std::string flag{category};
        auto event = std::string{"SLSF_Reloaded_Set"} + flag + "Flag";
        std::erase(event, ' ');
        CAPTURE(event, flag);
        REQUIRE(bridge.Dispatch(event, {EventString("Provider"), EventBool(true)}));
        CHECK(fx.api.GetModFlag("Provider", flag));
        CHECK(fx.api.GetAggregatedFlag(flag));
        REQUIRE(bridge.Dispatch(event, {EventString("Provider"), EventBool(false)}));
        CHECK_FALSE(fx.api.GetModFlag("Provider", flag));
        CHECK_FALSE(fx.api.GetAggregatedFlag(flag));
    }
}

TEST_CASE("typed fame events resolve Current on the mutation routes", "[public-api][events]") {
    ApiFixture fx;
    slsf::compat::ModEventBridge bridge;
    fx.api.AttachToBridge(bridge);
    fx.api.SetCurrentLocation("Winterhold");

    REQUIRE(bridge.Dispatch("SLSF_Reloaded_SendManualFameGain",
        {EventString("Whore"), EventString("Current"), EventInt(40), EventInt(40)}));
    CHECK(fx.api.GetFameValue("Winterhold", "Whore") == 40);
    CHECK(fx.api.GetFameValue("Whiterun", "Whore") == 0);

    REQUIRE(bridge.Dispatch("SLSF_Reloaded_SendManualFameDecay",
        {EventString("Current"), EventString("Whore"), EventInt(10), EventInt(10)}));
    CHECK(fx.api.GetFameValue("Winterhold", "Whore") == 30);

    REQUIRE(fx.api.SetFameValue("Whiterun", "Whore", 100).status == slsf::Status::Ready);
    REQUIRE(bridge.Dispatch("SLSF_Reloaded_SendManualFameSpread",
        {EventString("Whiterun"), EventString("Current"), EventString("Whore"), EventInt(50)}));
    CHECK(fx.api.GetFameValue("Winterhold", "Whore") == 80);
    CHECK(fx.api.GetFameValue("Whiterun", "Whore") == 100);

    REQUIRE(bridge.Dispatch("SLSF_Reloaded_SendManualFameGainAllInLocation",
        {EventString("Current"), EventInt(15), EventInt(15)}));
    CHECK(fx.api.GetFameValue("Winterhold", "Slut") == 15);
    REQUIRE(bridge.Dispatch("SLSF_Reloaded_SendManualFameDecayAllInLocation",
        {EventString("Current"), EventInt(5), EventInt(5)}));
    CHECK(fx.api.GetFameValue("Winterhold", "Slut") == 10);
    CHECK(fx.api.GetFameValue("Whiterun", "Slut") == 0);

    int rolls = 0;
    fx.api.SetGameplayCall([&](const std::string& name,
                               const std::vector<slsf::compat::ModEventValue>& args,
                               slsf::compat::ModEventValue&) {
        ++rolls;
        CHECK(name == "FameGainRoll");
        REQUIRE(args.size() == 4);
        CHECK(args[0].stringValue == "Winterhold");
        CHECK(args[1].boolValue);
        CHECK_FALSE(args[2].boolValue);
        CHECK_FALSE(args[3].boolValue);
        return true;
    });
    REQUIRE(bridge.Dispatch("SLSF_Reloaded_SendFameGainRoll", {EventString("Current")}));
    CHECK(rolls == 1);
    fx.api.SetCurrentLocation("");
    REQUIRE(bridge.Dispatch("SLSF_Reloaded_SendFameGainRoll", {EventString("Current")}));
    CHECK(rolls == 1);
}

TEST_CASE("EnableFameType is durable and separate from scoring flags", "[game-ready][api][events]") {
    ApiFixture fx;
    slsf::compat::ModEventBridge bridge;
    fx.api.AttachToBridge(bridge);
    REQUIRE(fx.api.RegisterMod("Provider"));
    const auto text = [](const char* value) { slsf::compat::ModEventValue out; out.type = slsf::compat::ModEventType::String; out.stringValue = value; return out; };
    slsf::compat::ModEventValue enabled; enabled.type = slsf::compat::ModEventType::Bool; enabled.boolValue = true;
    REQUIRE(bridge.Dispatch("SLSF_Reloaded_EnableFameType", {text("Cum Dump"), text("Provider"), enabled}));
    REQUIRE(fx.api.GetAggregatedFlag("EnableCumDump"));
    REQUIRE_FALSE(fx.api.GetAggregatedFlag("Cum Dump"));
    fx.api.SetExternalFlag("Provider", "EnableAirhead", true);
    REQUIRE(fx.api.GetModFlag("Provider", "EnableAirhead"));
    const auto saved = fx.dispatcher.Capture();
    REQUIRE(saved);
    const auto encoded = slsf::EncodeState(*saved);
    REQUIRE(encoded);
    const auto decoded = slsf::DecodeState(*encoded);
    REQUIRE(decoded);
    REQUIRE(decoded->namedProviders == saved->namedProviders);
}

TEST_CASE("PublicApi routes the 7 request events to reply ModEvents",
         "[slsf][api][public-api][events]") {
    ApiFixture fx;
    auto& api = fx.api;

    slsf::compat::ModEventBridge bridge;
    api.AttachToBridge(bridge);

    std::vector<std::pair<std::string, std::vector<slsf::compat::ModEventValue>>>
        replies;
    bridge.SetReplySink(
        [&replies](const std::string& name,
                  const std::vector<slsf::compat::ModEventValue>& values) {
            replies.emplace_back(name, values);
        });

    auto str = [](const std::string& s) {
        slsf::compat::ModEventValue v;
        v.type = slsf::compat::ModEventType::String;
        v.stringValue = s;
        return v;
    };

    (void)api.SetFameValue("Whiterun", "Whore", 20);
    REQUIRE(bridge.Dispatch("SLSF_Reloaded_RequestFame", {str("Whiterun"), str("Whore")})
                .has_value());
    REQUIRE(replies.size() == 1);
    REQUIRE(replies[0].first == "SLSF_Reloaded_ReturnRequestedFame");
    REQUIRE(replies[0].second.size() == 3);
    REQUIRE(replies[0].second[0].stringValue == "Whiterun");
    REQUIRE(replies[0].second[1].stringValue == "Whore");
    REQUIRE(replies[0].second[2].intValue == 20);

    replies.clear();
    REQUIRE(bridge.Dispatch("SLSF_Reloaded_RequestFame", {str("Nowhere"), str("Whore")})
                .has_value());
    REQUIRE(replies.empty());

    (void)api.RegisterMod("ModA");
    api.SetExternalFlag("ModA", "Whore", true);
    replies.clear();
    REQUIRE(bridge.Dispatch("SLSF_Reloaded_RequestFlagState", {str("NotAFlag")}).has_value());
    REQUIRE(replies.size() == 1);
    REQUIRE(replies[0].first == "SLSF_Reloaded_ReturnFlagState");
    REQUIRE(replies[0].second[0].stringValue == "NotAFlag");
    REQUIRE(replies[0].second[1].stringValue == "No");
    REQUIRE(replies[0].second[2].boolValue == false);

    replies.clear();
    REQUIRE(bridge.Dispatch("SLSF_Reloaded_RequestFlagState", {str("Whore")}).has_value());
    REQUIRE(replies.size() == 1);
    REQUIRE(replies[0].second[1].stringValue == "Yes");
    REQUIRE(replies[0].second[2].boolValue == true);

    api.SetCurrentLocation("Whiterun");
    replies.clear();
    REQUIRE(bridge.Dispatch("SLSF_Reloaded_RequestLocation", {}).has_value());
    REQUIRE(replies.size() == 1);
    REQUIRE(replies[0].first == "SLSF_Reloaded_ReturnRequestedLocation");
    REQUIRE(replies[0].second[0].stringValue == "Whiterun");

    replies.clear();
    REQUIRE(bridge
                .Dispatch("SLSF_Reloaded_RequestCumVisibility", {str("Any")})
                .has_value());
    REQUIRE(replies.empty());
    api.SetGameplayCall([](const std::string& name, const auto& arguments, auto& result) {
        if (name == "CumVisibility") {
            REQUIRE(arguments.size() == 1);
            REQUIRE(arguments[0].stringValue == "Any");
            result.type = slsf::compat::ModEventType::Bool;
            result.boolValue = true;
            return true;
        }
        if (name == "RecognitionTime") {
            REQUIRE(arguments.size() == 1);
            REQUIRE(arguments[0].stringValue == "Whiterun");
            result.type = slsf::compat::ModEventType::Float;
            result.floatValue = 2.5;
            return true;
        }
        return false;
    });
    REQUIRE(bridge.Dispatch("SLSF_Reloaded_RequestCumVisibility", {str("Any")}).has_value());
    REQUIRE(replies.size() == 1);
    REQUIRE(replies[0].first == "SLSF_Reloaded_ReturnRequestedCum");
    REQUIRE(replies[0].second[0].boolValue == true);

    replies.clear();
    REQUIRE(bridge
                .Dispatch("SLSF_Reloaded_RequestRecognitionTime", {str("Nowhere")})
                .has_value());
    REQUIRE(replies.empty());
    replies.clear();
    REQUIRE(bridge
                .Dispatch("SLSF_Reloaded_RequestRecognitionTime", {str("Whiterun")})
                .has_value());
    REQUIRE(replies.size() == 1);
    REQUIRE(replies[0].first == "SLSF_Reloaded_ReturnRecognitionTime");
    REQUIRE(replies[0].second[0].floatValue == 2.5);

    replies.clear();
    REQUIRE(bridge
                .Dispatch("SLSF_Reloaded_RequestModRegisterState", {str("ModA")})
                .has_value());
    REQUIRE(replies.size() == 1);
    REQUIRE(replies[0].first == "SLSF_Reloaded_ReturnModRegisteredState");
    REQUIRE(replies[0].second[0].stringValue == "ModA");
    REQUIRE(replies[0].second[1].boolValue == true);
    replies.clear();
    REQUIRE(bridge
                .Dispatch("SLSF_Reloaded_RequestModRegisterState", {str("ModB")})
                .has_value());
    REQUIRE(replies[0].second[1].boolValue == false);
}
TEST_CASE("PublicApi gates business commands on session state",
         "[slsf][api][public-api]") {
    auto gate = std::make_shared<slsf::runtime::SessionGate>();
    ApiFixture fx(gate);
    auto& api = fx.api;

    REQUIRE(api.BusinessOpen());
    const auto epoch = api.CurrentEpoch();
    CHECK(api.IsEpochCurrent(epoch));
    api.Executor().Suspend();
    CHECK_FALSE(api.BusinessOpen());
    CHECK_FALSE(api.IsEpochCurrent(epoch));
    api.Executor().Resume();
    CHECK(api.BusinessOpen());

    slsf::compat::ModEventBridge bridge;
    api.AttachToBridge(bridge);
    int replies = 0;
    api.SetReplySender([&](const auto&, const auto&) { ++replies; return true; });
    const auto generation = gate->Close();
    CHECK(api.Executor().Attached());
    CHECK_FALSE(api.BusinessOpen());
    CHECK_FALSE(api.IsEpochCurrent(epoch));
    CHECK(api.SetFameValueRegion(RegionId{0}, FameCategory::Whore, 5).status == slsf::Status::NotReady);
    REQUIRE(bridge.Dispatch("SLSF_Reloaded_RequestLocation", {EventBool(false)}));
    CHECK(replies == 0);
    gate->Open(generation);
    CHECK(api.IsEpochCurrent(epoch));
    REQUIRE(bridge.Dispatch("SLSF_Reloaded_RequestLocation", {EventBool(false)}));
    CHECK(replies == 1);

    api.Executor().Detach();
    REQUIRE(!api.BusinessOpen());

    const auto set = api.SetFameValueRegion(RegionId{0}, FameCategory::Whore, 5);
    REQUIRE(set.status == slsf::Status::Stale);
}
}
