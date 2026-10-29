#pragma once

#include "slsf/core/Configuration.h"
#include "slsf/core/Snapshots.h"
#include "slsf/core/persistence/PersistentState.h"
#include "slsf/persistence/Serialization.h"
#include "slsf/runtime/Dispatcher.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace slsf {

using Dispatcher = runtime::Dispatcher;
using RuntimeBindings = runtime::RuntimeBindings;

enum class SessionKind : std::uint8_t {
    NewGame = 0,
    Load,
    Revert,
};

enum class LifecyclePhase : std::uint8_t {
    Detached = 0,
    Preparing,
    Active,
    Failed,
};

// Configuration port.  The adapter returns the last valid snapshot on
// parse/IO failure.
class IConfigurationSource {
public:
    virtual ~IConfigurationSource() = default;
    [[nodiscard]] virtual ConfigurationSnapshot Current() const = 0;
    virtual ConfigurationUpdateResult Reload() = 0;
};

// Projects the typed cross-save snapshot onto the legacy named keys consumed
// by the settlement/gain rules.  Kept public so the projection has an
// independent oracle and the JSON adapter cannot silently diverge from it.
[[nodiscard]] Configuration MakeRuleConfiguration(const ConfigurationData& data);

// Built once at DataLoaded for the process lifetime.  Location indexes are
// immutable and shared by every session; per-save ownership is rebound by
// BindStateToRegions/RestoreState against this table.
struct StaticWorld {
    const RegionSnapshot* regions{};
};

struct SessionContext {
    SceneSnapshot scene;
    static constexpr std::uint64_t kNoRandomSeed = 0;
    std::uint64_t randomSeed{kNoRandomSeed};
};

struct LifecycleStatus {
    LifecyclePhase phase{LifecyclePhase::Detached};
    SessionKind kind{SessionKind::NewGame};
    Epoch epoch;
    ConfigurationRevision configurationRevision;
    std::uint32_t restoredAccounts{};
    std::uint32_t unresolvedAccounts{};
    std::uint32_t droppedConnectionEdges{};
    bool businessOpen{};
    std::string message;
};

class Lifecycle {
public:
    Lifecycle(StaticWorld world, IConfigurationSource& configuration);
    ~Lifecycle();

    Lifecycle(const Lifecycle&) = delete;
    Lifecycle& operator=(const Lifecycle&) = delete;

    // Startup and each session call this before building assets.  Failures
    // keep the source's last valid snapshot; the lifecycle reports the
    // status but never applies partially parsed data.
    ConfigurationUpdateResult ReloadConfiguration();

    bool PrepareNewGame(const SessionContext& context);
    bool PrepareLoad(const PersistentState& candidate, const SessionContext& context);
    bool PrepareRevert(const PersistentState& candidate, const SessionContext& context);

    // Attaches exactly once after host success. Failure discards the candidate
    // and closes the dispatcher; diagnostics remain available in Status().
    bool ActivatePrepared(bool hostSucceeded = true);

    // Drops the pending candidate (failed/cancelled load) without touching
    // the detached dispatcher's last publication.
    void DiscardPrepared();

    // Records a decode/bind failure that happened before PrepareLoad could
    // receive a candidate.  Always leaves the business switch closed.
    void RejectPreparation(std::string reason);

    // Menu / Revert / PreLoad.  Idempotent; invalidates all stamped events.
    void ReturnToMenu();

    // Capture takes the dispatcher lock once and returns an immutable copy;
    // callers encode it after this returns.
    Result<PersistentState> Capture() const;

    // Explicit closed gate for Papyrus/transport adapters.
    [[nodiscard]] bool BusinessOpen() const;
    [[nodiscard]] bool IsEpochCurrent(Epoch epoch) const;
    [[nodiscard]] Epoch CurrentEpoch() const;

    [[nodiscard]] Dispatcher& Executor() noexcept { return dispatcher_; }
    [[nodiscard]] const Dispatcher& Executor() const noexcept { return dispatcher_; }
    [[nodiscard]] LifecycleStatus Status() const;

private:
    struct Assets;
    struct Pending;

    bool Begin(SessionKind kind, const SessionContext& context,
               const PersistentState* candidate);
    bool BuildAssets(const ConfigurationSnapshot& configuration, const SessionContext& context,
                     std::unique_ptr<Assets>& out, std::uint32_t& droppedEdges,
                     std::string& error) const;
    Epoch NextEpoch();

    StaticWorld world_;
    IConfigurationSource& configuration_;
    Dispatcher dispatcher_;
    std::unique_ptr<Assets> active_;
    std::unique_ptr<Pending> pending_;
    // Engine callbacks may poll BusinessOpen from non-main threads.  Keep the
    // published gate atomic; the owned assets remain main-thread state.
    std::atomic<LifecyclePhase> phase_{LifecyclePhase::Detached};
    SessionKind kind_{SessionKind::NewGame};
    Epoch epoch_;
    std::uint64_t nextSession_{1};
    std::uint64_t nextRandomSeed_{1};
    std::uint32_t restoredAccounts_{};
    std::uint32_t unresolvedAccounts_{};
    std::uint32_t droppedEdges_{};
    std::string message_;
};

} // namespace slsf
