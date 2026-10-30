#include "slsf/compat/LegacyOutputs.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

namespace {

using slsf::FameCategory;
using slsf::compat::FameValueResolver;
using slsf::compat::LegacyOutputSink;
using slsf::compat::LegacyOutputs;

const std::uint32_t kExpectedGlobalLocalIds[25] = {
    0x0000080Du, 0x0000080Cu, 0x0000080Eu, 0x0000080Fu, 0x00000810u,
    0x00000811u,  0x00000812u, 0x00000813u, 0x00000814u, 0x00000815u,
    0x00000816u,  0x00000817u, 0x00000818u, 0x00000819u, 0x0000081Au,
    0x0000081Bu,  0x0000081Cu, 0x0000081Du, 0x0000081Eu, 0x0000081Fu,
    0x00000820u,  0x00000821u, 0x00000822u, 0x00000823u, 0x0000082Fu,
};

const char* kExpectedStorageSuffixes[20] = {
    "Anal",                "Argonian",            "Beastiality",
    "Dominant/Master",     "Exhibitionist/Exposed", "GentleLover",
    "Group",               "Khajiit",             "LikeMan",
    "LikeWoman",           "Masochist",           "Nasty",
    "Oral",                "Orc",                 "Pregnant",
    "Sadic",               "SkoomaUser",          "Slut",
    "Submissive/Slave",    "Whore",
};
int kExpectedStorageCategory[20] = {
    4, 16, 17, 7,  2,  11, 18, 15, 12, 13,
    10, 5, 3,  14, 6,  9,  -1, 1,  8,  0,
};

struct RecordingSink final : LegacyOutputSink {
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

TEST_CASE("LegacyOutputs pins the 25 Reloaded Current*Fame globals") {
    for (int c = 0; c < 25; ++c) {
        const auto category = static_cast<FameCategory>(c);
        CHECK(LegacyOutputs::CurrentFameGlobalKey(category) ==
              slsf::StableFormKey{"SLSF Reloaded.esp", kExpectedGlobalLocalIds[c]});
    }
    CHECK(std::string(LegacyOutputs::CurrentFameGlobalEditorId(FameCategory::Whore)) ==
          "CurrentWhoreFame");
    CHECK(std::string(LegacyOutputs::CurrentFameGlobalEditorId(FameCategory::Slut)) ==
          "CurrentSlutFame");
    CHECK(std::string(LegacyOutputs::CurrentFameGlobalEditorId(FameCategory::Unfaithful)) ==
          "CurrentCheatFame");
    CHECK(std::string(LegacyOutputs::CurrentFameGlobalEditorId(FameCategory::Airhead)) ==
          "CurrentAirheadFame");
    CHECK(LegacyOutputs::CurrentFameGlobalKey(FameCategory::Count) == slsf::StableFormKey{});
    CHECK(std::string(LegacyOutputs::CurrentFameGlobalEditorId(FameCategory::Count)) == "");
}
TEST_CASE("LegacyOutputs pins the 20 StorageUtil keys and categories") {
    REQUIRE(LegacyOutputs::kLegacyFameSlotCount == 20);
    for (std::size_t i = 0; i < 20; ++i) {
        CHECK(LegacyOutputs::StorageFameKey(i) ==
              "SLSF.LocationsFame.PC." + std::string(kExpectedStorageSuffixes[i]));
        if (kExpectedStorageCategory[i] < 0) {
            CHECK(!LegacyOutputs::StorageFameCategory(i).has_value());
        } else {
            CHECK(LegacyOutputs::StorageFameCategory(i).has_value());
            CHECK(*LegacyOutputs::StorageFameCategory(i) ==
                  static_cast<FameCategory>(kExpectedStorageCategory[i]));
        }
    }
    CHECK(!LegacyOutputs::StorageFameCategory(16).has_value());
    CHECK(LegacyOutputs::StorageFameKey(20).empty());
    CHECK(!LegacyOutputs::StorageFameCategory(20).has_value());
}
TEST_CASE("LegacyOutputs pins the 24 legacy location indices") {
    REQUIRE(LegacyOutputs::kLegacyLocationIndexCount == 24);
    const char* kExpectedNames[24] = {
        "Dawnstar",      "Falkreath",   "Markarth",    "Morthal",
        "Riften",        "Solitude",    "Whiterun",    "Windhelm",
        "Winterhold",    "Dragon Bridge", "Ivarstead", "Karthwasten",
        "Riverwood",     "Rorikstead",  "Shor's Stone", "",
        "Dushnikh Yal",  "Largashbur",  "Mor Khazgur", "Narzulbur",
        "",              "",            "Skaal Village", "Raven Rock",
    };
    for (std::size_t i = 0; i < 24; ++i) {
        const auto name = LegacyOutputs::LegacyLocationName(i);
        if (kExpectedNames[i][0] == '\0') {
            CHECK(!name.has_value());
        } else {
            CHECK(name.has_value());
            CHECK(*name == kExpectedNames[i]);
        }
    }
    CHECK(!LegacyOutputs::LegacyLocationName(24).has_value());
}
TEST_CASE("LegacyOutputs pins the 20 legacy esm bridge globals") {
    REQUIRE(LegacyOutputs::kLegacyFameSlotCount == 20);
    for (std::size_t i = 0; i < 20; ++i) {
        CHECK(LegacyOutputs::LegacyBridgeKey(i) ==
              slsf::StableFormKey{"SexLab - Sexual Fame [SLSF].esm",
                                  0x00024255u + static_cast<std::uint32_t>(i)});
        CHECK(LegacyOutputs::LegacyBridgeCategory(i) ==
              LegacyOutputs::StorageFameCategory(i));
    }
    CHECK(LegacyOutputs::LegacyBridgeKey(20) == slsf::StableFormKey{});
}
TEST_CASE("PublishCurrentFameGlobals writes all 25 from the resolver") {
    LegacyOutputs outputs;
    RecordingSink sink;
    FameValueResolver resolve = [](const std::string&, FameCategory category) {
        return 100 + static_cast<int>(slsf::FameCategoryIndex(category));
    };

    const auto written = outputs.PublishCurrentFameGlobals(sink, resolve, "Whiterun");
    CHECK(written == 25);
    REQUIRE(sink.globals.size() == 25);
    for (int c = 0; c < 25; ++c) {
        CHECK(sink.globals[c].first ==
              slsf::StableFormKey{"SLSF Reloaded.esp", kExpectedGlobalLocalIds[c]});
        CHECK(sink.globals[c].second == 100 + c);
    }
}
TEST_CASE("PublishCurrentFameGlobals writes zeros when nothing resolves") {
    LegacyOutputs outputs;
    RecordingSink sink;
    FameValueResolver resolveZero = [](const std::string&, FameCategory) { return 0; };

    const auto written =
        outputs.PublishCurrentFameGlobals(sink, resolveZero, "");
    CHECK(written == 25);
    for (const auto& [formId, value] : sink.globals) {
        (void)formId;
        CHECK(value == 0);
    }
}
TEST_CASE("PublishStorageUtilLists writes 20 entries per 24 indices") {
    LegacyOutputs outputs;
    RecordingSink sink;
    FameValueResolver resolve = [](const std::string&,
                                   FameCategory category) {
        return static_cast<int>(slsf::FameCategoryIndex(category));
    };

    const auto written = outputs.PublishStorageUtilLists(sink, resolve);
    CHECK(written == 20 * 24);
    REQUIRE(sink.storage.size() == 20 * 24);

    std::size_t index = 0;
    for (std::size_t locationIndex = 0; locationIndex < 24; ++locationIndex) {
        for (std::size_t fameIndex = 0; fameIndex < 20; ++fameIndex) {
            const auto& [key, loc, value] = sink.storage[index++];
            CHECK(key == "SLSF.LocationsFame.PC." +
                          std::string(kExpectedStorageSuffixes[fameIndex]));
            CHECK(loc == locationIndex);
            if (kExpectedStorageCategory[fameIndex] < 0) {
                CHECK(value == 0);
            } else if (!LegacyOutputs::LegacyLocationName(locationIndex).has_value()) {
                CHECK(value == 0);
            } else {
                CHECK(value == kExpectedStorageCategory[fameIndex]);
            }
        }
    }
    (void)resolve;
}
TEST_CASE("PublishLegacyBridge writes all 20 bridge globals") {
    LegacyOutputs outputs;
    RecordingSink sink;
    FameValueResolver resolve = [](const std::string&, FameCategory category) {
        return 50 + static_cast<int>(slsf::FameCategoryIndex(category));
    };

    const auto written = outputs.PublishLegacyBridge(sink, resolve, "Whiterun");
    CHECK(written == 20);
    REQUIRE(sink.bridge.size() == 20);
    for (std::size_t i = 0; i < 20; ++i) {
        CHECK(sink.bridge[i].first ==
              slsf::StableFormKey{"SexLab - Sexual Fame [SLSF].esm",
                                  0x00024255u + static_cast<std::uint32_t>(i)});
        if (kExpectedStorageCategory[i] < 0) {
            CHECK(sink.bridge[i].second == 0);
        } else {
            CHECK(sink.bridge[i].second ==
                  50 + kExpectedStorageCategory[i]);
        }
    }
}
}
