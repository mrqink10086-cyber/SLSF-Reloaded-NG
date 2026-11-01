#include "slsf/integrations/RaceMenu.h"
#include "slsf/integrations/SlaveTats.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdio>
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <random>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace {

using slsf::Epoch;
using slsf::integrations::CapabilityState;
using slsf::integrations::ExternalAvailability;
using slsf::integrations::ExternalGate;
using slsf::integrations::JcHandle;
using slsf::integrations::JcString;
using slsf::integrations::JcTable;
using slsf::integrations::RaceMenu;
using slsf::integrations::RaceMenuCapabilityRecord;
using slsf::integrations::RaceMenuExchangePort;
using slsf::integrations::RaceMenuSnapshot;
using slsf::integrations::SlaveTats;
using slsf::integrations::SlaveTatsArea;
using slsf::integrations::SlaveTatsPlacement;
using slsf::integrations::SlaveTatsPort;
using slsf::integrations::SlaveTatsCollectResult;
using slsf::integrations::SlaveTatsSnapshot;
using slsf::integrations::kMaxSlaveTatsNameBytes;
using slsf::integrations::kMaxSlaveTatsPlacements;
using slsf::integrations::kSlaveTatsAreaCount;
using slsf::integrations::kSlaveTatsInterfaceMessageType;
using slsf::integrations::kSlaveTatsInterfaceVersion;

struct FakeMap {
    void** vtable;
    void* overlay;
};

struct FakeOverlay {
    void** vtable;
    std::uint32_t version;
    std::array<std::uint32_t, 4> counts;
};

void* FakeQueryInterface(void* map, const char* name) {
    auto* self = static_cast<FakeMap*>(map);
    return std::string_view{name} == "Overlay" ? self->overlay : nullptr;
}

std::uint32_t FakeGetVersion(void* interface) {
    return static_cast<FakeOverlay*>(interface)->version;
}

std::uint32_t FakeGetOverlayCount(void* interface, std::uint32_t type, std::uint32_t location) {
    return type == 0 ? static_cast<FakeOverlay*>(interface)->counts.at(location) : 0;
}

class TestExchangePort final : public RaceMenuExchangePort {
public:
    int exchanges{};
    std::deque<void*> results;

    void* Exchange() noexcept override {
        exchanges += 1;
        if (results.empty()) {
            return nullptr;
        }
        void* next = results.front();
        results.pop_front();
        return next;
    }
};

struct FakeAddressesTable {
    std::uint32_t current_version{kSlaveTatsInterfaceVersion};
    void* pointers[15]{};
};

struct FakeJcObject {
    enum class Kind { Array, Entry };

    Kind kind{Kind::Entry};
    std::vector<std::int32_t> arrayItems;
    std::map<std::int32_t, std::int32_t> ints;
    std::map<std::int32_t, JcString> strs;
};

class FakeJcDomain {
public:
    std::map<std::int32_t, FakeJcObject> objects;
    std::deque<std::int32_t> released;
    std::int32_t nextId{100};
    bool negativeCount{};

    std::int32_t Create(FakeJcObject::Kind kind) {
        const std::int32_t id = nextId++;
        objects[id] = FakeJcObject{kind};
        return id;
    }
};

FakeJcDomain* g_jcDomain{};
std::map<std::uint64_t, std::string> g_stringTexts;
std::map<std::string, std::uint64_t> g_stringIds;
std::int32_t g_stringNextId{1};

FakeJcObject& Object(std::int32_t id) {
    return g_jcDomain->objects.at(id);
}

std::int32_t FakeArrayCount(void*, std::int32_t array) {
    if (g_jcDomain->negativeCount) {
        return -1;
    }
    const auto found = g_jcDomain->objects.find(array);
    if (found == g_jcDomain->objects.end() || found->second.kind != FakeJcObject::Kind::Array) {
        return -1;
    }
    return static_cast<std::int32_t>(found->second.arrayItems.size());
}

std::int32_t FakeArrayObject(void* domain) {
    return static_cast<FakeJcDomain*>(domain)->Create(FakeJcObject::Kind::Array);
}

std::int32_t FakeArrayGetObj(void*, std::int32_t array, std::int32_t index,
                             std::int32_t fallback) {
    const auto found = g_jcDomain->objects.find(array);
    if (found == g_jcDomain->objects.end() || found->second.kind != FakeJcObject::Kind::Array) {
        return fallback;
    }
    const auto& items = found->second.arrayItems;
    if (index < 0 || static_cast<std::size_t>(index) >= items.size()) {
        return fallback;
    }
    return items.at(static_cast<std::size_t>(index));
}

void FakeArrayAddObj(void*, std::int32_t array, std::int32_t value, std::int32_t index) {
    auto& items = g_jcDomain->objects.at(array).arrayItems;
    if (index < 0) {
        items.push_back(value);
        return;
    }
    items.insert(items.begin() + index, value);
}

JcString FakeMapGetStr(void*, std::int32_t object, JcString key, JcString fallback) {
    const auto found = g_jcDomain->objects.find(object);
    if (found == g_jcDomain->objects.end() || found->second.kind != FakeJcObject::Kind::Entry) {
        return fallback;
    }
    const auto strings = found->second.strs.find(static_cast<std::int32_t>(key.opaque));
    return strings == found->second.strs.end() ? fallback : strings->second;
}

std::int32_t FakeMapGetInt(void*, std::int32_t object, JcString key, std::int32_t fallback) {
    const auto found = g_jcDomain->objects.find(object);
    if (found == g_jcDomain->objects.end() || found->second.kind != FakeJcObject::Kind::Entry) {
        return fallback;
    }
    const auto ints = found->second.ints.find(static_cast<std::int32_t>(key.opaque));
    return ints == found->second.ints.end() ? fallback : ints->second;
}

std::int32_t FakeRelease(void*, std::int32_t object) {
    g_jcDomain->released.push_back(object);
    g_jcDomain->objects.erase(object);
    return 1;
}

std::int32_t FakeRetain(void*, std::int32_t object, JcString) {
    return object;
}

JcString FakeIntern(const char* text) {
    const std::string value{text};
    const auto known = g_stringIds.find(value);
    if (known != g_stringIds.end()) {
        return JcString{known->second};
    }
    const auto id = static_cast<std::uint64_t>(g_stringNextId++);
    g_stringIds.emplace(value, id);
    g_stringTexts.emplace(id, value);
    return JcString{id};
}

std::string FakeText(JcString value) {
    const auto found = g_stringTexts.find(value.opaque);
    return found == g_stringTexts.end() ? std::string{} : found->second;
}

class TestSlaveTatsPort final : public SlaveTatsPort {
public:
    JcTable table{};
    std::map<std::uint32_t, void*> actors;

    const JcTable* Table() noexcept override { return &table; }
    JcString Intern(const char* text) noexcept override { return FakeIntern(text); }
    std::string Text(JcString value) noexcept override { return FakeText(value); }
    void* ResolveActor(std::uint32_t formId) noexcept override {
        const auto found = actors.find(formId);
        return found == actors.end() ? nullptr : found->second;
    }
};

struct FakeField {
    std::string name;
    std::int32_t slot;
};

struct FakeSlaveTatsTarget {
    std::array<std::vector<FakeField>, 4> entries;
    bool fail{};
    bool negativeCount{};
};

bool FakeBatchByArea(void* target, std::int32_t body, std::int32_t face, std::int32_t hands,
                     std::int32_t feet) {
    auto* self = static_cast<FakeSlaveTatsTarget*>(target);
    if (self->fail) {
        return true;
    }
    const std::array<std::int32_t, 4> arrays{body, face, hands, feet};
    for (std::size_t area = 0; area < 4; ++area) {
        for (const FakeField& field : self->entries.at(area)) {
            const std::int32_t entry = g_jcDomain->Create(FakeJcObject::Kind::Entry);
            Object(entry).ints.emplace(static_cast<std::int32_t>(FakeIntern("slot").opaque),
                                       field.slot);
            Object(entry).strs.emplace(static_cast<std::int32_t>(FakeIntern("name").opaque),
                                       FakeIntern(field.name.c_str()));
            FakeArrayAddObj(g_jcDomain, arrays.at(area), entry, -1);
        }
    }
    if (self->negativeCount) {
        g_jcDomain->negativeCount = true;
    }
    return false;}

class SlaveTatsFixture {
public:
    FakeJcDomain domain;
    JcTable table;
    FakeAddressesTable broadcast;
    TestSlaveTatsPort port;
    FakeSlaveTatsTarget target;
    bool negativeCount{};
    SlaveTats tats;

    SlaveTatsFixture() : tats{port} {
        g_jcDomain = &domain;
        table.domain = &domain;
        table.arrayCount = &FakeArrayCount;
        table.arrayObject = &FakeArrayObject;
        table.arrayGetObj = &FakeArrayGetObj;
        table.mapGetStr = &FakeMapGetStr;
        table.mapGetInt = &FakeMapGetInt;
        table.release = &FakeRelease;
        table.retain = &FakeRetain;
        port.table = table;
        broadcast.pointers[14] = reinterpret_cast<void*>(&FakeBatchByArea);
        target.entries.at(static_cast<std::size_t>(SlaveTatsArea::Body)).assign(
            {FakeField{"Alpha", 0}, FakeField{"Beta", 2}, FakeField{"Gamma", 2}});
        target.entries.at(static_cast<std::size_t>(SlaveTatsArea::Face)).assign(
            {FakeField{"Theta", 1}, FakeField{"Iota", 1}});
        target.entries.at(static_cast<std::size_t>(SlaveTatsArea::Hands)).assign(
            {FakeField{"Mono", 0}});
        target.entries.at(static_cast<std::size_t>(SlaveTatsArea::Feet)).assign(
            {FakeField{"Omicron", 0}});
        port.actors.emplace(0x14u, &target);
        tats.OnSessionChanged(kEpoch);
    }

    ~SlaveTatsFixture() { g_jcDomain = nullptr; }

    static constexpr Epoch kEpoch{1, 2, 3};

    void Broadcast() {
        REQUIRE(tats.ConsumeInterfaceMessage(kSlaveTatsInterfaceMessageType, &broadcast,
                                             sizeof(void*)));
    }

    SlaveTatsCollectResult CollectReady() {
        Broadcast();
        auto result = tats.Collect(0x14u, SlaveTatsFixture::kEpoch);
        REQUIRE(result.gate == ExternalGate::Ready);
        return result;
    }
};

}

TEST_CASE("jc handle owns and releases exactly once", "[slavetats][jc][raii]") {
    FakeJcDomain domain;
    g_jcDomain = &domain;
    JcTable table;
    table.domain = &domain;
    table.arrayObject = &FakeArrayObject;
    table.release = &FakeRelease;

    {
        JcHandle empty;
        CHECK_FALSE(empty.Owns());
        CHECK(empty.get() == 0);
        empty.Release();
        CHECK(domain.released.empty());
    }
    {
        JcHandle owned{&table, FakeArrayObject(&domain)};
        REQUIRE(owned.Owns());
        CHECK(owned.get() != 0);
    }
    REQUIRE(domain.released.size() == 1);
    {
        JcHandle owned{&table, FakeArrayObject(&domain)};
        owned.Release();
        CHECK_FALSE(owned.Owns());
        CHECK(domain.released.size() == 2);
    }
    {
        JcHandle first{&table, FakeArrayObject(&domain)};
        JcHandle second{std::move(first)};
        CHECK(second.Owns());
        CHECK_FALSE(first.Owns());
        CHECK(first.get() == 0);
        JcHandle third;
        third = std::move(second);
        CHECK(third.Owns());
    }
    REQUIRE(domain.released.size() == 3);
    {
        JcHandle owned{&table, FakeArrayObject(&domain)};
        JcHandle& alias = owned;
        alias = std::move(owned);
        CHECK(owned.Owns());
    }
    REQUIRE(domain.released.size() == 4);
    CHECK(domain.objects.empty());
    g_jcDomain = nullptr;
}

TEST_CASE("racemenu gather waits until the exchange produces the map", "[racemenu]") {
    TestExchangePort port;
    RaceMenu menu{port};
    Epoch epoch{1, 2, 3};

    for (int attempt = 1; attempt <= 19; ++attempt) {
        CHECK(menu.Ensure(epoch).gate == ExternalGate::Waiting);
        CHECK(menu.Attempts() == static_cast<std::uint32_t>(attempt));
    }
    CHECK(menu.Ensure(epoch).gate == ExternalGate::Failed);
    CHECK(menu.LastError() == "RaceMenu interface exchange returned no map");

    void* mapVtable[1] = {reinterpret_cast<void*>(&FakeQueryInterface)};
    void* overlayVtable[15] = {};
    overlayVtable[1] = reinterpret_cast<void*>(&FakeGetVersion);
    overlayVtable[11] = reinterpret_cast<void*>(&FakeGetOverlayCount);
    FakeOverlay overlay{&overlayVtable[0], 2u, {3u, 0u, 1u, 4u}};
    FakeMap map{&mapVtable[0], &overlay};
    menu.Reset();
    port.results.push_back(&map);

    const auto ready = menu.Ensure(epoch);
    CHECK(ready.gate == ExternalGate::Ready);
    CHECK(ready.changed);
    CHECK(port.exchanges == 21);
    const auto snapshot = menu.Snapshot();
    CHECK(snapshot.availability == ExternalAvailability::Available);
    CHECK(snapshot.overlayInterfaceVersion == 2u);
    CHECK(snapshot.overlays.counts == std::array<std::uint32_t, 4>{3u, 0u, 1u, 4u});
    CHECK(menu.LastError().empty());
    CHECK(menu.Dirty() == false);

    const auto again = menu.Ensure(epoch);
    CHECK_FALSE(again.changed);
    CHECK(again.gate == ExternalGate::Ready);
    CHECK(port.exchanges == 21);
    std::vector<RaceMenuCapabilityRecord> records;
    menu.Capabilities(records);
    REQUIRE(records.size() == 3);
    CHECK(records.at(0).name == "InterfaceMap");
    CHECK(records.at(0).state == CapabilityState::Available);
    CHECK(records.at(1).name == "OverlayInterface");
    CHECK(records.at(1).state == CapabilityState::Available);
    CHECK(records.at(1).version == 2u);
    CHECK(records.at(2).name == "GetOverlayCount");
    CHECK(records.at(2).state == CapabilityState::Available);
}

TEST_CASE("racemenu rejects unregistered or incompatible overlay interfaces", "[racemenu]") {
    void* mapVtable[1] = {reinterpret_cast<void*>(&FakeQueryInterface)};
    void* overlayVtable[15] = {};
    overlayVtable[1] = reinterpret_cast<void*>(&FakeGetVersion);
    overlayVtable[11] = reinterpret_cast<void*>(&FakeGetOverlayCount);
    FakeOverlay overlay{&overlayVtable[0], 2u, {1u, 1u, 1u, 1u}};
    FakeMap map{&mapVtable[0], &overlay};
    Epoch epoch{7, 0, 0};

    SECTION("overlay interface missing") {
        map.overlay = nullptr;
        TestExchangePort port;
        port.results.push_back(&map);
        RaceMenu menu{port};
        auto result = menu.Ensure(epoch);
        CHECK(result.gate == ExternalGate::Missing);
        CHECK(result.changed);
        CHECK(menu.Snapshot().availability == ExternalAvailability::Missing);
        CHECK(port.exchanges == 1);
    }

    SECTION("overlay version below required") {
        overlay.version = 1u;
        TestExchangePort port;
        port.results.push_back(&map);
        RaceMenu menu{port};
        CHECK(menu.Ensure(epoch).gate == ExternalGate::Incompatible);
        CHECK(menu.Snapshot().availability == ExternalAvailability::Incompatible);
    }

    SECTION("version entry missing at the verified slot") {
        overlayVtable[1] = nullptr;
        TestExchangePort port;
        port.results.push_back(&map);
        RaceMenu menu{port};
        CHECK(menu.Ensure(epoch).gate == ExternalGate::Incompatible);
        CHECK(menu.Snapshot().availability == ExternalAvailability::Incompatible);
    }

    SECTION("count entry missing at the verified slot") {
        overlayVtable[11] = nullptr;
        TestExchangePort port;
        port.results.push_back(&map);
        RaceMenu menu{port};
        CHECK(menu.Ensure(epoch).gate == ExternalGate::Incompatible);
        CHECK(menu.Snapshot().availability == ExternalAvailability::Incompatible);
    }
}

TEST_CASE("racemenu lifecycle tracks dirty stale and sessions", "[racemenu][lifecycle]") {
    void* mapVtable[1] = {reinterpret_cast<void*>(&FakeQueryInterface)};
    void* overlayVtable[15] = {};
    overlayVtable[1] = reinterpret_cast<void*>(&FakeGetVersion);
    overlayVtable[11] = reinterpret_cast<void*>(&FakeGetOverlayCount);
    FakeOverlay overlay{&overlayVtable[0], 2u, {2u, 0u, 0u, 0u}};
    FakeMap map{&mapVtable[0], &overlay};
    TestExchangePort port;
    port.results.push_back(&map);
    RaceMenu menu{port};
    Epoch epoch{3, 4, 5};
    REQUIRE(menu.Ensure(epoch).gate == ExternalGate::Ready);

    SECTION("mark dirty outside the epoch is ignored") {
        menu.MarkDirty(Epoch{9, 9, 9});
        CHECK_FALSE(menu.Dirty());
        CHECK_FALSE(menu.Ensure(epoch).changed);
        CHECK(port.exchanges == 1);
    }

    SECTION("mark dirty re-queries and reports count change") {
        overlay.counts = {5u, 1u, 0u, 0u};
        port.results.push_back(&map);
        menu.MarkDirty(epoch);
        CHECK(menu.Dirty());
        CHECK(menu.Ensure(epoch).changed);
        CHECK(menu.Snapshot().overlays.counts == std::array<std::uint32_t, 4>{5u, 1u, 0u, 0u});
        CHECK(port.exchanges == 2);
        port.results.push_back(&map);
        menu.MarkDirty(epoch);
        CHECK_FALSE(menu.Ensure(epoch).changed);
        CHECK(port.exchanges == 3);
    }

    SECTION("stale epoch results are not applied") {
        overlay.counts = {9u, 9u, 9u, 9u};
        port.results.push_back(&map);
        menu.MarkDirty(epoch);
        CHECK(menu.Dirty());
        const auto stale = menu.Ensure(Epoch{4, 4, 4});
        CHECK(stale.gate == ExternalGate::Stale);
        CHECK_FALSE(stale.changed);
        CHECK(menu.Snapshot().epoch == epoch);
        CHECK(menu.Snapshot().overlays.counts.at(0) == 2u);
        CHECK(menu.Dirty());
    }

    SECTION("session change clears the snapshot and re-collects") {
        menu.OnSessionChanged(Epoch{6, 1, 1});
        CHECK(menu.Snapshot().epoch == Epoch{6, 1, 1});
        CHECK(menu.Snapshot().availability == ExternalAvailability::Unknown);
        CHECK(menu.Dirty());
        CHECK(menu.Gate() == ExternalGate::Changed);
        port.results.push_back(&map);
        CHECK(menu.Ensure(Epoch{6, 1, 1}).gate == ExternalGate::Ready);
        CHECK(menu.Snapshot().epoch == Epoch{6, 1, 1});
    }

    SECTION("cancel closes and reset reopens") {
        menu.Cancel();
        CHECK(menu.Gate() == ExternalGate::Closed);
        CHECK(menu.Ensure(epoch).gate == ExternalGate::Closed);
        menu.Reset();
        CHECK(menu.Gate() == ExternalGate::Waiting);
        CHECK(menu.Dirty());
    }
}

TEST_CASE("racemenu random exchange sequences stay consistent", "[racemenu][differential]") {
    std::mt19937_64 rng{1170u};
    void* mapVtable[1] = {reinterpret_cast<void*>(&FakeQueryInterface)};
    void* overlayVtable[15] = {};
    overlayVtable[1] = reinterpret_cast<void*>(&FakeGetVersion);
    overlayVtable[11] = reinterpret_cast<void*>(&FakeGetOverlayCount);
    FakeOverlay overlay{&overlayVtable[0], 2u, {0u, 0u, 0u, 0u}};
    FakeMap map{&mapVtable[0], &overlay};
    TestExchangePort port;
    Epoch epoch{1, 1, 0};
    std::vector<RaceMenuSnapshot> history;

    for (int step = 0; step < 300; ++step) {
        if (std::uniform_int_distribution<int>{0, 3}(rng) == 0) {
            overlay.version = std::uniform_int_distribution<std::uint32_t>{2, 3}(rng);
            overlay.counts.at(0) = std::uniform_int_distribution<std::uint32_t>{0, 16}(rng);
        }
        const bool deliver = std::uniform_int_distribution<int>{0, 2}(rng) != 0;
        if (deliver && (port.results.empty() || port.results.size() < 4)) {
            port.results.push_back(&map);
        }
        RaceMenu menu{port};
        for (int probe = 0; probe < 3; ++probe) {
            const auto result = menu.Ensure(epoch);
            history.push_back(menu.Snapshot());
            CHECK(menu.Attempts() <= 20u);
            if (result.gate == ExternalGate::Ready) {
                CHECK(menu.Snapshot().availability == ExternalAvailability::Available);
                CHECK(menu.Snapshot().overlayInterfaceVersion == overlay.version);
                CHECK(menu.Snapshot().overlays.counts.at(0) == overlay.counts.at(0));
                break;
            }
            CHECK(result.gate == ExternalGate::Waiting);
        }
    }
    CHECK_FALSE(history.empty());
}

TEST_CASE("slavetats interface broadcast protocol", "[slavetats][protocol]") {
    SlaveTatsFixture fixture;
    SlaveTats& tats = fixture.tats;
    CHECK(tats.Capability().state == CapabilityState::Unqueried);

    CHECK_FALSE(tats.ConsumeInterfaceMessage(kSlaveTatsInterfaceMessageType, nullptr,
                                             sizeof(void*)));
    CHECK(tats.Capability().state == CapabilityState::Unqueried);
    CHECK(tats.Capability().rejections == 1u);
    CHECK_FALSE(tats.ConsumeInterfaceMessage(kSlaveTatsInterfaceMessageType,
                                             &fixture.broadcast, 4u));
    CHECK(tats.Capability().rejections == 2u);
    CHECK_FALSE(tats.ConsumeInterfaceMessage(2, &fixture.broadcast, sizeof(void*)));
    CHECK(tats.Capability().rejections == 3u);

    fixture.broadcast.current_version = 5u;
    CHECK(tats.ConsumeInterfaceMessage(kSlaveTatsInterfaceMessageType, &fixture.broadcast,
                                       sizeof(void*)));
    CHECK(tats.Capability().state == CapabilityState::Incompatible);
    CHECK(tats.Capability().version == 5u);
    CHECK(tats.Capability().confirmations == 0u);
    CHECK(tats.Gate() == ExternalGate::Waiting);

    fixture.broadcast.current_version = kSlaveTatsInterfaceVersion;
    CHECK(tats.ConsumeInterfaceMessage(kSlaveTatsInterfaceMessageType, &fixture.broadcast,
                                       sizeof(void*)));
    CHECK(tats.Capability().state == CapabilityState::Available);
    CHECK(tats.Capability().confirmations == 1u);
    CHECK(tats.Gate() == ExternalGate::Waiting);
    CHECK(tats.ConsumeInterfaceMessage(kSlaveTatsInterfaceMessageType, &fixture.broadcast,
                                       sizeof(void*)));
    CHECK(tats.Capability().confirmations == 2u);
    CHECK_FALSE(tats.ConsumeInterfaceMessage(kSlaveTatsInterfaceMessageType, &fixture.broadcast,
                                             2u));
    CHECK(tats.Capability().rejections == 4u);
    CHECK(tats.Capability().state == CapabilityState::Available);
    CHECK_FALSE(tats.ConsumeInterfaceMessage(kSlaveTatsInterfaceMessageType, &fixture.broadcast,
                                             16u));
    CHECK(tats.Capability().rejections == 5u);
}

TEST_CASE("slavetats collects areas into owned records", "[slavetats][collect]") {
    SlaveTatsFixture fixture;
    SlaveTats& tats = fixture.tats;
    const Epoch epoch{1, 2, 3};
    auto result = fixture.CollectReady();
    CHECK(result.changed);
    const auto snapshot = tats.Snapshot();
    CHECK(snapshot.availability == ExternalAvailability::Available);
    CHECK(snapshot.interfaceVersion == kSlaveTatsInterfaceVersion);
    const auto& face =
        snapshot.areas.at(static_cast<std::size_t>(SlaveTatsArea::Face)).placements;
    const auto& feet =
        snapshot.areas.at(static_cast<std::size_t>(SlaveTatsArea::Feet)).placements;
    REQUIRE(face.size() == 2);
    CHECK(face.at(0).name == "Theta");
    CHECK(face.at(0).slot == 1);
    CHECK(face.at(1).name == "Iota");
    CHECK(face.at(1).slot == 1);
    REQUIRE(feet.size() == 1);
    CHECK(feet.at(0).name == "Omicron");
    CHECK(feet.at(0).slot == 0);
    const auto& body =
        snapshot.areas.at(static_cast<std::size_t>(SlaveTatsArea::Body)).placements;
    REQUIRE(body.size() == 3);
    CHECK(body.at(0).name == "Alpha");
    CHECK(body.at(0).slot == 0);
    CHECK(body.at(1).name == "Beta");
    CHECK(body.at(1).slot == 2);
    CHECK(body.at(2).name == "Gamma");
    CHECK(body.at(2).slot == 2);
    CHECK(snapshot.areas.at(static_cast<std::size_t>(SlaveTatsArea::Hands)).placements.size() ==
          1);
    CHECK(tats.Gate() == ExternalGate::Ready);
    CHECK(tats.Attempts() == 0u);
    CHECK(tats.LastError().empty());

    CHECK(fixture.domain.released.size() == 4);

    SECTION("repeat collect reports unchanged") {
        auto second = fixture.CollectReady();
        CHECK_FALSE(second.changed);
        CHECK(tats.Snapshot() == snapshot);
        CHECK(fixture.domain.released.size() == 8);
    }

    SECTION("mark dirty triggers recollect without change") {
        tats.MarkDirty(epoch);
        CHECK(tats.Dirty());
        auto collect = fixture.CollectReady();
        CHECK_FALSE(collect.changed);
        CHECK(tats.Attempts() == 0u);
    }

    SECTION("mark dirty outside epoch ignored") {
        tats.MarkDirty(Epoch{8, 8, 8});
        CHECK_FALSE(tats.Dirty());
    }

    SECTION("slot facts projection") {
        const auto facts =
            slsf::integrations::SlaveTatsSlotFacts(snapshot.Area(SlaveTatsArea::Body), 5);
        REQUIRE(facts.size() == 5);
        CHECK(facts.at(0).applied.IsKnownTrue());
        CHECK_FALSE(facts.at(1).applied.IsKnownTrue());
        CHECK(facts.at(2).applied.IsKnownTrue());
        CHECK(facts.at(4).applied.IsKnownFalse());
        CHECK(facts.at(3).subcategory == slsf::gameplay::TattooSubcategory::Unknown);
        CHECK(slsf::integrations::SlaveTatsSlotFacts(snapshot.Area(SlaveTatsArea::Body), 0)
                  .empty());
    }

    SECTION("stale collection") {
        const auto stale = tats.Collect(0x14u, Epoch{9, 9, 9});
        CHECK(stale.gate == ExternalGate::Stale);
        CHECK(tats.Snapshot() == snapshot);
    }

    SECTION("session change resets snapshot") {
        tats.OnSessionChanged(Epoch{5, 5, 5});
        CHECK(tats.Snapshot().epoch == Epoch{5, 5, 5});
        CHECK(tats.Snapshot().availability == ExternalAvailability::Unknown);
        CHECK(tats.Dirty());
        CHECK(tats.Gate() == ExternalGate::Changed);
        fixture.Broadcast();
        std::ignore = tats.Collect(0x14u, Epoch{5, 5, 5});
        REQUIRE(tats.Gate() == ExternalGate::Ready);
        CHECK(tats.Snapshot().epoch == Epoch{5, 5, 5});
    }

    SECTION("cancel and reset") {
        tats.Cancel();
        const auto closed = tats.Collect(0x14u, SlaveTatsFixture::kEpoch);
        CHECK(closed.gate == ExternalGate::Closed);
        CHECK(tats.Gate() == ExternalGate::Closed);
        tats.Reset();
        CHECK(tats.Gate() == ExternalGate::Waiting);
        CHECK(tats.Dirty());
    }
}

TEST_CASE("slavetats failure paths retry toward failed", "[slavetats][lifecycle]") {
    SlaveTatsFixture fixture;
    SlaveTats& tats = fixture.tats;
    fixture.Broadcast();
    fixture.target.fail = true;
    for (std::uint32_t attempt = 1; attempt <= 19; ++attempt) {
        const auto result = tats.Collect(0x14u, SlaveTatsFixture::kEpoch);
        CHECK(result.gate == ExternalGate::Waiting);
        CHECK(tats.Attempts() == attempt);
        CHECK(tats.LastError() == "batch tattoo query reported failure");
    }
    CHECK(tats.Collect(0x14u, SlaveTatsFixture::kEpoch).gate == ExternalGate::Failed);
    CHECK(tats.Gate() == ExternalGate::Failed);

    SECTION("actor unresolved") {
        fixture.target.fail = false;
        fixture.tats.Reset();
        fixture.Broadcast();
        for (std::uint32_t attempt = 1; attempt <= 20; ++attempt) {
            CHECK(tats.Collect(0x99u, SlaveTatsFixture::kEpoch).gate ==
                  (attempt < 20 ? ExternalGate::Waiting : ExternalGate::Failed));
        }
    }

    SECTION("missing batch pointer") {
        fixture.target.fail = false;
        fixture.tats.Reset();
        fixture.Broadcast();
        fixture.broadcast.pointers[14] = nullptr;
        CHECK(tats.Collect(0x14u, SlaveTatsFixture::kEpoch).gate == ExternalGate::Waiting);
    }

    SECTION("missing jc bindings") {
        fixture.target.fail = false;
        fixture.tats.Reset();
        fixture.Broadcast();
        fixture.port.table.arrayCount = nullptr;
        CHECK(tats.Collect(0x14u, SlaveTatsFixture::kEpoch).gate == ExternalGate::Waiting);
    }

    SECTION("negative array count is a batch failure") {
        fixture.target.fail = false;
        fixture.target.negativeCount = true;
        fixture.tats.Reset();
        fixture.Broadcast();
        const auto result = fixture.tats.Collect(0x14u, SlaveTatsFixture::kEpoch);
        CHECK(result.gate == ExternalGate::Waiting);
        CHECK_FALSE(result.changed);
        CHECK(tats.LastError() == "batch result array count is negative");
        CHECK(tats.Attempts() == 1u);
    }
}

TEST_CASE("slavetats rejects malformed records and truncates", "[slavetats][bounds]") {
    SlaveTatsFixture fixture;
    SlaveTats& tats = fixture.tats;
    fixture.Broadcast();
    fixture.target.entries.at(static_cast<std::size_t>(SlaveTatsArea::Body)).assign(
        {FakeField{std::string(kMaxSlaveTatsNameBytes, 'a'), 0}, FakeField{"bad", -1},
         FakeField{"kept", 3}});
    fixture.target.entries.at(static_cast<std::size_t>(SlaveTatsArea::Feet)).assign(
        {FakeField{std::string(kMaxSlaveTatsNameBytes + 1, 'z'), 1}});
    CHECK(fixture.CollectReady().changed);
    const auto snapshot = tats.Snapshot();
    const auto& body = snapshot.Area(SlaveTatsArea::Body);
    REQUIRE(body.placements.size() == 2);
    CHECK(body.placements.at(0).name == std::string(kMaxSlaveTatsNameBytes, 'a'));
    CHECK(body.rejectedEntries == 1u);
    const auto& feet = snapshot.Area(SlaveTatsArea::Feet);
    CHECK(feet.placements.empty());
    CHECK(feet.rejectedEntries == 1u);
}

TEST_CASE("slavetats truncates oversized areas at the record limit", "[slavetats][bounds]") {
    SlaveTatsFixture fixture;
    SlaveTats& tats = fixture.tats;
    fixture.Broadcast();
    auto& entries = fixture.target.entries.at(static_cast<std::size_t>(SlaveTatsArea::Body));
    entries.resize(kMaxSlaveTatsPlacements + 40);
    for (std::size_t index = 0; index < entries.size(); ++index) {
        entries.at(index).slot = static_cast<std::int32_t>(index);
        entries.at(index).name = "N" + std::to_string(index);
    }
    REQUIRE(fixture.CollectReady().changed);
    const SlaveTatsSnapshot snap = tats.Snapshot();
    const auto& body = snap.Area(SlaveTatsArea::Body);
    CHECK(body.placements.size() == kMaxSlaveTatsPlacements);
    CHECK(body.truncated);
    CHECK(body.rejectedEntries == 40u);
    CHECK(body.HasRecordAtSlot(0));
    CHECK(body.HasRecordAtSlot(255));
    CHECK_FALSE(body.HasRecordAtSlot(256));
}

TEST_CASE("slavetats random placements keep ordering and bounds", "[slavetats][differential]") {
    for (int round = 0; round < 40; ++round) {
        SlaveTatsFixture fixture;
        std::mt19937_64 rng{1170u + static_cast<std::uint64_t>(round)};
        fixture.Broadcast();
        const int perArea = static_cast<int>(std::uniform_int_distribution<int>{0, 300}(rng));
        auto& entries = fixture.target.entries.at(static_cast<std::size_t>(SlaveTatsArea::Body));
        entries.clear();
        for (int index = 0; index < perArea; ++index) {
            const int slotRoll = static_cast<int>(std::uniform_int_distribution<int>{-3, 12}(rng));
            entries.push_back(FakeField{"T" + std::to_string(index), slotRoll});
        }
        REQUIRE(fixture.tats.Collect(0x14u, SlaveTatsFixture::kEpoch).gate == ExternalGate::Ready);
        const SlaveTatsSnapshot snap = fixture.tats.Snapshot();
        const auto& body = snap.Area(SlaveTatsArea::Body);
        CHECK(body.placements.size() <= kMaxSlaveTatsPlacements);
        CHECK(std::is_sorted(body.placements.begin(), body.placements.end(),
                             [](const SlaveTatsPlacement& left, const SlaveTatsPlacement& right) {
                                 return left.slot < right.slot;
                             }));
        std::uint32_t expectedRejected = 0;
        for (const auto& field : entries) {
            if (field.slot < 0) {
                expectedRejected += 1;
            }
        }
        if (perArea > static_cast<int>(kMaxSlaveTatsPlacements)) {
            CHECK(body.truncated);
            const std::uint32_t validCount =
                static_cast<std::uint32_t>(perArea) - expectedRejected;
            const std::uint32_t expectedKept =
                std::min<std::uint32_t>(validCount, kMaxSlaveTatsPlacements);
            CHECK(body.rejectedEntries ==
                  static_cast<std::uint32_t>(perArea) - expectedKept);
        } else {
            CHECK(body.rejectedEntries == expectedRejected);
        }
    }
}

TEST_CASE("slavetats random messages stay consistent with rejections", "[slavetats][differential]") {
    SlaveTatsFixture fixture;
    std::mt19937_64 rng{1170u};
    const std::array<std::uint32_t, 4> versions{kSlaveTatsInterfaceVersion, 0u, 2u, 9u};
    for (int round = 0; round < 400; ++round) {
        const auto version = versions.at(std::uniform_int_distribution<int>{0, 3}(rng));
        const auto messageType = kSlaveTatsInterfaceMessageType *
                                 std::uniform_int_distribution<int>{0, 1}(rng);
        const auto length = std::uniform_int_distribution<std::size_t>{0, 9}(rng);
        fixture.broadcast.current_version = version;
        auto* data = length == 0 ? nullptr : &fixture.broadcast;
        bool delivered = false;
        if (length == sizeof(void*)) {
            const std::int64_t aligned = static_cast<std::int64_t>(length);
            delivered = fixture.tats.ConsumeInterfaceMessage(messageType, data, aligned);
        } else {
            delivered = false;
        }
        const auto capability = fixture.tats.Capability();
        const bool shaped = messageType == kSlaveTatsInterfaceMessageType &&
                            length == sizeof(void*);
        if (shaped && version == kSlaveTatsInterfaceVersion) {
            CHECK(delivered);
            CHECK(capability.state == CapabilityState::Available);
            CHECK(capability.version == version);
        } else if (shaped) {
            CHECK(delivered);
            CHECK(capability.state == CapabilityState::Incompatible);
            CHECK(capability.version == version);
        } else {
            CHECK_FALSE(delivered);
        }
        CHECK(fixture.tats.Gate() != ExternalGate::Ready);
    }
}

TEST_CASE("slavetats concurrent broadcasts and collects are stable", "[slavetats][threaded]") {
    SlaveTatsFixture fixture;
    SlaveTats& tats = fixture.tats;
    static FakeAddressesTable secondary{};
    std::atomic<bool> stop{false};
    std::thread consumer{[&]() {
        while (!stop.load(std::memory_order_relaxed)) {
            std::ignore = tats.ConsumeInterfaceMessage(kSlaveTatsInterfaceMessageType,
                                                       &secondary, sizeof(void*));
        }
    }};
    for (int round = 0; round < 200; ++round) {
        const auto result = tats.Collect(0x14u, SlaveTatsFixture::kEpoch);
        CHECK(result.gate != ExternalGate::Stale);
        CHECK(result.gate != ExternalGate::Closed);
        REQUIRE(tats.Snapshot().epoch == SlaveTatsFixture::kEpoch);
        CHECK(tats.Capability().state != CapabilityState::Incompatible);
        if (result.gate == ExternalGate::Ready) {
            CHECK(tats.Snapshot().availability == ExternalAvailability::Available);
        }
    }
    stop.store(true, std::memory_order_relaxed);
    consumer.join();
    fixture.CollectReady();
    CHECK(tats.Gate() == ExternalGate::Ready);
    CHECK(tats.Capability().state == CapabilityState::Available);
    CHECK(tats.Snapshot().availability == ExternalAvailability::Available);
}
