#pragma once

#include "slsf/core/Types.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace slsf {

struct LocationRecord {
    StableFormKey formId;
    StableFormKey parentFormId;
    StableRegionKey stableKey;
    std::string displayName;
};

struct OwnerRule {
    StableRegionKey owner;
    std::vector<StableRegionKey> children;
};

struct ApiAlias {
    std::string alias;
    StableRegionKey target;
};

struct RegionDefinitions {
    std::vector<StableRegionKey> keys;
    std::vector<OwnerRule> ownerRules;
    std::vector<ApiAlias> apiAliases;
    std::vector<StableRegionKey> customOrder;
};

struct EdgeDefinition {
    StableRegionKey from;
    StableRegionKey to;
    double retention{};
};

struct LocationIndexSnapshot {
    std::vector<LocationRecord> records;
    std::vector<LocationId> parents;
    std::vector<std::uint32_t> entry;
    std::vector<std::uint32_t> exit;
};

struct ApiAliasIndexEntry {
    // Lowercase, whitespace-collapsed alias.  The table is sorted by this key
    // so binding pays the normalization cost once per lookup instead of once
    // per registered alias.
    std::string alias;
    ApiLocationIndex apiIndex{};
};

struct RegionSnapshot {
    std::vector<StableRegionKey> regions;
    std::vector<OwnerRule> ownership;
    std::vector<ApiAlias> apiAliases;
    std::vector<StableRegionKey> customOrder;
    std::vector<ApiLocationIndex> regionApiIndex;
    std::vector<RegionId> locationOwners;
    // Precompiled alias lookup, sorted by normalized alias.  This is derived
    // data: apiAliases and customOrder remain the public source of truth.
    std::vector<ApiAliasIndexEntry> aliasIndex;
};

struct DiffusionGraph {
    std::vector<EdgeDefinition> edges;
};

struct SceneParticipant {
    StableFormKey actor;
    std::string role;
};

// A scene snapshot is a value object.  Consumers may copy it, move it into a
// queue, or keep it in a shared_ptr<const SceneSnapshot>; it never stores a
// span or an RE handle.
struct SceneSnapshot {
    Status status{Status::NotReady};
    Epoch epoch;
    std::uint64_t threadId{};
    std::vector<SceneParticipant> participants;
    std::vector<std::string> roles;
    std::vector<std::string> interactionFlags;
    std::vector<std::string> tags;
    std::vector<std::pair<std::string, std::string>> annotations;

    bool IsUsable() const noexcept { return status == Status::Ready; }
};

// PublishedState is immutable-by-convention.  Publish a copy or a
// shared_ptr<const PublishedState>; never expose a mutable reference to the
// live state outside the executor.
//
// The snapshot is intentionally narrow: it carries only what public readers
// may observe (F, public flags), the identity mapping that was in force for
// those values, and the applied configuration revision.  Local/remote rows,
// protection and provider contributions stay in the mutable model and are
// exposed only through Capture().
struct PublishedState {
    Revision revision;
    Epoch epoch;
    std::uint64_t configurationRevision{};
    // RegionId account for each account-major block of 32 fame/flags entries.
    std::vector<RegionId> regionIds;
    // LocationId -> account mapping used by this publication.
    std::vector<RegionId> locationAccounts;
    std::vector<double> fame;
    std::vector<std::uint8_t> flags;
};

using PublishedStatePtr = std::shared_ptr<const PublishedState>;

// The mutable aggregate is defined in reputation/ReputationState.h; Commands.h
// only needs the incomplete type for its entry-point declarations.
struct State;

struct PublicView {
    Revision revision;
    Epoch epoch;
    std::vector<double> fame;
    std::vector<std::uint8_t> flags;
    std::vector<double> recognitionTime;
};

struct QueryRequest {
    enum class Kind : std::uint8_t {
        Fame = 0,
        Flag,
        RecognitionTime,
        Location,
    };

    Kind kind{Kind::Fame};
    LocationId location;
    FameCategory category{FameCategory::Count};
    std::string name;
};

Result<LocationIndexSnapshot> BuildLocationIndex(std::span<const LocationRecord> records);
Result<RegionSnapshot> BindRegions(const LocationIndexSnapshot& index,
                                   const RegionDefinitions& definitions);
bool IsAncestor(const LocationIndexSnapshot& snapshot, LocationId ancestor, LocationId location,
                bool includeSelf);
std::optional<RegionId> ResolveRegion(const RegionSnapshot& snapshot, LocationId location);
Result<ApiLocationIndex> ResolveLegacyLocation(const RegionSnapshot& snapshot,
                                               std::string_view alias);
Result<DiffusionGraph> BuildDiffusionGraph(const RegionSnapshot& regions,
                                          std::vector<EdgeDefinition> edges);

}