#include "slsf/config/ConfigurationStore.h"
#include "slsf/io/JsonTransfer.h"

#include <cstdint>
#include <string>
#include <string_view>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    std::string_view text(reinterpret_cast<const char*>(data), size);
    std::string error;
    const slsf::io::JsonPtr parsed = slsf::io::JsonValue::Parse(text, 1u << 20, &error);
    if (parsed == nullptr) {
        return 0;
    }
    if (!error.empty()) {
        __builtin_trap();
    }
    const std::string compact = slsf::io::JsonValue::Write(*parsed, -1);
    std::string rereadError;
    const slsf::io::JsonPtr reparsed =
        slsf::io::JsonValue::Parse(compact, 1u << 20, &rereadError);
    if (reparsed == nullptr) {
        __builtin_trap();
    }
    if (slsf::io::JsonValue::Write(*reparsed, -1) != compact) {
        __builtin_trap();
    }
    if (!slsf::io::LegacyDocument::FromValue(parsed).has_value()) {
        return 0;
    }
    slsf::io::LegacyDocument legacy = slsf::io::LegacyDocument::FromValue(parsed).value();
    const std::string written = legacy.Write();
    std::string legacyError;
    const slsf::io::JsonPtr legacyReparsed =
        slsf::io::JsonValue::Parse(written, 1u << 20, &legacyError);
    if (legacyReparsed == nullptr) {
        __builtin_trap();
    }
    const slsf::io::LegacyDocument legacyAgain =
        slsf::io::LegacyDocument::FromValue(legacyReparsed).value();
    if (legacyAgain.Write() != written) {
        __builtin_trap();
    }

    legacy.SetInt("fuzz.whore", 7);
    legacy.SetString("modindex[0]", "Example Mod.esp");
    if (!legacy.HasKey("int", "fuzz.whore") || !legacy.HasKey("string", "modindex[0]")) {
        __builtin_trap();
    }
    const std::string edited = legacy.Write();
    const slsf::io::JsonPtr editedParsed =
        slsf::io::JsonValue::Parse(edited, 1u << 20, &legacyError);
    if (editedParsed == nullptr) {
        __builtin_trap();
    }
    const slsf::io::JsonPtr originalInts = parsed->MemberAt("int");
    const slsf::io::JsonPtr editedInts = editedParsed->MemberAt("int");
    if (originalInts != nullptr && originalInts->IsObject()) {
        if (editedInts == nullptr || !editedInts->IsObject()) {
            __builtin_trap();
        }
        for (const auto& member : originalInts->ObjectMembers()) {
            if (!editedInts->HasMember(member.first)) {
                __builtin_trap();
            }
        }
    }
    const auto import = slsf::io::ExtractLegacyImport(
        editedParsed, std::vector<std::string>{"Whore"});
    if (!import.has_value()) {
        return 0;
    }
    std::size_t seeded = 0;
    for (const auto& entry : import->fame) {
        if (entry.location == "fuzz" && entry.category == slsf::FameCategory::Whore &&
            entry.value == 7) {
            ++seeded;
        }
    }
    if (seeded != 1) {
        __builtin_trap();
    }
    return 0;
}
