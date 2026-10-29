#include "slsf/core/reputation/ReputationState.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char* argv[]) {
    using namespace slsf;
    for (const std::size_t accountCount : {128u, 4096u, 16384u, 65535u}) {
        const std::size_t writeCount = argc > 1 ? std::stoull(argv[1])
                                              : accountCount == 65535u ? 100u : 300u;
        State state;
        state.accounts.resize(accountCount);
        state.accountKeys.resize(accountCount);
        state.locationAccounts.reserve(accountCount);
        for (std::size_t i = 0; i < accountCount; ++i) {
            state.accountKeys[i] = StableRegionKey{"probe.esp", static_cast<std::uint32_t>(i + 1)};
            state.locationAccounts.emplace_back(static_cast<std::uint32_t>(i));
        }
        RuleContext context;
        ExplicitCommand command;
        command.kind = ExplicitCommandKind::Set;
        command.target = RegionId(0);
        command.category = FameCategory::Whore;
        const auto start = std::chrono::steady_clock::now();
        for (std::size_t write = 0; write < writeCount; ++write) {
            command.value = static_cast<double>(write + 1);
            if (ApplyExplicit(state, command, context).status != Status::Ready) {
                return 1;
            }
        }
        const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - start).count();
        std::cout << accountCount << " accounts, " << writeCount << " Set writes: "
                  << elapsed << " us, " << static_cast<double>(elapsed) / writeCount
                  << " us/write\n";
    }
}
