#pragma once

#include "slsf/core/gameplay/Witness.h"
#include "slsf/engine/EngineFacts.h"
#include "slsf/engine/GameData.h"

#include <string>
#include <vector>

namespace slsf::engine {

// High-level legal-context query front end.  Every public query first inspects
// IEngineBackend::Context(); an illegal context fails with Error::Code::
// InvalidState before any actor query is issued.  Not internally synchronized:
// use only from the game main thread or the owning executor thread.
class ActorQueriesService {
public:
    ActorQueriesService(IEngineBackend& backend, GameDataService& gameData);

    Result<ActorSnapshot> Query(const StableFormKey& actor, const StableFormKey& player);
    Result<std::vector<ActorSnapshot>> QueryAll(const std::vector<StableFormKey>& actors,
                                                const StableFormKey& player);
    Result<ActorSnapshot> QueryCached(const StableFormKey& actor) const;

    Status RefreshCurrentActors(const std::vector<StableFormKey>& actors,
                                const StableFormKey& player);

private:
    Result<ActorSnapshot> QueryUnguarded(const StableFormKey& actor,
                                         const StableFormKey& player);
    Result<ActorSnapshot> QueryCachedUnguarded(const StableFormKey& actor) const;

    IEngineBackend& backend_;
    GameDataService& gameData_;
};

// Copy the engine snapshot into the core witness DTO.  This is the only
// translation point; core gameplay code never sees CommonLib/SKSE types.
[[nodiscard]] gameplay::NpcScanTargetFacts ToNpcScanTargetFacts(
    const ActorSnapshot& snapshot);

}