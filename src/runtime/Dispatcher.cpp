#include "slsf/runtime/Dispatcher.h"
#include "slsf/core/reputation/ReputationState.h"
#include "slsf/core/reputation/Settlement.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace slsf::runtime {

Dispatcher::Flow Dispatcher::Flow::promise_type::get_return_object() noexcept {
    return Flow{std::coroutine_handle<promise_type>::from_promise(*this)};
}

Dispatcher::Flow::Flow(std::coroutine_handle<promise_type> handle) noexcept
    : handle_(handle) {}

Dispatcher::Flow::~Flow() {
    if (!handle_) {
        return;
    }
    if (!handle_.done()) {
        auto& promise = handle_.promise();
        const std::shared_ptr<WaitEntry> entry = promise.entry;
        if (promise.owner && entry) {
            if (auto settled = promise.owner->RetireWait(entry, true, true)) {
                settled->resume();
            }
        }
        promise.stopping = true;
        if (!handle_.done()) {
            handle_.resume();
        }
    }
    handle_.destroy();
}

Dispatcher::Flow::Flow(Flow&& other) noexcept
    : handle_(std::exchange(other.handle_, {})) {}

Dispatcher::Flow& Dispatcher::Flow::operator=(Flow&& other) noexcept {
    if (this != &other) {
        if (handle_) {
            Flow dying{std::exchange(handle_, {})};
        }
        handle_ = std::exchange(other.handle_, {});
    }
    return *this;
}

void Dispatcher::Flow::Start() {
    if (!handle_ || handle_.done()) {
        return;
    }
    auto& promise = handle_.promise();
    if (promise.started || promise.stopping) {
        return;
    }
    promise.started = true;
    handle_.resume();
}

bool Dispatcher::Flow::Done() const noexcept {
    return handle_ && handle_.done();
}

std::exception_ptr Dispatcher::Flow::Failure() const noexcept {
    return handle_ ? handle_.promise().failure : std::exception_ptr{};
}

bool Dispatcher::Flow::CancelWait() {
    if (!handle_ || handle_.done()) {
        return false;
    }
    auto& promise = handle_.promise();
    const std::shared_ptr<WaitEntry> entry = promise.entry;
    if (!promise.owner || !entry) {
        return false;
    }
    auto retired = promise.owner->RetireWait(entry, true, false);
    if (!retired) {
        return false;
    }
    retired->resume();
    return true;
}

bool Dispatcher::Delay::await_suspend(std::coroutine_handle<Flow::promise_type> frame) {
    auto& promise = frame.promise();
    if (!promise.owner || promise.stopping) {
        aborted_ = true;
        return false;
    }
    return promise.owner->RegisterWait(*this, frame, seconds_, domain_);
}

Dispatcher::Flow Dispatcher::SpawnFlow(std::function<Flow()> body) {
    if (!body) {
        return {};
    }
    auto keepBody = std::make_unique<std::function<Flow()>>(std::move(body));
    Flow flow = (*keepBody)();
    if (flow.handle_) {
        auto& promise = flow.handle_.promise();
        promise.owner = this;
        promise.keepBody = keepBody.release();
        flow.Start();
    }
    return flow;
}

bool Dispatcher::RegisterWait(Delay& awaiter, std::coroutine_handle<Flow::promise_type> frame,
                              double seconds, ClockDomain domain) {
    if (!std::isfinite(seconds)) {
        awaiter.aborted_ = true;
        return false;
    }
    clocks_.Tick();
    std::lock_guard<std::mutex> guard(mutex_);
    auto& promise = frame.promise();
    if (!state_ || promise.stopping) {
        awaiter.aborted_ = true;
        return false;
    }
    auto entry = std::make_shared<WaitEntry>();
    entry->awaiter = &awaiter;
    entry->live = true;
    entry->token = scheduler_.Schedule(
        domain, DeadlineLocked(domain, seconds),
        [this, weak = std::weak_ptr<WaitEntry>(entry)] {
            if (auto fired = weak.lock()) {
                FireWait(fired);
            }
        });
    if (entry->token.id.value == 0) {
        awaiter.aborted_ = true;
        return false;
    }
    entry->continuation = frame;
    entry->position = waits_.insert(waits_.end(), entry);
    promise.entry = std::move(entry);
    return true;
}

std::optional<std::coroutine_handle<Dispatcher::Flow::promise_type>>
Dispatcher::RetireWait(const std::shared_ptr<WaitEntry>& entry, bool aborted, bool stopping) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!entry || !entry->live) {
        return std::nullopt;
    }
    entry->live = false;
    if (entry->awaiter) {
        if (aborted) {
            entry->awaiter->aborted_ = true;
        }
        entry->awaiter = nullptr;
    }
    std::optional<std::coroutine_handle<Flow::promise_type>> frame = entry->continuation;
    entry->continuation = {};
    if (frame) {
        frame->promise().entry.reset();
        if (stopping) {
            frame->promise().stopping = true;
        }
    }
    std::ignore = scheduler_.Cancel(entry->token);
    waits_.erase(entry->position);
    return frame;
}

void Dispatcher::FireWait(const std::shared_ptr<WaitEntry>& entry) {
    if (!entry) {
        return;
    }
    auto frame = RetireWait(entry, false, false);
    if (frame) {
        frame->resume();
    }
}

Dispatcher::~Dispatcher() {
    std::vector<time::TaskBody> retired;
    std::deque<EngineEvent> events;
    std::lock_guard<std::mutex> guard(mutex_);
    for (auto& wait : waits_) {
        if (!wait->live) {
            continue;
        }
        wait->live = false;
        if (wait->awaiter) {
            wait->awaiter->aborted_ = true;
            wait->awaiter = nullptr;
        }
        if (wait->continuation) {
            auto& promise = wait->continuation.promise();
            promise.entry.reset();
            promise.owner = nullptr;
        }
        wait->continuation = {};
    }
    waits_.clear();
    state_.reset();
    events.swap(events_);
    scheduler_.NewSession(retired);
}

std::shared_ptr<const ConfigurationData> Dispatcher::Settings() const {
    std::lock_guard lock(mutex_);
    return bindings_.settings;
}

bool Dispatcher::Attach(Epoch epoch, const RuntimeBindings& bindings, State state) {
    std::uint64_t generation;
    {
        std::lock_guard<std::mutex> guard(mutex_);
        if (state_) {
            return false;
        }
        generation = lifecycleGeneration_;
    }
    state.epoch = epoch;
    auto workspace = MakeSettlementWorkspace();
    std::shared_ptr<const PreparedSettlement> candidate;
    if (bindings.graph && bindings.parameters) {
        auto prepared = PrepareSettlement(
            state, *bindings.graph,
            bindings.configuration ? *bindings.configuration : Configuration{},
            bindings.regions);
        if (prepared) {
            candidate = std::move(prepared).value();
        }
    }
    std::vector<time::TaskBody> retired;
    std::lock_guard<std::mutex> guard(mutex_);
    if (state_ || lifecycleGeneration_ != generation) {
        return false;
    }
    ++lifecycleGeneration_;
    state_ = std::move(state);
    epoch_ = epoch;
    bindings_ = bindings;
    workspace_ = std::move(workspace);
    prepared_ = std::move(candidate);
    suspended_ = false;
    lastAdvance_.reset();
    changes_.clear();
    dirty_.clear();
    clocks_.Reset();
    scheduler_.NewSession(retired);
    PublishLocked();
    return true;
}

Result<void> Dispatcher::Reconfigure(const RuntimeBindings& bindings,
                                     const ConfigurationSnapshot& configuration) {
    if (!configuration.HasData() || !bindings.graph || !bindings.parameters ||
        !bindings.configuration || !bindings.regions || !bindings.rng) {
        return Fail(Error::Code::InvalidArgument, "incomplete configuration bindings");
    }
    State candidate;
    std::uint64_t generation;
    Revision revision;
    {
        std::lock_guard guard(mutex_);
        if (!state_ || suspended_) {
            return Fail(Error::Code::InvalidState, "no active session to reconfigure");
        }
        auto now = clocks_.Game();
        if (lastAdvance_ && now.AsDays() < lastAdvance_->AsDays()) {
            now = *lastAdvance_;
        }
        auto settled = AdvanceLocked(now);
        if (!settled) return std::unexpected(settled.error());
        PublishLocked();
        candidate = *state_;
        generation = lifecycleGeneration_;
        revision = state_->revision;
    }
    auto bound = BindStateToRegions(candidate, *bindings.regions);
    if (!bound) return std::unexpected(bound.error());
    candidate.configurationRevision = configuration.revision.value;
    RebuildDerivedSets(candidate, configuration.data.get());
    ++candidate.revision.value;
    auto prepared = PrepareSettlement(candidate, *bindings.graph,
                                       *bindings.configuration, bindings.regions);
    if (!prepared) return std::unexpected(prepared.error());
    auto workspace = MakeSettlementWorkspace();
    std::lock_guard guard(mutex_);
    if (!state_ || suspended_ || lifecycleGeneration_ != generation ||
        state_->revision != revision) {
        return Fail(Error::Code::InvalidState, "session changed during configuration update");
    }
    state_ = std::move(candidate);
    bindings_ = bindings;
    prepared_ = std::move(*prepared);
    workspace_ = std::move(workspace);
    PublishLocked();
    return {};
}

void Dispatcher::Detach() {
    std::vector<std::coroutine_handle<Flow::promise_type>> resumes;
    std::vector<time::TaskBody> retired;
    std::deque<EngineEvent> events;
    {
        std::lock_guard<std::mutex> guard(mutex_);
        ++lifecycleGeneration_;
        state_.reset();
        prepared_.reset();
        workspace_.reset();
        bindings_ = RuntimeBindings{};
        events.swap(events_);
        changes_.clear();
        dirty_.clear();
        clocks_.Reset();
        scheduler_.NewSession(retired);
        resumes.reserve(waits_.size());
        for (auto& wait : waits_) {
            if (!wait->live) {
                continue;
            }
            wait->live = false;
            wait->token = TimerToken{};
            if (wait->awaiter) {
                wait->awaiter->aborted_ = true;
                wait->awaiter = nullptr;
            }
            if (wait->continuation) {
                auto frame = wait->continuation;
                frame.promise().entry.reset();
                resumes.push_back(frame);
            }
            wait->continuation = {};
        }
        waits_.clear();
    }
    for (auto frame : resumes) {
        frame.resume();
    }
}

bool Dispatcher::Attached() const {
    std::lock_guard<std::mutex> guard(mutex_);
    return state_.has_value();
}

Epoch Dispatcher::CurrentEpoch() const {
    std::lock_guard<std::mutex> guard(mutex_);
    return epoch_;
}

void Dispatcher::Suspend() {
    std::lock_guard<std::mutex> guard(mutex_);
    suspended_ = true;
}

void Dispatcher::Resume() {
    std::lock_guard<std::mutex> guard(mutex_);
    suspended_ = false;
}

bool Dispatcher::Suspended() const {
    std::lock_guard<std::mutex> guard(mutex_);
    return suspended_;
}

Result<PersistentState> Dispatcher::Capture() const {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!state_) {
        return Fail(Error::Code::InvalidState, "no attached session to capture");
    }
    return CaptureState(*state_);
}

CommandResult Dispatcher::EditState(const std::function<Result<void>(State&)>& edit) {
    State candidate;
    std::uint64_t generation;
    Revision revision;
    {
        std::lock_guard guard(mutex_);
        if (!state_ || suspended_) {
            return {Status::NotReady, {}, "no active session to edit"};
        }
        auto now = clocks_.Game();
        if (lastAdvance_ && now.AsDays() < lastAdvance_->AsDays()) now = *lastAdvance_;
        const auto advanced = AdvanceLocked(now);
        if (!advanced) return {Status::Invalid, state_->revision, advanced.error().message};
        PublishLocked();
        candidate = *state_;
        generation = lifecycleGeneration_;
        revision = state_->revision;
    }
    const auto applied = edit(candidate);
    if (!applied) return {Status::Invalid, revision, applied.error().message};
    auto captured = CaptureState(candidate);
    if (!captured) return {Status::Invalid, revision, captured.error().message};
    candidate.revision.value = revision.value + 1;
    std::lock_guard guard(mutex_);
    if (!state_ || suspended_ || lifecycleGeneration_ != generation || state_->revision != revision) {
        return {Status::Stale, revision, "session changed during edit"};
    }
    state_ = std::move(candidate);
    prepared_.reset();
    PublishLocked();
    return {Status::Ready, state_->revision, {}};
}

CommandResult Dispatcher::Execute(const ExplicitCommand& command) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!state_) {
        return CommandResult{Status::Stale, Revision{}, "executor detached"};
    }
    if (suspended_) {
        return CommandResult{Status::NotReady, state_->revision, "executor suspended"};
    }
    GameTime now = clocks_.Game();
    if (lastAdvance_ && now.AsDays() < lastAdvance_->AsDays()) {
        now = *lastAdvance_;
    }
    const Revision before = state_->revision;
    const auto settled = AdvanceLocked(now);
    if (!settled) {
        return CommandResult{Status::NotReady, state_->revision, settled.error().message};
    }
    RuleContext context;
    context.clock = now;
    context.configuration = bindings_.configuration;
    context.rng = bindings_.rng;
    auto result = ApplyExplicit(*state_, command, context);
    if (result.status == Status::Ready || state_->revision != before) {
        PublishLocked();
    }
    return result;
}

namespace {
slsf::GameTime FloorToLastAdvance(const std::optional<slsf::GameTime>& lastAdvance,
                                 slsf::GameTime now) {
    if (lastAdvance && now.AsDays() < lastAdvance->AsDays()) {
        return *lastAdvance;
    }
    return now;
}

}

Dispatcher::GainOutcome Dispatcher::GainFame(GameTime now, RegionId account,
                                             FameCategory category,
                                             const GainFameInputs& ruleInputs) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!state_) {
        return GainOutcome{Status::Stale, Revision{}, 0, "executor detached"};
    }
    if (suspended_) {
        return GainOutcome{Status::NotReady, state_->revision, 0, "executor suspended"};
    }
    if (!bindings_.configuration || !bindings_.rng) {
        return GainOutcome{Status::NotReady, state_->revision, 0,
                           "settlement inputs not bound"};
    }
    now = FloorToLastAdvance(lastAdvance_, now);
    const Revision before = state_->revision;
    const auto settled = AdvanceLocked(now);
    if (!settled) {
        return GainOutcome{Status::NotReady, state_->revision, 0,
                           settled.error().message};
    }
    auto result = slsf::ApplyGainFame(*state_, now, account, category, ruleInputs,
                                      *bindings_.configuration, *bindings_.rng);
    if (!result) {
        return GainOutcome{Status::Invalid, state_->revision, 0, result.error().message};
    }
    if (state_->revision != before) {
        PublishLocked();
    }
    return GainOutcome{Status::Ready, state_->revision, *result, ""};
}

CommandResult Dispatcher::DecayBatch(int multiplier) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!state_ || suspended_ || !bindings_.configuration || !bindings_.rng)
        return {Status::NotReady, {}, "settlement inputs not bound"};
    const auto now = FloorToLastAdvance(lastAdvance_, clocks_.Game());
    const auto settled = AdvanceLocked(now);
    if (!settled) return {Status::NotReady, state_->revision, settled.error().message};
    const auto result = ApplyDecayBatch(*state_, now, multiplier, *bindings_.configuration, bindings_.regions, *bindings_.rng);
    PublishLocked();
    return {result ? Status::Ready : Status::Invalid, state_->revision, result ? "" : result.error().message};
}

Dispatcher::ApplyOutcome Dispatcher::ApplyManualSpread(RegionId source, RegionId target,
                                                       FameCategory category,
                                                       int percent) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!state_) {
        return ApplyOutcome{Status::Stale, Revision{}, 0.0, "executor detached"};
    }
    if (suspended_) {
        return ApplyOutcome{Status::NotReady, state_->revision, 0.0, "executor suspended"};
    }
    if (!bindings_.configuration) {
        return ApplyOutcome{Status::NotReady, state_->revision, 0.0,
                            "settlement inputs not bound"};
    }
    const Revision before = state_->revision;
    const auto settled = AdvanceLocked(FloorToLastAdvance(lastAdvance_, clocks_.Game()));
    if (!settled) {
        return ApplyOutcome{Status::NotReady, state_->revision, 0.0,
                            settled.error().message};
    }
    auto result =
        slsf::ApplyManualSpread(*state_, source, target, category, percent,
                                *bindings_.configuration);
    if (!result) {
        return ApplyOutcome{Status::Invalid, state_->revision, 0.0, result.error().message};
    }
    if (state_->revision != before) {
        PublishLocked();
    }
    return ApplyOutcome{Status::Ready, state_->revision, *result, ""};
}

Dispatcher::ApplyOutcome Dispatcher::ApplyManualDecay(RegionId target, FameCategory category,
                                                      int minimum, int maximum) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!state_) {
        return ApplyOutcome{Status::Stale, Revision{}, 0.0, "executor detached"};
    }
    if (suspended_) {
        return ApplyOutcome{Status::NotReady, state_->revision, 0.0, "executor suspended"};
    }
    if (!bindings_.configuration || !bindings_.rng) {
        return ApplyOutcome{Status::NotReady, state_->revision, 0.0,
                            "settlement inputs not bound"};
    }
    const Revision before = state_->revision;
    const auto settled = AdvanceLocked(FloorToLastAdvance(lastAdvance_, clocks_.Game()));
    if (!settled) {
        return ApplyOutcome{Status::NotReady, state_->revision, 0.0,
                            settled.error().message};
    }
    auto result = slsf::ApplyManualDecay(*state_, target, category, minimum, maximum,
                                         *bindings_.configuration, *bindings_.rng);
    if (!result) {
        return ApplyOutcome{Status::Invalid, state_->revision, 0.0, result.error().message};
    }
    if (state_->revision != before) {
        PublishLocked();
    }
    return ApplyOutcome{Status::Ready, state_->revision, *result, ""};
}

Dispatcher::ApplyOutcome Dispatcher::ApplyManualGain(RegionId target, FameCategory category,
                                                     int minimum, int maximum) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!state_) {
        return ApplyOutcome{Status::Stale, Revision{}, 0.0, "executor detached"};
    }
    if (suspended_) {
        return ApplyOutcome{Status::NotReady, state_->revision, 0.0, "executor suspended"};
    }
    if (!bindings_.configuration || !bindings_.rng) {
        return ApplyOutcome{Status::NotReady, state_->revision, 0.0,
                            "settlement inputs not bound"};
    }
    const Revision before = state_->revision;
    const auto settled = AdvanceLocked(FloorToLastAdvance(lastAdvance_, clocks_.Game()));
    if (!settled) {
        return ApplyOutcome{Status::NotReady, state_->revision, 0.0,
                            settled.error().message};
    }
    auto result = slsf::ApplyManualGain(*state_, target, category, minimum, maximum,
                                        *bindings_.configuration, *bindings_.rng);
    if (!result) {
        return ApplyOutcome{Status::Invalid, state_->revision, 0.0, result.error().message};
    }
    if (state_->revision != before) {
        PublishLocked();
    }
    return ApplyOutcome{Status::Ready, state_->revision, *result, ""};
}

CommandResult Dispatcher::ApplySpreadPulse(GameTime now,
                                                       std::span<const RegionId> sources) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!state_) {
        return CommandResult{Status::Stale, Revision{}, "executor detached"};
    }
    if (suspended_) {
        return CommandResult{Status::NotReady, state_->revision, "executor suspended"};
    }
    if (!bindings_.configuration || !bindings_.graph || !bindings_.parameters) {
        return CommandResult{Status::NotReady, state_->revision,
                             "settlement inputs not bound"};
    }
    now = FloorToLastAdvance(lastAdvance_, now);
    const Revision before = state_->revision;
    const auto settled = AdvanceLocked(now);
    if (!settled) {
        return CommandResult{Status::NotReady, state_->revision, settled.error().message};
    }
    auto result = slsf::ApplySpreadPulse(*state_, now, sources, *bindings_.graph,
                                         *bindings_.parameters,
                                         *bindings_.configuration);
    if (!result) {
        return CommandResult{Status::Invalid, state_->revision, result.error().message};
    }
    if (state_->revision != before) {
        PublishLocked();
    }
    return CommandResult{Status::Ready, state_->revision, ""};
}

Result<PublicView> Dispatcher::Query(const QueryRequest& request) const {
    auto snapshot = Snapshot();
    if (!snapshot) {
        return std::unexpected(Error{Error::Code::InvalidState, "no published snapshot yet"});
    }
    return slsf::Query(*snapshot, request);
}

void Dispatcher::ScheduleChange(ScheduledChange change) {
    if (!std::isfinite(change.at.AsSeconds())) {
        return;
    }
    std::lock_guard<std::mutex> guard(mutex_);
    if (!state_) {
        return;
    }
    const auto at = change.at;
    const auto position =
        std::upper_bound(changes_.begin(), changes_.end(), at,
                         [](GameTime when, const ScheduledChange& existing) {
                             return when.AsSeconds() < existing.at.AsSeconds();
                         });
    changes_.insert(position, std::move(change));
    ++stats_.changesScheduled;
}

std::size_t Dispatcher::PendingChanges() const {
    std::lock_guard<std::mutex> guard(mutex_);
    return changes_.size();
}

Result<AdvanceResult> Dispatcher::AdvanceTime(GameTime now) {
    std::lock_guard<std::mutex> guard(mutex_);
    auto result = AdvanceLocked(now);
    if (result) {
        PublishLocked();
    }
    return result;
}

Result<AdvanceResult> Dispatcher::AdvanceLocked(GameTime now) {
    if (!state_) {
        return std::unexpected(Error{Error::Code::InvalidState, "executor detached"});
    }
    if (suspended_) {
        return std::unexpected(Error{Error::Code::InvalidState, "executor suspended"});
    }
    const double seconds = now.AsSeconds();
    if (!std::isfinite(seconds)) {
        return std::unexpected(Error{Error::Code::InvalidArgument,
                                     "non-finite game time; clock floor unchanged"});
    }
    if (lastAdvance_ && seconds < lastAdvance_->AsSeconds()) {
        return std::unexpected(Error{Error::Code::InvalidArgument,
                                     "clock rewound; Detach+Attach for a new session"});
    }

    AdvanceInputs inputs;
    inputs.graph = bindings_.graph;
    inputs.parameters = bindings_.parameters;
    inputs.configuration = bindings_.configuration;
    inputs.rng = bindings_.rng;
    inputs.regions = bindings_.regions;
    inputs.prepared = prepared_;
    inputs.workspace = workspace_;
    inputs.scheduledChanges = std::move(changes_);
    auto result = Advance(*state_, now, inputs);
    changes_ = std::move(inputs.scheduledChanges);
    if (result) {
        lastAdvance_ = now;
    }
    return result;
}

void Dispatcher::Post(EventBody body) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!state_) {
        ++stats_.eventsDroppedStale;
        return;
    }
    events_.push_back(EngineEvent{epoch_, std::move(body)});
    ++stats_.eventsPosted;
}

void Dispatcher::Post(Epoch epoch, EventBody body) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!state_) {
        ++stats_.eventsDroppedStale;
        return;
    }
    events_.push_back(EngineEvent{epoch, std::move(body)});
    ++stats_.eventsPosted;
}

std::size_t Dispatcher::PendingEvents() const {
    std::lock_guard<std::mutex> guard(mutex_);
    return events_.size();
}

void Dispatcher::SetDirtySink(DirtySink sink) {
    std::lock_guard<std::mutex> guard(mutex_);
    dirtySink_.swap(sink);
}

void Dispatcher::RequestDirtyRefresh(RegionId target) {
    clocks_.Tick();
    std::lock_guard<std::mutex> guard(mutex_);
    if (!state_) {
        return;
    }
    ++stats_.dirtyRequests;
    auto [slot, inserted] = dirty_.try_emplace(target);
    if (!inserted) {
        ++stats_.dirtyMerged;
        return;
    }
    const double deadline = DomainNowLocked(ClockDomain::Active) + kDirtyWindowSeconds;
    slot->second.deadline = deadline;
    slot->second.token = scheduler_.Schedule(
        ClockDomain::Active, deadline,
        [this, target, generation = lifecycleGeneration_] { FireDirty(target, generation); });
}

std::size_t Dispatcher::PendingDirty() const {
    std::lock_guard<std::mutex> guard(mutex_);
    return dirty_.size();
}

void Dispatcher::FireDirty(RegionId target, std::uint64_t generation) {
    DirtySink sink;
    {
        std::lock_guard<std::mutex> guard(mutex_);
        if (!state_ || generation != lifecycleGeneration_) {
            return;
        }
        auto node = dirty_.extract(target);
        if (node.empty()) {
            return;
        }
        ++stats_.dirtyFired;
        sink = dirtySink_;
    }
    if (sink) {
        sink(target);
    }
}

void Dispatcher::Step() {
    {
        std::lock_guard<std::mutex> guard(mutex_);
        ++stats_.steps;
    }
    const ClockSnapshot snapshot = clocks_.Tick();

    bool active = false;
    std::uint64_t generation;
    std::uint64_t schedulerSession;
    {
        std::lock_guard<std::mutex> guard(mutex_);
        active = state_.has_value() && !suspended_;
        generation = lifecycleGeneration_;
        schedulerSession = scheduler_.Session();
    }
    if (!active) {
        return;
    }

    const bool dueChange = [&] {
        std::lock_guard<std::mutex> guard(mutex_);
        return HasDueChangeLocked(snapshot.game);
    }();
    if (dueChange || scheduler_.HasDue(ClockDomain::Game, snapshot.game.AsDays())) {
        const auto settled = AdvanceTime(snapshot.game);
        if (!settled) {
            std::lock_guard<std::mutex> guard(mutex_);
            ++stats_.advanceErrors;
        }
    }

    auto runDue = [this, generation, schedulerSession](ClockDomain domain, double now) {
        {
            std::lock_guard<std::mutex> guard(mutex_);
            if (!state_ || lifecycleGeneration_ != generation) {
                return;
            }
        }
        for (auto& task : scheduler_.AcquireDue(domain, now)) {
            {
                std::lock_guard<std::mutex> guard(mutex_);
                if (!state_ || lifecycleGeneration_ != generation ||
                    task.session != schedulerSession || scheduler_.Session() != schedulerSession) {
                    return;
                }
            }
            if (!task.body) {
                continue;
            }
            try {
                task.body();
            } catch (...) {
                std::lock_guard<std::mutex> guard(mutex_);
                ++stats_.timerErrors;
            }
        }
    };
    runDue(ClockDomain::Game, snapshot.game.AsDays());
    runDue(ClockDomain::Real, snapshot.real.value);
    runDue(ClockDomain::Active, snapshot.active.value);

    for (;;) {
        EngineEvent event;
        {
            std::lock_guard<std::mutex> guard(mutex_);
            if (!state_ || lifecycleGeneration_ != generation || events_.empty()) {
                break;
            }
            event = std::move(events_.front());
            events_.pop_front();
        }
        bool valid = false;
        {
            std::lock_guard<std::mutex> guard(mutex_);
            valid = state_.has_value() && event.epoch == epoch_;
        }
        if (!valid) {
            std::lock_guard<std::mutex> guard(mutex_);
            ++stats_.eventsDroppedStale;
            continue;
        }
        if (event.body) {
            event.body();
        }
        std::lock_guard<std::mutex> guard(mutex_);
        ++stats_.eventsConsumed;
    }
}

Dispatcher::Stats Dispatcher::stats() const {
    std::lock_guard<std::mutex> guard(mutex_);
    return stats_;
}

void Dispatcher::WakeTick(GameTime game) {
    if (std::isfinite(game.AsSeconds())) {
        clocks_.RecordGameTime(game);
    }
    Step();
}

TimerToken Dispatcher::Schedule(double seconds, ClockDomain domain, time::TaskBody body) {
    if (!std::isfinite(seconds)) {
        return TimerToken{};
    }
    clocks_.Tick();
    std::lock_guard<std::mutex> guard(mutex_);
    return scheduler_.Schedule(domain, DeadlineLocked(domain, seconds), std::move(body));
}

void Dispatcher::PublishLocked() {
    PublishedState view = MakePublishedState(*state_);
    published_.store(std::make_shared<const PublishedState>(std::move(view)),
                     std::memory_order_release);
    ++stats_.published;
}

double Dispatcher::DomainNowLocked(ClockDomain domain) {
    switch (domain) {
    case ClockDomain::Game:
        return clocks_.Game().AsDays();
    case ClockDomain::Real:
        return clocks_.Real().value;
    case ClockDomain::Active:
        return clocks_.Active().value;
    }
    return 0.0;
}

double Dispatcher::DeadlineLocked(ClockDomain domain, double seconds) {
    const double delay = domain == ClockDomain::Game ? seconds / 86400.0 : seconds;
    return DomainNowLocked(domain) + (std::max<double>)(0.0, delay);
}

bool Dispatcher::HasDueChangeLocked(GameTime now) const {
    return !changes_.empty() && changes_.front().at.AsSeconds() <= now.AsSeconds();
}

}
