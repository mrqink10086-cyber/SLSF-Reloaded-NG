#include "slsf/core/Snapshots.h"
#include "slsf/core/Configuration.h"
#include "slsf/core/reputation/ReputationState.h"
#include <cstdio>
#include <memory>

int probe(bool canonical) {
    const unsigned oldForm = 0xFE012ABCu;
    const unsigned newForm = 0xFE037ABCu;
    const unsigned mask = canonical ? 0xFFFu : 0x00FFFFFFu;
    slsf::StableRegionKey oldKey{"audit.esl", oldForm & mask};
    slsf::StableRegionKey newKey{"audit.esl", newForm & mask};
    slsf::RegionSnapshot regions;
    regions.regions.push_back(newKey);
    regions.locationOwners.push_back(slsf::RegionId{0});
    slsf::PersistentState persistent;
    persistent.codecVersion = slsf::kPersistentStateCodecVersion;
    slsf::LocationPersistentState saved;
    saved.key = oldKey;
    saved.local[0] = 37.5;
    saved.remote[0] = 37.5;
    persistent.locations.push_back(saved);
    slsf::ConfigurationSnapshot configuration;
    configuration.revision = slsf::ConfigurationRevision{1};
    configuration.data = std::make_shared<slsf::ConfigurationData>();
    slsf::State state;
    auto restored = slsf::RestoreState(state, persistent, regions, configuration);
    if (!restored) { std::puts("unexpected restore error"); return 2; }
    std::printf("canonical=%d oldKey=%06X newKey=%06X restored=%zu unresolved=%zu currentLocal=%.1f\n",
        canonical, oldKey.localFormId, newKey.localFormId,
        restored->restoredAccounts, restored->unresolvedAccounts, state.accounts[0].local[0]);
    const bool expected = canonical
        ? restored->restoredAccounts == 1 && restored->unresolvedAccounts == 0 && state.accounts[0].local[0] == 37.5
        : restored->restoredAccounts == 0 && restored->unresolvedAccounts == 1 && state.accounts[0].local[0] == 0.0;
    return expected ? 0 : 3;
}

int main() { return probe(false) | probe(true); }
