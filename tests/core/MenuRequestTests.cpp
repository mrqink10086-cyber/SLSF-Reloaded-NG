#include <catch2/catch_test_macros.hpp>

#include "slsf/config/ConfigurationStore.h"
#include "slsf/ui/Menu.h"

#include <memory>
#include <string>

using slsf::ConfigurationData;
using slsf::ConfigurationSnapshot;
using slsf::ConfigurationUpdateResult;
using slsf::ui::Menu;
using slsf::ui::MenuRequestQueue;

namespace {

struct PortCounters {
    int snapshotCalls = 0;
    int reloadCalls = 0;
    int commitCalls = 0;
    ConfigurationData committed;
    ConfigurationUpdateResult nextReload{
        slsf::ConfigurationUpdateStatus::Applied, slsf::ConfigurationRevision{2}, "reloaded"};
    ConfigurationUpdateResult nextCommit{
        slsf::ConfigurationUpdateStatus::Applied, slsf::ConfigurationRevision{7}, "committed"};

    Menu::Ports Ports() {
        Menu::Ports ports;
        ports.snapshot = [this] {
            ++snapshotCalls;
            auto data = std::make_shared<const ConfigurationData>(ConfigurationData{});
            return ConfigurationSnapshot{slsf::ConfigurationRevision{2}, std::move(data),
                                         nextReload};
        };
        ports.reload = [this] {
            ++reloadCalls;
            return nextReload;
        };
        ports.commit = [this](ConfigurationData edit) {
            ++commitCalls;
            committed = std::move(edit);
            return nextCommit;
        };
        return ports;
    }
};

struct CloseCommitPorts {
    int snapshotCalls = 0;
    int reloadCalls = 0;
    int commitCalls = 0;
    bool commitFails = true;

    Menu::Ports Ports() {
        Menu::Ports ports;
        ports.snapshot = [this] {
            ++snapshotCalls;
            auto data = std::make_shared<const ConfigurationData>(ConfigurationData{});
            return ConfigurationSnapshot{slsf::ConfigurationRevision{1},
                                         std::move(data), ConfigurationUpdateResult{}};
        };
        ports.reload = [this] {
            ++reloadCalls;
            return ConfigurationUpdateResult{
                slsf::ConfigurationUpdateStatus::Applied,
                slsf::ConfigurationRevision{2}, "reloaded from disk"};
        };
        ports.commit = [this](ConfigurationData) {
            ++commitCalls;
            return commitFails
                       ? ConfigurationUpdateResult{
                             slsf::ConfigurationUpdateStatus::IoError,
                             slsf::ConfigurationRevision{1}, "configuration write failed: injected"}
                       : ConfigurationUpdateResult{
                             slsf::ConfigurationUpdateStatus::Applied,
                             slsf::ConfigurationRevision{2}, "configuration updated"};
        };
        return ports;
    }
};

}

TEST_CASE("menu request queue merges duplicate requests posted in the same round",
          "[menu]") {
    MenuRequestQueue queue;
    queue.Post(MenuRequestQueue::Kind::CloseCommit);
    queue.Post(MenuRequestQueue::Kind::CloseCommit);
    queue.Post(MenuRequestQueue::Kind::CloseCommit);
    REQUIRE(queue.Pending(MenuRequestQueue::Kind::CloseCommit));
    REQUIRE_FALSE(queue.Pending(MenuRequestQueue::Kind::Configure));

    int closeCommits = 0;
    int configures = 0;
    queue.Drain([&](MenuRequestQueue::Kind kind) {
        if (kind == MenuRequestQueue::Kind::CloseCommit) {
            ++closeCommits;
        } else {
            ++configures;
        }
    });
    REQUIRE(closeCommits == 1);
    REQUIRE(configures == 0);
    REQUIRE(queue.HandledCount() == 1);
    REQUIRE(queue.DrainCount() == 1);
    REQUIRE_FALSE(queue.Pending(MenuRequestQueue::Kind::CloseCommit));
    REQUIRE_FALSE(queue.Pending(MenuRequestQueue::Kind::Configure));
}

TEST_CASE("menu request queue dispatches each distinct kind once per drain", "[menu]") {
    MenuRequestQueue queue;
    queue.Post(MenuRequestQueue::Kind::CloseCommit);
    queue.Post(MenuRequestQueue::Kind::CloseCommit);
    queue.Post(MenuRequestQueue::Kind::Configure);
    queue.Post(MenuRequestQueue::Kind::Configure);

    int totalHandled = 0;
    queue.Drain([&](MenuRequestQueue::Kind) { ++totalHandled; });
    REQUIRE(totalHandled == 2);
    REQUIRE(queue.HandledCount() == 2);

    int secondRound = 0;
    queue.Drain([&](MenuRequestQueue::Kind) { ++secondRound; });
    REQUIRE(secondRound == 0);
    REQUIRE(queue.DrainCount() == 2);
}

TEST_CASE("menu frame routes configure requests to a single reload", "[menu]") {
    Menu::ResetForTest();
    PortCounters counters;
    Menu::Instance().Bind(counters.Ports());
    Menu::Instance().Queue().Post(MenuRequestQueue::Kind::Configure);

    Menu::Instance().Frame();
    REQUIRE(counters.reloadCalls == 1);
    REQUIRE(counters.commitCalls == 0);
    REQUIRE(Menu::Instance().Queue().HandledCount() == 1);
    REQUIRE(Menu::Instance().Queue().DrainCount() == 1);
    REQUIRE(Menu::Instance().LastUpdateMessage() == "reloaded");
}

TEST_CASE("menu close without edits flushes one read and commits nothing", "[menu]") {
    Menu::ResetForTest();
    CloseCommitPorts ports;
    Menu::Instance().Bind(ports.Ports());
    Menu::Instance().Queue().Post(MenuRequestQueue::Kind::CloseCommit);
    Menu::Instance().Queue().Post(MenuRequestQueue::Kind::CloseCommit);
    Menu::Instance().Queue().Post(MenuRequestQueue::Kind::Configure);

    Menu::Instance().Frame();
    REQUIRE(ports.commitCalls == 0);
    REQUIRE(ports.reloadCalls == 2);
    REQUIRE(ports.snapshotCalls == 1);
    REQUIRE(Menu::Instance().Queue().HandledCount() == 2);
    REQUIRE(Menu::Instance().Queue().DrainCount() == 1);
    REQUIRE(Menu::Instance().LastUpdateMessage() == "reloaded from disk");
}

TEST_CASE("close merge sends an edited draft once and keeps the failure visible",
          "[menu]") {
    Menu::ResetForTest();
    CloseCommitPorts ports;
    Menu::Instance().Bind(ports.Ports());

    Menu::Instance().Queue().Post(MenuRequestQueue::Kind::CloseCommit);
    Menu::Instance().Frame();
    REQUIRE(ports.commitCalls == 0);
    REQUIRE(ports.reloadCalls == 1);

    const bool edited = Menu::Instance().EditDraftForTest(
        [](ConfigurationData& draft) { draft.categories.multipliers[0] = 0.5; });
    REQUIRE(edited);

    Menu::Instance().Queue().Post(MenuRequestQueue::Kind::CloseCommit);
    Menu::Instance().Queue().Post(MenuRequestQueue::Kind::CloseCommit);
    Menu::Instance().Frame();
    REQUIRE(ports.commitCalls == 1);
    REQUIRE(ports.reloadCalls == 1);
    REQUIRE(Menu::Instance().LastUpdateMessage() ==
            "configuration write failed: injected");

    ports.commitFails = false;
    Menu::Instance().Queue().Post(MenuRequestQueue::Kind::CloseCommit);
    Menu::Instance().Frame();
    REQUIRE(ports.commitCalls == 2);
    REQUIRE(ports.reloadCalls == 1);
    REQUIRE(Menu::Instance().LastUpdateMessage() == "configuration updated");

    Menu::Instance().Queue().Post(MenuRequestQueue::Kind::CloseCommit);
    Menu::Instance().Frame();
    REQUIRE(ports.commitCalls == 2);
    REQUIRE(ports.reloadCalls == 2);
}

TEST_CASE("menu frame refreshes the cached snapshot before draining", "[menu]") {
    Menu::ResetForTest();
    PortCounters counters;
    Menu::Instance().Bind(counters.Ports());
    Menu::Instance().Frame();
    REQUIRE(counters.snapshotCalls == 1);
    REQUIRE(counters.reloadCalls == 0);
    Menu::Instance().Frame();
    REQUIRE(counters.snapshotCalls == 2);
}

TEST_CASE("uninstalled menu stays inert when the framework module is missing", "[menu]") {
    Menu::ResetForTest();
    PortCounters counters;
    const bool installed = Menu::Instance().Install(counters.Ports());
    if (!installed) {
        REQUIRE_FALSE(Menu::Instance().Installed());
        Menu::Instance().Queue().Post(MenuRequestQueue::Kind::Configure);
        Menu::Instance().Frame();
        REQUIRE(counters.snapshotCalls == 0);
        REQUIRE(counters.reloadCalls == 0);
        REQUIRE(Menu::Instance().Queue().DrainCount() == 1);
    } else {
        Menu::Instance().Unregister();
        REQUIRE_FALSE(Menu::Instance().Installed());
    }
    REQUIRE(Menu::Instance().Queue().DrainCount() == 1);
}
