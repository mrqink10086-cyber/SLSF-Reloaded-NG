#include "slsf/ui/FameMenu.h"
#include "slsf/api/PublicApi.h"

#include <algorithm>

namespace slsf::ui {

CommandResult ExecuteFameCommand(api::PublicApi& api, FameCommand command,
    const std::string& location, const std::string& target, const std::string& category,
    int amount, int extra, const std::function<void()>& publish) {
    CommandResult result;
    const auto apply = [&result](const auto& outcome) {
        result.status = outcome.status;
        result.message = outcome.message;
        result.revision = outcome.revision;
    };
    switch (command) {
    case FameCommand::SetFame: result = api.SetFameValue(location, category, amount); break;
    case FameCommand::ClearFame: result = api.ClearFame(location, category); break;
    case FameCommand::ClearAllFame: result = api.ClearAllFame(location); break;
    case FameCommand::ManualSpread: apply(api.ManualSpread(location, target, category, amount)); break;
    case FameCommand::ManualDecay: apply(api.ManualDecay(location, category, (std::min)(amount, extra), (std::max)(amount, extra))); break;
    case FameCommand::ManualGain: apply(api.ManualGain(location, category, (std::min)(amount, extra), (std::max)(amount, extra))); break;
    case FameCommand::SpreadPulse: result = api.SpreadPulse({location}); break;
    }
    if (result.status == Status::Ready && publish) publish();
    return result;
}

FameReadout ReadFameMenu(const api::PublicApi& api, const std::string& location) {
    return api.ReadRegionalFame(location);
}

}
