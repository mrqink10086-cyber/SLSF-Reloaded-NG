#include "slsf/api/PublicApi.h"
#include "slsf/config/ConfigurationStore.h"
#include "slsf/core/reputation/GainRules.h"
#include "slsf/io/JsonTransfer.h"
#include "slsf/runtime/Lifecycle.h"
#include <iostream>
#include <memory>
#include <string>

int main() {
    slsf::RegionSnapshot regions;
    const slsf::StableRegionKey key{"review.esp", 1};
    regions.regions = {key};
    regions.customOrder = {key};
    regions.regionApiIndex = {slsf::ApiLocationIndex{0}};
    regions.aliasIndex = {{"whiterun", slsf::ApiLocationIndex{0}}};
    auto defaults = slsf::config::ConfigurationStore::LegacyDefaults();
    auto store = slsf::config::ConfigurationStore::Open(
        std::make_unique<slsf::config::MemoryConfigurationFile>(
            slsf::io::JsonValue::Write(slsf::config::ConfigurationStore::Encode(defaults))));
    std::size_t notifications = 0;
    store->BindNotify([&](const slsf::ConfigurationSnapshot&) { ++notifications; });
    slsf::Lifecycle lifecycle({&regions}, *store);
    if (!lifecycle.PrepareNewGame({}) || !lifecycle.ActivatePrepared()) {
        std::cout << "activation_failed=" << lifecycle.Status().message << '\n';
        return 1;
    }
    const auto initialRevision = lifecycle.Status().configurationRevision.value;
    auto edit = *store->Current().data;
    edit.features.reduceFameAtNight = !edit.features.reduceFameAtNight;
    const auto update = store->ApplyEdit(edit);
    std::cout << "config_update_success=" << update.Succeeded()
              << " notifications=" << notifications
              << " initial_revision=" << initialRevision
              << " store_revision=" << store->Current().revision.value
              << " status_revision=" << lifecycle.Status().configurationRevision.value
              << " published_revision=" << lifecycle.Executor().Snapshot()->configurationRevision << '\n';
    slsf::api::PublicApi api(lifecycle.Executor(), regions);
    const auto set = api.SetFameValue("whiterun", "Whore", 20);
    if (set.status != slsf::Status::Ready) {
        std::cout << "set_failed=" << set.message << '\n';
        return 2;
    }
    const auto parsed = slsf::io::JsonValue::Parse(
        "{\"int\":{\"whiterun.whore\":12,\"unknown.whore\":7}}", 4096);
    const auto imported = slsf::io::ExtractLegacyImport(parsed, slsf::api::PublicApi::FlagNames());
    if (!imported) return 3;
    const auto before = api.GetFameValue("whiterun", "Whore").value_or(-1);
    std::size_t applied = 0, rejected = 0;
    for (const auto& entry : imported->fame) {
        const auto result = api.SetFameValue(entry.location,
            std::string(slsf::ToString(entry.category)), entry.value);
        if (result.status == slsf::Status::Ready) ++applied;
        else ++rejected;
    }
    std::cout << "import_before=" << before << " applied=" << applied
              << " rejected=" << rejected
              << " import_after=" << api.GetFameValue("whiterun", "Whore").value_or(-1) << '\n';
    return 0;
}
