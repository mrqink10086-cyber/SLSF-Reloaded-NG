#include "slsf/integrations/CapabilityRegistry.h"
#include <cstdio>

int main() {
    using namespace slsf::integrations;
    CapabilityRegistry* current = nullptr;
    CapabilityRegistry registry([&](const CapabilityRequirement& requirement) {
        std::puts("provider entered; synchronously reentering the same capability");
        std::fflush(stdout);
        current->Ensure(requirement.id);
        return CapabilityProbeResult{};
    });
    current = &registry;
    CapabilityRequirement requirement;
    requirement.id = "reentry";
    requirement.displayName = "reentry";
    requirement.business = "audit";
    if (!registry.Require(requirement)) return 2;
    registry.Ensure(requirement.id);
    std::puts("unexpectedly returned from the recursive query");
    return 3;
}
