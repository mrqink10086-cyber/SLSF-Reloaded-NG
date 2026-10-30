#pragma once

#include "slsf/engine/EngineFacts.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace slsf::engine {

// Owns the loaded-plugin list and the per-actor fact cache.  MarkDirty only
// records the target; RefreshDirty performs exactly one backend QueryActor per
// dirty target and merges the result into the cache.  A NotFound result is
// terminal for the current load: the target is cached as Missing and removed
// from the dirty set, so an unloaded or deleted actor cannot pollute every later
// refresh.  A caller that learns the actor is available again must MarkDirty it.
//
// The service is not internally synchronized.  Call it only from the game main
// thread or the unified executor thread that owns this instance.
class GameDataService {
public:
    explicit GameDataService(IEngineBackend& backend);

    Status RefreshPlugins();
    [[nodiscard]] const std::vector<LoadedPlugin>& Plugins() const noexcept { return plugins_; }
    [[nodiscard]] bool IsPluginLoaded(std::string_view plugin) const;
    [[nodiscard]] Result<StableFormKey> MakeStableKey(std::string_view plugin,
                                                      std::uint32_t localFormId) const;

    Status MarkDirty(const StableFormKey& actor);
    Status Store(ActorSnapshot snapshot);
    Status RefreshDirty(const StableFormKey& player);

    // Refresh only the supplied actors, never unrelated historical dirty
    // entries.  Every supplied key is marked dirty first; duplicates are queried
    // once.  Used by the high-level scene scan.
    Status RefreshTargets(const std::vector<StableFormKey>& targets,
                          const StableFormKey& player);

    // Drop a cached snapshot and its dirty bit.  Use when an adapter proves the
    // Form is gone and cannot return; RefreshDirty already forgets NotFound
    // targets itself.
    Status Forget(const StableFormKey& actor);

    // The returned pointer is owned by the cache and is invalidated by Store,
    // RefreshDirty, RefreshPlugins, Forget, or Clear.  Copy it before mutating.
    [[nodiscard]] const ActorSnapshot* Find(const StableFormKey& actor) const;
    [[nodiscard]] std::size_t DirtyCount() const noexcept { return dirty_.size(); }
    [[nodiscard]] std::size_t CachedCount() const noexcept { return cache_.size(); }
    void Clear();

private:
    [[nodiscard]] StableFormKey NormalizeKey(const StableFormKey& actor) const;
    Status RefreshTargetsImpl(const std::vector<StableFormKey>& targets,
                              const StableFormKey& player);
    void StoreUnknown(const StableFormKey& actor, const StableFormKey& player, Status status);

    IEngineBackend& backend_;
    std::vector<LoadedPlugin> plugins_;
    std::map<StableFormKey, ActorSnapshot> cache_;
    std::set<StableFormKey> dirty_;
};

}