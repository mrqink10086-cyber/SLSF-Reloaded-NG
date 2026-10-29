#pragma once

#include "slsf/core/Types.h"

#include <string>
#include <vector>

namespace slsf {

// Boundary facts; distinct from core-owned contributions in reputation/ProviderRegistry.h
// so both headers compose in one translation unit.
struct ProviderFlagFact {
    std::string provider;
    FameCategory category{FameCategory::Count};
    bool enabled{};
};

struct ProviderFactState {
    std::vector<ProviderFlagFact> flags;
};

struct ProviderFactTransition {
    std::vector<ProviderFlagFact> flags;
    std::vector<FameCategory> wipeOnce;
};

class IProviderPort {
public:
    virtual ~IProviderPort() = default;
    virtual Result<ProviderFactState> Snapshot() const = 0;
    virtual Result<void> Submit(const ProviderFactTransition& transition) = 0;
};

} // namespace slsf
