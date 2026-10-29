#include "slsf/core/location/RegionRegistry.h"
#include "tests/reference/LocationReference.h"

#include <iostream>

int main() {
    using namespace slsf;
    const StableRegionKey r1{"region.esp", 1};
    const StableRegionKey r2{"region.esp", 2};
    const StableRegionKey r3{"region.esp", 3};

    const std::vector<LocationRecord> records = {
        LocationRecord{{"loc.esp", 1}, {}, r3, "parent"},
        LocationRecord{{"loc.esp", 2}, {"loc.esp", 1}, r2, "child"},
    };
    const auto index = BuildLocationIndex(records);
    if (!index) {
        std::cerr << index.error().message << '\n';
        return 1;
    }

    RegionDefinitions definitions;
    definitions.keys = {r1, r2, r3};
    definitions.ownerRules = {OwnerRule{r1, {r2}}};
    definitions.customOrder = {r2, r3, r1};

    const auto production = BindRegions(*index, definitions);
    if (!production) {
        std::cerr << production.error().message << '\n';
        return 1;
    }
    const auto actual = ResolveRegion(*production, LocationId(1));
    const auto reference = test::ResolveRegionReference(*index, definitions, LocationId(1));
    std::cout << "production owner RegionId=" << (actual ? actual->value : 9999)
              << ", independent reference owner RegionId="
              << (reference ? reference->value : 9999) << '\n';
}
