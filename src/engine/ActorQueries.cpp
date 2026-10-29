#include "slsf/engine/ActorQueries.h"

#include "slsf/core/gameplay/Visibility.h"

#include <cmath>
#include <utility>

namespace slsf::engine {
namespace {

static_assert(kEquipmentSlotCount == gameplay::kArmorSlotCount,
              "engine equipment slots must match the gameplay armor slots");

constexpr const char* kLegalContextMessage =
    "engine queries require the game main thread and loaded game";

constexpr std::uint32_t kMaxLocalFormId = 0x00FFFFFFu;

Status ValidateSnapshot(const ActorSnapshot& snapshot, const StableFormKey& expectedActor,
                         const StableFormKey& expectedPlayer) noexcept {
    if (snapshot.actor.plugin.empty() || snapshot.actor != expectedActor ||
        snapshot.actor.localFormId == 0 ||
        snapshot.actor.localFormId > kMaxLocalFormId) {
        return Status::Invalid;
    }
    if (snapshot.player.plugin.empty() || snapshot.player != expectedPlayer ||
        snapshot.player.localFormId == 0 ||
        snapshot.player.localFormId > kMaxLocalFormId) {
        return Status::Invalid;
    }
    if (snapshot.relationshipKnown.IsKnownTrue() &&
        !IsRelationshipRankValid(snapshot.relationshipRank)) {
        return Status::Invalid;
    }
    if (snapshot.distanceKnown.IsKnownTrue() &&
        (!std::isfinite(snapshot.distance) || snapshot.distance < 0.0)) {
        return Status::Invalid;
    }
    if (snapshot.detectionKnown.IsKnownTrue() && snapshot.detectionLevel < 0) {
        return Status::Invalid;
    }
    return Status::Ready;
}

} // namespace

ActorQueriesService::ActorQueriesService(IEngineBackend& backend, GameDataService& gameData)
    : backend_(backend), gameData_(gameData) {}

Result<ActorSnapshot> ActorQueriesService::Query(const StableFormKey& actor,
                                                  const StableFormKey& player) {
    if (!IsLegalContext(backend_.Context())) {
        return Fail(Error::Code::InvalidState, kLegalContextMessage);
    }
    return QueryUnguarded(actor, player);
}

Result<std::vector<ActorSnapshot>> ActorQueriesService::QueryAll(
    const std::vector<StableFormKey>& actors,
    const StableFormKey& player) {
    if (!IsLegalContext(backend_.Context())) {
        return Fail(Error::Code::InvalidState, kLegalContextMessage);
    }

    std::vector<ActorSnapshot> snapshots;
    snapshots.reserve(actors.size());
    for (const auto& actor : actors) {
        auto queried = QueryUnguarded(actor, player);
        if (!queried) {
            return Fail(queried.error().code, queried.error().message);
        }
        snapshots.push_back(std::move(*queried));
    }
    return snapshots;
}

Result<ActorSnapshot> ActorQueriesService::QueryCached(const StableFormKey& actor) const {
    if (!IsLegalContext(backend_.Context())) {
        return Fail(Error::Code::InvalidState, kLegalContextMessage);
    }
    return QueryCachedUnguarded(actor);
}

Status ActorQueriesService::RefreshCurrentActors(const std::vector<StableFormKey>& actors,
                                                  const StableFormKey& player) {
    if (!IsLegalContext(backend_.Context())) {
        return Status::NotReady;
    }
    // Refresh only the supplied scene actors.  Unrelated historical dirty
    // targets must not be queried by, or fail, this scan.
    return gameData_.RefreshTargets(actors, player);
}

Result<ActorSnapshot> ActorQueriesService::QueryUnguarded(const StableFormKey& actor,
                                                           const StableFormKey& player) {
    auto queried = backend_.QueryActor(actor, player);
    if (!queried) {
        return Fail(queried.error().code, queried.error().message);
    }

    auto snapshot = std::move(*queried);
    if (ValidateSnapshot(snapshot, actor, player) != Status::Ready) {
        return Fail(Error::Code::InvalidState,
                    "engine returned an invalid actor snapshot");
    }

    if (gameData_.Store(snapshot) != Status::Ready) {
        return Fail(Error::Code::InvalidState,
                    "engine snapshot could not be stored");
    }
    return snapshot;
}

Result<ActorSnapshot> ActorQueriesService::QueryCachedUnguarded(
    const StableFormKey& actor) const {
    const auto* snapshot = gameData_.Find(actor);
    if (snapshot == nullptr) {
        return Fail(Error::Code::NotFound, "actor is not present in the engine fact cache");
    }
    return *snapshot;
}

gameplay::NpcScanTargetFacts ToNpcScanTargetFacts(const ActorSnapshot& snapshot) {
    gameplay::NpcScanTargetFacts facts;
    facts.actor = snapshot.actor;
    facts.validLocation = snapshot.validLocation;
    facts.dead = snapshot.dead;
    facts.inCombat = snapshot.inCombat;
    facts.sexLabActive = snapshot.sexLabActive;
    facts.lineOfSight = snapshot.lineOfSight;
    facts.playerDetected = snapshot.playerDetected;
    facts.distance = snapshot.distanceKnown.IsKnownTrue() &&
                             std::isfinite(snapshot.distance) && snapshot.distance >= 0.0
                         ? gameplay::ObservedDouble::Known(snapshot.distance)
                         : gameplay::ObservedDouble{};
    facts.relationshipKnown = snapshot.relationshipKnown;
    facts.relationshipRank = snapshot.relationshipRank;
    return facts;
}

} // namespace slsf::engine
