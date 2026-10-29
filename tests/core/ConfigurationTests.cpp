#include <catch2/catch_test_macros.hpp>

#include "slsf/core/Configuration.h"

#include <memory>

using slsf::ConfigurationData;
using slsf::ConfigurationRevision;
using slsf::ConfigurationSnapshot;
using slsf::ConfigurationUpdateResult;
using slsf::ConfigurationUpdateStatus;

TEST_CASE("configuration data has a pure value default shape", "[configuration]") {
    const ConfigurationData data;
    REQUIRE(data.schemaVersion == slsf::kConfigurationSchemaVersion);
    REQUIRE(data.features.reduceFameAtNight);
    REQUIRE(data.features.fameSpreadRestrictions);
    REQUIRE(data.features.dynamicAnonymity);
    REQUIRE(data.categories.multipliers.size() == slsf::kFameCategoryCount);
    for (const double multiplier : data.categories.multipliers) {
        REQUIRE(multiplier == 1.0);
    }
    REQUIRE(data.relationships.forbiddenByFriend.Empty());
    REQUIRE(data.relationships.forbiddenByLover.Empty());
    REQUIRE(data.parameters.maxFame == 150.0);
    REQUIRE(data.connections.empty());
    REQUIRE(data.menu.selectedRegions.empty());

    ConfigurationData copy = data;
    REQUIRE(copy == data);
    copy.categories.multipliers[0] = 2.0;
    REQUIRE_FALSE(copy == data);

    ConfigurationData forbidden = data;
    REQUIRE(forbidden.relationships.forbiddenByFriend.Set(slsf::FameCategory::Cuck));
    REQUIRE_FALSE(forbidden == data);
}

TEST_CASE("configuration revisions distinguish unset from applied", "[configuration]") {
    const ConfigurationRevision unset;
    REQUIRE_FALSE(static_cast<bool>(unset));
    REQUIRE(unset == ConfigurationRevision{});

    const ConfigurationRevision first{1};
    const ConfigurationRevision second{2};
    REQUIRE(static_cast<bool>(first));
    REQUIRE(first < second);
    REQUIRE(first != second);
}

TEST_CASE("configuration update results keep failure details and success state", "[configuration]") {
    ConfigurationUpdateResult result;
    REQUIRE(result.status == ConfigurationUpdateStatus::Invalid);
    REQUIRE_FALSE(result.Succeeded());

    result.status = ConfigurationUpdateStatus::Applied;
    result.revision = ConfigurationRevision{7};
    result.message = "loaded Data/SKSE/Plugins/SLSFReloadedNG.json";
    REQUIRE(result.Succeeded());
    REQUIRE(result.revision.value == 7);
    REQUIRE_FALSE(result.message.empty());

    result.status = ConfigurationUpdateStatus::Unchanged;
    REQUIRE(result.Succeeded());

    result.status = ConfigurationUpdateStatus::IoError;
    REQUIRE_FALSE(result.Succeeded());

    const ConfigurationUpdateResult copy = result;
    REQUIRE(copy == result);
}

TEST_CASE("configuration snapshots bind immutable data to one revision", "[configuration]") {
    auto data = std::make_shared<ConfigurationData>();
    data->parameters.decayPerDay = 0.5;

    ConfigurationSnapshot snapshot;
    REQUIRE_FALSE(snapshot.HasData());
    REQUIRE(snapshot.revision.value == 0);

    snapshot.revision = ConfigurationRevision{3};
    snapshot.data = data;
    snapshot.lastUpdate.status = ConfigurationUpdateStatus::Applied;
    snapshot.lastUpdate.revision = snapshot.revision;

    REQUIRE(snapshot.HasData());
    REQUIRE(snapshot.data->parameters.decayPerDay == 0.5);
    REQUIRE(snapshot.lastUpdate.Succeeded());
    REQUIRE(snapshot.lastUpdate.revision == snapshot.revision);

    const ConfigurationData& immutable = *snapshot.data;
    REQUIRE(immutable.schemaVersion == slsf::kConfigurationSchemaVersion);
}
