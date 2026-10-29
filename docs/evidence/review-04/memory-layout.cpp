#include "slsf/core/gameplay/AnimationClassifier.h"
#include "slsf/core/gameplay/Comments.h"
#include "slsf/core/gameplay/Visibility.h"
#include "slsf/core/gameplay/Witness.h"
#include "slsf/engine/ActorQueries.h"

#include <iostream>

int main() {
    using namespace slsf;
    using namespace slsf::engine;
    using namespace slsf::gameplay;
    std::cout << "ActorSnapshot=" << sizeof(ActorSnapshot) << '\n'
              << "EquipmentSnapshot=" << sizeof(EquipmentSnapshot) << '\n'
              << "EquippedItemSnapshot=" << sizeof(EquippedItemSnapshot) << '\n'
              << "GameDataService=" << sizeof(GameDataService) << '\n'
              << "VisibilityFacts=" << sizeof(VisibilityFacts) << '\n'
              << "AnimationFacts=" << sizeof(AnimationFacts) << '\n'
              << "NpcScanTargetFacts=" << sizeof(NpcScanTargetFacts) << '\n'
              << "NpcScanEntry=" << sizeof(NpcScanEntry) << '\n'
              << "NpcScanner=" << sizeof(NpcScanner) << '\n'
              << "FameGainRollFacts=" << sizeof(FameGainRollFacts) << '\n'
              << "CommentFacts=" << sizeof(CommentFacts) << '\n';
}
