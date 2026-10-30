#include "slsf/compat/PapyrusBindings.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace {

using slsf::LegacyValueType;
using slsf::StableFormKey;
using slsf::compat::PapyrusBindings;
using slsf::compat::PapyrusFunction;
using slsf::compat::PapyrusInstance;
using slsf::compat::PapyrusValue;

PapyrusValue Str(std::string text) {
    PapyrusValue v;
    v.type = LegacyValueType::String;
    v.stringValue = std::move(text);
    return v;
}
PapyrusValue B(bool value) {
    PapyrusValue v;
    v.type = LegacyValueType::Bool;
    v.boolValue = value;
    return v;
}
PapyrusValue I(std::int64_t value) {
    PapyrusValue v;
    v.type = LegacyValueType::Int;
    v.intValue = value;
    return v;
}
PapyrusValue F(double value) {
    PapyrusValue v;
    v.type = LegacyValueType::Float;
    v.floatValue = value;
    return v;
}
PapyrusValue Form(std::uint32_t localFormId) {
    PapyrusValue v;
    v.type = LegacyValueType::Form;
    v.formKey = StableFormKey{"Skyrim.esm", localFormId};
    return v;
}
PapyrusValue Actor(std::uint32_t localFormId) {
    PapyrusValue v;
    v.type = LegacyValueType::Actor;
    v.formKey = StableFormKey{"Skyrim.esm", localFormId};
    return v;
}
TEST_CASE("M32 frozen table has 124 functions (86 public, 38 internal)") {
    PapyrusBindings bindings;
    CHECK(bindings.Functions().size() == 124);
    CHECK(bindings.PublicCount() == 86);
    CHECK(bindings.InternalCount() == 38);
    CHECK(bindings.PublicFunctions().size() == 86);
}
TEST_CASE("M32 pins the DataManager.GetFameValue declaration") {
    PapyrusBindings bindings;
    const auto* fn = bindings.Find("SLSF_Reloaded_DataManager.GetFameValue");
    REQUIRE(fn != nullptr);
    CHECK(fn->script == "SLSF_Reloaded_DataManager");
    CHECK(fn->name == "GetFameValue");
    CHECK(fn->returnTypeName == "Int");
    CHECK(fn->returnType == LegacyValueType::Int);
    CHECK(fn->parameters.size() == 2);
    CHECK(fn->parameters[0].name == "LocationName");
    CHECK(fn->parameters[0].type == LegacyValueType::String);
    CHECK(!fn->parameters[0].hasDefault);
    CHECK(fn->parameters[1].name == "FameCategory");
    CHECK(fn->parameters[1].type == LegacyValueType::String);
    CHECK(!fn->isGlobal);
    CHECK(!fn->isNative);
    CHECK(fn->isPublic);
    CHECK(fn->classification == "public");
    CHECK(fn->invalidInputResult == "0");
    CHECK(fn->binding == slsf::compat::PapyrusBindingKind::Quest);
    CHECK(fn->quest == StableFormKey{"SLSF Reloaded.esp", 0x80Au});
    CHECK(fn->questEditorId == "SLSF_Reloaded_Managers");
    CHECK(fn->aliasIndex == -1);
}
TEST_CASE("M32 pins invalid-input results for the query functions") {
    PapyrusBindings bindings;
    const auto* fame = bindings.Find("SLSF_Reloaded_DataManager.GetFameValue");
    REQUIRE(fame != nullptr);
    CHECK(bindings.ConvertInvalidInput(*fame).type == LegacyValueType::Int);
    CHECK(bindings.ConvertInvalidInput(*fame).intValue == 0);

    const auto* sexWorker =
        bindings.Find("SLSF_Reloaded_DataManager.CheckSexWorker");
    REQUIRE(sexWorker != nullptr);
    CHECK(sexWorker->returnType == LegacyValueType::Bool);
    const auto sexInvalid = bindings.ConvertInvalidInput(*sexWorker);
    CHECK(sexInvalid.type == LegacyValueType::Bool);
    CHECK(sexInvalid.boolValue == false);

    const auto* doesFlag =
        bindings.Find("SLSF_Reloaded_DataManager.DoesFlagExist");
    REQUIRE(doesFlag != nullptr);
    CHECK(doesFlag->returnType == LegacyValueType::String);
    const auto flagInvalid = bindings.ConvertInvalidInput(*doesFlag);
    CHECK(flagInvalid.type == LegacyValueType::String);
    CHECK(flagInvalid.stringValue == "No");

    const auto* currentLoc =
        bindings.Find("SLSF_Reloaded_LocationManager.CurrentLocationName");
    REQUIRE(currentLoc != nullptr);
    CHECK(currentLoc->parameters.empty());
    const auto noneText = bindings.ConvertInvalidInput(*currentLoc);
    CHECK(noneText.type == LegacyValueType::String);
    CHECK(noneText.stringValue == "$NoneText");

    const auto* fameLoc =
        bindings.Find("SLSF_Reloaded_LocationManager.GetFameLocation");
    REQUIRE(fameLoc != nullptr);
    const auto nullStr = bindings.ConvertInvalidInput(*fameLoc);
    CHECK(nullStr.type == LegacyValueType::String);
    CHECK(nullStr.stringValue == "Null");

    const auto* log = bindings.Find("SLSF_Reloaded_Logger.Log");
    REQUIRE(log != nullptr);
    CHECK(log->isGlobal);
    CHECK(log->isNative);
    CHECK(log->binding == slsf::compat::PapyrusBindingKind::GlobalNative);
    CHECK(log->quest == StableFormKey{});
    CHECK(bindings.ConvertInvalidInput(*log).type == LegacyValueType::None);

    const auto* startup =
        bindings.Find("SLSF_Reloaded_MasterClock.Startup");
    REQUIRE(startup != nullptr);
    CHECK(!startup->isPublic);
    CHECK(startup->classification == "internal");
    CHECK(startup->binding == slsf::compat::PapyrusBindingKind::Quest);
    CHECK(startup->quest == StableFormKey{"SLSF Reloaded.esp", 0x839u});
    CHECK(startup->questEditorId == "SLSF_Reloaded_MasterClock");
    CHECK(bindings.ConvertInvalidInput(*startup).type == LegacyValueType::None);
}
TEST_CASE("M32 pins the reference-alias bound analyzer declaration") {
    PapyrusBindings bindings;
    const auto* fn =
        bindings.Find("SLSF_Reloaded_SexAnimationAnalyzer.AnimationSubAnalyze");
    REQUIRE(fn != nullptr);
    CHECK(fn->parameters.size() == 7);
    CHECK(fn->parameters[0].name == "Actors");
    CHECK(fn->parameters[0].type == LegacyValueType::ActorArray);
    CHECK(fn->parameters[1].name == "PlayerController");
    CHECK(fn->parameters[1].type == LegacyValueType::Script);
    CHECK(fn->parameters[5].name == "Friend");
    CHECK(fn->parameters[5].type == LegacyValueType::Bool);
    CHECK(fn->parameters[5].hasDefault);
    CHECK(fn->parameters[6].name == "Lover");
    CHECK(fn->parameters[6].hasDefault);
    CHECK(fn->binding == slsf::compat::PapyrusBindingKind::ReferenceAlias);
    CHECK(fn->quest == StableFormKey{"SLSF Reloaded.esp", 0x808u});
    CHECK(fn->questEditorId == "SLSF_Reloaded_Main");
    CHECK(fn->aliasIndex == 0);
    CHECK(fn->aliasName == "PlayerAlias");

    CHECK(bindings.IsBoundTo(*fn, PapyrusInstance{{"SLSF Reloaded.esp", 0x808u}, 0}));
    CHECK(!bindings.IsBoundTo(*fn, PapyrusInstance{{"SLSF Reloaded.esp", 0x808u}, -1}));
    CHECK(!bindings.IsBoundTo(*fn, PapyrusInstance{{"SLSF Reloaded.esp", 0x808u}, 1}));
    CHECK(!bindings.IsBoundTo(*fn, PapyrusInstance{{"SLSF Reloaded.esp", 0x80Au}, 0}));
}
TEST_CASE("M32 validates quest and global bindings") {
    PapyrusBindings bindings;
    const auto* fame = bindings.Find("SLSF_Reloaded_DataManager.GetFameValue");
    REQUIRE(fame != nullptr);
    CHECK(bindings.IsBoundTo(*fame, PapyrusInstance{{"SLSF Reloaded.esp", 0x80Au}, -1}));
    CHECK(!bindings.IsBoundTo(*fame, PapyrusInstance{{"SLSF Reloaded.esp", 0x808u}, -1}));
    CHECK(!bindings.IsBoundTo(*fame, PapyrusInstance{{"SLSF Reloaded.esp", 0x80Au}, 0}));

    const auto* log = bindings.Find("SLSF_Reloaded_Logger.Log");
    REQUIRE(log != nullptr);
    CHECK(bindings.IsBoundTo(*log, PapyrusInstance{{}, -1}));
    CHECK(bindings.IsBoundTo(*log, PapyrusInstance{{"SLSF Reloaded.esp", 0x80Au}, -1}));
}
TEST_CASE("M32 Find resolves by id and by script+function") {
    PapyrusBindings bindings;
    CHECK(bindings.Find("SLSF_Reloaded_Logger.SetLogLevel") != nullptr);
    CHECK(bindings.Find("SLSF_Reloaded_Logger", "SetLogLevel") != nullptr);
    CHECK(bindings.Find("SLSF_Reloaded_Logger.Log") != nullptr);
    CHECK(bindings.Find("does-not-exist") == nullptr);
    CHECK(bindings.Find("SLSF_Reloaded_Logger", "does-not-exist") == nullptr);
}
TEST_CASE("Papyrus quest identity includes the plugin and ignores filename case",
          "[slsf][formid-fix]") {
    PapyrusBindings bindings;
    const auto* fame = bindings.Find("SLSF_Reloaded_DataManager.GetFameValue");
    REQUIRE(fame);
    CHECK(bindings.IsBoundTo(*fame, {{"slsf reloaded.ESP", 0x80A}, -1}));
    CHECK_FALSE(bindings.IsBoundTo(*fame, {{"Other.esp", 0x80A}, -1}));
    CHECK_FALSE(bindings.IsBoundTo(*fame, {{"SLSF Reloaded.esp", 0x0600080A}, -1}));
    CHECK_FALSE(bindings.IsBoundTo(*fame, {{}, -1}));
    auto unbound = *fame;
    unbound.quest = {};
    CHECK_FALSE(bindings.IsBoundTo(unbound, {{}, -1}));
}
TEST_CASE("M32 converts exact argument lists and rejects wrong counts") {
    PapyrusBindings bindings;
    const auto* fame = bindings.Find("SLSF_Reloaded_DataManager.GetFameValue");
    REQUIRE(fame != nullptr);

    std::vector<PapyrusValue> out;
    CHECK(bindings.ConvertArguments(
        *fame, {Str("Whiterun"), Str("Whore")}, out));
    CHECK(out.size() == 2);
    CHECK(out[0].type == LegacyValueType::String);
    CHECK(out[0].stringValue == "Whiterun");
    CHECK(out[1].stringValue == "Whore");

    CHECK(!bindings.ConvertArguments(*fame, {Str("Whiterun")}, out));
    CHECK(!bindings.ConvertArguments(
        *fame, {Str("Whiterun"), Str("Whore"), Str("x")}, out));
    CHECK(!bindings.ConvertArguments(*fame, {I(1), Str("Whore")}, out));
}
TEST_CASE("M32 fills contracted defaults for omitted trailing parameters") {
    PapyrusBindings bindings;
    const auto* fn =
        bindings.Find("SLSF_Reloaded_SexAnimationAnalyzer.AnimationSubAnalyze");
    REQUIRE(fn != nullptr);

    std::vector<PapyrusValue> in = {
        PapyrusValue{},
        PapyrusValue{},
        I(0),
        B(false),
        Str("Whiterun"),
    };
    in[0].type = LegacyValueType::ActorArray;
    in[1].type = LegacyValueType::Script;

    std::vector<PapyrusValue> out;
    CHECK(bindings.ConvertArguments(*fn, std::move(in), out));
    CHECK(out.size() == 7);
    CHECK(out[5].type == LegacyValueType::Bool);
    CHECK(out[5].boolValue == false);
    CHECK(out[6].type == LegacyValueType::Bool);
    CHECK(out[6].boolValue == false);
}
TEST_CASE("M32 applies Papyrus implicit conversions") {
    PapyrusBindings bindings;

    const auto* update =
        bindings.Find("SLSF_Reloaded_FameManager.UpdateFame");
    REQUIRE(update != nullptr);
    REQUIRE(update->parameters.size() == 1);
    REQUIRE(update->parameters[0].type == LegacyValueType::Int);
    std::vector<PapyrusValue> out;
    CHECK(bindings.ConvertArguments(*update, {B(true)}, out));
    CHECK(out.size() == 1);
    CHECK(out[0].type == LegacyValueType::Int);
    CHECK(out[0].intValue == 1);

    const auto* setExit =
        bindings.Find("SLSF_Reloaded_DynamicAnonymity.SetLastExitTime");
    REQUIRE(setExit != nullptr);
    REQUIRE(setExit->parameters.size() == 2);
    REQUIRE(setExit->parameters[1].type == LegacyValueType::Float);
    CHECK(bindings.ConvertArguments(*setExit, {Str("Whiterun"), I(7)}, out));
    CHECK(out.size() == 2);
    CHECK(out[1].type == LegacyValueType::Float);
    CHECK(out[1].floatValue == 7.0);

    const auto* fetch =
        bindings.Find("SLSF_Reloaded_LocationManager.FetchLocationName");
    REQUIRE(fetch != nullptr);
    REQUIRE(fetch->parameters.size() == 1);
    REQUIRE(fetch->parameters[0].type == LegacyValueType::Form);
    CHECK(bindings.ConvertArguments(*fetch, {Actor(0x14)}, out));
    CHECK(out.size() == 1);
    CHECK(out[0].type == LegacyValueType::Form);
}
}
