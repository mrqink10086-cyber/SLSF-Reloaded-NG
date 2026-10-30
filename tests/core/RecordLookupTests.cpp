#include "slsf/compat/LegacyOutputs.h"
#include "slsf/compat/PapyrusBindings.h"
#include "slsf/engine/RecordLookup.h"

#include <catch2/catch_test_macros.hpp>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

using slsf::StableFormKey;
using slsf::engine::LoadedRecordKey;
using slsf::engine::LookupLoadedRecord;

struct File {
    std::string name;
    bool light{};
    bool IsLight() const { return light; }
    const char* GetFilename() const { return name.c_str(); }
};

struct Form {
    File* file{};
    std::uint32_t local{};
    std::uint32_t runtime{};
    virtual ~Form() = default;
    const File* GetFile(int) const { return file; }
    std::uint32_t GetLocalFormID() const { return local; }
};
struct Global final : Form { int value{}; };
struct Quest final : Form {};

struct LoadedData {
    std::map<std::string, File> files;
    std::map<StableFormKey, std::unique_ptr<Form>> records;
    std::vector<StableFormKey> lookups;

    const File* LookupModByName(std::string_view name) {
        const auto it = files.find(std::string(name));
        return it == files.end() ? nullptr : &it->second;
    }
    template <class T>
    T* LookupForm(std::uint32_t local, std::string_view plugin) {
        StableFormKey key{std::string(plugin), local};
        lookups.push_back(key);
        const auto it = records.find(key);
        return it == records.end() ? nullptr : dynamic_cast<T*>(it->second.get());
    }
    template <class T>
    T* Add(const StableFormKey& key, std::uint32_t runtime) {
        auto form = std::make_unique<T>();
        form->file = &files.at(key.plugin);
        form->local = key.localFormId;
        form->runtime = runtime;
        auto* result = form.get();
        records.emplace(key, std::move(form));
        return result;
    }
};

struct EngineSink final : slsf::compat::LegacyOutputSink {
    LoadedData& data;
    std::size_t writes{};
    explicit EngineSink(LoadedData& loaded) : data(loaded) {}
    void SetCurrentFameGlobal(const StableFormKey& key, int value) override {
        if (auto* global = LookupLoadedRecord<Global>(&data, key)) {
            global->value = value;
            ++writes;
        }
    }
    void SetStorageIntList(const std::string&, std::size_t, int) override {}
    void SetLegacyBridgeGlobal(const StableFormKey& key, int value) override {
        SetCurrentFameGlobal(key, value);
    }
};

TEST_CASE("legacy Global and quest lookups survive full and light load order changes",
          "[slsf][formid-fix]") {
    using slsf::compat::LegacyOutputs;
    for (const std::uint32_t runtimeBase : {0x06000000u, 0x35000000u,
                                           0xFE000000u, 0xFE037000u}) {
        CAPTURE(runtimeBase);
        LoadedData data;
        const bool light = (runtimeBase >> 24) == 0xFE;
        data.files.emplace("SLSF Reloaded.esp", File{"SLSF Reloaded.esp", light});
        data.files.emplace("SexLab - Sexual Fame [SLSF].esm",
                           File{"SexLab - Sexual Fame [SLSF].esm", false});
        for (std::size_t c = 0; c < slsf::kFameCategoryCount; ++c) {
            const auto key = LegacyOutputs::CurrentFameGlobalKey(
                static_cast<slsf::FameCategory>(c));
            data.Add<Global>(key, runtimeBase + key.localFormId);
        }
        for (std::size_t i = 0; i < 20; ++i) {
            const auto key = LegacyOutputs::LegacyBridgeKey(i);
            data.Add<Global>(key, 0x42000000u + key.localFormId);
        }
        slsf::compat::PapyrusBindings bindings;
        for (const auto& fn : bindings.Functions()) {
            if (!fn.quest.plugin.empty() && !data.records.contains(fn.quest)) {
                data.Add<Quest>(fn.quest, runtimeBase + fn.quest.localFormId);
            }
        }
        EngineSink sink(data);
        LegacyOutputs outputs;
        const slsf::compat::FameValueResolver resolve = [](const std::string&,
                                                          slsf::FameCategory c) {
            return 20 + static_cast<int>(slsf::FameCategoryIndex(c));
        };
        CHECK(outputs.PublishCurrentFameGlobals(sink, resolve, "Whiterun") == 25);
        CHECK(outputs.PublishLegacyBridge(sink, resolve, "Whiterun") == 20);
        CHECK(sink.writes == 45);
        for (std::size_t c = 0; c < slsf::kFameCategoryCount; ++c) {
            const auto key = LegacyOutputs::CurrentFameGlobalKey(
                static_cast<slsf::FameCategory>(c));
            const auto* global = dynamic_cast<Global*>(data.records.at(key).get());
            REQUIRE(global);
            CHECK(global->runtime == runtimeBase + key.localFormId);
            CHECK(global->value == 20 + c);
        }
        for (const auto& fn : bindings.Functions()) {
            if (fn.quest.plugin.empty()) continue;
            const auto* quest = LookupLoadedRecord<Quest>(&data, fn.quest);
            REQUIRE(quest);
            CHECK(quest->runtime == runtimeBase + fn.quest.localFormId);
            CHECK(bindings.IsBoundTo(fn, {LoadedRecordKey(quest), fn.aliasIndex}));
        }
        for (const auto& key : data.lookups) {
            CHECK(key.localFormId < 0x01000000u);
            if (key.plugin == "SLSF Reloaded.esp") CHECK(key.localFormId <= 0xFFFu);
        }
    }
}

TEST_CASE("loaded record lookup rejects absent records and wrong types",
          "[slsf][formid-fix]") {
    LoadedData data;
    data.files.emplace("SLSF Reloaded.esp", File{"SLSF Reloaded.esp", true});
    const StableFormKey whore{"SLSF Reloaded.esp", 0x80D};
    const StableFormKey managers{"SLSF Reloaded.esp", 0x80A};
    data.Add<Quest>(whore, 0xFE03780D);
    data.Add<Global>(managers, 0xFE03780A);
    EngineSink sink(data);
    sink.SetCurrentFameGlobal(whore, 90);
    sink.SetCurrentFameGlobal({"SLSF Reloaded.esp", 0x80C}, 90);
    sink.SetLegacyBridgeGlobal({"SexLab - Sexual Fame [SLSF].esm", 0x24255}, 90);
    CHECK(sink.writes == 0);
    CHECK_FALSE(LookupLoadedRecord<Quest>(&data, managers));
    CHECK_FALSE(LookupLoadedRecord<Global>(&data, {"Other.esp", 0x80D}));
    CHECK(LoadedRecordKey(static_cast<Quest*>(nullptr)) == StableFormKey{});
    Quest temporary;
    CHECK(LoadedRecordKey(&temporary) == StableFormKey{});

    data.records.erase(whore);
    auto* ready = data.Add<Global>(whore, 0xFE03780D);
    sink.SetCurrentFameGlobal(whore, 90);
    CHECK(sink.writes == 1);
    CHECK(ready->value == 90);
}

TEST_CASE("loaded record lookup never sends full runtime IDs as local IDs",
          "[slsf][formid-fix]") {
    LoadedData data;
    data.files.emplace("SLSF Reloaded.esp", File{"SLSF Reloaded.esp", true});
    CHECK_FALSE(LookupLoadedRecord<Global>(static_cast<LoadedData*>(nullptr),
                                          {"SLSF Reloaded.esp", 0x80D}));
    for (const StableFormKey& key : {
             StableFormKey{}, {"", 0x80D}, {"SLSF Reloaded.esp", 0},
             {"SLSF Reloaded.esp", 0x0600080D}, {"SLSF Reloaded.esp", 0xFE03780D},
             {"SLSF Reloaded.esp", 0x3780D}}) {
        CHECK_FALSE(LookupLoadedRecord<Global>(&data, key));
    }
    CHECK(data.lookups.empty());
}

}
