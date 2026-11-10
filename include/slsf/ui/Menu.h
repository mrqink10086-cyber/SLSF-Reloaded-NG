#pragma once

#include "slsf/config/ConfigurationStore.h"
#include "slsf/core/Snapshots.h"
#include "slsf/ui/FameMenu.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace slsf::ui {

class MenuRequestQueue {
public:
    enum class Kind : std::uint8_t { Configure, CloseCommit };

    void Post(Kind kind) const { pending_[static_cast<std::size_t>(kind)] = true; }
    void Drain(const std::function<void(Kind)>& handler);
    [[nodiscard]] bool Pending(Kind kind) const noexcept {
        return pending_[static_cast<std::size_t>(kind)];
    }
    [[nodiscard]] std::size_t DrainCount() const noexcept { return drainCount_; }
    [[nodiscard]] std::size_t HandledCount() const noexcept { return handledCount_; }

private:
    static constexpr std::size_t kKindCount = 2;

    mutable bool pending_[kKindCount]{false, false};
    std::size_t drainCount_{0};
    std::size_t handledCount_{0};
};

class Menu final {
public:
    using FameCommand = ui::FameCommand;

    struct Ports {
        std::function<ConfigurationSnapshot()> snapshot;
        std::function<ConfigurationUpdateResult()> reload;
        std::function<ConfigurationUpdateResult(ConfigurationData)> commit;
        std::function<const RegionSnapshot*()> regions;
        std::function<std::string()> status;
        std::function<FameReadout(const std::string& location)> fameReadout;
        std::function<std::string(FameCommand command, const std::string& location,
                                  const std::string& target, const std::string& category,
                                  int amount, int extra)>
            fameCommand;
        std::function<std::vector<std::string>()> legacyFiles;
        std::function<std::string(const std::string& file)> legacyImport;
    };

    static Menu& Instance() noexcept;

    void Unregister() noexcept;
    void Bind(Ports ports);
    static void ResetForTest() noexcept;

    [[nodiscard]] bool Install(Ports ports);
    [[nodiscard]] bool Installed() const noexcept { return installed_; }

    void Frame();

    [[nodiscard]] const MenuRequestQueue& Queue() const noexcept { return queue_; }

    [[nodiscard]] std::string LastUpdateMessage() const;

    [[nodiscard]] bool EditDraftForTest(
        const std::function<void(ConfigurationData&)>& mutate);

private:
    Menu() = default;

    static void __stdcall HandleFrameworkEvent(std::uint32_t type);
    enum class CommitOutcome : std::uint8_t { Nothing, Committed, Failed };
    void Render();
    void RenderOverview();
    void RenderCategories();
    void RenderFame();
    void RenderConnections();
    void RenderBalance();
    void RenderCommands();
    void RenderLegacy();
    void RenderDiagnostics();

    [[nodiscard]] const ConfigurationData* CurrentData() const;
    void RefreshDraft();
    [[nodiscard]] CommitOutcome ApplyDraft();

    bool installed_{false};
    std::uint64_t eventToken_{0};
    Ports ports_{};
    MenuRequestQueue queue_{};
    ConfigurationSnapshot cached_{};
    std::optional<ConfigurationData> draft_;
    std::uint64_t draftRevision_{0};
    std::string lastAction_;
    std::string lastCommandResult_;
    std::string fameLocation_;
    int sourceIndex_{-1};
    int targetIndex_{-1};
    int commandLocationIndex_{-1};
    int commandTargetIndex_{-1};
    int commandCategoryIndex_{0};
    int commandValue_{30};
    int commandMinimum_{0};
    int commandMaximum_{50};
    int commandPercent_{30};
    std::vector<std::string> legacyFiles_;
};

}
