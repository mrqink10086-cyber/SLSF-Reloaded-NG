#include "slsf/runtime/Lifecycle.h"

#include "slsf/core/Commands.h"
#include "slsf/core/location/RegionRegistry.h"
#include "slsf/core/reputation/GainRules.h"
#include "slsf/core/reputation/ReputationState.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <string>
#include <string_view>
#include <utility>

namespace slsf {
namespace {

constexpr double kDefaultEdgeRetention = 0.6;

class SessionRandom final : public IRandom {
public:
    explicit SessionRandom(std::uint64_t seed) : state_(seed == 0 ? 0x9E3779B97F4A7C15ull : seed) {}

    std::int32_t UniformInt(std::int32_t lo, std::int32_t hi) override {
        if (lo >= hi) {
            return lo;
        }
        std::uint64_t value = Next();
        const auto span = static_cast<std::uint64_t>(
            static_cast<std::int64_t>(hi) - static_cast<std::int64_t>(lo) + 1);
        return static_cast<std::int32_t>(static_cast<std::int64_t>(lo) +
                                         static_cast<std::int64_t>(value % span));
    }

private:
    std::uint64_t Next() {
        std::uint64_t z = (state_ += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30u)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27u)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31u);
    }

    std::uint64_t state_;
};

std::string UpdateMessage(const ConfigurationUpdateResult& update) {
    switch (update.status) {
    case ConfigurationUpdateStatus::Applied:
        return "configuration applied";
    case ConfigurationUpdateStatus::Unchanged:
        return "configuration unchanged";
    case ConfigurationUpdateStatus::Invalid:
        return "configuration invalid; keeping last valid snapshot";
    case ConfigurationUpdateStatus::IoError:
        return "configuration I/O error; keeping last valid snapshot";
    }
    return "configuration update failed";
}
double ValidOr(double value, double fallback) {
    return std::isfinite(value) ? value : fallback;
}
}
Configuration MakeRuleConfiguration(const ConfigurationData& data) {
    Configuration rules;
    // Category multipliers are authoritative in the typed snapshot; use the
    // per-category keys instead of the legacy global multiplier.
    rules.values.emplace(std::string{kGainConfigKeyUseGlobal}, 0.0);
    rules.values.emplace(std::string{kGainConfigKeyMultiplier}, 1.0);
    for (std::size_t index = 0; index < data.categories.multipliers.size(); ++index) {
        std::string key{kGainConfigKeyCategoryMultiplierPrefix};
        key += std::to_string(static_cast<unsigned>(index));
        const double multiplier = data.categories.multipliers[index];
        rules.values.emplace(std::move(key),
                            std::isfinite(multiplier) && multiplier >= 0.0 ? multiplier : 1.0);
    }
    rules.values.emplace(std::string{kGainConfigKeyNightReduction},
                         data.features.reduceFameAtNight ? 1.0 : 0.0);
    rules.values.emplace(std::string{kGainConfigKeyNightStart},
                         ValidOr(data.timing.nightStart, 22.0));
    rules.values.emplace(std::string{kGainConfigKeyNightEnd},
                         ValidOr(data.timing.nightEnd, 6.0));
    // Legacy stores half-hour batches; the typed snapshot stores days.
    const double decayDays = ValidOr(data.timing.decayPeriodDays, 0.5);
    rules.values.emplace(std::string{kGainConfigKeyDecayTimeNeeded},
                         std::max(0.0, decayDays * 48.0));
    return rules;
}
struct Lifecycle::Assets {
    ConfigurationSnapshot configuration;
    Configuration rules;
    Parameters parameters;
    DiffusionGraph graph;
    SceneSnapshot scene;
    std::shared_ptr<SessionRandom> random;
};

struct Lifecycle::Pending {
    SessionKind kind{SessionKind::NewGame};
    std::unique_ptr<Assets> assets;
    State state;
    Epoch epoch;
    std::uint32_t restoredAccounts{};
    std::uint32_t unresolvedAccounts{};
    std::uint32_t droppedEdges{};
    std::string message;
};

Lifecycle::Lifecycle(StaticWorld world, IConfigurationSource& configuration,
                     std::shared_ptr<runtime::SessionGate> hostGate)
    : world_(world), configuration_(configuration), hostGate_(std::move(hostGate)), dispatcher_(hostGate_) {
    PublishStatus();
}

Lifecycle::~Lifecycle() {
    dispatcher_.Detach();
}
ConfigurationUpdateResult Lifecycle::ReloadConfiguration() {
    auto update = configuration_.Reload();
    if (update.status == ConfigurationUpdateStatus::Applied ||
        update.status == ConfigurationUpdateStatus::Unchanged) {
        if (!configuration_.Current().HasData()) {
            update.status = ConfigurationUpdateStatus::Invalid;
            update.message = "configuration source has no valid snapshot";
        } else {
            update = ApplyCurrentConfiguration();
        }
    }
    message_ = update.message;
    PublishStatus();
    return update;
}
ConfigurationUpdateResult Lifecycle::ApplyCurrentConfiguration() {
    const auto configuration = configuration_.Current();
    ConfigurationUpdateResult update{ConfigurationUpdateStatus::Unchanged,
                                      configuration.revision, "configuration unchanged"};
    if (!configuration.HasData()) {
        update.status = ConfigurationUpdateStatus::Invalid;
        update.message = "configuration source has no valid snapshot";
    } else if (active_ && active_->configuration.revision != configuration.revision) {
        std::unique_ptr<Assets> candidate;
        std::uint32_t droppedEdges = 0;
        std::string error;
        const SessionContext context{.scene = active_->scene, .randomSeed = 1};
        if (BuildAssets(configuration, context, candidate, droppedEdges, error)) {
            candidate->random = active_->random;
            const RuntimeBindings bindings{
                .graph = &candidate->graph, .parameters = &candidate->parameters,
                .configuration = &candidate->rules, .regions = world_.regions,
                .rng = candidate->random.get(), .settings = candidate->configuration.data};
            const auto applied = dispatcher_.Reconfigure(bindings, configuration);
            if (applied) {
                active_ = std::move(candidate);
                droppedEdges_ = droppedEdges;
                update.status = ConfigurationUpdateStatus::Applied;
                update.message = "configuration applied to active session";
            } else {
                error = applied.error().message;
            }
        }
        if (!error.empty()) {
            update.status = ConfigurationUpdateStatus::Invalid;
            update.revision = active_->configuration.revision;
            update.message = std::move(error);
        }
    }
    message_ = update.message;
    PublishStatus();
    return update;
}
bool Lifecycle::BuildAssets(const ConfigurationSnapshot& configuration,
                            const SessionContext& context, std::unique_ptr<Assets>& out,
                            std::uint32_t& droppedEdges, std::string& error) const {
    if (!configuration.HasData()) {
        error = "no valid configuration snapshot";
        return false;
    }
    if (world_.regions == nullptr) {
        error = "static location/region table is not built";
        return false;
    }
    if (!std::is_sorted(world_.regions->regions.begin(), world_.regions->regions.end())) {
        error = "static region table is not sorted";
        return false;
    }

    auto assets = std::make_unique<Assets>();
    assets->configuration = configuration;
    assets->parameters = configuration.data->parameters;
    assets->scene = context.scene;
    assets->random = std::make_shared<SessionRandom>(context.randomSeed);
    assets->rules = MakeRuleConfiguration(*configuration.data);

    std::vector<EdgeDefinition> edges;
    const auto retention = std::isfinite(assets->parameters.spreadRetention)
                               ? std::clamp(assets->parameters.spreadRetention, 0.0, 1.0)
                               : kDefaultEdgeRetention;
    const double edgeRetention = retention;
    for (const RegionConnectionSetting& connection : configuration.data->connections) {
        if (!connection.enabled) {
            continue;
        }
        if (!FindRegion(*world_.regions, connection.from) ||
            !FindRegion(*world_.regions, connection.to) ||
            connection.from == connection.to) {
            ++droppedEdges;
            continue;
        }
        edges.push_back(EdgeDefinition{connection.from, connection.to, edgeRetention});
    }
    auto graph = BuildDiffusionGraph(*world_.regions, std::move(edges));
    if (!graph) {
        error = graph.error().message;
        return false;
    }
    assets->graph = std::move(*graph);
    out = std::move(assets);
    return true;
}
Epoch Lifecycle::NextEpoch() {
    return Epoch{nextSession_++, 0, 0};
}
bool Lifecycle::Begin(SessionKind kind, const SessionContext& context,
                      const PersistentState* candidate) {
    ReturnToMenu();
    kind_ = kind;
    phase_ = LifecyclePhase::Preparing;
    restoredAccounts_ = 0;
    unresolvedAccounts_ = 0;
    droppedEdges_ = 0;
    message_.clear();

    const ConfigurationUpdateResult update = ReloadConfiguration();
    std::string configurationWarning;
    if (update.status == ConfigurationUpdateStatus::Invalid ||
        update.status == ConfigurationUpdateStatus::IoError) {
        configurationWarning = update.message;
    }
    const ConfigurationSnapshot configuration = configuration_.Current();
    if (!configuration.HasData()) {
        phase_ = LifecyclePhase::Failed;
        message_ = "no valid configuration snapshot";
        return false;
    }

    SessionContext effective = context;
    if (effective.randomSeed == SessionContext::kNoRandomSeed) {
        effective.randomSeed = nextRandomSeed_++;
    }

    auto pending = std::make_unique<Pending>();
    pending->kind = kind;
    pending->epoch = NextEpoch();
    if (!BuildAssets(configuration, effective, pending->assets, pending->droppedEdges,
                     pending->message)) {
        phase_ = LifecyclePhase::Failed;
        message_ = pending->message;
        return false;
    }

    if (candidate != nullptr) {
        auto restored = RestoreState(pending->state, *candidate, *world_.regions, configuration);
        if (!restored) {
            phase_ = LifecyclePhase::Failed;
            message_ = restored.error().message;
            return false;
        }
        pending->restoredAccounts = static_cast<std::uint32_t>(restored->restoredAccounts);
        pending->unresolvedAccounts = static_cast<std::uint32_t>(restored->unresolvedAccounts);
        pending->message = "save bound; awaiting activation";
    } else {
        auto bound = BindStateToRegions(pending->state, *world_.regions);
        if (!bound) {
            phase_ = LifecyclePhase::Failed;
            message_ = bound.error().message;
            return false;
        }
        pending->state.configurationRevision = configuration.revision.value;
        pending->message = "new state bound; awaiting activation";
    }

    restoredAccounts_ = pending->restoredAccounts;
    unresolvedAccounts_ = pending->unresolvedAccounts;
    droppedEdges_ = pending->droppedEdges;
    message_ = pending->message;
    if (!configurationWarning.empty()) {
        message_ += "; ";
        message_ += configurationWarning;
    }
    pending_ = std::move(pending);
    return true;
}
bool Lifecycle::PrepareNewGame(const SessionContext& context) {
    const auto ready = Begin(SessionKind::NewGame, context, nullptr);
    PublishStatus();
    return ready;
}
bool Lifecycle::PrepareLoad(const PersistentState& candidate, const SessionContext& context) {
    const auto ready = Begin(SessionKind::Load, context, &candidate);
    PublishStatus();
    return ready;
}
bool Lifecycle::PrepareRevert(const PersistentState& candidate, const SessionContext& context) {
    const auto ready = Begin(SessionKind::Revert, context, &candidate);
    PublishStatus();
    return ready;
}
bool Lifecycle::ActivatePrepared(bool hostSucceeded, std::optional<std::uint64_t> hostGeneration) {
    if (!hostSucceeded) {
        RejectPreparation("host load failed; prepared session discarded");
        return false;
    }
    if (!pending_) {
        RejectPreparation("no prepared session to activate");
        return false;
    }
    auto pending = std::move(pending_);
    const RuntimeBindings bindings{
        .graph = &pending->assets->graph,
        .parameters = &pending->assets->parameters,
        .configuration = &pending->assets->rules,
        .regions = world_.regions,
        .rng = pending->assets->random.get(),
        .settings = pending->assets->configuration.data,
    };
    if (!dispatcher_.Attach(pending->epoch, bindings, std::move(pending->state))) {
        RejectPreparation("dispatcher refused activation; business stays closed");
        return false;
    }
    active_ = std::move(pending->assets);
    epoch_ = pending->epoch;
    phase_ = LifecyclePhase::Active;
    restoredAccounts_ = pending->restoredAccounts;
    unresolvedAccounts_ = pending->unresolvedAccounts;
    droppedEdges_ = pending->droppedEdges;
    message_ = "session active";
    if (hostGate_ && hostGeneration) hostGate_->Open(*hostGeneration);
    PublishStatus();
    return true;
}
void Lifecycle::DiscardPrepared() {
    if (phase_ == LifecyclePhase::Preparing) {
        phase_ = LifecyclePhase::Detached;
    }
    pending_.reset();
    message_ = "prepared session discarded";
    PublishStatus();
}
void Lifecycle::RejectPreparation(std::string reason) {
    ReturnToMenu();
    phase_ = LifecyclePhase::Failed;
    message_ = std::move(reason);
    PublishStatus();
}
void Lifecycle::ReturnToMenu() {
    dispatcher_.Detach();
    active_.reset();
    pending_.reset();
    phase_ = LifecyclePhase::Detached;
    message_ = "returned to menu";
    PublishStatus();
}
Result<PersistentState> Lifecycle::Capture() const {
    if (!BusinessOpen()) {
        return Fail(Error::Code::InvalidState, "no active session to capture");
    }
    return dispatcher_.Capture();
}
bool Lifecycle::BusinessOpen() const {
    return phase_ == LifecyclePhase::Active && dispatcher_.Attached() && !dispatcher_.Suspended();
}
bool Lifecycle::IsEpochCurrent(Epoch epoch) const {
    return BusinessOpen() && dispatcher_.CurrentEpoch() == epoch;
}
Epoch Lifecycle::CurrentEpoch() const {
    return dispatcher_.CurrentEpoch();
}
LifecycleStatus Lifecycle::Status() const {
    return *publishedStatus_.load();
}
void Lifecycle::PublishStatus() {
    LifecycleStatus status;
    status.phase = phase_;
    status.kind = kind_;
    status.epoch = pending_ ? pending_->epoch : dispatcher_.CurrentEpoch();
    status.configurationRevision = active_ ? active_->configuration.revision
        : pending_ ? pending_->assets->configuration.revision : configuration_.Current().revision;
    status.restoredAccounts = restoredAccounts_;
    status.unresolvedAccounts = unresolvedAccounts_;
    status.droppedConnectionEdges = droppedEdges_;
    status.businessOpen = BusinessOpen();
    status.message = message_;
    publishedStatus_.store(std::make_shared<const LifecycleStatus>(std::move(status)));
}
}
