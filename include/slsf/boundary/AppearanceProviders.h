#pragma once

#include "slsf/integrations/RaceMenu.h"
#include "slsf/integrations/SlaveTats.h"

namespace slsf::boundary {

void SubscribeAppearanceProviders();
void InitializeAppearanceProviders();
[[nodiscard]] integrations::RaceMenuSnapshot QueryRaceMenu(const Epoch& epoch);
[[nodiscard]] integrations::SlaveTatsSnapshot QuerySlaveTats(RE::Actor* actor, const Epoch& epoch);

}
