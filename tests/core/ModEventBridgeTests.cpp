#include "slsf/compat/ModEventBridge.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace {

using slsf::StableFormKey;
using slsf::compat::ModEvent;
using slsf::compat::ModEventBridge;
using slsf::compat::ModEventValue;

ModEventValue Str(std::string text) {
    ModEventValue v;
    v.type = slsf::compat::ModEventType::String;
    v.stringValue = std::move(text);
    return v;
}
ModEventValue B(bool value) {
    ModEventValue v;
    v.type = slsf::compat::ModEventType::Bool;
    v.boolValue = value;
    return v;
}
ModEventValue I(std::int64_t value) {
    ModEventValue v;
    v.type = slsf::compat::ModEventType::Int;
    v.intValue = value;
    return v;
}
ModEventValue F(double value) {
    ModEventValue v;
    v.type = slsf::compat::ModEventType::Float;
    v.floatValue = value;
    return v;
}
ModEventValue Form(std::uint32_t localFormId) {
    ModEventValue v;
    v.type = slsf::compat::ModEventType::Form;
    v.formValue = StableFormKey{"Skyrim.esm", localFormId};
    v.hasForm = true;
    return v;
}
TEST_CASE("M33 exposes exactly 52 inputs and 7 replies") {
    CHECK(ModEventBridge::InputCount() == 52);
    CHECK(ModEventBridge::ReplyCount() == 7);
    CHECK(ModEventBridge::InputContracts().size() == 52);
    CHECK(ModEventBridge::ReplyContracts().size() == 7);
}
TEST_CASE("M33 pins representative input contracts") {
    ModEventBridge bridge;
    const auto* roll = bridge.FindInput("SLSF_Reloaded_SendFameGainRoll");
    REQUIRE(roll != nullptr);
    CHECK(roll->handler == "OnExternalFameGainRoll");
    CHECK(roll->parameters.size() == 1);
    CHECK(roll->parameters[0].name == "EventLocation");
    CHECK(roll->parameters[0].type == slsf::compat::ModEventType::String);

    const auto* gain = bridge.FindInput("SLSF_Reloaded_SendFameGain");
    REQUIRE(gain != nullptr);
    CHECK(gain->handler == "OnExternalFameGain");
    REQUIRE(gain->parameters.size() == 4);
    CHECK(gain->parameters[0].name == "Category");
    CHECK(gain->parameters[0].type == slsf::compat::ModEventType::String);
    CHECK(gain->parameters[1].name == "EventLocation");
    CHECK(gain->parameters[1].type == slsf::compat::ModEventType::String);
    CHECK(gain->parameters[2].name == "MinFame");
    CHECK(gain->parameters[2].type == slsf::compat::ModEventType::Int);
    CHECK(gain->parameters[3].name == "MaxFame");
    CHECK(gain->parameters[3].type == slsf::compat::ModEventType::Int);

    const auto* whore = bridge.FindInput("SLSF_Reloaded_SetWhoreFlag");
    REQUIRE(whore != nullptr);
    CHECK(whore->handler == "OnExternalWhoreFlag");
    REQUIRE(whore->parameters.size() == 2);
    CHECK(whore->parameters[0].name == "ModName");
    CHECK(whore->parameters[0].type == slsf::compat::ModEventType::String);
    CHECK(whore->parameters[1].name == "Flag");
    CHECK(whore->parameters[1].type == slsf::compat::ModEventType::Bool);

    const auto* reqLoc = bridge.FindInput("SLSF_Reloaded_RequestLocation");
    REQUIRE(reqLoc != nullptr);
    CHECK(reqLoc->handler == "OnRequestLocation");
    REQUIRE(reqLoc->parameters.size() == 1);
    CHECK(reqLoc->parameters[0].name == "Strict");
    CHECK(reqLoc->parameters[0].type == slsf::compat::ModEventType::Bool);
}
TEST_CASE("M33 pins the 7 reply contracts") {
    ModEventBridge bridge;
    const auto* loc = bridge.FindReply("SLSF_Reloaded_ReturnRequestedLocation");
    REQUIRE(loc != nullptr);
    CHECK(loc->handler.empty());
    REQUIRE(loc->parameters.size() == 1);
    CHECK(loc->parameters[0].name == "LocationName");
    CHECK(loc->parameters[0].type == slsf::compat::ModEventType::String);

    const auto* rec = bridge.FindReply("SLSF_Reloaded_ReturnRecognitionTime");
    REQUIRE(rec != nullptr);
    REQUIRE(rec->parameters.size() == 1);
    CHECK(rec->parameters[0].name == "RecognitionTime");
    CHECK(rec->parameters[0].type == slsf::compat::ModEventType::Float);

    const auto* fame = bridge.FindReply("SLSF_Reloaded_ReturnRequestedFame");
    REQUIRE(fame != nullptr);
    REQUIRE(fame->parameters.size() == 3);
    CHECK(fame->parameters[0].name == "LocationName");
    CHECK(fame->parameters[1].name == "Category");
    CHECK(fame->parameters[2].name == "RequestedFame");
    CHECK(fame->parameters[2].type == slsf::compat::ModEventType::Int);

    const auto* cum = bridge.FindReply("SLSF_Reloaded_ReturnRequestedCum");
    REQUIRE(cum != nullptr);
    REQUIRE(cum->parameters.size() == 1);
    CHECK(cum->parameters[0].name == "Visible");
    CHECK(cum->parameters[0].type == slsf::compat::ModEventType::Bool);

    const auto* flag = bridge.FindReply("SLSF_Reloaded_ReturnFlagState");
    REQUIRE(flag != nullptr);
    REQUIRE(flag->parameters.size() == 3);
    CHECK(flag->parameters[0].name == "FlagName");
    CHECK(flag->parameters[1].name == "FlagExists");
    CHECK(flag->parameters[2].name == "RequestedFlagState");
    CHECK(flag->parameters[2].type == slsf::compat::ModEventType::Bool);

    const auto* modFlag = bridge.FindReply("SLSF_Reloaded_ReturnModFlagState");
    REQUIRE(modFlag != nullptr);
    REQUIRE(modFlag->parameters.size() == 5);
    CHECK(modFlag->parameters[0].name == "ModName");
    CHECK(modFlag->parameters[1].name == "IsModRegistered");
    CHECK(modFlag->parameters[1].type == slsf::compat::ModEventType::Bool);
    CHECK(modFlag->parameters[2].name == "FlagName");
    CHECK(modFlag->parameters[3].name == "FlagExists");
    CHECK(modFlag->parameters[4].name == "RequestedModFlagState");
    CHECK(modFlag->parameters[4].type == slsf::compat::ModEventType::Bool);

    const auto* modReg = bridge.FindReply("SLSF_Reloaded_ReturnModRegisteredState");
    REQUIRE(modReg != nullptr);
    REQUIRE(modReg->parameters.size() == 2);
    CHECK(modReg->parameters[0].name == "ModName");
    CHECK(modReg->parameters[1].name == "IsModRegistered");
    CHECK(modReg->parameters[1].type == slsf::compat::ModEventType::Bool);
}
TEST_CASE("M33 parses valid input payloads and reads values by name") {
    ModEventBridge bridge;
    const auto result = bridge.ParseInput(
        "SLSF_Reloaded_SendFameGain",
        {Str("Whore"), Str("Whiterun"), I(5), I(20)});
    REQUIRE(result.has_value());
    const ModEvent& ev = *result;
    CHECK(ev.eventName == "SLSF_Reloaded_SendFameGain");
    CHECK(ev.handler == "OnExternalFameGain");
    CHECK(ev.StringValue("Category") == "Whore");
    CHECK(ev.StringValue("EventLocation") == "Whiterun");
    CHECK(ev.IntValue("MinFame") == 5);
    CHECK(ev.IntValue("MaxFame") == 20);
    CHECK(ev.IntValue("DoesNotExist", -1) == -1);
}
TEST_CASE("M33 fills contract defaults for omitted parameters") {
    ModEventBridge bridge;
    const auto result = bridge.ParseInput("SLSF_Reloaded_RegisterMod", {});
    REQUIRE(result.has_value());
    CHECK(result->StringValue("ModName") == "-EMPTY-");
}
TEST_CASE("M33 rejects unknown events, extra args, and wrong types") {
    ModEventBridge bridge;
    CHECK(!bridge.ParseInput("Not.A.Real.Event", {}).has_value());

    CHECK(!bridge.ParseInput(
        "SLSF_Reloaded_RequestLocation", {B(true), B(false)}).has_value());

    CHECK(!bridge.ParseInput(
        "SLSF_Reloaded_SendFameGain", {I(1), Str("Whiterun"), I(5), I(20)})
        .has_value());
}
TEST_CASE("M33 parses and validates the Form-typed spread request") {
    ModEventBridge bridge;
    const auto result =
        bridge.ParseInput("SLSF_Reloaded_SendFameSpread", {Form(0x14)});
    CHECK(result.has_value());
    if (result.has_value()) {
        const auto* key = result->FormValue("SpreadFromLocation");
        REQUIRE(key != nullptr);
        CHECK(key->localFormId == 0x14);
    }
}
TEST_CASE("M33 dispatches to a registered handler exactly once") {
    ModEventBridge bridge;
    int calls = 0;
    std::string seenLocation;
    const bool registered =
        bridge.RegisterHandler("SLSF_Reloaded_RequestLocation",
                               [&](const ModEvent& ev) {
                                   ++calls;
                                   seenLocation = ev.StringValue("StrictLocation", "");
                               });
    CHECK(registered);
    CHECK(bridge.HandlerCount() == 1);

    const auto result =
        bridge.Dispatch("SLSF_Reloaded_RequestLocation", {B(true)});
    REQUIRE(result.has_value());
    CHECK(calls == 1);
    CHECK(!bridge.Dispatch("Not.A.Real.Event", {}).has_value());
    CHECK(!bridge.RegisterHandler("SLSF_Reloaded_ReturnRequestedLocation",
                                  [](const ModEvent&) {}));
}
TEST_CASE("M33 forwards validated replies to the sink") {
    ModEventBridge bridge;
    std::string sinkEvent;
    int sinkCalls = 0;
    bridge.SetReplySink(
        [&](const std::string& name, const std::vector<ModEventValue>&) {
            sinkEvent = name;
            ++sinkCalls;
        });

    const auto parsed =
        bridge.ParseReply("SLSF_Reloaded_ReturnRequestedLocation",
                          {Str("Whiterun")});
    REQUIRE(parsed.has_value());
    CHECK(bridge.ValidateReply(*parsed));
    CHECK(bridge.SendReply(*parsed));
    CHECK(sinkCalls == 1);
    CHECK(sinkEvent == "SLSF_Reloaded_ReturnRequestedLocation");

    const auto bad =
        bridge.ParseReply("SLSF_Reloaded_ReturnRequestedLocation", {I(1)});
    CHECK(!bad.has_value());

    ModEvent unknown;
    unknown.eventName = "SLSF_Reloaded_NotAReply";
    CHECK(!bridge.SendReply(unknown));
    CHECK(sinkCalls == 1);
}
}