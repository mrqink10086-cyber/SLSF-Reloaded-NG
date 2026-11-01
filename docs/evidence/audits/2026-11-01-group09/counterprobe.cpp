#define main OriginalAppearanceProbeMain
#include "../../../../tests/probe/AppearanceProbe.cpp"
#undef main

namespace {

struct RaceFixture {
    std::array<void*, 12> overlayTable{};
    std::array<void*, 2> mapTable{};
    FakeInterfaceMap map{};
    struct Port final : RaceMenuExchangePort {
        void* map{};
        int calls{};
        void* Exchange() noexcept override { ++calls; return map; }
    } port;
    RaceFixture() {
        overlayTable[1] = reinterpret_cast<void*>(&GetVersionThunk);
        overlayTable[11] = reinterpret_cast<void*>(&GetOverlayCountThunk);
        mapTable[0] = reinterpret_cast<void*>(&QueryInterfaceThunk);
        map.vtable = mapTable.data();
        map.overlay.vtable = overlayTable.data();
        map.overlay.counts = {3, 1, 0, 2};
        port.map = &map;
    }
};

struct TatsFixture {
    jcfake::Domain domain;
    jcfake::FakeTarget target;
    struct Addresses {
        std::uint32_t version{kSlaveTatsInterfaceVersion};
        void* pointers[kSlaveTatsKnownPrefixPointers]{};
    } addresses;
    FakeSlaveTatsPort port;
    TatsFixture() {
        jcfake::g_domain = &domain;
        jcfake::g_target = &target;
        port.table.domain = &domain;
        addresses.pointers[14] = reinterpret_cast<void*>(&jcfake::BatchByAreaThunk);
    }
    void Confirm(SlaveTats& tats) {
        if (!tats.ConsumeInterfaceMessage(1, &addresses, sizeof(void*))) std::abort();
        tats.OnSessionChanged(kEpoch);
    }
};

std::uint64_t objectReads{};
std::int32_t LargeCount(void*, std::int32_t) noexcept { return 100000; }
std::int32_t MissingObject(void*, std::int32_t, std::int32_t, std::int32_t) noexcept {
    ++objectReads;
    return 0;
}

}

int main() {
    const Epoch other{8, 0x1234, 0x5678};
    {
        RaceFixture f;
        RaceMenu menu{f.port};
        menu.OnSessionChanged(kEpoch);
        const auto first = menu.Ensure(kEpoch);
        const auto stale = menu.Ensure(other);
        const auto valid = menu.Ensure(kEpoch);
        std::printf("race_stale first_ready=%d stale=%d valid_ready=%d gate_stale=%d exchanges=%d\n",
            first.gate == ExternalGate::Ready, stale.gate == ExternalGate::Stale,
            valid.gate == ExternalGate::Ready, menu.Gate() == ExternalGate::Stale, f.port.calls);
        menu.Reset();
        const auto reset = menu.Ensure(other);
        std::printf("race_reset_new_epoch_stale=%d\n", reset.gate == ExternalGate::Stale);
    }
    {
        RaceFixture f;
        f.port.map = nullptr;
        slsf::integrations::RaceMenuOptions options;
        options.timing.maxAttempts = 2;
        RaceMenu menu{f.port, options};
        for (int i = 0; i < 5; ++i) std::ignore = menu.Ensure(kEpoch);
        std::printf("race_retry max=2 calls=%d attempts=%u failed=%d\n", f.port.calls,
            menu.Attempts(), menu.Gate() == ExternalGate::Failed);
    }
    {
        TatsFixture f;
        f.target.entries[0].assign(256, SlaveTatsPlacement{"repeat", 0});
        f.target.entries[0].push_back(SlaveTatsPlacement{"late_slot", 1});
        SlaveTats tats{f.port};
        f.Confirm(tats);
        const auto result = tats.Collect(0x14, kEpoch);
        const auto body = tats.Snapshot().Area(SlaveTatsArea::Body);
        const auto facts = slsf::integrations::SlaveTatsSlotFacts(body, 2);
        std::printf("tats_trunc ready=%d placements=%zu rejected=%u truncated=%d actual_slot1=1 slot1_known_false=%d\n",
            result.gate == ExternalGate::Ready, body.placements.size(), body.rejectedEntries,
            body.truncated, facts[1].applied.IsKnownFalse());
        tats.Reset();
        const auto reset = tats.Collect(0x14, other);
        std::printf("tats_reset_new_epoch_stale=%d\n", reset.gate == ExternalGate::Stale);
    }
    {
        TatsFixture f;
        f.target.entries[0] = {{std::string(129, 'X'), 1}};
        SlaveTats tats{f.port};
        f.Confirm(tats);
        const auto result = tats.Collect(0x14, kEpoch);
        const auto body = tats.Snapshot().Area(SlaveTatsArea::Body);
        const auto facts = slsf::integrations::SlaveTatsSlotFacts(body, 2);
        std::printf("tats_reject ready=%d rejected=%u actual_slot1=1 slot1_known_false=%d\n",
            result.gate == ExternalGate::Ready, body.rejectedEntries, facts[1].applied.IsKnownFalse());
    }
    {
        TatsFixture f;
        f.port.table.arrayCount = &LargeCount;
        f.port.table.arrayGetObj = &MissingObject;
        SlaveTats tats{f.port};
        f.Confirm(tats);
        const auto result = tats.Collect(0x14, kEpoch);
        std::printf("tats_scan per_area_count=100000 object_reads=%llu cap=256 ready=%d\n",
            static_cast<unsigned long long>(objectReads), result.gate == ExternalGate::Ready);
    }
    {
        TatsFixture f;
        slsf::integrations::SlaveTatsOptions options;
        options.timing.maxAttempts = 2;
        SlaveTats tats{f.port, options};
        f.Confirm(tats);
        for (int i = 0; i < 5; ++i) std::ignore = tats.Collect(0, kEpoch);
        std::printf("tats_retry max=2 attempts=%u failed=%d\n", tats.Attempts(),
            tats.Gate() == ExternalGate::Failed);
    }
    return 0;
}
