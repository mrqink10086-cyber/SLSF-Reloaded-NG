#include "slsf/integrations/CapabilityRegistry.h"
#include <iostream>

int main() {
    using namespace slsf::integrations;
    int calls = 0;
    CapabilityRegistry registry([&](const CapabilityRequirement&) {
        ++calls;
        CapabilityProbeResult result;
        result.loaded = true;
        result.pending = true;
        if (calls > 1) { result.version = 1; result.apiVersion = 1; }
        return result;
    });
    CapabilityRequirement requirement;
    requirement.id = "probe";
    requirement.displayName = "probe";
    requirement.business = "probe";
    requirement.requiredApiVersion = 1;
    registry.Require(requirement);
    const auto first = registry.Ensure("probe");
    const auto second = registry.Ensure("probe");
    std::cout << "first=" << CapabilityStateName(first)
              << " second=" << CapabilityStateName(second)
              << " calls=" << calls << "\n";
}