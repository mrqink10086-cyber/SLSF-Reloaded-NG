#pragma once

#include "slsf/core/Snapshots.h"

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace slsf {

enum class ExplicitCommandKind : std::uint8_t {
    Set = 0,
    Clear,
    ClearAll,
    Wipe,
    SetFlag,
    ClearFlag,
};

struct ExplicitCommand {
    ExplicitCommandKind kind{ExplicitCommandKind::Set};
    RegionId target;
    FameCategory category{FameCategory::Count};
    double value{};
};

struct Configuration {
    std::map<std::string, double, std::less<>> values;

    double Get(std::string_view key, double fallback = 0.0) const {
        const auto it = values.find(key);
        return it == values.end() ? fallback : it->second;
    }
};

struct RuleContext {
    GameTime clock;
    const Configuration* configuration{};
    std::vector<Fact> facts;
    IRandom* rng{};
};

struct Parameters {
    double decayPerDay{};
    double relaxationPerDay{};
    double spreadThreshold{};
    double spreadRetention{};
    double maxFame{150.0};

    friend bool operator==(const Parameters&, const Parameters&) = default;
};

struct ScheduledChange {
    GameTime at;
    ExplicitCommand command;
};

class PreparedSettlement;
struct SettlementWorkspace;

struct AdvanceInputs {
    const DiffusionGraph* graph{};
    const Parameters* parameters{};
    // Decay rolls resolve the per-category legacy multiplier from this
    // snapshot; a null configuration uses the default multiplier 1.0.
    const Configuration* configuration{};
    // One-shot queue: due entries are erased by a successful Advance so the
    // same batch cannot be replayed at the same timestamp. Future entries stay.
    std::vector<ScheduledChange> scheduledChanges;
    IRandom* rng{};
    DiffusionBackend backend{DiffusionBackend::Auto};
    const RegionSnapshot* regions{};
    // A prepared snapshot owns its account/region bindings, graph, custom-region keys,
    // and rules. Rebuild after any of those or configuration changes; numeric writes reuse it.
    std::shared_ptr<const PreparedSettlement> prepared;
    // Mutable scratch shared by AdvanceInputs copies; calls using it must be serialized.
    std::shared_ptr<SettlementWorkspace> workspace;
};

struct CommandResult {
    Status status{Status::Invalid};
    Revision revision;
    std::string message;
};

struct AdvanceResult {
    Status status{Status::Ready};
    Revision revision;
    GameTime advancedTo;
};

CommandResult ApplyExplicit(State& state, const ExplicitCommand& command, const RuleContext& context);
PublishedState MakePublishedState(const State& state);
// Before each due command, settle through max(command time, prior boundary), then settle through now.
// Each elapsed segment decays local rows before a frozen propagation hop.
// Success consumes due commands; rejection preserves state and the queue.
Result<AdvanceResult> Advance(State& state, GameTime now, AdvanceInputs& inputs);
Result<PublicView> Query(const PublishedState& state, const QueryRequest& request);

}
