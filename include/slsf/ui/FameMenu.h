#pragma once

#include "slsf/api/PublicApi.h"

#include <functional>
#include <string>

namespace slsf::ui {

enum class FameCommand : std::uint8_t {
    SetFame, ClearFame, ClearAllFame, ManualSpread, ManualDecay, ManualGain, SpreadPulse,
};

using FameReadout = api::RegionalFameView;

CommandResult ExecuteFameCommand(api::PublicApi& api, FameCommand command,
    const std::string& location, const std::string& target, const std::string& category,
    int amount, int extra, const std::function<void()>& publish);
FameReadout ReadFameMenu(const api::PublicApi& api, const std::string& location);

}
