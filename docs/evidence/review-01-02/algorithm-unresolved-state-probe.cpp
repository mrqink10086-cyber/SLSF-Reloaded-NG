#include "slsf/core/persistence/StateCodec.h"
#include "slsf/core/reputation/ReputationState.h"

#include <array>
#include <iostream>
#include <vector>

int main() {
    using namespace slsf;
    const StableRegionKey r1{"region.esp", 1};
    const auto category = static_cast<std::size_t>(FameCategory::Whore);

    State initial;
    initial.accounts.resize(1);
    initial.accountKeys = {r1};
    initial.locationAccounts = {RegionId(0)};
    initial.accounts[0].local[FameCategory::Whore] = 42.0;
    initial.accounts[0].remote[FameCategory::Whore] = 9.0;
    initial.accounts[0].flags[category] = 1;
    initial.accounts[0].protectionUntilDay[category] = 123.5;
    initial.accounts[0].settledAt = 77.25;
    RecomputeAccount(initial.accounts[0]);

    const auto captured = CaptureState(initial);
    if (!captured) {
        std::cerr << captured.error().message << '\n';
        return 1;
    }
    const auto bytes = EncodeState(*captured);
    if (!bytes) {
        std::cerr << bytes.error().message << '\n';
        return 1;
    }
    const auto decoded = DecodeState(*bytes);
    if (!decoded) {
        std::cerr << decoded.error().message << '\n';
        return 1;
    }

    ConfigurationSnapshot configuration;
    State withoutRegion;
    const auto firstRestore = RestoreState(withoutRegion, *decoded, RegionSnapshot{}, configuration);
    if (!firstRestore) {
        std::cerr << firstRestore.error().message << '\n';
        return 1;
    }
    const auto unresolvedCapture = CaptureState(withoutRegion);
    if (!unresolvedCapture) {
        std::cerr << unresolvedCapture.error().message << '\n';
        return 1;
    }
    const auto unresolvedBytes = EncodeState(*unresolvedCapture);
    if (!unresolvedBytes) {
        std::cerr << unresolvedBytes.error().message << '\n';
        return 1;
    }
    const auto unresolvedDecoded = DecodeState(*unresolvedBytes);
    if (!unresolvedDecoded) {
        std::cerr << unresolvedDecoded.error().message << '\n';
        return 1;
    }

    RegionSnapshot withRegion;
    withRegion.regions = {r1};
    State afterReturn;
    const auto secondRestore = RestoreState(afterReturn, *unresolvedDecoded, withRegion, configuration);
    if (!secondRestore) {
        std::cerr << secondRestore.error().message << '\n';
        return 1;
    }

    const auto& restored = afterReturn.accounts[0];
    std::cout << "first restore unresolved=" << firstRestore->unresolvedAccounts
              << ", second restore restored=" << secondRestore->restoredAccounts
              << ", local=" << restored.local[FameCategory::Whore]
              << ", remote=" << restored.remote[FameCategory::Whore]
              << ", flag=" << static_cast<unsigned>(restored.flags[category])
              << ", protection=" << restored.protectionUntilDay[category]
              << ", settledAt=" << restored.settledAt << '\n';
}
