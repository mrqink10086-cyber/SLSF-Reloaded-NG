#include <catch2/catch_test_macros.hpp>

#include "slsf/io/JsonTransfer.h"

#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <vector>

#ifndef SLSF_FIXTURE_DIR
#define SLSF_FIXTURE_DIR "tests/fixtures"
#endif

using slsf::io::JsonPtr;
using slsf::io::JsonValue;
using slsf::io::LegacyDocument;

namespace {

JsonPtr ParseOr(std::string_view text, std::string* error) {
    return JsonValue::Parse(text, 1u << 20, error);
}

} // namespace

TEST_CASE("json parser accepts every legal shape used by the config and legacy files",
          "[json]") {
    const std::vector<const char*> accepted = {
        "{\"schemaVersion\": 1}",
        "{\"schemaVersion\": 1.5}",
        "{\"delta\": -0.25e2}",
        "{\"flags\": [true, false, true]}",
        "{\"plain\": \"some text\"}",
        "{\"escapes\": \"quote\\\" backslash\\\\ slash\\/ newline\\n end\"}",
        "{\"hex\": \"\\u0041\\u00e5\\u4e2d\"}",
        "[1, 2, [3, 4], {\"inner\": 5}]",
        "{\"deep\": {\"a\": {\"b\": {\"c\": []}}}}",
        "7",
        "true",
        "null",
        "{}",
        "[]",
        "   {}  ",
    };
    for (const char* text : accepted) {
        std::string error;
        const JsonPtr parsed = ParseOr(text, &error);
        INFO(error);
        REQUIRE(parsed != nullptr);
    }
}

TEST_CASE("json parser rejects malformed documents and reports the failing byte",
          "[json]") {
    struct Case {
        const char* text;
        const char* fragment;
    };
    const std::vector<Case> rejected = {
        {"", "unexpected end of document"},
        {"    ", "unexpected end of document"},
        {"{\"a\":}", "unexpected '}'"},
        {"{\"a\": 1,}", "expected object key"},
        {"[1, 2", "expected ',' or ']'"},
        {"{\"a\": 1", "expected ',' or '}'"},
        {"{\"a\":1}extra", "expected end of document"},
        {"{\"a\":1}{\"b\":2}", "expected end of document"},
        {"1 trailing", "expected end of document"},
        {"01", "expected end of document"},
        {"+1", "unexpected '+'"},
        {"-.5", "invalid number"},
        {"1.2.3", "expected end of document"},
        {"1.0e", "missing exponent digits"},
        {"Infinity", "unexpected 'I'"},
        {"NaN", "unexpected 'N'"},
        {"\"unterminated", "unterminated string"},
        {"\"bad\\escape\"", "invalid escape sequence"},
        {"\"trail\\\"", "unterminated string"},
        {"'single'", "unexpected '''"},
        {"{,}", "expected object key"},
        {"{\"a\" 1}", "expected ':' after object key"},
    };
    for (const Case& entry : rejected) {
        std::string error;
        const JsonPtr parsed = ParseOr(entry.text, &error);
        INFO(entry.text);
        INFO(error);
        REQUIRE(parsed == nullptr);
        CHECK_FALSE(error.empty());
        CHECK(error.find(entry.fragment) != std::string::npos);
    }
}

TEST_CASE("json parser enforces the documented size and depth limits", "[json]") {
    const std::string oversized(1u << 21, 'x');
    std::string error;
    REQUIRE(JsonValue::Parse(oversized, 1u << 20, &error) == nullptr);
    REQUIRE(error.find("size limit") != std::string::npos);

    std::string deep;
    for (int index = 0; index < 96; ++index) {
        deep += '[';
    }
    for (int index = 0; index < 96; ++index) {
        deep += ']';
    }
    REQUIRE(JsonValue::Parse(deep, 1u << 20, &error) == nullptr);
    REQUIRE(error.find("nesting too deep") != std::string::npos);

    std::string shallow;
    for (int index = 0; index < 48; ++index) {
        shallow += '[';
    }
    for (int index = 0; index < 48; ++index) {
        shallow += ']';
    }
    std::string shallowError;
    REQUIRE(ParseOr(shallow, &shallowError) != nullptr);
}

TEST_CASE("legacy document reads exporter-shaped type groups case-insensitively",
          "[json]") {
    std::string error;
    const char* text =
        "{\"int\": {\"whiterun.whore\": 12, \"modtotal\": 3, \"locationtotal\": 2, "
        "\"whiterun.famespreadchance\": 55},"
        " \"float\": {\"temp.value\": 1.5},"
        " \"string\": {\"modindex[0]\": \"Example Mod.esp\", "
        "\"location_string_0\": \"Favorite House\"},"
        " \"form\": {\"location_form_0\": \"0x00085248|Skyrim.esm\"}}";
    const JsonPtr root = ParseOr(text, &error);
    REQUIRE(root != nullptr);

    const auto document = LegacyDocument::FromValue(root);
    REQUIRE(document.has_value());

    REQUIRE(document->GetInt("Whiterun.Whore", -999) == 12);
    REQUIRE(document->GetInt("MODTOTAL", -999) == 3);
    REQUIRE(document->GetInt("LocationTotal", -999) == 2);
    REQUIRE(document->GetInt("whiterun.FAMESPREADCHANCE", -999) == 55);
    REQUIRE(document->GetInt("absent", -999) == -999);
    REQUIRE(document->GetFloat("Temp.Value", -999) == 1.5);
    REQUIRE(document->GetString("ModIndex[0]", "missing") == "Example Mod.esp");
    REQUIRE(document->GetString("LOCATION_STRING_0", "missing") == "Favorite House");
    REQUIRE(document->GetForm("Location_Form_0", "missing") == "0x00085248|Skyrim.esm");
    REQUIRE(document->HasKey("int", "whiterun.whore"));
    REQUIRE_FALSE(document->HasKey("int", "absent"));
    REQUIRE_FALSE(document->HasKey("form", "whiterun.whore"));
}

TEST_CASE("legacy document writes the four PapyrusUtil groups and rereads them",
          "[json]") {
    LegacyDocument writer;
    writer.SetInt("Riften.Slut", 4);
    writer.SetFloat("Riften.FameSpreadChance", 40.0);
    writer.SetString("ModIndex[1]", "Second Mod.esl");
    writer.SetForm("Location_String_1", "0x1234|Example.esp");

    const std::string written = writer.Write();
    std::string error;
    const JsonPtr roundtripRoot = ParseOr(written, &error);
    REQUIRE(roundtripRoot != nullptr);
    const auto reread = LegacyDocument::FromValue(roundtripRoot);
    REQUIRE(reread.has_value());
    REQUIRE(reread->GetInt("riften.slut", 0) == 4);
    REQUIRE(reread->GetFloat("riften.famespreadchance", 0) == 40.0);
    REQUIRE(reread->GetString("modindex[1]", "") == "Second Mod.esl");
    REQUIRE(reread->GetForm("location_string_1", "") == "0x1234|Example.esp");
    REQUIRE(written.find("\"int\"") != std::string::npos);
    REQUIRE(written.find("\"float\"") != std::string::npos);
    REQUIRE(written.find("\"string\"") != std::string::npos);
    REQUIRE(written.find("\"form\"") != std::string::npos);
    REQUIRE(written.find("Old Field") == std::string::npos);
}

TEST_CASE("legacy document rejects documents whose groups are not objects",
          "[json]") {
    REQUIRE(LegacyDocument::FromValue(nullptr).error().code ==
            slsf::Error::Code::InvalidArgument);

    std::string error;
    const JsonPtr wrongRoot = slsf::io::JsonValue::MakeArray();
    auto nonObject = LegacyDocument::FromValue(wrongRoot);
    REQUIRE_FALSE(nonObject.has_value());
    REQUIRE(nonObject.error().message == "legacy document root is not an object");

    const JsonPtr wrongGroup = ParseOr("{\"int\": 5}", &error);
    REQUIRE(wrongGroup != nullptr);
    auto badGroup = LegacyDocument::FromValue(wrongGroup);
    REQUIRE_FALSE(badGroup.has_value());
    REQUIRE(badGroup.error().message == "int group is not an object");
}

TEST_CASE("json parser accepts a UTF-8 byte order mark", "[json]") {
    const std::string bom((const char*)u8"\uFEFF");
    std::string error;
    const JsonPtr parsed = ParseOr(bom + "{\"schemaVersion\": 1}", &error);
    REQUIRE(parsed != nullptr);
    REQUIRE(error.empty());
}

TEST_CASE("writer emits null for absent child pointers", "[json]") {
    auto array = std::make_shared<slsf::io::JsonValue>(slsf::io::JsonKind::Array);
    array->AppendArrayItem(nullptr);
    auto object = std::make_shared<slsf::io::JsonValue>(slsf::io::JsonKind::Object);
    auto nested = std::make_shared<slsf::io::JsonValue>(slsf::io::JsonKind::Object);
    nested->AttachMember("hole", nullptr);
    object->AttachMember("child", std::move(nested));

    std::string error;
    const JsonPtr reparsed = ParseOr(JsonValue::Write(*array, 0), &error);
    REQUIRE(reparsed != nullptr);
    REQUIRE(reparsed->ArraySize() == 1);
    REQUIRE(reparsed->ArrayAt(0)->IsNull());

    const JsonPtr reparsedObject = ParseOr(JsonValue::Write(*object, 0), &error);
    REQUIRE(reparsedObject != nullptr);
    REQUIRE(reparsedObject->MemberAt("child") != nullptr);
    REQUIRE(reparsedObject->MemberAt("child")->MemberAt("hole")->IsNull());
}

TEST_CASE("legacy document edits on a parsed root preserve the read-back values",
          "[json]") {
    const JsonPtr parsed = ParseOr(
        R"({"int": {"whiterun.whore": 4, "riften.whore": 9}, "form": {"x": "0x1|a.esp"}})",
        nullptr);
    const auto doc = LegacyDocument::FromValue(parsed);
    REQUIRE(doc.has_value());
    const_cast<LegacyDocument&>(*doc).SetInt("Whiterun.Whore", 11);

    std::string error;
    const JsonPtr reparsed = ParseOr(doc->Write(), &error);
    REQUIRE(reparsed != nullptr);
    const auto again = LegacyDocument::FromValue(reparsed);
    REQUIRE(again.has_value());
    REQUIRE(again->GetInt("Whiterun.Whore", -1.0) == 11.0);
    REQUIRE(again->GetInt("Riften.Whore", -1.0) == 9.0);
    REQUIRE(again->GetForm("X", "") == "0x1|a.esp");
}

TEST_CASE("legacy importer extracts fame, spread, mods and flags", "[json]") {
    const std::string fixturePath =
        std::string(SLSF_FIXTURE_DIR) + "/json/legacy/legacy_exporter.json";
    std::ifstream file(fixturePath, std::ios::binary);
    REQUIRE(file.is_open());
    const std::string bytes((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
    std::string error;
    const JsonPtr parsed = ParseOr(bytes, &error);
    REQUIRE(parsed != nullptr);

    const auto import = slsf::io::ExtractLegacyImport(
        parsed,
        std::vector<std::string>{std::string(slsf::kFameCategoryNames[0]), "Sex Worker"});
    REQUIRE(import.has_value());
    REQUIRE(import->fame.size() == 3);
    const auto whoreOnWhiterun = std::find_if(
        import->fame.begin(), import->fame.end(), [](const auto& entry) {
            return entry.location == "whiterun" &&
                   entry.category == slsf::FameCategory::Whore;
        });
    REQUIRE(whoreOnWhiterun != import->fame.end());
    REQUIRE(whoreOnWhiterun->value == 12);
    REQUIRE(import->spread.size() == 1);
    REQUIRE(import->spread[0].location == "whiterun");
    REQUIRE(import->spread[0].chance == 55);
    REQUIRE(import->mods.size() == 2);
    REQUIRE(import->mods[0] == "Example Mod.esp");
    REQUIRE(import->mods[1] == "Second Mod.esl");
    REQUIRE(import->modFlags.size() == 2);
    REQUIRE(import->modFlags[0].flags.at("Sex Worker"));
}

TEST_CASE("json numbers round-trip through the compact writer", "[json]") {
    constexpr double values[] = {
        0.0, 1.0, -7.25, 0.5, 40.0, 1e6, 30.0, 0.6, 2.5, 1.5e-3, 123456789.0,
        3.141592653589793};
    for (const double value : values) {
        const JsonPtr number = JsonValue::MakeNumber(value);
        REQUIRE(number != nullptr);
        const std::string text = JsonValue::Write(*number, -1);
        std::string error;
        const JsonPtr parsed = ParseOr(text, &error);
        INFO(error);
        REQUIRE(parsed != nullptr);
        REQUIRE(parsed->NumberAt(0.0) == value);
    }
    REQUIRE(JsonValue::MakeNumber(std::numeric_limits<double>::quiet_NaN()) == nullptr);
    REQUIRE(
        JsonValue::MakeNumber(std::numeric_limits<double>::signaling_NaN()) == nullptr);
    REQUIRE(JsonValue::MakeNumber(std::numeric_limits<double>::infinity()) == nullptr);
}
