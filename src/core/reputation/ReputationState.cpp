#include "slsf/core/reputation/ReputationState.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <utility>

namespace slsf {
namespace {

Revision NextRevision(const State& state) noexcept {
    return Revision{state.revision.value + 1};
}

void ClearCategory(AccountState& account, FameCategory category) noexcept {
    account.local[category] = 0.0;
    account.remote[category] = 0.0;
    account.fame[category] = 0.0;
    account.protectionUntilDay[static_cast<std::size_t>(category)] = 0.0;
}

void ClearPersistedCategory(LocationPersistentState& saved, FameCategory category) noexcept {
    const auto index = static_cast<std::size_t>(category);
    saved.local[index] = 0.0;
    saved.remote[index] = 0.0;
    saved.protectionUntilDay[index] = 0.0;
}

std::uint64_t StableProviderId(std::string_view name) noexcept {
    std::uint64_t hash = 14695981039346656037ull;
    for (const char ch : name) {
        hash ^= static_cast<std::uint64_t>(static_cast<unsigned char>(ch));
        hash *= 1099511628211ull;
    }
    return hash == 0 ? 1 : hash;
}

std::optional<std::size_t> FindRegionIndex(const std::vector<StableRegionKey>& regions,
                                           const StableRegionKey& key) noexcept {
    const auto it = std::lower_bound(regions.begin(), regions.end(), key);
    if (it == regions.end() || *it != key) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(it - regions.begin());
}

LocationPersistentState CaptureRegion(const StableRegionKey& key, const AccountState& region) {
    LocationPersistentState saved;
    saved.key = key;
    saved.local = region.local.values;
    saved.remote = region.remote.values;
    saved.protectionUntilDay = region.protectionUntilDay;
    saved.flags = region.flags;
    saved.settledAt = region.settledAt;
    saved.remoteSettledAt = region.remoteSettledAt;
    saved.anonymity = region.anonymity;
    saved.auxiliaryState = region.auxiliaryState;
    return saved;
}

AccountState RestoreRegion(const LocationPersistentState& saved) {
    AccountState region;
    region.local.values = saved.local;
    region.remote.values = saved.remote;
    region.protectionUntilDay = saved.protectionUntilDay;
    region.flags = saved.flags;
    region.settledAt = saved.settledAt;
    region.remoteSettledAt = saved.remoteSettledAt;
    region.anonymity = saved.anonymity;
    region.auxiliaryState = saved.auxiliaryState;
    return region;
}

}

void RecomputeAccount(AccountState& account) noexcept {
    account.positiveMask.Clear();
    account.localMask.Clear();
    account.flaggedMask.Clear();
    for (std::size_t i = 0; i < kFameCategoryCount; ++i) {
        const auto category = static_cast<FameCategory>(i);
        account.fame[i] = std::max(account.local[i], account.remote[i]);
        if (account.fame[i] > 0.0) {
            account.positiveMask.Set(category);
        }
        if (account.local[i] > 0.0) {
            account.localMask.Set(category);
        }
        if (account.flags[i] != 0) {
            account.flaggedMask.Set(category);
        }
    }
    for (std::size_t i = kFameCategoryCount; i < kFameSlotCount; ++i) {
        account.fame[i] = 0.0;
    }
}

void RefreshNumericSets(State& state) noexcept {
    state.derived.numericPositive.Clear();
    state.derived.numericLocal.Clear();
    state.derived.flagged.Clear();
    state.positiveRegionCounts.fill(0);
    state.localRegionCounts.fill(0);
    state.flaggedRegionCounts.fill(0);
    for (auto& account : state.accounts) {
        RecomputeAccount(account);
        state.derived.numericPositive |= account.positiveMask;
        state.derived.numericLocal |= account.localMask;
        state.derived.flagged |= account.flaggedMask;
        account.positiveMask.ForEach([&](FameCategory category) {
            ++state.positiveRegionCounts[static_cast<std::size_t>(category)];
        });
        account.localMask.ForEach([&](FameCategory category) {
            ++state.localRegionCounts[static_cast<std::size_t>(category)];
        });
        account.flaggedMask.ForEach([&](FameCategory category) {
            ++state.flaggedRegionCounts[static_cast<std::size_t>(category)];
        });
    }
}

void UpdateRegionCounts(CategoryMask before, CategoryMask after,
                        std::array<std::uint32_t, kFameCategoryCount>& counts,
                        CategoryMask& combined) noexcept {
    (after & ~before).ForEach([&](FameCategory category) {
        ++counts[static_cast<std::size_t>(category)];
        combined.Set(category);
    });
    (before & ~after).ForEach([&](FameCategory category) {
        if (--counts[static_cast<std::size_t>(category)] == 0) {
            combined.Reset(category);
        }
    });
}

void RebuildDerivedSets(State& state, const ConfigurationData* configuration) noexcept {
    state.derived = DerivedCategorySets{};
    RefreshNumericSets(state);
    state.derived.providerActive = state.providers.ActiveCategories();
    if (configuration != nullptr) {
        state.derived.relationshipForbidden =
            configuration->relationships.forbiddenByFriend |
            configuration->relationships.forbiddenByLover;
    }
}

RegionId AccountForLocation(const State& state, LocationId location) noexcept {
    if (!location || location.value >= state.locationAccounts.size()) {
        return RegionId{};
    }
    const auto account = state.locationAccounts[location.value];
    if (!account || account.value >= state.accounts.size()) {
        return RegionId{};
    }
    return account;
}

CommandResult ApplyExplicit(State& state, const ExplicitCommand& command,
                            const RuleContext& context) {
    static_cast<void>(context);

    if (command.kind == ExplicitCommandKind::ClearAll) {
        for (auto& account : state.accounts) {
            account.local = NumericRow{};
            account.remote = NumericRow{};
            account.protectionUntilDay.fill(0.0);
        }
        for (auto& saved : state.unresolvedAccounts) {
            saved.local.fill(0.0);
            saved.remote.fill(0.0);
            saved.protectionUntilDay.fill(0.0);
        }
        RefreshNumericSets(state);
        state.revision = NextRevision(state);
        return CommandResult{Status::Ready, state.revision, {}};
    }

    if (!IsValidFameCategory(command.category)) {
        return CommandResult{Status::Invalid, state.revision, "invalid fame category"};
    }

    if (command.kind == ExplicitCommandKind::Wipe) {
        for (auto& target : state.accounts) {
            ClearCategory(target, command.category);
        }
        for (auto& saved : state.unresolvedAccounts) {
            ClearPersistedCategory(saved, command.category);
        }
        RefreshNumericSets(state);
        state.revision = NextRevision(state);
        return CommandResult{Status::Ready, state.revision, {}};
    }

    const auto accountId = command.target;
    if (!accountId || accountId.value >= state.accounts.size()) {
        return CommandResult{Status::Invalid, state.revision, "invalid reputation region"};
    }
    auto& account = state.accounts[accountId.value];
    const auto oldPositive = account.positiveMask;
    const auto oldLocal = account.localMask;
    const auto oldFlagged = account.flaggedMask;

    switch (command.kind) {
    case ExplicitCommandKind::Set:
        if (!std::isfinite(command.value)) {
            return CommandResult{Status::Invalid, state.revision, "non-finite fame value"};
        }
        account.local[command.category] = command.value;
        account.remote[command.category] =
            std::min(account.remote[command.category], command.value);
        break;
    case ExplicitCommandKind::Clear:
        ClearCategory(account, command.category);
        break;
    case ExplicitCommandKind::Wipe:
        break;
    case ExplicitCommandKind::SetFlag:
        account.flags[static_cast<std::size_t>(command.category)] = 1;
        break;
    case ExplicitCommandKind::ClearFlag:
        account.flags[static_cast<std::size_t>(command.category)] = 0;
        break;
    case ExplicitCommandKind::ClearAll:
        break;
    default:
        return CommandResult{Status::Invalid, state.revision, "invalid explicit command kind"};
    }

    RecomputeAccount(account);
    UpdateRegionCounts(oldPositive, account.positiveMask,
                       state.positiveRegionCounts, state.derived.numericPositive);
    UpdateRegionCounts(oldLocal, account.localMask,
                       state.localRegionCounts, state.derived.numericLocal);
    UpdateRegionCounts(oldFlagged, account.flaggedMask,
                       state.flaggedRegionCounts, state.derived.flagged);
    state.revision = NextRevision(state);
    return CommandResult{Status::Ready, state.revision, {}};
}

Result<void> ApplyProviderTransition(State& state, const ProviderTransition& transition) {
    if (transition.status == Status::Invalid) {
        return Fail(Error::Code::InvalidArgument, "invalid provider transition");
    }
    for (const auto category : transition.wipeCategories) {
        if (!IsValidFameCategory(category)) {
            return Fail(Error::Code::InvalidArgument, "provider wipe names an invalid category");
        }
    }
    for (const auto category : transition.wipeCategories) {
        for (auto& account : state.accounts) {
            ClearCategory(account, category);
            account.flags[static_cast<std::size_t>(category)] = 0;
        }
        for (auto& saved : state.unresolvedAccounts) {
            ClearPersistedCategory(saved, category);
            saved.flags[static_cast<std::size_t>(category)] = 0;
        }
    }
    if (!transition.wipeCategories.empty()) {
        RefreshNumericSets(state);
    }
    state.derived.providerActive = state.providers.ActiveCategories();
    if (!transition.wipeCategories.empty()) {
        state.revision = NextRevision(state);
    }
    return {};
}

Result<void> BindStateToRegions(State& state, const RegionSnapshot& regions) {
    if (regions.regions.size() > std::numeric_limits<std::uint32_t>::max()) {
        return Fail(Error::Code::OutOfRange, "too many region accounts");
    }
    if (!std::is_sorted(regions.regions.begin(), regions.regions.end())) {
        return Fail(Error::Code::InvalidArgument, "region snapshot is not sorted");
    }
    for (const auto owner : regions.locationOwners) {
        if (owner && owner.value >= regions.regions.size()) {
            return Fail(Error::Code::InvalidArgument, "location owner is outside the region table");
        }
    }

    std::map<StableRegionKey, std::size_t> previousAccounts;
    if (!state.accounts.empty()) {
        if (state.accountKeys.size() != state.accounts.size()) {
            return Fail(Error::Code::InvalidState, "account rows have no stable identity");
        }
        for (std::size_t i = 0; i < state.accountKeys.size(); ++i) {
            if (state.accountKeys[i].plugin.empty()) {
                return Fail(Error::Code::InvalidState, "account has an empty stable key");
            }
            if (!previousAccounts.emplace(state.accountKeys[i], i).second) {
                return Fail(Error::Code::InvalidState, "duplicate account stable key");
            }
        }
    }

    std::map<StableRegionKey, const LocationPersistentState*> savedRegions;
    for (const auto& saved : state.unresolvedAccounts) {
        if (saved.key.plugin.empty() ||
            !savedRegions.emplace(saved.key, &saved).second) {
            return Fail(Error::Code::InvalidState, "invalid unresolved account identity");
        }
    }
    std::vector<AccountState> accounts(regions.regions.size());
    std::set<StableRegionKey> seen;
    for (std::size_t i = 0; i < regions.regions.size(); ++i) {
        if (regions.regions[i].plugin.empty()) {
            return Fail(Error::Code::InvalidArgument, "region key has no plugin");
        }
        if (!seen.emplace(regions.regions[i]).second) {
            return Fail(Error::Code::InvalidArgument, "duplicate region key");
        }
        const auto previous = previousAccounts.find(regions.regions[i]);
        if (previous != previousAccounts.end()) {
            accounts[i] = state.accounts[previous->second];
        } else if (const auto saved = savedRegions.find(regions.regions[i]);
                   saved != savedRegions.end()) {
            accounts[i] = RestoreRegion(*saved->second);
        }
        RecomputeAccount(accounts[i]);
    }

    std::vector<LocationPersistentState> unresolved;
    for (const auto& saved : state.unresolvedAccounts) {
        if (!seen.contains(saved.key)) {
            unresolved.push_back(saved);
        }
    }
    for (std::size_t i = 0; i < state.accounts.size(); ++i) {
        if (!seen.contains(state.accountKeys[i])) {
            unresolved.push_back(CaptureRegion(state.accountKeys[i], state.accounts[i]));
        }
    }

    // Build every allocating copy before the first move so a failed allocation
    // leaves the previously published mapping intact.
    std::vector<StableRegionKey> nextKeys = regions.regions;
    std::vector<RegionId> nextLocations = regions.locationOwners;
    state.unresolvedAccounts = std::move(unresolved);
    state.accounts = std::move(accounts);
    state.accountKeys = std::move(nextKeys);
    state.locationAccounts = std::move(nextLocations);
    RefreshNumericSets(state);
    state.revision = NextRevision(state);
    return {};
}

Result<PersistentState> CaptureState(const State& state) {
    if (state.accounts.size() != state.accountKeys.size()) {
        return Fail(Error::Code::InvalidState, "account rows have no stable identity");
    }
    for (const auto owner : state.locationAccounts) {
        if (owner && owner.value >= state.accounts.size()) {
            return Fail(Error::Code::InvalidState, "location owner is outside the account table");
        }
    }

    PersistentState capture;
    capture.codecVersion = kPersistentStateCodecVersion;
    capture.locations.reserve(state.accounts.size() + state.unresolvedAccounts.size());
    for (std::size_t i = 0; i < state.accounts.size(); ++i) {
        const auto& account = state.accounts[i];
        const auto& key = state.accountKeys[i];
        if (key.plugin.empty()) {
            return Fail(Error::Code::InvalidState, "account has an empty stable key");
        }
        capture.locations.push_back(CaptureRegion(key, account));
    }
    capture.locations.insert(capture.locations.end(), state.unresolvedAccounts.begin(),
                             state.unresolvedAccounts.end());
    std::sort(capture.locations.begin(), capture.locations.end(),
              [](const auto& left, const auto& right) { return left.key < right.key; });

    capture.contributions.reserve(state.providers.contributions.size());
    for (const auto& contribution : state.providers.contributions) {
        capture.contributions.push_back(
            ProviderContributionState{contribution.registrationId,
                                      contribution.categories});
    }
    capture.customRegions = state.customRegions;
    return capture;
}

Result<RestoreReport> RestoreState(State& state, const PersistentState& persistent,
                                   const RegionSnapshot& regions,
                                   const ConfigurationSnapshot& configuration) {
    if (persistent.schemaVersion != kPersistentStateSchemaVersion) {
        return Fail(Error::Code::Unsupported, "unsupported persistent state schema");
    }
    if (persistent.codecVersion != kPersistentStateCodecVersion &&
        persistent.codecVersion != kLegacyPersistentStateCodecVersion) {
        return Fail(Error::Code::Unsupported, "unsupported persistent state codec version");
    }
    if (persistent.contributions.size() > kMaxProviderRegistrations) {
        return Fail(Error::Code::OutOfRange, "too many persisted provider registrations");
    }
    if (regions.regions.size() > std::numeric_limits<std::uint32_t>::max()) {
        return Fail(Error::Code::OutOfRange, "too many region accounts");
    }
    if (!std::is_sorted(regions.regions.begin(), regions.regions.end())) {
        return Fail(Error::Code::InvalidArgument, "region snapshot is not sorted");
    }
    for (const auto owner : regions.locationOwners) {
        if (owner && owner.value >= regions.regions.size()) {
            return Fail(Error::Code::InvalidArgument, "location owner is outside the region table");
        }
    }

    std::set<StableRegionKey> regionKeys;
    for (const auto& key : regions.regions) {
        if (key.plugin.empty()) {
            return Fail(Error::Code::InvalidArgument, "region key has no plugin");
        }
        if (!regionKeys.emplace(key).second) {
            return Fail(Error::Code::InvalidArgument, "duplicate region key");
        }
    }

    std::set<StableRegionKey> customRegionKeys;
    for (const auto& region : persistent.customRegions) {
        if (!customRegionKeys.emplace(region.key).second) {
            return Fail(Error::Code::InvalidArgument, "duplicate custom region key");
        }
    }

    State restored;
    restored.accounts.resize(regions.regions.size());
    restored.accountKeys = regions.regions;
    restored.locationAccounts = regions.locationOwners;
    restored.customRegions = persistent.customRegions;

    RestoreReport report;
    std::set<StableRegionKey> seen;
    for (const auto& location : persistent.locations) {
        if (location.key.plugin.empty()) {
            return Fail(Error::Code::InvalidArgument, "persisted account key has no plugin");
        }
        if (!seen.emplace(location.key).second) {
            return Fail(Error::Code::InvalidArgument, "duplicate persisted account key");
        }
        const auto index = FindRegionIndex(regions.regions, location.key);
        if (!index) {
            restored.unresolvedAccounts.push_back(location);
            ++report.unresolvedAccounts;
            continue;
        }
        auto& account = restored.accounts[*index];
        account = RestoreRegion(location);
        RecomputeAccount(account);
        ++report.restoredAccounts;
    }

    if (persistent.codecVersion >= 2) {
        restored.providers.contributions.reserve(persistent.contributions.size());
        for (const auto& contribution : persistent.contributions) {
            ProviderContribution runtime;
            runtime.registrationId = contribution.registrationId;
            runtime.categories = contribution.categories;
            restored.providers.contributions.push_back(std::move(runtime));
        }
    } else {
        std::map<std::string, std::size_t, std::less<>> byName;
        for (const auto& fact : persistent.providerFlags) {
            if (!fact.enabled || !IsValidFameCategory(fact.category)) {
                continue;
            }
            auto it = byName.find(fact.provider);
            if (it == byName.end()) {
                if (restored.providers.contributions.size() >= kMaxProviderRegistrations) {
                    return Fail(Error::Code::OutOfRange,
                                "too many restored provider registrations");
                }
                ProviderContribution contribution;
                contribution.registrationId = StableProviderId(fact.provider);
                it = byName.emplace(fact.provider, restored.providers.contributions.size()).first;
                restored.providers.contributions.push_back(std::move(contribution));
            }
            restored.providers.contributions[it->second].categories.Set(fact.category);
        }
    }
    if (restored.providers.contributions.size() > kMaxProviderRegistrations) {
        return Fail(Error::Code::OutOfRange, "too many restored provider registrations");
    }
    restored.providers.RebuildActiveCounts();

    restored.configurationRevision = configuration.revision.value;
    RebuildDerivedSets(restored, configuration.data.get());
    restored.revision = Revision{};
    restored.epoch = Epoch{};

    report.legacySettingsIgnored =
        persistent.codecVersion < 2 && !persistent.settings.empty();
    report.legacyGraphEditsIgnored =
        persistent.codecVersion < 2 && !persistent.graphEdits.empty();

    state = std::move(restored);
    return report;
}

PublishedState MakePublishedState(const State& state) {
    PublishedState output;
    output.revision = state.revision;
    output.epoch = state.epoch;
    output.configurationRevision = state.configurationRevision;
    output.locationAccounts = state.locationAccounts;
    for (auto& owner : output.locationAccounts) {
        if (owner && owner.value >= state.accounts.size()) {
            owner = RegionId{};
        }
    }
    output.regionIds.reserve(state.accounts.size());
    output.fame.reserve(state.accounts.size() * kFameSlotCount);
    output.flags.reserve(state.accounts.size() * kFameSlotCount);
    for (std::size_t i = 0; i < state.accounts.size(); ++i) {
        output.regionIds.push_back(RegionId(static_cast<std::uint32_t>(i)));
        const auto& account = state.accounts[i];
        output.fame.insert(output.fame.end(), account.fame.values.begin(),
                           account.fame.values.end());
        output.flags.insert(output.flags.end(), account.flags.begin(), account.flags.end());
    }
    return output;
}
Result<PublicView> Query(const PublishedState& state, const QueryRequest& request) {
    if (!request.location || request.location.value >= state.locationAccounts.size()) {
        return std::unexpected(Error{Error::Code::NotFound, "location not published"});
    }
    const RegionId region = state.locationAccounts[request.location.value];
    if (!region) {
        return std::unexpected(Error{Error::Code::NotFound, "location has no reputation region"});
    }
    std::size_t row = region.value;
    if (row >= state.regionIds.size() || state.regionIds[row] != region) {
        const auto account = std::find(state.regionIds.begin(), state.regionIds.end(), region);
        if (account == state.regionIds.end()) {
            return std::unexpected(Error{Error::Code::NotFound, "reputation region not published"});
        }
        row = static_cast<std::size_t>(std::distance(state.regionIds.begin(), account));
    }
    const std::size_t base = row * kFameSlotCount;

    if ((request.kind == QueryRequest::Kind::Fame || request.kind == QueryRequest::Kind::Flag)
        && static_cast<std::uint32_t>(request.category) >= kFameCategoryCount) {
        return Fail(Error::Code::InvalidArgument, "invalid fame category");
    }

    switch (request.kind) {
    case QueryRequest::Kind::Fame:
        [[fallthrough]];
    case QueryRequest::Kind::Flag: {
        const auto slot = static_cast<std::size_t>(request.category);
        PublicView view;
        view.revision = state.revision;
        view.epoch = state.epoch;
        if (base + slot >= state.fame.size() || base + slot >= state.flags.size()) {
            return std::unexpected(Error{Error::Code::Internal, "published row is truncated"});
        }
        if (request.kind == QueryRequest::Kind::Fame) {
            view.fame.push_back(state.fame.at(base + slot));
        } else {
            view.flags.push_back(state.flags.at(base + slot));
        }
        return view;
    }
    case QueryRequest::Kind::RecognitionTime:
        return std::unexpected(Error{Error::Code::NotFound,
                                     "recognition time is not part of the published projection"});
    case QueryRequest::Kind::Location: {
        PublicView view;
        view.revision = state.revision;
        view.epoch = state.epoch;
        return view;
    }
    }
    return std::unexpected(Error{Error::Code::Internal, "unhandled public query kind"});
}

}
