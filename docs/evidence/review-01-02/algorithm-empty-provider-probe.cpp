#include "slsf/core/persistence/StateCodec.h"
#include "slsf/core/reputation/ReputationState.h"

#include <iostream>
#include <vector>

int main() {
    using namespace slsf;
    State state;
    for (std::uint64_t id = 1; id <= kMaxProviderRegistrations; ++id) {
        const auto transition = UpdateProvider(
            state.providers, ProviderCommand{id, ProviderPhase::Enabled, {}});
        if (transition.status != Status::Ready) {
            std::cout << "unexpected rejection at id " << id << '\n';
            return 1;
        }
    }
    const auto next = UpdateProvider(
        state.providers, ProviderCommand{256, ProviderPhase::Enabled,
                                         {FameCategory::Whore}});
    const auto captured = CaptureState(state);
    if (!captured) {
        std::cout << "capture failed: " << captured.error().message << '\n';
        return 1;
    }
    const auto encoded = EncodeState(*captured);
    std::cout << "empty Enabled rows=" << state.providers.contributions.size()
              << ", next real provider status=" << static_cast<int>(next.status)
              << ", capture=ok, encode=" << (encoded ? "ok" : "failed") << '\n';
    if (!encoded) {
        std::cout << "encode error: " << encoded.error().message << '\n';
    }
}
