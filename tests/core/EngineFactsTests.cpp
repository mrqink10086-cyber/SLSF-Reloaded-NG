#include <catch2/catch_test_macros.hpp>

#include "slsf/core/gameplay/Common.h"
#include "slsf/engine/ActorQueries.h"
#include "slsf/engine/GameData.h"

#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace slsf;
using namespace slsf::engine;

StableFormKey Key(std::string plugin, std::uint32_t localFormId) {
    return StableFormKey{std::move(plugin), localFormId};
}
ActorSnapshot ReadyActor(const StableFormKey& actor, const StableFormKey& player,
                         int relationshipRank = 0) {
    ActorSnapshot snapshot;
    snapshot.status = Status::Ready;
    snapshot.actor = actor;
    snapshot.player = player;
    snapshot.name = "Actor";
    snapshot.location = Key("Skyrim.esm", 0x1A26F);
    snapshot.loaded = Fact::Known(true);
    snapshot.validLocation = Fact::Known(true);
    snapshot.dead = Fact::Known(false);
    snapshot.inCombat = Fact::Known(false);
    snapshot.sexLabActive = Fact::Known(false);
    snapshot.lineOfSight = Fact::Known(true);
    snapshot.playerDetected = Fact::Known(true);
    snapshot.distanceKnown = Fact::Known(true);
    snapshot.distance = 12.5;
    snapshot.relationshipKnown = Fact::Known(true);
    snapshot.relationshipRank = relationshipRank;
    snapshot.detectionKnown = Fact::Known(true);
    snapshot.detectionLevel = 3;
    snapshot.equipment.slots[32].present = true;
    snapshot.equipment.slots[32].keywords = {"SLS_BikiniArmor"};
    return snapshot;
}
class FakeBackend final : public IEngineBackend {
public:
    EngineContext context{true, true};
    std::vector<LoadedPlugin> plugins{
        {"Skyrim.esm", PluginType::Esm},
        {"SLSF.esp", PluginType::Esp},
        {"Tattoos.esl", PluginType::Esl},
    };
    std::map<StableFormKey, ActorSnapshot> results;
    std::optional<Error> collectError;

    mutable int contextCalls{};
    mutable int collectCalls{};
    mutable int queryCalls{};
    mutable std::vector<StableFormKey> queriedActors;
    mutable std::vector<StableFormKey> queriedPlayers;

    EngineContext Context() const override {
        ++contextCalls;
        return context;
    }

    Result<std::vector<LoadedPlugin>> CollectLoadedPlugins() const override {
        ++collectCalls;
        if (collectError) {
            return std::unexpected(*collectError);
        }
        return plugins;
    }

    Result<ActorSnapshot> QueryActor(const StableFormKey& actor,
                                     const StableFormKey& player) const override {
        ++queryCalls;
        queriedActors.push_back(actor);
        queriedPlayers.push_back(player);
        const auto it = results.find(actor);
        if (it == results.end()) {
            return Fail(Error::Code::NotFound, "actor not found");
        }
        return it->second;
    }
};

}
TEST_CASE("game data refresh canonicalizes loaded plugin names", "[engine][gamedata]") {
    FakeBackend backend;
    GameDataService gameData(backend);

    REQUIRE(gameData.RefreshPlugins() == Status::Ready);
    REQUIRE(backend.collectCalls == 1);
    REQUIRE(gameData.Plugins().size() == 3);
    REQUIRE(gameData.IsPluginLoaded("skyrim.ESM"));
    REQUIRE(gameData.IsPluginLoaded("SLSF.esp"));
    REQUIRE_FALSE(gameData.IsPluginLoaded("missing.esp"));
    REQUIRE_FALSE(gameData.IsPluginLoaded(""));

    const auto key = gameData.MakeStableKey("slsf.ESP", 0x800);
    REQUIRE(key.has_value());
    REQUIRE(key->plugin == "SLSF.esp");
    REQUIRE(key->localFormId == 0x800);

    const auto missing = gameData.MakeStableKey("Missing.esp", 1);
    REQUIRE_FALSE(missing.has_value());
    REQUIRE(missing.error().code == Error::Code::NotFound);
    const auto empty = gameData.MakeStableKey("", 1);
    REQUIRE_FALSE(empty.has_value());
    REQUIRE(empty.error().code == Error::Code::InvalidArgument);

    const auto zeroId = gameData.MakeStableKey("SLSF.esp", 0);
    REQUIRE_FALSE(zeroId.has_value());
    REQUIRE(zeroId.error().code == Error::Code::InvalidArgument);
    const auto tooLarge = gameData.MakeStableKey("SLSF.esp", 0x01000000u);
    REQUIRE_FALSE(tooLarge.has_value());
    REQUIRE(tooLarge.error().code == Error::Code::InvalidArgument);
    const auto maxId = gameData.MakeStableKey("SLSF.esp", 0x00FFFFFFu);
    REQUIRE(maxId.has_value());
    REQUIRE(maxId->localFormId == 0x00FFFFFFu);
}
TEST_CASE("game data refuses illegal contexts and preserves the cache on failure",
          "[engine][gamedata]") {
    FakeBackend backend;
    GameDataService gameData(backend);
    const StableFormKey actor = Key("Skyrim.esm", 0x100);
    const StableFormKey player = Key("Skyrim.esm", 0x14);

    REQUIRE(gameData.RefreshPlugins() == Status::Ready);
    REQUIRE(gameData.Store(ReadyActor(actor, player)) == Status::Ready);
    REQUIRE(gameData.CachedCount() == 1);

    backend.context = EngineContext{false, true};
    REQUIRE(gameData.RefreshPlugins() == Status::NotReady);
    REQUIRE(backend.collectCalls == 1);
    REQUIRE(backend.queryCalls == 0);
    REQUIRE(gameData.CachedCount() == 1);

    backend.context = EngineContext{true, true};
    backend.collectError = Error{Error::Code::NotFound, "no plugin list"};
    REQUIRE(gameData.RefreshPlugins() == Status::Missing);
    REQUIRE(gameData.Plugins().size() == 3);
    REQUIRE(gameData.CachedCount() == 1);

    backend.collectError = Error{Error::Code::Internal, "backend failure"};
    REQUIRE(gameData.RefreshPlugins() == Status::NotReady);
    REQUIRE(gameData.CachedCount() == 1);

    backend.collectError.reset();
    REQUIRE(gameData.RefreshPlugins() == Status::Ready);
    REQUIRE(gameData.CachedCount() == 0);
    REQUIRE(gameData.DirtyCount() == 0);
}
TEST_CASE("dirty refresh issues exactly one engine query per target", "[engine][gamedata]") {
    FakeBackend backend;
    GameDataService gameData(backend);
    const StableFormKey player = Key("Skyrim.esm", 0x14);
    const StableFormKey first = Key("Skyrim.esm", 0x100);
    const StableFormKey second = Key("SLSF.esp", 0x800);

    backend.results[first] = ReadyActor(first, player, 3);
    backend.results[second] = ReadyActor(second, player, -2);

    REQUIRE(gameData.MarkDirty(first) == Status::Ready);
    REQUIRE(gameData.MarkDirty(second) == Status::Ready);
    REQUIRE(gameData.DirtyCount() == 2);
    REQUIRE(gameData.RefreshDirty(player) == Status::Ready);
    REQUIRE(backend.queryCalls == 2);
    REQUIRE(gameData.DirtyCount() == 0);
    REQUIRE(gameData.CachedCount() == 2);
    REQUIRE(gameData.Find(first)->relationshipRank == 3);
    REQUIRE(gameData.Find(second)->relationshipRank == -2);
    REQUIRE(backend.queriedPlayers.size() == 2);
    REQUIRE(backend.queriedPlayers[0] == player);
    REQUIRE(backend.queriedPlayers[1] == player);

    const int callsAfterFirstRefresh = backend.queryCalls;
    REQUIRE(gameData.RefreshDirty(player) == Status::Ready);
    REQUIRE(backend.queryCalls == callsAfterFirstRefresh);
}
TEST_CASE("plugin refresh and the cache share one canonical key spelling",
          "[engine][gamedata]") {
    FakeBackend backend;
    GameDataService gameData(backend);
    const StableFormKey player = Key("skyrim.esm", 0x14);
    const StableFormKey canonicalPlayer = Key("Skyrim.esm", 0x14);
    const StableFormKey canonical = Key("SLSF.esp", 0x800);
    const StableFormKey lower = Key("slsf.esp", 0x800);
    backend.results[canonical] = ReadyActor(canonical, player);

    // Dirty keys recorded before the load order is known are normalized when
    // RefreshPlugins succeeds, rather than being dropped or duplicated.
    REQUIRE(gameData.MarkDirty(lower) == Status::Ready);
    REQUIRE(gameData.RefreshPlugins() == Status::Ready);
    REQUIRE(gameData.DirtyCount() == 1);
    REQUIRE(gameData.RefreshDirty(player) == Status::Ready);
    REQUIRE(backend.queryCalls == 1);
    REQUIRE(backend.queriedActors[0] == canonical);
    REQUIRE(backend.queriedPlayers[0] == canonicalPlayer);
    REQUIRE(gameData.DirtyCount() == 0);
    REQUIRE(gameData.CachedCount() == 1);
    REQUIRE(gameData.Find(lower) != nullptr);
    REQUIRE(gameData.Find(lower)->actor == canonical);
    REQUIRE(gameData.Find(lower)->player == canonicalPlayer);

    // Store follows the same normalization and clears a differently-spelled
    // dirty key.
    ActorSnapshot snapshot = ReadyActor(lower, player);
    REQUIRE(gameData.MarkDirty(lower) == Status::Ready);
    REQUIRE(gameData.Store(snapshot) == Status::Ready);
    REQUIRE(gameData.DirtyCount() == 0);
    REQUIRE(gameData.CachedCount() == 1);
    REQUIRE(gameData.Find(canonical) != nullptr);
}
TEST_CASE("targeted refresh never drains unrelated dirty history", "[engine][gamedata]") {
    FakeBackend backend;
    GameDataService gameData(backend);
    const StableFormKey player = Key("Skyrim.esm", 0x14);
    const StableFormKey historical = Key("Skyrim.esm", 0x100);
    const StableFormKey target = Key("SLSF.esp", 0x800);
    backend.results[historical] = ReadyActor(historical, player);
    backend.results[target] = ReadyActor(target, player);

    REQUIRE(gameData.RefreshPlugins() == Status::Ready);
    REQUIRE(gameData.MarkDirty(historical) == Status::Ready);

    REQUIRE(gameData.RefreshTargets({target, target}, player) == Status::Ready);
    REQUIRE(backend.queryCalls == 1);
    REQUIRE(backend.queriedActors[0] == target);
    REQUIRE(gameData.DirtyCount() == 1);
    REQUIRE(gameData.Find(target) != nullptr);
    REQUIRE(gameData.Find(historical) == nullptr);

    REQUIRE(gameData.RefreshTargets({Key("SLSF.esp", 0)}, player) == Status::Invalid);
    REQUIRE(backend.queryCalls == 1);

    REQUIRE(gameData.RefreshDirty(player) == Status::Ready);
    REQUIRE(backend.queryCalls == 2);
    REQUIRE(gameData.DirtyCount() == 0);
    REQUIRE(gameData.Find(historical) != nullptr);
}
TEST_CASE("dirty refresh canonicalizes and rejects invalid player keys",
          "[engine][gamedata]") {
    FakeBackend backend;
    GameDataService gameData(backend);
    const StableFormKey canonicalPlayer = Key("Skyrim.esm", 0x14);
    const StableFormKey lowerPlayer = Key("skyrim.esm", 0x14);
    const StableFormKey actor = Key("SLSF.esp", 0x800);
    REQUIRE(gameData.RefreshPlugins() == Status::Ready);
    backend.results[actor] = ReadyActor(actor, lowerPlayer);

    REQUIRE(gameData.MarkDirty(actor) == Status::Ready);
    REQUIRE(gameData.RefreshDirty(lowerPlayer) == Status::Ready);
    REQUIRE(backend.queryCalls == 1);
    REQUIRE(backend.queriedPlayers[0] == canonicalPlayer);
    REQUIRE(gameData.Find(actor)->player == canonicalPlayer);
    REQUIRE(gameData.DirtyCount() == 0);

    // An invalid player key must fail before touching the backend or disturbing
    // the dirty/cache state.  RefreshTargets has the same contract.
    REQUIRE(gameData.MarkDirty(actor) == Status::Ready);
    const int callsBeforeInvalidPlayer = backend.queryCalls;
    REQUIRE(gameData.RefreshDirty(StableFormKey{}) == Status::Invalid);
    REQUIRE(backend.queryCalls == callsBeforeInvalidPlayer);
    REQUIRE(gameData.DirtyCount() == 1);
    REQUIRE(gameData.Find(actor) != nullptr);

    REQUIRE(gameData.RefreshTargets({actor}, Key("SLSF.esp", 0)) == Status::Invalid);
    REQUIRE(backend.queryCalls == callsBeforeInvalidPlayer);
    REQUIRE(gameData.DirtyCount() == 1);
}
TEST_CASE("forget removes both the cached snapshot and its dirty bit", "[engine][gamedata]") {
    FakeBackend backend;
    GameDataService gameData(backend);
    const StableFormKey player = Key("Skyrim.esm", 0x14);
    const StableFormKey actor = Key("SLSF.esp", 0x800);
    backend.results[actor] = ReadyActor(actor, player);

    REQUIRE(gameData.RefreshPlugins() == Status::Ready);
    REQUIRE(gameData.Store(ReadyActor(actor, player)) == Status::Ready);
    REQUIRE(gameData.MarkDirty(actor) == Status::Ready);
    REQUIRE(gameData.CachedCount() == 1);
    REQUIRE(gameData.DirtyCount() == 1);
    REQUIRE(gameData.Forget(Key("slsf.esp", 0x800)) == Status::Ready);
    REQUIRE(gameData.CachedCount() == 0);
    REQUIRE(gameData.DirtyCount() == 0);
    REQUIRE(gameData.Find(actor) == nullptr);
    REQUIRE(gameData.Forget(Key("SLSF.esp", 0)) == Status::Invalid);
}
TEST_CASE("dirty refresh retries transient failures but forgets missing actors",
          "[engine][gamedata]") {
    FakeBackend backend;
    GameDataService gameData(backend);
    const StableFormKey player = Key("Skyrim.esm", 0x14);
    const StableFormKey good = Key("Skyrim.esm", 0x100);
    const StableFormKey missing = Key("SLSF.esp", 0x800);
    const StableFormKey invalid = Key("SLSF.esp", 0x801);

    backend.results[good] = ReadyActor(good, player);
    ActorSnapshot badRank = ReadyActor(invalid, player);
    badRank.relationshipRank = 9;
    backend.results[invalid] = badRank;

    REQUIRE(gameData.MarkDirty(good) == Status::Ready);
    REQUIRE(gameData.MarkDirty(missing) == Status::Ready);
    REQUIRE(gameData.MarkDirty(invalid) == Status::Ready);
    REQUIRE(gameData.RefreshDirty(player) == Status::Missing);
    REQUIRE(backend.queryCalls == 3);
    // NotFound is terminal for this load: only the invalid snapshot stays queued.
    REQUIRE(gameData.DirtyCount() == 1);
    REQUIRE(gameData.CachedCount() == 3);
    REQUIRE(gameData.Find(good)->status == Status::Ready);
    REQUIRE(gameData.Find(missing)->status == Status::Missing);
    REQUIRE(gameData.Find(invalid)->status == Status::Invalid);
    REQUIRE(gameData.Find(missing)->actor == missing);
    REQUIRE(gameData.Find(missing)->player == player);
    REQUIRE(gameData.Find(invalid)->status != Status::Ready);

    backend.results[invalid] = ReadyActor(invalid, player);
    REQUIRE(gameData.RefreshDirty(player) == Status::Ready);
    REQUIRE(gameData.DirtyCount() == 0);
    REQUIRE(gameData.Find(missing)->status == Status::Missing);

    // MarkDirty re-arms a forgotten actor once the adapter sees it again.
    backend.results[missing] = ReadyActor(missing, player);
    REQUIRE(gameData.MarkDirty(missing) == Status::Ready);
    REQUIRE(gameData.RefreshDirty(player) == Status::Ready);
    REQUIRE(gameData.DirtyCount() == 0);
    REQUIRE(gameData.Find(missing)->status == Status::Ready);
}
TEST_CASE("a returned non-ready snapshot stays dirty as a partial observation",
          "[engine][gamedata]") {
    FakeBackend backend;
    GameDataService gameData(backend);
    const StableFormKey player = Key("Skyrim.esm", 0x14);
    const StableFormKey actor = Key("SLSF.esp", 0x800);
    ActorSnapshot partial = ReadyActor(actor, player);
    partial.status = Status::Missing;
    backend.results[actor] = partial;

    REQUIRE(gameData.MarkDirty(actor) == Status::Ready);
    REQUIRE(gameData.RefreshDirty(player) == Status::Missing);
    REQUIRE(backend.queryCalls == 1);
    REQUIRE(gameData.CachedCount() == 1);
    REQUIRE(gameData.Find(actor)->status == Status::Missing);
    // Only Error::Code::NotFound is terminal; a returned snapshot with a
    // non-Ready status is retained for the next refresh.
    REQUIRE(gameData.DirtyCount() == 1);
    REQUIRE(gameData.RefreshDirty(player) == Status::Missing);
    REQUIRE(backend.queryCalls == 2);
    REQUIRE(gameData.DirtyCount() == 1);
}
TEST_CASE("dirty refresh rejects actor mismatches, negative facts and partial snapshots",
          "[engine][gamedata]") {
    FakeBackend backend;
    GameDataService gameData(backend);
    const StableFormKey player = Key("Skyrim.esm", 0x14);
    const StableFormKey actor = Key("Skyrim.esm", 0x100);
    const StableFormKey other = Key("Skyrim.esm", 0x101);

    SECTION("an actor key mismatch invalidates the result") {
        backend.results[actor] = ReadyActor(other, player);
        REQUIRE(gameData.MarkDirty(actor) == Status::Ready);
        REQUIRE(gameData.RefreshDirty(player) == Status::Invalid);
        REQUIRE(gameData.DirtyCount() == 1);
        REQUIRE(gameData.Find(actor)->status == Status::Invalid);
    }

    SECTION("negative distance and detection are invalid") {
        ActorSnapshot negativeDistance = ReadyActor(actor, player);
        negativeDistance.distance = -1.0;
        backend.results[actor] = negativeDistance;
        REQUIRE(gameData.MarkDirty(actor) == Status::Ready);
        REQUIRE(gameData.RefreshDirty(player) == Status::Invalid);
        REQUIRE(gameData.DirtyCount() == 1);

        ActorSnapshot negativeDetection = ReadyActor(actor, player);
        negativeDetection.detectionLevel = -1;
        backend.results[actor] = negativeDetection;
        REQUIRE(gameData.RefreshDirty(player) == Status::Invalid);
        REQUIRE(gameData.DirtyCount() == 1);
    }

    SECTION("non-finite distances are invalid") {
        for (const double invalidDistance :
             {std::numeric_limits<double>::quiet_NaN(),
              std::numeric_limits<double>::infinity(),
              -std::numeric_limits<double>::infinity()}) {
            ActorSnapshot snapshot = ReadyActor(actor, player);
            snapshot.distance = invalidDistance;
            backend.results[actor] = snapshot;
            REQUIRE(gameData.MarkDirty(actor) == Status::Ready);
            REQUIRE(gameData.RefreshDirty(player) == Status::Invalid);
            REQUIRE(gameData.DirtyCount() == 1);
            REQUIRE(gameData.Find(actor)->status == Status::Invalid);
        }
    }

    SECTION("a partial snapshot is cached but remains dirty") {
        ActorSnapshot partial = ReadyActor(actor, player);
        partial.status = Status::NotReady;
        backend.results[actor] = partial;
        REQUIRE(gameData.MarkDirty(actor) == Status::Ready);
        REQUIRE(gameData.RefreshDirty(player) == Status::NotReady);
        REQUIRE(gameData.DirtyCount() == 1);
        REQUIRE(gameData.Find(actor)->status == Status::NotReady);
    }

    SECTION("an illegal context performs no query") {
        backend.context = EngineContext{true, false};
        REQUIRE(gameData.MarkDirty(actor) == Status::Ready);
        REQUIRE(gameData.RefreshDirty(player) == Status::NotReady);
        REQUIRE(backend.queryCalls == 0);
        REQUIRE(gameData.DirtyCount() == 1);
    }
}
TEST_CASE("game data store validates facts and clears the dirty bit", "[engine][gamedata]") {
    FakeBackend backend;
    GameDataService gameData(backend);
    const StableFormKey player = Key("Skyrim.esm", 0x14);
    const StableFormKey actor = Key("Skyrim.esm", 0x100);

    ActorSnapshot snapshot = ReadyActor(actor, player);
    REQUIRE(gameData.MarkDirty(actor) == Status::Ready);
    REQUIRE(gameData.Store(snapshot) == Status::Ready);
    REQUIRE(gameData.DirtyCount() == 0);
    REQUIRE(gameData.Find(actor) != nullptr);

    ActorSnapshot partial = ReadyActor(actor, player);
    partial.status = Status::NotReady;
    REQUIRE(gameData.Store(partial) == Status::Ready);
    REQUIRE(gameData.DirtyCount() == 1);

    ActorSnapshot noActor;
    REQUIRE(gameData.Store(noActor) == Status::Invalid);

    ActorSnapshot noPlayer = ReadyActor(actor, StableFormKey{});
    REQUIRE(gameData.Store(noPlayer) == Status::Invalid);

    ActorSnapshot badRank = ReadyActor(actor, player);
    badRank.relationshipRank = 5;
    REQUIRE(gameData.Store(badRank) == Status::Invalid);

    ActorSnapshot negativeDetection = ReadyActor(actor, player);
    negativeDetection.detectionLevel = -3;
    REQUIRE(gameData.Store(negativeDetection) == Status::Invalid);

    ActorSnapshot nanDistance = ReadyActor(actor, player);
    nanDistance.distance = std::numeric_limits<double>::quiet_NaN();
    REQUIRE(gameData.Store(nanDistance) == Status::Invalid);
    ActorSnapshot infiniteDistance = ReadyActor(actor, player);
    infiniteDistance.distance = std::numeric_limits<double>::infinity();
    REQUIRE(gameData.Store(infiniteDistance) == Status::Invalid);

    std::uint32_t emptyId = 0;
    (void)emptyId;
    REQUIRE(gameData.MarkDirty(StableFormKey{}) == Status::Invalid);
}
TEST_CASE("actor queries require a legal engine context before touching the backend",
          "[engine][queries]") {
    FakeBackend backend;
    GameDataService gameData(backend);
    ActorQueriesService queries(backend, gameData);
    const StableFormKey actor = Key("Skyrim.esm", 0x100);
    const StableFormKey player = Key("Skyrim.esm", 0x14);
    backend.results[actor] = ReadyActor(actor, player);

    for (const EngineContext illegal : {EngineContext{false, true},
                                        EngineContext{true, false},
                                        EngineContext{false, false}}) {
        backend.context = illegal;
        const auto queried = queries.Query(actor, player);
        REQUIRE_FALSE(queried.has_value());
        REQUIRE(queried.error().code == Error::Code::InvalidState);
        const auto all = queries.QueryAll({actor}, player);
        REQUIRE_FALSE(all.has_value());
        REQUIRE(all.error().code == Error::Code::InvalidState);
        const auto cached = queries.QueryCached(actor);
        REQUIRE_FALSE(cached.has_value());
        REQUIRE(cached.error().code == Error::Code::InvalidState);
        REQUIRE(queries.RefreshCurrentActors({actor}, player) == Status::NotReady);
    }

    REQUIRE(backend.queryCalls == 0);
    REQUIRE(backend.contextCalls == 12);
    REQUIRE(gameData.CachedCount() == 0);
}
TEST_CASE("actor queries cache successful snapshots and expose cached lookups",
          "[engine][queries]") {
    FakeBackend backend;
    GameDataService gameData(backend);
    ActorQueriesService queries(backend, gameData);
    const StableFormKey player = Key("Skyrim.esm", 0x14);
    const StableFormKey first = Key("Skyrim.esm", 0x100);
    const StableFormKey second = Key("Tattoos.esl", 0x800);
    backend.results[first] = ReadyActor(first, player, 2);
    backend.results[second] = ReadyActor(second, player, 4);

    const auto one = queries.Query(first, player);
    REQUIRE(one.has_value());
    REQUIRE(one->actor == first);
    REQUIRE(backend.queryCalls == 1);
    REQUIRE(gameData.CachedCount() == 1);

    const auto cached = queries.QueryCached(first);
    REQUIRE(cached.has_value());
    REQUIRE(cached->relationshipRank == 2);
    REQUIRE(backend.queryCalls == 1);

    const auto absent = queries.QueryCached(second);
    REQUIRE_FALSE(absent.has_value());
    REQUIRE(absent.error().code == Error::Code::NotFound);

    const auto both = queries.QueryAll({first, second}, player);
    REQUIRE(both.has_value());
    REQUIRE(both->size() == 2);
    REQUIRE(backend.queryCalls == 3);
    REQUIRE(gameData.CachedCount() == 2);

    const int contextBefore = backend.contextCalls;
    const auto allAgain = queries.QueryAll({first, second}, player);
    REQUIRE(allAgain.has_value());
    REQUIRE(backend.contextCalls == contextBefore + 1);
    REQUIRE(backend.queryCalls == 5);
}
TEST_CASE("actor queries reject a snapshot that belongs to another player",
          "[engine][queries]") {
    FakeBackend backend;
    GameDataService gameData(backend);
    ActorQueriesService queries(backend, gameData);
    const StableFormKey player = Key("Skyrim.esm", 0x14);
    const StableFormKey otherPlayer = Key("Skyrim.esm", 0x15);
    const StableFormKey actor = Key("Skyrim.esm", 0x100);
    backend.results[actor] = ReadyActor(actor, otherPlayer);

    const auto queried = queries.Query(actor, player);
    REQUIRE_FALSE(queried.has_value());
    REQUIRE(queried.error().code == Error::Code::InvalidState);
    REQUIRE(backend.queryCalls == 1);
    REQUIRE(gameData.CachedCount() == 0);
}
TEST_CASE("actor queries reject non-finite distances before caching",
          "[engine][queries]") {
    FakeBackend backend;
    GameDataService gameData(backend);
    ActorQueriesService queries(backend, gameData);
    const StableFormKey player = Key("Skyrim.esm", 0x14);
    const StableFormKey actor = Key("Skyrim.esm", 0x100);
    for (const double invalidDistance :
         {std::numeric_limits<double>::quiet_NaN(),
          std::numeric_limits<double>::infinity()}) {
        ActorSnapshot snapshot = ReadyActor(actor, player);
        snapshot.distance = invalidDistance;
        backend.results[actor] = snapshot;
        const auto queried = queries.Query(actor, player);
        REQUIRE_FALSE(queried.has_value());
        REQUIRE(queried.error().code == Error::Code::InvalidState);
        REQUIRE(backend.queryCalls >= 1);
        REQUIRE(gameData.CachedCount() == 0);
    }
}
TEST_CASE("actor queries propagate an engine error after earlier successes",
          "[engine][queries]") {
    FakeBackend backend;
    GameDataService gameData(backend);
    ActorQueriesService queries(backend, gameData);
    const StableFormKey player = Key("Skyrim.esm", 0x14);
    const StableFormKey first = Key("Skyrim.esm", 0x100);
    const StableFormKey missing = Key("Skyrim.esm", 0x101);
    backend.results[first] = ReadyActor(first, player);

    const auto all = queries.QueryAll({first, missing}, player);
    REQUIRE_FALSE(all.has_value());
    REQUIRE(all.error().code == Error::Code::NotFound);
    REQUIRE(backend.queryCalls == 2);
    REQUIRE(gameData.Find(first) != nullptr);
    REQUIRE(gameData.Find(missing) == nullptr);
}
TEST_CASE("refresh current actors marks each target and merges one query per actor",
          "[engine][queries]") {
    FakeBackend backend;
    GameDataService gameData(backend);
    ActorQueriesService queries(backend, gameData);
    const StableFormKey player = Key("Skyrim.esm", 0x14);
    const StableFormKey first = Key("Skyrim.esm", 0x100);
    const StableFormKey second = Key("SLSF.esp", 0x800);
    backend.results[first] = ReadyActor(first, player, 1);
    backend.results[second] = ReadyActor(second, player, 4);

    REQUIRE(queries.RefreshCurrentActors({first, second}, player) == Status::Ready);
    REQUIRE(backend.queryCalls == 2);
    REQUIRE(gameData.DirtyCount() == 0);
    REQUIRE(gameData.CachedCount() == 2);
    REQUIRE(gameData.Find(first)->relationshipRank == 1);
    REQUIRE(gameData.Find(second)->relationshipRank == 4);
}
TEST_CASE("actor snapshots translate into the core witness DTO", "[engine][queries]") {
    const StableFormKey actor = Key("Skyrim.esm", 0x100);
    const StableFormKey player = Key("Skyrim.esm", 0x14);
    ActorSnapshot snapshot = ReadyActor(actor, player, -1);
    snapshot.validLocation = Fact::Known(false);
    snapshot.dead = Fact::Known(true);
    snapshot.inCombat = Fact::Known(true);
    snapshot.sexLabActive = Fact::Known(true);
    snapshot.lineOfSight = Fact::Known(false);
    snapshot.playerDetected = Fact::Known(false);
    snapshot.relationshipKnown = Fact::Known(true);
    snapshot.distanceKnown = Fact::Known(true);
    snapshot.distance = 42.0;

    const auto facts = ToNpcScanTargetFacts(snapshot);
    REQUIRE(facts.actor == actor);
    REQUIRE(facts.validLocation.IsKnownFalse());
    REQUIRE(facts.dead.IsKnownTrue());
    REQUIRE(facts.inCombat.IsKnownTrue());
    REQUIRE(facts.sexLabActive.IsKnownTrue());
    REQUIRE(facts.lineOfSight.IsKnownFalse());
    REQUIRE(facts.playerDetected.IsKnownFalse());
    REQUIRE(facts.relationshipKnown.IsKnownTrue());
    REQUIRE(facts.relationshipRank == -1);
    REQUIRE(facts.distance.IsKnown());
    REQUIRE(facts.distance.value == 42.0);
    REQUIRE(gameplay::ClassifyRelationshipRank(facts.relationshipRank) ==
            gameplay::RelationshipGroup::Enemy);

    snapshot.distanceKnown = Fact{};
    const auto unknownDistance = ToNpcScanTargetFacts(snapshot);
    REQUIRE_FALSE(unknownDistance.distance.IsKnown());
}