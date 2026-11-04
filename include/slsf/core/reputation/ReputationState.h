#pragma once

#include "slsf/core/Commands.h"
#include "slsf/core/Configuration.h"
#include "slsf/core/NumericRow.h"
#include "slsf/core/Snapshots.h"
#include "slsf/core/persistence/PersistentState.h"
#include "slsf/core/reputation/ProviderRegistry.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace slsf {

// Reputation rows are keyed by owning RegionId. Resolve a LocationId to its
// owning region before numeric reads or writes; multiple locations can share a row.
// Callers must resolve LocationId to RegionId first. The index is unchecked;
// validate the account against the published mapping before indexing.
constexpr std::size_t ReputationSlotIndex(RegionId account, FameCategory category) noexcept {
    return static_cast<std::size_t>(account.value) * kFameSlotCount +
           static_cast<std::size_t>(category);
}

// Reputation values for one owning region. The masks are derived from values and flags.
struct AccountState {
    NumericRow local{};
    NumericRow remote{};
    NumericRow fame{};
    std::array<std::uint8_t, kFameSlotCount> flags{};
    std::array<double, kFameSlotCount> protectionUntilDay{};
    double settledAt{};
    // Fully anchored game-time of the last settlement pass used by remote
    // relaxation: an elapsed [remoteSettledAt, now) interval is consumed at
    // most once even when decay only anchors over whole periods (a repeat
    // settle at the same timestamp relaxes zero).
    double remoteSettledAt{};
    std::uint8_t anonymity{};
    std::array<std::uint8_t, 4> auxiliaryState{};

    CategoryMask positiveMask;
    CategoryMask localMask;
    CategoryMask flaggedMask;
};

// Derived from reputation and active configuration; persistence stores only source values.
struct DerivedCategorySets {
    CategoryMask numericPositive;
    CategoryMask numericLocal;
    CategoryMask flagged;
    CategoryMask providerActive;
    CategoryMask relationshipForbidden;
};

struct State {
    // Rows hold owning-region reputation; accountKeys stores each row's stable identity.
    std::vector<AccountState> accounts;
    std::vector<StableRegionKey> accountKeys;
    // LocationId to owning RegionId for reputation. An invalid RegionId means unresolved.
    std::vector<RegionId> locationAccounts;
    std::vector<LocationPersistentState> unresolvedAccounts;
    std::vector<CustomRegionState> customRegions;
    std::vector<NamedProviderState> namedProviders;
    std::vector<RegionSpreadState> spreadChances;
    std::vector<RegionRecognitionState> recognition;
    ProviderState providers;
    DerivedCategorySets derived;
    std::array<std::uint32_t, kFameCategoryCount> positiveRegionCounts{};
    std::array<std::uint32_t, kFameCategoryCount> localRegionCounts{};
    std::array<std::uint32_t, kFameCategoryCount> flaggedRegionCounts{};
    Epoch epoch;
    Revision revision;
    std::uint64_t configurationRevision{};
};

// Recomputes derived fame and category masks from local/remote values and flags.
void RecomputeAccount(AccountState& account) noexcept;

// A null configuration leaves relationshipForbidden empty.
void RebuildDerivedSets(State& state, const ConfigurationData* configuration = nullptr) noexcept;

// Resolves a place to its owning region without changing reputation values.
RegionId AccountForLocation(const State& state, LocationId location) noexcept;

// Binds reputation rows to regions by stable key, preserving state across region changes.
// The mapping is installed only after it is fully built.
Result<void> BindStateToRegions(State& state, const RegionSnapshot& regions);

// Applies wipe categories from a confirmed provider invalidation, clearing their reputation
// values and flags across regions. The registry must establish that no other provider contributes.
Result<void> ApplyProviderTransition(State& state, const ProviderTransition& transition);

// Captures local/remote reputation, flags, protection and settlement times, contributions,
// and custom regions. Derived fame and category sets are rebuilt on restore.
Result<PersistentState> CaptureState(const State& state);

struct RestoreReport {
    std::size_t restoredAccounts{};
    std::size_t unresolvedAccounts{};
    bool legacySettingsIgnored{};
    bool legacyGraphEditsIgnored{};
};

// Restores against the active region mapping and configuration. Missing stable keys remain
// unresolved; v1 settings and graph edits do not override the active cross-save JSON configuration.
Result<RestoreReport> RestoreState(State& state, const PersistentState& persistent,
                                   const RegionSnapshot& regions,
                                   const ConfigurationSnapshot& configuration);

}
