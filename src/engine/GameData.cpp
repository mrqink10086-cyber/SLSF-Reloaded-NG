#include "slsf/engine/GameData.h"

#include <cmath>
#include <string>
#include <utility>

namespace slsf::engine {
namespace {

constexpr std::uint32_t kMaxLocalFormId = 0x00FFFFFFu;

[[nodiscard]] bool IsValidFormId(std::uint32_t localFormId) noexcept {
    return localFormId != 0 && localFormId <= kMaxLocalFormId;
}
[[nodiscard]] bool IsValidKey(const StableFormKey& key) noexcept {
    return !key.plugin.empty() && IsValidFormId(key.localFormId);
}
[[nodiscard]] char AsciiLower(char value) noexcept {
    return value >= 'A' && value <= 'Z' ? static_cast<char>(value + ('a' - 'A')) : value;
}
[[nodiscard]] bool EqualsAsciiIgnoreCase(std::string_view lhs,
                                          std::string_view rhs) noexcept {
    if (lhs.size() != rhs.size()) {
        return false;
    }
    for (std::size_t i = 0; i < lhs.size(); ++i) {
        if (AsciiLower(lhs[i]) != AsciiLower(rhs[i])) {
            return false;
        }
    }
    return true;
}
Status ValidateSnapshot(const ActorSnapshot& snapshot) noexcept {
    if (snapshot.actor.plugin.empty() || !IsValidFormId(snapshot.actor.localFormId)) {
        return Status::Invalid;
    }
    if (snapshot.player.plugin.empty() || !IsValidFormId(snapshot.player.localFormId)) {
        return Status::Invalid;
    }
    if (snapshot.relationshipKnown.IsKnownTrue() &&
        !IsRelationshipRankValid(snapshot.relationshipRank)) {
        return Status::Invalid;
    }
    // NaN compares false against every bound, and an infinite distance is not
    // a usable engine observation either.
    if (snapshot.distanceKnown.IsKnownTrue() &&
        (!std::isfinite(snapshot.distance) || snapshot.distance < 0.0)) {
        return Status::Invalid;
    }
    if (snapshot.detectionKnown.IsKnownTrue() && snapshot.detectionLevel < 0) {
        return Status::Invalid;
    }
    return Status::Ready;
}
}
GameDataService::GameDataService(IEngineBackend& backend) : backend_(backend) {}

Status GameDataService::RefreshPlugins() {
    if (!IsLegalContext(backend_.Context())) {
        return Status::NotReady;
    }
    auto loaded = backend_.CollectLoadedPlugins();
    if (!loaded) {
        return StatusFromError(loaded.error());
    }
    plugins_ = std::move(*loaded);
    // Cached snapshots belong to the old load order.  Pending refresh requests
    // are still meaningful for the same stable plugin/FormID keys, so keep them.
    cache_.clear();
    std::set<StableFormKey> normalizedDirty;
    for (const auto& key : dirty_) {
        normalizedDirty.insert(NormalizeKey(key));
    }
    dirty_ = std::move(normalizedDirty);
    return Status::Ready;
}
bool GameDataService::IsPluginLoaded(std::string_view plugin) const {
    if (plugin.empty()) {
        return false;
    }
    for (const auto& entry : plugins_) {
        if (EqualsAsciiIgnoreCase(entry.name, plugin)) {
            return true;
        }
    }
    return false;
}
Result<StableFormKey> GameDataService::MakeStableKey(std::string_view plugin,
                                                    std::uint32_t localFormId) const {
    if (plugin.empty()) {
        return Fail(Error::Code::InvalidArgument, "plugin name must not be empty");
    }
    if (!IsValidFormId(localFormId)) {
        return Fail(Error::Code::InvalidArgument,
                    "local form id must be in 1..0xFFFFFF");
    }
    for (const auto& entry : plugins_) {
        if (EqualsAsciiIgnoreCase(entry.name, plugin)) {
            return StableFormKey{entry.name, localFormId};
        }
    }
    return Fail(Error::Code::NotFound,
                "plugin is not loaded: " + std::string(plugin));
}
StableFormKey GameDataService::NormalizeKey(const StableFormKey& actor) const {
    StableFormKey key = actor;
    if (key.plugin.empty()) {
        return key;
    }
    for (const auto& entry : plugins_) {
        if (EqualsAsciiIgnoreCase(entry.name, key.plugin)) {
            key.plugin = entry.name;
            return key;
        }
    }
    return key;
}
Status GameDataService::MarkDirty(const StableFormKey& actor) {
    if (actor.plugin.empty() || !IsValidFormId(actor.localFormId)) {
        return Status::Invalid;
    }
    dirty_.insert(NormalizeKey(actor));
    return Status::Ready;
}
Status GameDataService::Store(ActorSnapshot snapshot) {
    snapshot.actor = NormalizeKey(snapshot.actor);
    snapshot.player = NormalizeKey(snapshot.player);
    const auto validation = ValidateSnapshot(snapshot);
    if (validation != Status::Ready) {
        return validation;
    }
    const auto key = snapshot.actor;
    const Status snapshotStatus = snapshot.status;
    cache_[key] = std::move(snapshot);
    if (snapshotStatus == Status::Ready) {
        dirty_.erase(key);
    } else {
        // Keep the target queued so a later legal-context refresh can replace a
        // partial or failed observation.
        dirty_.insert(key);
    }
    return Status::Ready;
}
Status GameDataService::RefreshDirty(const StableFormKey& player) {
    if (!IsLegalContext(backend_.Context())) {
        return Status::NotReady;
    }
    const StableFormKey canonicalPlayer = NormalizeKey(player);
    if (!IsValidKey(canonicalPlayer)) {
        return Status::Invalid;
    }
    const std::vector<StableFormKey> targets(dirty_.begin(), dirty_.end());
    return RefreshTargetsImpl(targets, canonicalPlayer);
}
Status GameDataService::RefreshTargets(const std::vector<StableFormKey>& targets,
                                       const StableFormKey& player) {
    if (!IsLegalContext(backend_.Context())) {
        return Status::NotReady;
    }
    const StableFormKey canonicalPlayer = NormalizeKey(player);
    if (!IsValidKey(canonicalPlayer)) {
        return Status::Invalid;
    }

    std::set<StableFormKey> unique;
    for (const auto& target : targets) {
        if (target.plugin.empty() || !IsValidFormId(target.localFormId)) {
            return Status::Invalid;
        }
        unique.insert(NormalizeKey(target));
    }
    for (const auto& target : unique) {
        const auto marked = MarkDirty(target);
        if (marked != Status::Ready) {
            return marked;
        }
    }

    const std::vector<StableFormKey> ordered(unique.begin(), unique.end());
    return RefreshTargetsImpl(ordered, canonicalPlayer);
}
Status GameDataService::RefreshTargetsImpl(const std::vector<StableFormKey>& targets,
                                           const StableFormKey& player) {
    Status firstFailure = Status::Ready;

    for (const auto& actor : targets) {
        if (!dirty_.contains(actor)) {
            continue;
        }
        auto queried = backend_.QueryActor(actor, player);
        if (!queried) {
            const auto status = StatusFromError(queried.error());
            StoreUnknown(actor, player, status);
            if (status == Status::Missing) {
                // A Form that the backend cannot resolve is gone for this load.
                // Forget the dirty bit so it cannot poison every later refresh;
                // MarkDirty re-arms it when the adapter sees the actor again.
                dirty_.erase(actor);
            } else {
                dirty_.insert(actor);
            }
            if (firstFailure == Status::Ready) {
                firstFailure = status;
            }
            continue;
        }

        auto snapshot = std::move(*queried);
        snapshot.actor = NormalizeKey(snapshot.actor);
        snapshot.player = NormalizeKey(snapshot.player);
        if (snapshot.actor != actor) {
            StoreUnknown(actor, player, Status::Invalid);
            dirty_.insert(actor);
            if (firstFailure == Status::Ready) {
                firstFailure = Status::Invalid;
            }
            continue;
        }

        const auto validation = ValidateSnapshot(snapshot);
        if (validation != Status::Ready) {
            StoreUnknown(actor, player, validation);
            dirty_.insert(actor);
            if (firstFailure == Status::Ready) {
                firstFailure = validation;
            }
            continue;
        }

        const auto snapshotStatus = snapshot.status;
        cache_[actor] = std::move(snapshot);
        if (snapshotStatus == Status::Ready) {
            dirty_.erase(actor);
        } else {
            // Partial facts are kept in the cache but the target stays dirty so
            // the next refresh can complete it.
            dirty_.insert(actor);
            if (firstFailure == Status::Ready) {
                firstFailure = snapshotStatus;
            }
        }
    }

    return firstFailure;
}
Status GameDataService::Forget(const StableFormKey& actor) {
    if (actor.plugin.empty() || !IsValidFormId(actor.localFormId)) {
        return Status::Invalid;
    }
    const auto key = NormalizeKey(actor);
    cache_.erase(key);
    dirty_.erase(key);
    return Status::Ready;
}
const ActorSnapshot* GameDataService::Find(const StableFormKey& actor) const {
    const auto it = cache_.find(NormalizeKey(actor));
    return it == cache_.end() ? nullptr : &it->second;
}
void GameDataService::Clear() {
    plugins_.clear();
    cache_.clear();
    dirty_.clear();
}
void GameDataService::StoreUnknown(const StableFormKey& actor,
                                   const StableFormKey& player,
                                   Status status) {
    ActorSnapshot snapshot;
    snapshot.status = status;
    snapshot.actor = actor;
    snapshot.player = player;
    cache_[actor] = std::move(snapshot);
}
}