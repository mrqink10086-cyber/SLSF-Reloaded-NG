#include <slsf/api/PublicApi.h>
#include <slsf/core/Commands.h>
#include <slsf/core/Configuration.h>
#include <slsf/core/Snapshots.h>
#include <slsf/core/Types.h>
#include <slsf/core/reputation/DiffusionGraph.h>
#include <slsf/core/reputation/ReputationState.h>
#include <slsf/core/reputation/Settlement.h>
#include <slsf/runtime/Dispatcher.h>

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
    snapshot.regions = {Key(0), Key(1), Key(2)};
    snapshot.customOrder = {Key(0), Key(1), Key(2)};
    snapshot.regionApiIndex = {
        slsf::ApiLocationIndex{0}, slsf::ApiLocationIndex{1}, slsf::ApiLocationIndex{2}};
    snapshot.aliasIndex = {
        {"solitude", slsf::ApiLocationIndex{2}},
        {"whiterun", slsf::ApiLocationIndex{0}},
        {"winterhold", slsf::ApiLocationIndex{1}},
    };
    return snapshot;
}
slsf::State MakeState(std::size_t count = 3) {
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

    ApiFixture()
        : bindings(MakeBindings())
        , dispatcher()
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
    REQUIRE(replies.size() == 1);
    REQUIRE(replies[0].first == "SLSF_Reloaded_ReturnRequestedCum");
    REQUIRE(replies[0].second[0].boolValue == false);

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
    REQUIRE(replies[0].second[0].floatValue == 0.0);

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
    ApiFixture fx;
    auto& api = fx.api;

    REQUIRE(api.BusinessOpen());

    api.Executor().Detach();
    REQUIRE(!api.BusinessOpen());

    const auto set = api.SetFameValueRegion(RegionId{0}, FameCategory::Whore, 5);
    REQUIRE(set.status == slsf::Status::Stale);
}
}
