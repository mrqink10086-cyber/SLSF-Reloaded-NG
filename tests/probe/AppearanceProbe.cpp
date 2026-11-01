// Offline appearance probe.  Compile-time section pins mirrored ABI facts;
// run-time section drives both adapters against in-memory vector tables.
// Live 1.6.1170 observations (providers absent on this host) print as
// pending markers.

#include "slsf/core/gameplay/Visibility.h"
#include "slsf/integrations/RaceMenu.h"
#include "slsf/integrations/SlaveTats.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

using slsf::Epoch;
using slsf::integrations::ExternalAvailability;
using slsf::integrations::ExternalGate;
using slsf::integrations::JcHandle;
using slsf::integrations::JcString;
using slsf::integrations::JcTable;
using slsf::integrations::kMaxSlaveTatsNameBytes;
using slsf::integrations::kMaxSlaveTatsPlacements;
using slsf::integrations::kRaceMenuOverlayLocationCount;
using slsf::integrations::kSlaveTatsAreaCount;
using slsf::integrations::kSlaveTatsInterfaceMessageType;
using slsf::integrations::kSlaveTatsInterfaceVersion;
using slsf::integrations::kSlaveTatsKnownPrefixPointers;
using slsf::integrations::kSkeeInterfaceExchangeMessage;
using slsf::integrations::kSkeeOverlayInterfaceVersion;
using slsf::integrations::RaceMenu;
using slsf::integrations::RaceMenuExchangePort;
using slsf::integrations::RaceMenuOverlayLocation;
using slsf::integrations::SlaveTats;
using slsf::integrations::SlaveTatsArea;
using slsf::integrations::SlaveTatsPlacement;
using slsf::integrations::SlaveTatsPort;

static_assert(kSlaveTatsInterfaceMessageType == 1);
static_assert(kSlaveTatsInterfaceVersion == 1);
static_assert(kSlaveTatsKnownPrefixPointers == 15);
static_assert(kSlaveTatsAreaCount == 4);
static_assert(kRaceMenuOverlayLocationCount == 4);
static_assert(kSkeeInterfaceExchangeMessage == 0x9E3779B9u);
static_assert(kSkeeOverlayInterfaceVersion == 2);
static_assert(kMaxSlaveTatsPlacements == 256);
static_assert(kMaxSlaveTatsNameBytes == 128);

static_assert(static_cast<int>(SlaveTatsArea::Body) ==
              static_cast<int>(slsf::gameplay::TattooArea::Body));
static_assert(static_cast<int>(SlaveTatsArea::Face) ==
              static_cast<int>(slsf::gameplay::TattooArea::Face));
static_assert(static_cast<int>(SlaveTatsArea::Hands) ==
              static_cast<int>(slsf::gameplay::TattooArea::Hands));
static_assert(static_cast<int>(SlaveTatsArea::Feet) ==
              static_cast<int>(slsf::gameplay::TattooArea::Feet));

// skee batch order (body, face, hands, feet) follows SlaveTatsNG
// get_applied_tattoos_by_area; the legacy SLSF order Body/Hands/Feet/Face is
// deliberately not mirrored here.
static_assert(static_cast<int>(RaceMenuOverlayLocation::Body) == 0);static_assert(static_cast<int>(RaceMenuOverlayLocation::Hand) == 1);
static_assert(static_cast<int>(RaceMenuOverlayLocation::Feet) == 2);
static_assert(static_cast<int>(RaceMenuOverlayLocation::Face) == 3);

static_assert(sizeof(JcString) == sizeof(void*));
static_assert(offsetof(JcString, opaque) == 0);

static_assert(
    std::is_same_v<decltype(JcTable::arrayCount), std::int32_t (*)(void*, std::int32_t)>);
static_assert(std::is_same_v<decltype(JcTable::arrayObject), std::int32_t (*)(void*)>);
static_assert(std::is_same_v<decltype(JcTable::mapGetStr),
                             JcString (*)(void*, std::int32_t, JcString, JcString)>);
static_assert(std::is_same_v<decltype(JcTable::release), std::int32_t (*)(void*, std::int32_t)>);
static_assert(
    std::is_same_v<decltype(JcTable::retain), std::int32_t (*)(void*, std::int32_t, JcString)>);

static_assert(!std::is_copy_constructible_v<JcHandle>);
static_assert(!std::is_copy_assignable_v<JcHandle>);
static_assert(std::is_move_constructible_v<JcHandle> && std::is_move_assignable_v<JcHandle>);
static_assert(std::is_nothrow_move_constructible_v<JcHandle>);

constexpr Epoch kEpoch{7, 0x1234, 0x5678};

int Fail(const char* what) {
    std::printf("probe_result=FAIL reason=%s\n", what);
    return 1;
}

// Mirrors RaceMenu.cpp slot reads: map slot 0 QueryInterface; overlay
// slots 1=GetVersion, 11=GetOverlayCount.

using QueryInterfaceFn = void* (*)(void* map, const char* name);
using GetVersionFn = std::uint32_t (*)(void* overlay);
using GetOverlayCountFn = std::uint32_t (*)(void* overlay, std::uint32_t type,
                                            std::uint32_t location);

constexpr std::size_t kOverlayVersionSlot = 1;
constexpr std::size_t kOverlayCountSlot = 11;

struct FakeOverlayInterface {
    void** vtable{};
    std::array<std::uint32_t, kRaceMenuOverlayLocationCount> counts{};
};

struct FakeInterfaceMap {
    void** vtable{};
    FakeOverlayInterface overlay{};
};

void* QueryInterfaceThunk(void* self, const char*) noexcept {
    return &static_cast<FakeInterfaceMap*>(self)->overlay;
}

std::uint32_t GetVersionThunk(void*) noexcept { return kSkeeOverlayInterfaceVersion; }

std::uint32_t GetOverlayCountThunk(void* self, std::uint32_t type, std::uint32_t location) noexcept {
    if (type != 0 || location >= kRaceMenuOverlayLocationCount) {
        return 0;
    }
    return static_cast<FakeOverlayInterface*>(self)->counts.at(location);
}

int RunRaceMenuSection() {
    FakeOverlayInterface overlayState{{}, {3u, 1u, 0u, 2u}};
    std::array<void*, 12> overlayVtable{};
    overlayVtable.at(kOverlayVersionSlot) = reinterpret_cast<void*>(&GetVersionThunk);
    overlayVtable.at(kOverlayCountSlot) = reinterpret_cast<void*>(&GetOverlayCountThunk);
    overlayState.vtable = overlayVtable.data();

    FakeInterfaceMap mapState{nullptr, {}};
    std::array<void*, 2> mapVtable{};
    mapVtable.at(0) = reinterpret_cast<void*>(&QueryInterfaceThunk);
    mapState.vtable = mapVtable.data();
    mapState.overlay = overlayState;

    class Port final : public RaceMenuExchangePort {
    public:
        void* Exchange() noexcept override { return map_; }
        void* map_{};
    } port;
    port.map_ = &mapState;

    RaceMenu menu{port};
    const auto ensure = menu.Ensure(kEpoch);
    if (ensure.gate != ExternalGate::Ready || !ensure.changed) {
        return Fail("racemenu_exchange_ready");
    }
    const auto snapshot = menu.Snapshot();
    if (snapshot.availability != ExternalAvailability::Available ||
        snapshot.overlayInterfaceVersion != kSkeeOverlayInterfaceVersion ||
        snapshot.epoch != kEpoch ||
        snapshot.overlays.counts != std::array<std::uint32_t, 4>{3u, 1u, 0u, 2u}) {
        return Fail("racemenu_snapshot");
    }
    std::vector<slsf::integrations::RaceMenuCapabilityRecord> records;
    menu.Capabilities(records);
    if (records.size() != 3 || records.at(0).name.empty() ||
        records.at(0).state != slsf::integrations::CapabilityState::Available ||
        records.at(2).state != slsf::integrations::CapabilityState::Available) {
        return Fail("racemenu_capability_records");
    }
    std::printf(
        "probe_racemenu_exchange=ready:overlay_version=%u:counts=%u,%u,%u,%u:records=%zu\n",
        snapshot.overlayInterfaceVersion, snapshot.overlays.counts.at(0),
        snapshot.overlays.counts.at(1), snapshot.overlays.counts.at(2),
        snapshot.overlays.counts.at(3), records.size());
    const auto again = menu.Ensure(kEpoch);
    if (again.changed) {
        return Fail("racemenu_exchange_idempotent");
    }
    return 0;
}

namespace jcfake {

struct JcStringLess {
    bool operator()(const JcString& left, const JcString& right) const noexcept {
        return left.opaque < right.opaque;
    }
};

struct Entry {
    std::map<std::string, std::int64_t> ints;
    std::map<std::string, JcString> strs;
};

struct Domain {
    std::map<std::int32_t, std::vector<std::int32_t>> arrays;
    std::map<std::int32_t, Entry> entries;
    std::map<JcString, std::string, JcStringLess> strings;
    std::int32_t nextId = 100;
    std::uint32_t creates = 0;
    std::uint32_t releases = 0;
    std::uint32_t retains = 0;
};

Domain* g_domain = nullptr;

JcString Intern(Domain& domain, const char* text) {
    const auto hashed = static_cast<std::uint64_t>(
        std::hash<std::string_view>{}(text) & 0x7FFFFFFFFFFFFFFFull);
    JcString out{hashed == 0 ? 0x1 : hashed};
    domain.strings.insert_or_assign(out, std::string{text});
    return out;
}

std::string Lookup(Domain& domain, JcString value) {
    const auto it = domain.strings.find(value);
    return it == domain.strings.end() ? std::string{} : it->second;
}

std::int32_t ArrayCount(void* d, std::int32_t h) noexcept {
    auto* domain = static_cast<Domain*>(d);
    const auto it = domain->arrays.find(h);
    return it == domain->arrays.end()
               ? -1
               : static_cast<std::int32_t>(it->second.size());
}

std::int32_t ArrayObject(void* d) noexcept {
    auto* domain = static_cast<Domain*>(d);
    const auto id = domain->nextId++;
    domain->arrays.emplace(id, std::vector<std::int32_t>{});
    ++domain->creates;
    return id;
}

std::int32_t ArrayGetObj(void* d, std::int32_t h, std::int32_t index, std::int32_t fallback) noexcept {
    auto* domain = static_cast<Domain*>(d);
    const auto it = domain->arrays.find(h);
    if (it == domain->arrays.end() || index < 0 ||
        static_cast<std::size_t>(index) >= it->second.size()) {
        return fallback;
    }
    return it->second.at(static_cast<std::size_t>(index));
}

void ArrayAddObj(void* d, std::int32_t h, std::int32_t value, std::int32_t) noexcept {
    auto* domain = static_cast<Domain*>(d);
    const auto it = domain->arrays.find(h);
    if (it != domain->arrays.end()) {
        it->second.push_back(value);
    }
}

JcString MapGetStr(void* d, std::int32_t object, JcString key, JcString fallback) noexcept {
    auto* domain = static_cast<Domain*>(d);
    const auto it = domain->entries.find(object);
    if (it == domain->entries.end()) {
        return fallback;
    }
    const auto entry = it->second.strs.find(Lookup(*domain, key));
    return entry == it->second.strs.end() ? fallback : entry->second;
}

std::int32_t MapGetInt(void* d, std::int32_t object, JcString key, std::int32_t fallback) noexcept {
    auto* domain = static_cast<Domain*>(d);
    const auto it = domain->entries.find(object);
    if (it == domain->entries.end()) {
        return fallback;
    }
    const auto entry = it->second.ints.find(Lookup(*domain, key));
    return entry == it->second.ints.end() ? fallback : static_cast<std::int32_t>(entry->second);
}

std::int32_t Release(void* d, std::int32_t) noexcept {
    ++static_cast<Domain*>(d)->releases;
    return 1;
}

std::int32_t Retain(void* d, std::int32_t, JcString) noexcept {
    ++static_cast<Domain*>(d)->retains;
    return 1;
}

using BatchByAreaFn = bool (*)(void* target, std::int32_t body, std::int32_t face,
                               std::int32_t hands, std::int32_t feet);

struct FakeTarget {
    std::array<std::vector<SlaveTatsPlacement>, kSlaveTatsAreaCount> entries;
    bool fail{};
};

FakeTarget* g_target = nullptr;

bool BatchByAreaThunk(void* self, std::int32_t body, std::int32_t face, std::int32_t hands,
                      std::int32_t feet) noexcept {
    auto* target = static_cast<FakeTarget*>(self);
    if (target->fail) {
        return true;
    }
    const std::array<std::int32_t, kSlaveTatsAreaCount> areaArrays{body, face, hands, feet};
    for (std::size_t area = 0; area < kSlaveTatsAreaCount; ++area) {
        for (const auto& placement : target->entries.at(area)) {
            const auto id = g_domain->nextId++;
            auto& entry = g_domain->entries[id];
            entry.ints.insert_or_assign("slot", static_cast<std::int64_t>(placement.slot));
            entry.strs.insert_or_assign("name", Intern(*g_domain, placement.name.c_str()));
            ArrayAddObj(g_domain, areaArrays.at(area), id, -1);
        }
    }
    return false;
}

} // namespace jcfake

class FakeSlaveTatsPort final : public SlaveTatsPort {
public:
    FakeSlaveTatsPort() {
        table.domain = jcfake::g_domain;
        table.arrayCount = &jcfake::ArrayCount;
        table.arrayObject = &jcfake::ArrayObject;
        table.arrayGetObj = &jcfake::ArrayGetObj;
        table.arrayAddObj = &jcfake::ArrayAddObj;
        table.mapGetStr = &jcfake::MapGetStr;
        table.mapGetInt = &jcfake::MapGetInt;
        table.release = &jcfake::Release;
        table.retain = &jcfake::Retain;
    }

    const JcTable* Table() noexcept override { return &table; }
    JcString Intern(const char* text) noexcept override {
        return jcfake::Intern(*jcfake::g_domain, text);
    }
    std::string Text(JcString value) noexcept override {
        return jcfake::Lookup(*jcfake::g_domain, value);
    }
    void* ResolveActor(std::uint32_t formId) noexcept override {
        return formId == 0x14 ? jcfake::g_target : nullptr;
    }

    JcTable table{};
};

int RunSlaveTatsSection() {
    static jcfake::Domain domain;
    domain = jcfake::Domain{};
    jcfake::g_domain = &domain;
    static jcfake::FakeTarget target;
    target = jcfake::FakeTarget{};
    jcfake::g_target = &target;
    target.entries.at(static_cast<std::size_t>(SlaveTatsArea::Body)) = {
        SlaveTatsPlacement{"Alpha", 0}, SlaveTatsPlacement{"Beta", 2},
        SlaveTatsPlacement{"Gamma", 2}};
    target.entries.at(static_cast<std::size_t>(SlaveTatsArea::Hands)) = {
        SlaveTatsPlacement{"Mono", 0}};

    struct AddressesTable {
        std::uint32_t current_version;
        void* pointers[kSlaveTatsKnownPrefixPointers];
    } addresses{kSlaveTatsInterfaceVersion, {}};
    addresses.pointers[14] = reinterpret_cast<void*>(&jcfake::BatchByAreaThunk);

    FakeSlaveTatsPort port;
    SlaveTats tats{port};
    if (!tats.ConsumeInterfaceMessage(kSlaveTatsInterfaceMessageType, &addresses, sizeof(void*))) {
        return Fail("slavetats_broadcast_accepted");
    }
    const auto capability = tats.Capability();
    if (capability.state != slsf::integrations::CapabilityState::Available ||
        capability.version != kSlaveTatsInterfaceVersion) {
        return Fail("slavetats_capability_available");
    }
    tats.OnSessionChanged(kEpoch);
    const auto collect = tats.Collect(0x14, kEpoch);
    if (collect.gate != ExternalGate::Ready) {
        return Fail("slavetats_collect_ready");
    }
    const auto snapshot = tats.Snapshot();
    if (snapshot.availability != ExternalAvailability::Available ||
        snapshot.interfaceVersion != kSlaveTatsInterfaceVersion ||
        snapshot.epoch != kEpoch) {
        return Fail("slavetats_snapshot_head");
    }
    const auto& body = snapshot.Area(SlaveTatsArea::Body);
    const auto& hands = snapshot.Area(SlaveTatsArea::Hands);
    if (body.placements.size() != 3 || body.placements.at(0).name != "Alpha" ||
        body.placements.at(0).slot != 0 || body.placements.at(1).slot != 2 ||
        body.placements.at(2).slot != 2 || body.placements.at(1).name != "Beta" ||
        body.placements.at(2).name != "Gamma" || !body.HasRecordAtSlot(0) ||
        !body.HasRecordAtSlot(2)) {
        return Fail("slavetats_body_placements");
    }
    if (hands.placements.size() != 1 || hands.placements.at(0).name != "Mono" ||
        !snapshot.Area(SlaveTatsArea::Face).placements.empty() ||
        !snapshot.Area(SlaveTatsArea::Feet).placements.empty()) {
        return Fail("slavetats_other_placements");
    }
    const auto slotFacts = slsf::integrations::SlaveTatsSlotFacts(body, 3);
    if (slotFacts.size() != 3 || !slotFacts.at(0).applied.IsKnownTrue() ||
        !slotFacts.at(1).applied.IsKnownFalse() || !slotFacts.at(2).applied.IsKnownTrue() ||
        slotFacts.at(0).subcategory != slsf::gameplay::TattooSubcategory::Unknown) {
        return Fail("slavetats_slot_facts");
    }
    std::printf(
        "probe_slavetats_batch=body:3,hands:1,face:0,feet:0;slotfacts=slot0=true,slot1=false,slot2=true\n");
    if (domain.releases != domain.creates) {
        std::printf(
            "probe_slavetats_jc_ownership=FAIL creates=%u releases=%u retains=%u\n",
            domain.creates, domain.releases, domain.retains);
        return Fail("slavetats_jc_ownership");
    }
    std::printf(
        "probe_slavetats_jc_ownership=balanced:creates=%u:releases=%u:retains=%u\n",
        domain.creates, domain.releases, domain.retains);

    const auto changed = tats.Collect(0x14, kEpoch);
    if (changed.gate != ExternalGate::Ready || changed.changed) {
        return Fail("slavetats_collect_idempotent");
    }
    return 0;
}

} // namespace

int main() {
    std::printf(
        "probe_target_runtime=1.6.1170:providers-pending(racemenu,skee,slavetatsng,jcontainers)\n");
    if (const int race = RunRaceMenuSection(); race != 0) {
        return race;
    }
    if (const int tats = RunSlaveTatsSection(); tats != 0) {
        return tats;
    }

    // On-target minimum-query checklist, printed for the live 1.6.1170 run.
    std::printf(
        "probe_racemenu_pending=dispatch:0x9E3779B9,data=InterfaceExchangeMessage{void*interfaceMap:8B},sender='SKEE',phase=kPostPostLoad\n");
    std::printf(
        "probe_racemenu_pending=query:'Overlay'->GetVersion()==2,GetOverlayCount(type=Normal=0,location=0..3)->u32,vtable slots 1/11\n");
    std::printf(
        "probe_slavetats_pending=listener:messageType==1,data=AddressesTable*{8B},version==1,confirmed prefix=15 pointers\n");
    std::printf(
        "probe_slavetats_pending=query:get_applied_tattoos_by_area at slot 14(offset 120),batch(target,body,face,hands,feet)\n");
    std::printf(
        "probe_jc_pending=bindings:JFormDB::getObj(target,'.SlaveTats.applied')->array handle;per-entry mapGetStr('name')/mapGetInt('slot');temp arrays via RAII retain/release\n");
    std::printf("probe_result=OFFLINE_PASS\n");
    return 0;
}
