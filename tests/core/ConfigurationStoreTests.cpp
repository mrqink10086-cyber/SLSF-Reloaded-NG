#include <catch2/catch_test_macros.hpp>

#include "slsf/config/ConfigurationStore.h"

#include <windows.h>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <memory>

static std::string MakeUniqueFileName(std::string_view prefix) {
    static std::atomic<int> counter{0};
    return std::string(prefix) + std::to_string(::GetCurrentProcessId()) + "_" +
           std::to_string(counter.fetch_add(1)) + ".json";
}

using slsf::ConfigurationData;
using slsf::ConfigurationSnapshot;
using slsf::ConfigurationUpdateStatus;
using slsf::config::ConfigurationStore;
using slsf::config::DiskConfigurationFile;
using slsf::config::MemoryConfigurationFile;

namespace {

std::unique_ptr<ConfigurationStore> MakeStore(MemoryConfigurationFile** fileOut = nullptr) {
    auto file = std::make_unique<MemoryConfigurationFile>(std::string());
    if (fileOut != nullptr) {
        *fileOut = file.get();
    }
    return ConfigurationStore::Open(std::move(file));
}

} // namespace

TEST_CASE("configuration store generates defaults for a missing file", "[configuration]") {
    MemoryConfigurationFile* file = nullptr;
    const auto store = MakeStore(&file);

    std::uint64_t notifiedRevision = 0;
    int notifyCount = 0;
    store->BindNotify([&](const ConfigurationSnapshot& snapshot) {
        ++notifyCount;
        notifiedRevision = snapshot.revision.value;
    });

    const auto update = store->Reload();
    REQUIRE(update.status == ConfigurationUpdateStatus::Applied);
    REQUIRE(update.Succeeded());
    REQUIRE(update.revision.value == 1);
    REQUIRE(store->Current().revision.value == 1);
    REQUIRE(store->Current().HasData());
    REQUIRE(store->Current().data.get() != nullptr);
    REQUIRE(store->Current().data->categories.minimumFameToSpread == 30.0);
    REQUIRE(store->Current().data->parameters.maxFame == 150.0);
    REQUIRE(store->NotifyCount() == 1);
    REQUIRE(notifyCount == 1);
    REQUIRE(notifiedRevision == 1);
    REQUIRE(file->Bytes().find("\"schemaVersion\": 1") != std::string::npos);
}

TEST_CASE("configuration store keeps revision and suppresses notify on unchanged reload",
          "[configuration]") {
    const auto store = MakeStore();
    std::uint64_t notified = 0;
    store->BindNotify([&](const ConfigurationSnapshot& snapshot) { notified = snapshot.revision.value; });
    REQUIRE(store->Reload().Succeeded());
    const auto second = store->Reload();
    REQUIRE(second.status == ConfigurationUpdateStatus::Unchanged);
    REQUIRE(second.revision.value == 1);
    REQUIRE(store->NotifyCount() == 1);
}

TEST_CASE("configuration store returns the decode truth for a pristine encode mirror",
          "[configuration]") {
    const auto store = MakeStore();
    REQUIRE(store->Reload().Succeeded());

    const std::string encoded = slsf::io::JsonValue::Write(*ConfigurationStore::Encode(*store->Current().data), 2);
    std::string error;
    const auto parsed = slsf::io::JsonValue::Parse(encoded, 1u << 20, &error);
    REQUIRE(parsed != nullptr);
    const auto decoded = ConfigurationStore::Decode(parsed, slsf::config::ConfigurationStore::LegacyDefaults());
    REQUIRE(decoded.has_value());
    REQUIRE(*decoded == *store->Current().data);
}

TEST_CASE("configuration store rejects invalid json and keeps the last valid config",
          "[configuration]") {
    MemoryConfigurationFile* file = nullptr;
    const auto store = MakeStore(&file);
    REQUIRE(store->Reload().Succeeded());
    const ConfigurationSnapshot before = store->Current();

    file->OverwriteExternally("{\"schemaVersion\": 1, \"timing\": {\"decayPeriodDays\": ");
    const auto update = store->Reload();
    REQUIRE(update.status == ConfigurationUpdateStatus::Invalid);
    REQUIRE(update.message.find("configuration JSON is invalid") != std::string::npos);
    REQUIRE(store->Current().revision == before.revision);
    REQUIRE(store->Current().data != nullptr);
    REQUIRE(*store->Current().data == *before.data);
}

TEST_CASE("configuration store rejects unsupported schema versions", "[configuration]") {
    MemoryConfigurationFile* file = nullptr;
    const auto store = MakeStore(&file);
    REQUIRE(store->Reload().Succeeded());
    const ConfigurationSnapshot before = store->Current();

    file->OverwriteExternally("{\"schemaVersion\": 2}");
    const auto update = store->Reload();
    REQUIRE(update.status == ConfigurationUpdateStatus::Invalid);
    REQUIRE(update.message.find("unsupported schema version") != std::string::npos);
    REQUIRE(*store->Current().data == *before.data);
}

TEST_CASE("configuration store keeps state and reports io errors on write failures",
          "[configuration]") {
    MemoryConfigurationFile* file = nullptr;
    const auto store = MakeStore(&file);
    std::uint64_t notified = 0;
    store->BindNotify([&](const ConfigurationSnapshot& snapshot) { notified = snapshot.revision.value; });
    REQUIRE(store->Reload().Succeeded());
    const ConfigurationSnapshot before = store->Current();
    const std::string previousBytes = file->Bytes();

    ConfigurationData edit = *before.data;
    edit.categories.multipliers[0] = 2.5;
    file->FailNextWrite();
    const auto update = store->ApplyEdit(edit);
    REQUIRE(update.status == ConfigurationUpdateStatus::IoError);
    REQUIRE(update.message.find("configuration write failed") != std::string::npos);
    REQUIRE(*store->Current().data == *before.data);
    REQUIRE(store->Current().revision.value == 1);
    REQUIRE(file->Bytes() == previousBytes);
    REQUIRE(store->NotifyCount() == 1);
}

TEST_CASE("configuration store validates edits before touching the file", "[configuration]") {
    MemoryConfigurationFile* file = nullptr;
    const auto store = MakeStore(&file);
    REQUIRE(store->Reload().Succeeded());
    const std::string previousBytes = file->Bytes();

    ConfigurationData invalid = *store->Current().data;
    invalid.categories.multipliers[2] = -0.5;
    const auto rejected = store->ApplyEdit(invalid);
    REQUIRE(rejected.status == ConfigurationUpdateStatus::Invalid);
    REQUIRE(rejected.message.find("configuration edit rejected") != std::string::npos);
    REQUIRE(file->Bytes() == previousBytes);

    ConfigurationData overRange = *store->Current().data;
    overRange.parameters.maxFame = 1e9;
    const auto bounded = store->ApplyEdit(overRange);
    REQUIRE(bounded.status == ConfigurationUpdateStatus::Invalid);
    REQUIRE(file->Bytes() == previousBytes);
}

TEST_CASE("configuration store commits edits through write-then-reread", "[configuration]") {
    MemoryConfigurationFile* file = nullptr;
    const auto store = MakeStore(&file);
    std::uint64_t notified = 0;
    store->BindNotify([&](const ConfigurationSnapshot& snapshot) { notified = snapshot.revision.value; });
    REQUIRE(store->Reload().Succeeded());
    REQUIRE(store->NotifyCount() == 1);

    ConfigurationData edit = *store->Current().data;
    edit.categories.multipliers[slsf::FameCategoryIndex(slsf::FameCategory::Oral)] = 1.5;
    edit.relationships.forbiddenByFriend.Set(slsf::FameCategory::Bound);
    edit.relationships.forbiddenByLover.Set(slsf::FameCategory::CumDump);
    edit.connections.push_back(
        slsf::RegionConnectionSetting{slsf::StableRegionKey{"Whiterun.esp", 256},
                                      slsf::StableRegionKey{"Whiterun.esp", 512}, true});
    edit.menu.selectedRegions = {
        slsf::StableRegionKey{"Whiterun.esp", 256},
        slsf::StableRegionKey{"Riften.esp", 1024},
    };

    const auto update = store->ApplyEdit(edit);
    INFO(update.message);
    REQUIRE(update.status == ConfigurationUpdateStatus::Applied);
    REQUIRE(update.revision.value == 2);
    REQUIRE(store->NotifyCount() == 2);
    REQUIRE(store->Current().data->categories.multipliers[slsf::FameCategoryIndex(slsf::FameCategory::Oral)] == 1.5);
    REQUIRE(store->Current().data->relationships.forbiddenByFriend.Test(
        slsf::FameCategory::Bound));
    REQUIRE(store->Current().data->relationships.forbiddenByLover.Test(
        slsf::FameCategory::CumDump));
    REQUIRE(store->Current().data->connections.size() == 1);
    REQUIRE(store->Current().data->connections.front().from ==
            slsf::StableRegionKey{"Whiterun.esp", 256});
    REQUIRE(store->Current().data->menu.selectedRegions.size() == 2);
    REQUIRE(store->Current().data->menu.selectedRegions.front() ==
            slsf::StableRegionKey{"Riften.esp", 1024});

    const auto reread = store->Reload();
    REQUIRE(reread.status == ConfigurationUpdateStatus::Unchanged);
    REQUIRE(reread.revision.value == 2);

    REQUIRE(file->Bytes().find("\"forbiddenByFriend\": [\n      \"bound\"\n    ]") !=
            std::string::npos);
    REQUIRE(file->Bytes().find("\"forbiddenByLover\": [\n      \"cum dump\"\n    ]") !=
            std::string::npos);
}

TEST_CASE("configuration store resorts connections and selected regions canonically",
          "[configuration]") {
    const auto store = MakeStore();
    REQUIRE(store->Reload().Succeeded());

    ConfigurationData edit = *store->Current().data;
    edit.connections.push_back(
        slsf::RegionConnectionSetting{slsf::StableRegionKey{"Riften.esp", 256},
                                      slsf::StableRegionKey{"Riften.esp", 512}, true});
    edit.connections.push_back(
        slsf::RegionConnectionSetting{slsf::StableRegionKey{"A.esp", 256},
                                      slsf::StableRegionKey{"B.esp", 512}, false});
    edit.connections.push_back(
        slsf::RegionConnectionSetting{slsf::StableRegionKey{"A.esp", 256},
                                      slsf::StableRegionKey{"B.esp", 512}, false});
    edit.menu.selectedRegions = {
        slsf::StableRegionKey{"Zeb.esp", 10},
        slsf::StableRegionKey{"A.esp", 2},
        slsf::StableRegionKey{"Zeb.esp", 10},
        slsf::StableRegionKey{"A.esp", 2},
    };
    const auto update = store->ApplyEdit(edit);
    INFO(update.message);
    REQUIRE(update.Succeeded());
    const auto& connections = store->Current().data->connections;
    REQUIRE(connections.size() == 2);
    REQUIRE(connections.front().from == slsf::StableRegionKey{"A.esp", 256});
    REQUIRE(connections.front().to == slsf::StableRegionKey{"B.esp", 512});
    REQUIRE_FALSE(connections.front().enabled);
    REQUIRE(connections.back().from == slsf::StableRegionKey{"Riften.esp", 256});
    REQUIRE(connections.back().to == slsf::StableRegionKey{"Riften.esp", 512});
    const auto& selected = store->Current().data->menu.selectedRegions;
    REQUIRE(selected.size() == 2);
    REQUIRE(selected.front() == slsf::StableRegionKey{"A.esp", 2});
    REQUIRE(selected.back() == slsf::StableRegionKey{"Zeb.esp", 10});
}

TEST_CASE("configuration store increments revisions per distinct committed content",
          "[configuration]") {
    const auto store = MakeStore();
    std::uint64_t notified = 0;
    store->BindNotify([&](const ConfigurationSnapshot& snapshot) { notified = snapshot.revision.value; });
    REQUIRE(store->Reload().Succeeded());

    ConfigurationData first = *store->Current().data;
    first.timing.decayPeriodDays = 2.0;
    REQUIRE(store->ApplyEdit(first).revision.value == 2);
    REQUIRE(store->Reload().revision.value == 2);
    REQUIRE(store->NotifyCount() == 2);

    ConfigurationData second = first;
    second.timing.decayPeriodDays = 3.0;
    REQUIRE(store->ApplyEdit(second).revision.value == 3);
    ConfigurationData identical = second;
    identical.timing.decayPeriodDays = 3.0;
    const auto repeated = store->ApplyEdit(identical);
    INFO(repeated.message);
    REQUIRE(repeated.status == ConfigurationUpdateStatus::Unchanged);
    REQUIRE(repeated.revision.value == 3);
    REQUIRE(store->NotifyCount() == 3);
}

TEST_CASE("configuration store applies legacy defaults", "[configuration]") {
    const auto defaults = ConfigurationStore::LegacyDefaults();
    REQUIRE(defaults.schemaVersion == slsf::kConfigurationSchemaVersion);
    REQUIRE(defaults.features.reduceFameAtNight);
    REQUIRE(defaults.categories.minimumFameToSpread == 30.0);
    REQUIRE(defaults.categories.maximumSpreadCategories == 5);
    REQUIRE(defaults.categories.maximumSpreadPercentage == 30.0);
    for (const double multiplier : defaults.categories.multipliers) {
        REQUIRE(multiplier == 1.0);
    }
    REQUIRE(defaults.relationships.friendHighFameThreshold == 0.0);
    REQUIRE(defaults.relationships.loverHighFameThreshold == 50.0);
    REQUIRE(defaults.relationships.friendBetrayChance == 50.0);
    REQUIRE(defaults.relationships.loverBetrayChance == 25.0);
    REQUIRE(defaults.timing.decayPeriodDays == 0.5);
    REQUIRE(defaults.timing.spreadPeriodDays == 1.0);
    REQUIRE(defaults.timing.nightStart == 22.0);
    REQUIRE(defaults.timing.nightEnd == 6.0);
    REQUIRE(defaults.timing.failedSpreadIncrease == 10.0);
    REQUIRE(defaults.timing.successfulSpreadReduction == 10.0);
    REQUIRE(defaults.parameters.decayPerDay == 2.0);
    REQUIRE(defaults.parameters.relaxationPerDay == 1.0);
    REQUIRE(defaults.parameters.spreadThreshold == 30.0);
    REQUIRE(defaults.parameters.spreadRetention == 0.6);
    REQUIRE(defaults.parameters.maxFame == 150.0);
}

static std::filesystem::path TestScratchDirectory() {
    std::filesystem::create_directories("build/test-results/config-scratch");
    return "build/test-results/config-scratch";
}

TEST_CASE("disk configuration file persists and replaces atomically", "[configuration]") {
    const std::filesystem::path target =
        TestScratchDirectory() / MakeUniqueFileName("slsf_core_config_");
    {
        auto file = std::make_unique<DiskConfigurationFile>(target.string());
        REQUIRE_FALSE(file->Exists());
        const auto firstWrite = file->Write(R"({"a":1})");
        REQUIRE(firstWrite.has_value());
        REQUIRE(file->Exists());
        const auto replaceWrite = file->Write(R"({"a":2})");
        REQUIRE(replaceWrite.has_value());
        const auto bytes = file->Read();
        REQUIRE(bytes.has_value());
        REQUIRE(*bytes == "{\"a\":2}");
        REQUIRE_FALSE(std::filesystem::exists(target.string() + ".tmp"));
    }
    std::error_code ignored;
    std::filesystem::remove(target, ignored);
    std::filesystem::remove(target.string() + ".tmp", ignored);
}

TEST_CASE("disk configuration file reports io errors on unwritable targets",
          "[configuration]") {
    const std::filesystem::path directory = TestScratchDirectory();
    const std::filesystem::path target = directory / MakeUniqueFileName("slsf_core_locked_");
    {
        std::ofstream blocker(target, std::ios::binary | std::ios::trunc);
        blocker << "not a directory";
    }
    const auto file = std::make_unique<DiskConfigurationFile>((target / "cfg.json").string());
    REQUIRE_FALSE(file->Exists());
    REQUIRE(file->Read().error().code == slsf::Error::Code::Io);
    const auto written = file->Write("{}");
    REQUIRE_FALSE(written.has_value());
    REQUIRE(written.error().code == slsf::Error::Code::Io);
    REQUIRE(file->Exists() == false);
    REQUIRE_FALSE(std::filesystem::exists((target / "cfg.json.tmp").string()));

    std::error_code ignored;
    std::filesystem::remove(target, ignored);
}

TEST_CASE("configuration store reports io error when default generation fails",
          "[configuration]") {
    MemoryConfigurationFile* file = nullptr;
    const auto store = MakeStore(&file);
    file->FailNextWrite();
    store->BindNotify([](const ConfigurationSnapshot&) {});

    const auto update = store->Reload();
    REQUIRE(update.status == ConfigurationUpdateStatus::IoError);
    REQUIRE(update.message.find("could not be generated") != std::string::npos);
    REQUIRE_FALSE(update.Succeeded());
    REQUIRE(update.revision.value == 1);
    REQUIRE(store->Current().HasData());
    REQUIRE(store->Current().data->categories.minimumFameToSpread == 30.0);
    REQUIRE(store->NotifyCount() == 1);
}

TEST_CASE("configuration store re-enters snapshot reads from the notify callback",
          "[configuration]") {
    MemoryConfigurationFile* file = nullptr;
    const auto store = MakeStore(&file);
    file->OverwriteExternally("{\"schemaVersion\": 1, \"timing\": {\"nightStart\": 1.0}}");

    std::uint64_t observed = 0;
    int calls = 0;
    store->BindNotify([&](const ConfigurationSnapshot&) {
        ++calls;
        observed = store->Current().revision.value;
    });

    const auto update = store->Reload();
    REQUIRE(update.Succeeded());
    REQUIRE(store->NotifyCount() == 1);
    REQUIRE(observed == 1);
    REQUIRE(calls == 1);
}

TEST_CASE("configuration store applies edit edits that only validate passes admit",
          "[configuration]") {
    MemoryConfigurationFile* file = nullptr;
    const auto store = MakeStore(&file);
    store->BindNotify([](const ConfigurationSnapshot&) {});
    REQUIRE(store->Reload().Succeeded());

    ConfigurationData edit = *store->Current().data;
    edit.timing.nightStart = 23.9995;
    edit.timing.nightEnd = 23.999;

    const auto update = store->ApplyEdit(edit);
    REQUIRE(update.Succeeded());
    REQUIRE(update.status == ConfigurationUpdateStatus::Applied);
    REQUIRE(store->Current().data->timing.nightStart == 23.9995);
    REQUIRE(store->NotifyCount() == 2);
}

TEST_CASE("configuration store accepts files without an explicit schemaVersion",
          "[configuration]") {
    MemoryConfigurationFile* file = nullptr;
    const auto store = MakeStore(&file);

    file->OverwriteExternally(
        "{\"categories\": {\"minimumFameToSpread\": 25}}");
    const auto update = store->Reload();
    REQUIRE(update.Succeeded());
    REQUIRE(update.revision.value == 1);
    REQUIRE(store->Current().data->categories.minimumFameToSpread == 25.0);
    REQUIRE(store->Current().data->schemaVersion == slsf::kConfigurationSchemaVersion);
}
