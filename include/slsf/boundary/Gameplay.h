#pragma once

#include "slsf/api/PublicApi.h"
#include <memory>

namespace slsf::boundary {
class Gameplay {
public:
    Gameplay(api::PublicApi& api, const RegionSnapshot& regions,
             std::map<StableFormKey, RegionId> owners);
    ~Gameplay();
    void Tick();
    std::optional<RegionId> ResolveForm(const StableFormKey& key) const;
    void OnEvent(std::string_view topic, std::string_view payload);
    bool Invoke(const std::string& name, const std::vector<compat::ModEventValue>& arguments,
                compat::ModEventValue& result);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
