#include "slsf/core/gameplay/Visibility.h"

#include <iostream>

int main() {
    using namespace slsf;
    using namespace slsf::gameplay;

    VisibilityFacts facts;
    facts.status = Status::Ready;
    facts.anonymous = Fact::Known(false);
    facts.mods.deviousDevices = Fact::Known(true);
    facts.worn.deviousHeavyBondage = Fact::Known(true);
    facts.worn.deviousLockable = Fact::Known(false);
    facts.worn.devious.hood = Fact::Known(true);

    const BondageResult result = VisibilityRules{}.EvaluateBondage(facts);
    std::cout << "case=Ready,DD=true,Heavy=true,Lockable=false,Hood=true,Anonymous=false\n";
    std::cout << "native_status=" << static_cast<int>(result.wearingUnhideable.status)
              << " native_wearingUnhideable=" << result.wearingUnhideable.value << '\n';
    std::cout << "native_status=" << static_cast<int>(result.visiblyBound.status)
              << " native_visiblyBound=" << result.visiblyBound.value << '\n';

    // Papyrus resets IsWearingUnhideable before the outer Heavy/Lockable branch.
    // The device scan is inside the Lockable ElseIf, so Heavy skips it and leaves
    // the reset value false. The heavy branch itself sets IsVisiblyBound true.
    constexpr bool legacyWearingUnhideable = false;
    constexpr bool legacyVisiblyBound = true;
    std::cout << "legacy_wearingUnhideable=" << legacyWearingUnhideable
              << " legacy_visiblyBound=" << legacyVisiblyBound << '\n';

    return result.wearingUnhideable.status == Status::Ready &&
                   !result.wearingUnhideable.value && result.visiblyBound.IsReady() &&
                   result.visiblyBound.value
               ? 0
               : 1;
}
