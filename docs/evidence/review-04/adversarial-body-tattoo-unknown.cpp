#include "slsf/core/gameplay/Visibility.h"

#include <array>
#include <iostream>

int main() {
    using namespace slsf;
    using namespace slsf::gameplay;

    for (const Fact andInstalled : {Fact::Known(true), Fact::Known(false), Fact{}}) {
        VisibilityFacts facts;
        facts.status = Status::Ready;
        facts.mods.slaveTats = Fact::Known(true);
        facts.mods.sls = Fact::Known(true);
        facts.andRanks.installed = andInstalled;
        facts.andRanks.rankIsOne[static_cast<std::size_t>(AndLocation::Chest)] =
            Fact::Known(true);
        for (ItemFacts& item : facts.items) {
            item.present = Fact::Known(false);
        }

        TattooAreaFacts& body = facts.tattoos.body;
        body.installed = Fact::Known(true);
        body.appliedCountKnown = Fact::Known(true);
        body.appliedCount = 1;
        body.slots = {{Fact::Known(true), TattooSubcategory::Chest}};

        const RuleDecision decision = VisibilityRules{}.BodyTattooVisible(facts, 0);
        std::cout << "and_installed=";
        if (andInstalled.IsKnownTrue()) {
            std::cout << "true";
        } else if (andInstalled.IsKnownFalse()) {
            std::cout << "false";
        } else {
            std::cout << "unknown";
        }
        std::cout << " chest_rank=true decision_status="
                  << static_cast<int>(decision.status)
                  << " visible=" << decision.value
                  << " reason=" << decision.reason << '\n';
    }
}
