// Target-version probe for the group 04 engine contract.
//
// The compile-time section checks the ABI facts this project relies on against
// the vendored CommonLibSSE-NG headers.  The run-time section exercises the
// pure DTO translation and prints the exact query contract that the engine
// backend must satisfy.  Live game queries (relationship, detection, LOS) are
// listed as pending because this host has no running game backend; Skyrim
// 1.6.1170 is installed but is not launched by this standalone probe.

#include "slsf/core/gameplay/Visibility.h"
#include "slsf/engine/ActorQueries.h"

#include <RE/A/Actor.h>
#include <RE/B/BGSRelationship.h>
#include <RE/U/UI.h>
#include <SKSE/Interfaces.h>
#include <SKSE/Version.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <type_traits>
#include <utility>

namespace {

using Level = RE::BGSRelationship::RELATIONSHIP_LEVEL;

static_assert(static_cast<int>(Level::kLover) == 0);
static_assert(static_cast<int>(Level::kAlly) == 1);
static_assert(static_cast<int>(Level::kConfidant) == 2);
static_assert(static_cast<int>(Level::kFriend) == 3);
static_assert(static_cast<int>(Level::kAcquaintance) == 4);
static_assert(static_cast<int>(Level::kRival) == 5);
static_assert(static_cast<int>(Level::kFoe) == 6);
static_assert(static_cast<int>(Level::kEnemy) == 7);
static_assert(static_cast<int>(Level::kArchnemesis) == 8);
static_assert(sizeof(RE::BGSRelationship) == 0x40);

using HasLineOfSightFn = bool (RE::Actor::*)(RE::TESObjectREFR*, bool&);
using RequestDetectionLevelFn =
    std::int32_t (RE::Actor::*)(RE::Actor*, RE::DETECTION_PRIORITY);
using RequestLosFn = std::int32_t (RE::Actor::*)(RE::Actor*, float);

// These assertions verify the CommonLibSSE-NG signatures the future adapter
// must call.  They do not claim that the engine behaviour was exercised.
static_assert(std::is_same_v<
              decltype(static_cast<HasLineOfSightFn>(
                  &RE::Actor::HasLineOfSight)),
              HasLineOfSightFn>);
static_assert(std::is_same_v<
              decltype(static_cast<RequestDetectionLevelFn>(
                  &RE::Actor::RequestDetectionLevel)),
              RequestDetectionLevelFn>);
static_assert(std::is_same_v<
              decltype(static_cast<RequestLosFn>(&RE::Actor::RequestLOS)),
              RequestLosFn>);
static_assert(std::is_same_v<
              decltype(&RE::BGSRelationship::GetRelationship),
              RE::BGSRelationship* (*)(RE::TESNPC*, RE::TESNPC*)>);

static_assert(SKSE::RUNTIME_SSE_LATEST == SKSE::RUNTIME_SSE_1_6_1170);

// The Skyrim equipment slot indices used by SLSF_Reloaded_VisibilityManager.psc.
static_assert(static_cast<std::size_t>(slsf::gameplay::ArmorSlot::Body) == 32);
static_assert(static_cast<std::size_t>(slsf::gameplay::ArmorSlot::Collar) == 45);
static_assert(static_cast<std::size_t>(slsf::gameplay::ArmorSlot::Belt) == 49);
static_assert(static_cast<std::size_t>(slsf::gameplay::ArmorSlot::Bra) == 56);
static_assert(static_cast<std::size_t>(slsf::gameplay::ArmorSlot::Harness) == 58);
static_assert(slsf::engine::kEquipmentSlotCount == slsf::gameplay::kArmorSlotCount);

int Fail(const char* a_what) {
    std::printf("probe_result=FAIL reason=%s\n", a_what);
    return 1;
}

} // namespace

int main() {
    const auto version = SKSE::RUNTIME_SSE_LATEST;
    std::printf("probe_target_runtime=%u.%u.%u.%u\n",
                static_cast<unsigned>(version.major()),
                static_cast<unsigned>(version.minor()),
                static_cast<unsigned>(version.patch()),
                static_cast<unsigned>(version.build()));
    std::printf("probe_relationship_sizeof=%zu\n", sizeof(RE::BGSRelationship));
    std::printf("probe_relationship_static_levels=0..8\n");
    // This is a project assumption, not an engine observation.  The live
    // RelationshipRanks::value[] table must be read on 1.6.1170 to confirm it.
    std::printf("probe_relationship_runtime_mapping=assumed:4-level;unverified\n");

    const slsf::StableFormKey actor{"Skyrim.esm", 0x100};
    const slsf::StableFormKey player{"Skyrim.esm", 0x14};
    slsf::engine::ActorSnapshot snapshot;
    snapshot.status = slsf::Status::Ready;
    snapshot.actor = actor;
    snapshot.player = player;
    snapshot.relationshipKnown = slsf::Fact::Known(true);
    snapshot.relationshipRank = 4;
    snapshot.distanceKnown = slsf::Fact::Known(true);
    snapshot.distance = 10.0;
    const auto facts = slsf::engine::ToNpcScanTargetFacts(snapshot);
    if (facts.actor != actor || facts.relationshipRank != 4 ||
        slsf::gameplay::ClassifyRelationshipRank(facts.relationshipRank) !=
            slsf::gameplay::RelationshipGroup::Lover) {
        return Fail("dto_relationship_translation");
    }
    std::printf("probe_dto_relationship=rank4->Lover;distance=10\n");

    snapshot.relationshipKnown = slsf::Fact{};
    snapshot.relationshipRank = 0;
    const auto unknownFacts = slsf::engine::ToNpcScanTargetFacts(snapshot);
    if (unknownFacts.relationshipKnown.IsKnownTrue() ||
        unknownFacts.relationshipKnown.IsKnownFalse()) {
        return Fail("dto_relationship_unknown_preserved");
    }
    std::printf("probe_relationship_unknown_dto=unknown-preserved\n");

    const slsf::engine::EngineContext legal{true, true};
    const slsf::engine::EngineContext wrongThread{false, true};
    const slsf::engine::EngineContext notLoaded{true, false};
    if (!slsf::engine::IsLegalContext(legal) ||
        slsf::engine::IsLegalContext(wrongThread) ||
        slsf::engine::IsLegalContext(notLoaded)) {
        return Fail("engine_context_contract");
    }

    std::printf("probe_context_contract=unit-tested:legal:(mainThread&&inGame);else-InvalidState-no-backend-call\n");
    std::printf("probe_detection_signature=RequestDetectionLevel(Actor*,DETECTION_PRIORITY)->int32\n");
    std::printf("probe_los_signature=HasLineOfSight(TESObjectREFR*,bool&)->bool\n");

    // ---- Group 05: WakeSource entry-point evidence (B01) ------------------
    // The unified executor is driven from engine wake entries.  These
    // assertions pin the runtime APIs the adapter must call; the printed
    // markers list what a live 1.6.1170 session still has to confirm.
    static_assert(std::is_same_v<decltype(&RE::UI::GetSingleton),
                                 RE::UI* (*)()>);
    static_assert(std::is_same_v<decltype(std::declval<RE::UI&>().IsMenuOpen(
                                     std::declval<const std::string_view&>())),
                                 bool>);
    static_assert(
        std::is_same_v<decltype(std::declval<const SKSE::TaskInterface&>().AddTask(
                           std::declval<SKSE::TaskInterface::TaskFn>())),
                       void>);
    static_assert(SKSE::RUNTIME_SSE_LATEST == SKSE::RUNTIME_SSE_1_6_1170);
    std::printf("probe_wake_game_thread=SKSE::GetTaskInterface()->AddTask(fn)->per-frame-callback-valid\n");
    std::printf("probe_wake_menu_state=RE::UI::IsMenuOpen(menuName)->bool\n");
    std::printf("probe_wake_menu_events=RE::MainMenu/BGSSaveLoadManager observers pending live check\n");
    std::printf("probe_wake_runtime_timing=unverified:menu-open-wake,load-screen-wake,no-input-idle-wake\n");
    std::printf("probe_wake_checklist=1.6.1170:log AddTask cadence, MenuOpen/Close deltas, loading-screen wake\n");

    std::printf("probe_runtime_behaviour=unverified:relationship-record,detection-level,line-of-sight\n");
    std::printf("probe_runtime_game_queries=pending:no-running-game-backend\n");
    std::printf("probe_result=STATIC_PASS\n");
    return 0;
}
