#include "slsf/integrations/DeviousDevices.h"

#include "ExternalWire.h"

#include <algorithm>
#include <utility>

namespace slsf::integrations {
namespace {

using detail::EscapeField;
using detail::ParseAvailability;
using detail::ParseHex32;
using detail::ParseLocalFormId;
using detail::ParseInt64;
using detail::ParseUint32;
using detail::SplitWireLine;
using detail::SplitWireLines;

}

const char* DdngDeviceSlotName(DdngDeviceSlot value) noexcept {
    switch (value) {
    case DdngDeviceSlot::Hood: return "Hood";
    case DdngDeviceSlot::Harness: return "Harness";
    case DdngDeviceSlot::Belt: return "Belt";
    case DdngDeviceSlot::Bra: return "Bra";
    case DdngDeviceSlot::HeavyBondage: return "HeavyBondage";
    case DdngDeviceSlot::ArmCuffs: return "ArmCuffs";
    case DdngDeviceSlot::ArmCuffsFront: return "ArmCuffsFront";
    case DdngDeviceSlot::Armbinder: return "Armbinder";
    case DdngDeviceSlot::ArmbinderElbow: return "ArmbinderElbow";
    case DdngDeviceSlot::Gloves: return "Gloves";
    case DdngDeviceSlot::LegCuffs: return "LegCuffs";
    case DdngDeviceSlot::Boots: return "Boots";
    case DdngDeviceSlot::Gag: return "Gag";
    case DdngDeviceSlot::GagPanel: return "GagPanel";
    case DdngDeviceSlot::Suit: return "Suit";
    case DdngDeviceSlot::Corset: return "Corset";
    case DdngDeviceSlot::Blindfold: return "Blindfold";
    case DdngDeviceSlot::Lockable: return "Lockable";
    }
    return "Unknown";
}

const DdngActorState* DdngSnapshot::Find(const StableFormKey& actor) const {
    const auto found = std::find_if(actors.begin(), actors.end(),
                                    [&](const DdngActorState& item) {
                                        return item.actor == actor;
                                    });
    return found == actors.end() ? nullptr : &*found;
}

bool ParseDdngWire(std::string_view wire, const Epoch& expected, DdngSnapshot& out,
                   std::string& error) {
    error.clear();
    DdngSnapshot parsed;
    std::vector<std::string_view> lines;
    if (!SplitWireLines(wire, lines)) {
        error = "wire is empty or too large";
        return false;
    }
    std::vector<std::string> fields;
    bool header = false;
    bool version = false;
    bool availability = false;
    bool providerSeen = false;
    bool epochSeen = false;
    bool revisionSeen = false;
    for (std::string_view line : lines) {
        if (!SplitWireLine(line, fields)) {
            error = "wire line is malformed";
            return false;
        }
        if (!header) {
            if (fields.size() != 1 || fields[0] != "SLSFDD1") {
                error = "wire header is missing";
                return false;
            }
            header = true;
            continue;
        }
        if (fields.empty()) {
            continue;
        }
        const std::string& key = fields[0];
        if (key == "version") {
            std::uint32_t value = 0;
            if (version || fields.size() != 2 || !ParseUint32(fields[1], value) ||
                value != 1) {
                error = "wire version is not supported";
                return false;
            }
            version = true;
        } else if (key == "availability") {
            if (availability || fields.size() != 2 ||
                !ParseAvailability(fields[1], parsed.availability)) {
                error = "availability is invalid";
                return false;
            }
            availability = true;
        } else if (key == "provider") {
            if (providerSeen) {
                error = "duplicate provider line";
                return false;
            }
            if (fields.size() != 4) {
                error = "provider line has the wrong field count";
                return false;
            }
            providerSeen = true;
            parsed.provider.name = std::move(fields[1]);
            parsed.provider.version = std::move(fields[2]);
            if (!ParseUint32(fields[3], parsed.provider.apiVersion)) {
                error = "provider api version is invalid";
                return false;
            }
        } else if (key == "epoch") {
            std::int64_t session = 0;
            std::int64_t index = 0;
            std::int64_t scene = 0;
            if (epochSeen || fields.size() != 4 || !ParseInt64(fields[1], session) ||
                !ParseInt64(fields[2], index) || !ParseInt64(fields[3], scene) ||
                session < 0 || index < 0 || scene < 0) {
                error = "epoch line is invalid";
                return false;
            }
            parsed.epoch = Epoch{static_cast<std::uint64_t>(session),
                                 static_cast<std::uint64_t>(index),
                                 static_cast<std::uint64_t>(scene)};
            epochSeen = true;
        } else if (key == "revision") {
            std::int64_t value = 0;
            if (revisionSeen || fields.size() != 2 ||
                !ParseInt64(fields[1], value) || value < 0) {
                error = "revision is invalid";
                return false;
            }
            revisionSeen = true;
            parsed.equipmentRevision = static_cast<std::uint64_t>(value);
        } else if (key == "actor") {
            if (fields.size() != 5 || fields[1].empty()) {
                error = "actor line is invalid";
                return false;
            }
            DdngActorState actor;
            actor.actor.plugin = std::move(fields[1]);
            if (!ParseLocalFormId(fields[2], actor.actor.localFormId) ||
                !ParseHex32(fields[3], actor.bondage) ||
                !ParseHex32(fields[4], actor.devices) ||
                actor.bondage >= ddng_bondage::Total ||
                actor.devices >= (1u << kDdngDeviceSlotCount)) {
                error = "actor state is invalid";
                return false;
            }
            if (parsed.actors.size() >= kMaxExternalActors) {
                error = "too many actors";
                return false;
            }
            for (const DdngActorState& existing : parsed.actors) {
                if (existing.actor == actor.actor) {
                    error = "duplicate actor line";
                    return false;
                }
            }
            parsed.actors.push_back(std::move(actor));
        } else {
            error = "unknown wire key";
            return false;
        }
    }
    if (!header || !version || !availability || !epochSeen) {
        error = "wire is missing required fields";
        return false;
    }
    if (parsed.availability == ExternalAvailability::Available &&
        (!providerSeen || !revisionSeen)) {
        error = "available wire is missing provider state";
        return false;
    }
    if (!(parsed.epoch == expected)) {
        error = "wire epoch does not match the requested scene";
        return false;
    }
    out = std::move(parsed);
    return true;
}

Fact DdngInstallationFact(const DdngSnapshot& snapshot) {
    switch (snapshot.availability) {
    case ExternalAvailability::Available:
        return Fact::Known(true);
    case ExternalAvailability::Missing:
        return Fact::Known(false);
    case ExternalAvailability::Incompatible:
        return Fact{FactState::Incompatible};
    case ExternalAvailability::Unknown:
        return Fact{FactState::Unknown};
    }
    return Fact{FactState::Unknown};
}

void MarkDdngFactsUnavailable(gameplay::AnimationDeviousFacts& facts, FactState state) {
    const Fact value{state};
    facts.lockable = value;
    facts.hood = value;
    facts.harness = value;
    facts.belt = value;
    facts.bra = value;
    facts.heavyBondage = value;
    facts.armCuffs = value;
    facts.armCuffsFront = value;
    facts.armbinder = value;
    facts.armbinderElbow = value;
    facts.gloves = value;
    facts.legCuffs = value;
    facts.boots = value;
    facts.gag = value;
    facts.gagPanel = value;
    facts.suit = value;
    facts.corset = value;
    facts.blindfold = value;
}

DdngApplyResult ApplyDdngToAnimation(const DdngSnapshot& snapshot,
                                     const StableFormKey& actor,
                                     gameplay::AnimationDeviousFacts& facts) {
    if (snapshot.availability != ExternalAvailability::Available) {
        MarkDdngFactsUnavailable(facts, FactStateForAvailability(snapshot.availability));
        return {false, "provider is not available"};
    }
    const DdngActorState* state = snapshot.Find(actor);
    if (state == nullptr) {
        MarkDdngFactsUnavailable(facts, FactState::Unknown);
        return {false, "actor is not present in the equipment snapshot"};
    }
    const std::uint32_t mask = state->devices;
    const bool handsBound =
        (state->bondage & (ddng_bondage::HandsBound | ddng_bondage::HandsBoundNoAnim)) != 0;
    const bool blockingGag = (state->bondage & ddng_bondage::GaggedBlocking) != 0;
    const bool chastifiedGenital = (state->bondage & ddng_bondage::ChastifiedGenital) != 0;
    const bool chastifiedAnal = (state->bondage & ddng_bondage::ChastifiedAnal) != 0;
    const bool chastifiedBreasts = (state->bondage & ddng_bondage::ChastifiedBreasts) != 0;
    const bool mittens = (state->bondage & ddng_bondage::Mittens) != 0;
    facts.lockable = Fact::Known(HasDdngDevice(mask, DdngDeviceSlot::Lockable));
    facts.hood = Fact::Known(HasDdngDevice(mask, DdngDeviceSlot::Hood));
    facts.harness = Fact::Known(HasDdngDevice(mask, DdngDeviceSlot::Harness));
    facts.belt = Fact::Known(HasDdngDevice(mask, DdngDeviceSlot::Belt) ||
                             chastifiedGenital || chastifiedAnal);
    facts.bra = Fact::Known(HasDdngDevice(mask, DdngDeviceSlot::Bra) || chastifiedBreasts);
    facts.heavyBondage =
        Fact::Known(HasDdngDevice(mask, DdngDeviceSlot::HeavyBondage) || handsBound);
    facts.armCuffs = Fact::Known(HasDdngDevice(mask, DdngDeviceSlot::ArmCuffs));
    facts.armCuffsFront = Fact::Known(HasDdngDevice(mask, DdngDeviceSlot::ArmCuffsFront));
    facts.armbinder = Fact::Known(HasDdngDevice(mask, DdngDeviceSlot::Armbinder));
    facts.armbinderElbow = Fact::Known(HasDdngDevice(mask, DdngDeviceSlot::ArmbinderElbow));
    facts.gloves = Fact::Known(HasDdngDevice(mask, DdngDeviceSlot::Gloves) || mittens);
    facts.legCuffs = Fact::Known(HasDdngDevice(mask, DdngDeviceSlot::LegCuffs));
    facts.boots = Fact::Known(HasDdngDevice(mask, DdngDeviceSlot::Boots) ||
                              (state->bondage & ddng_bondage::Boots) != 0);
    facts.gag = Fact::Known(HasDdngDevice(mask, DdngDeviceSlot::Gag) || blockingGag);
    facts.gagPanel = Fact::Known(HasDdngDevice(mask, DdngDeviceSlot::GagPanel));
    facts.suit = Fact::Known(HasDdngDevice(mask, DdngDeviceSlot::Suit));
    facts.corset = Fact::Known(HasDdngDevice(mask, DdngDeviceSlot::Corset));
    facts.blindfold = Fact::Known(HasDdngDevice(mask, DdngDeviceSlot::Blindfold) ||
                                  (state->bondage & ddng_bondage::Blindfolded) != 0);
    return {true, {}};
}

struct DeviousDevices::Impl : std::enable_shared_from_this<DeviousDevices::Impl> {
public:
    using Completion = std::function<void(const DdngSnapshot&, ExternalGate)>;

    Impl(PapyrusCalls& calls, DdngOptions options = {});
    void Shutdown();


    RequestId RequestState(const Epoch& epoch, const std::vector<StableFormKey>& actors,
                           std::int64_t nowMs, Completion completion = {});
    void OnEquipmentChanged(const Epoch& epoch);
    void Advance(std::int64_t nowMs);

    [[nodiscard]] ExternalGate Gate() const;
    [[nodiscard]] DdngSnapshot Snapshot() const;
    [[nodiscard]] std::uint64_t Revision() const;
    [[nodiscard]] std::uint32_t Attempts() const;
    [[nodiscard]] std::string BuildRequestPayload(const Epoch& epoch,
                                                  const std::vector<StableFormKey>& actors) const;

    void Cancel();
    void Reset();

private:
    void BeginRequest(const Epoch& epoch, std::int64_t nowMs);
    void HandleResult(std::uint64_t token, PapyrusCallResult result);
    void HandleFailure(std::uint64_t token, PapyrusCallKind kind);
    [[nodiscard]] bool SameActors(const std::vector<StableFormKey>& lhs,
                                  const std::vector<StableFormKey>& rhs) const;

    PapyrusCalls& calls_;
    DdngOptions options_;
    mutable std::mutex mutex_;
    DdngSnapshot snapshot_;
    ExternalGate gate_{ExternalGate::Changed};
    Epoch epoch_{};
    std::vector<StableFormKey> requestedActors_;
    bool active_{};
    bool dirty_{true};
    bool shuttingDown_{};
    std::uint64_t generation_{};
    std::uint64_t pendingToken_{};
    RequestId pendingId_{};
    std::int64_t pendingStartedAtMs_{};
    std::uint32_t attempts_{};
    std::int64_t retryAtMs_{-1};
    std::int64_t lastNowMs_{};
    Completion completion_;
};

DeviousDevices::Impl::Impl(PapyrusCalls& calls, DdngOptions options)
    : calls_(calls), options_(std::move(options)) {}

void DeviousDevices::Impl::Shutdown() {
    RequestId id;
    {
        std::lock_guard<std::mutex> guard(mutex_);
        shuttingDown_ = true;
        active_ = false;
        pendingToken_ = 0;
        id = pendingId_;
        pendingId_ = {};
        completion_ = {};
    }
    if (id) {
        calls_.Cancel(id);
    }
}

std::string DeviousDevices::Impl::BuildRequestPayload(
    const Epoch& epoch, const std::vector<StableFormKey>& actors) const {
    std::string payload = "SLSFDDQ1\nversion\t1\nepoch\t";
    payload += std::to_string(epoch.session);
    payload += '\t';
    payload += std::to_string(epoch.index);
    payload += '\t';
    payload += std::to_string(epoch.scene);
    payload += '\n';
    for (const StableFormKey& actor : actors) {
        payload += "actor\t";
        payload += EscapeField(actor.plugin);
        payload += '\t';
        payload += std::to_string(actor.localFormId);
        payload += '\n';
    }
    return payload;
}

bool DeviousDevices::Impl::SameActors(const std::vector<StableFormKey>& lhs,
                                const std::vector<StableFormKey>& rhs) const {
    return lhs.size() == rhs.size() &&
           std::is_permutation(lhs.begin(), lhs.end(), rhs.begin());
}

RequestId DeviousDevices::Impl::RequestState(const Epoch& epoch,
                                       const std::vector<StableFormKey>& actors,
                                       std::int64_t nowMs, Completion completion) {
    if (!calls_.IsEpochCurrent(epoch)) {
        RequestId id;
        Completion pendingCompletion;
        Completion newCompletion;
        DdngSnapshot staleSnapshot;
        bool isCurrentScene = false;
        {
            std::lock_guard<std::mutex> guard(mutex_);
            isCurrentScene = epoch == epoch_;
            staleSnapshot = snapshot_;
            if (completion) {
                newCompletion = std::move(completion);
            }
            if (isCurrentScene) {
                active_ = false;
                dirty_ = true;
                gate_ = ExternalGate::Stale;
                snapshot_ = {};
                snapshot_.epoch = epoch;
                pendingCompletion = std::move(completion_);
                id = pendingId_;
                pendingId_ = {};
                pendingToken_ = 0;
                retryAtMs_ = -1;
            }
        }
        if (id) {
            calls_.Cancel(id);
        }
        staleSnapshot.epoch = epoch;
        if (pendingCompletion) {
            pendingCompletion(staleSnapshot, ExternalGate::Stale);
        }
        if (newCompletion) {
            newCompletion(staleSnapshot, ExternalGate::Stale);
        }
        return {};
    }
    DdngSnapshot ready;
    ExternalGate readyGate = ExternalGate::Changed;
    bool alreadyReady = false;
    bool inFlight = false;
    RequestId supersededId;
    Completion supersededCompletion;
    DdngSnapshot supersededSnapshot;
    Completion replacedCompletion;
    DdngSnapshot replacedSnapshot;
    {
        std::lock_guard<std::mutex> guard(mutex_);
        if (shuttingDown_ || actors.empty() || actors.size() > kMaxExternalActors) {
            return {};
        }
        lastNowMs_ = std::max(lastNowMs_, nowMs);
        const bool sameScene = active_ && epoch == epoch_ &&
                               SameActors(actors, requestedActors_);
        const bool closedScene = !active_ && epoch == epoch_ &&
                                 gate_ == ExternalGate::Closed;
        if (closedScene) {
            ready = snapshot_;
            readyGate = ExternalGate::Closed;
            alreadyReady = true;
        } else if (sameScene && pendingToken_ != 0) {
            if (completion) {
                if (completion_) {
                    replacedCompletion = std::move(completion_);
                    replacedSnapshot = snapshot_;
                }
                completion_ = std::move(completion);
            }
            inFlight = true;
        } else if (sameScene && !dirty_ && gate_ != ExternalGate::Waiting &&
                   gate_ != ExternalGate::Changed) {
            ready = snapshot_;
            readyGate = gate_;
            alreadyReady = true;
        } else if (sameScene &&
                   (gate_ == ExternalGate::Failed ||
                    gate_ == ExternalGate::Missing ||
                    gate_ == ExternalGate::Incompatible ||
                    gate_ == ExternalGate::Closed)) {
            ready = snapshot_;
            readyGate = gate_;
            alreadyReady = true;
        } else {
            if (completion && completion_) {
                replacedCompletion = std::move(completion_);
                replacedSnapshot = snapshot_;
            }
            if (!sameScene) {
                attempts_ = 0;
                if (pendingToken_ != 0) {
                    supersededId = pendingId_;
                    supersededCompletion = std::move(completion_);
                    supersededSnapshot = snapshot_;
                    pendingId_ = {};
                    pendingToken_ = 0;
                    retryAtMs_ = -1;
                }
            }
            epoch_ = epoch;
            requestedActors_ = actors;
            active_ = true;
            dirty_ = true;
            retryAtMs_ = -1;
            gate_ = ExternalGate::Waiting;
            snapshot_ = DdngSnapshot{};
            snapshot_.epoch = epoch;
            snapshot_.availability = ExternalAvailability::Unknown;
            if (completion) {
                completion_ = std::move(completion);
            }
        }
    }
    if (supersededId) {
        calls_.Cancel(supersededId);
    }
    if (supersededCompletion) {
        supersededCompletion(supersededSnapshot, ExternalGate::Stale);
    }
    if (replacedCompletion) {
        replacedCompletion(replacedSnapshot, ExternalGate::Changed);
    }
    if (inFlight) {
        std::lock_guard<std::mutex> guard(mutex_);
        return pendingId_;
    }
    if (alreadyReady) {
        if (completion) {
            completion(ready, readyGate);
        }
        return {};
    }
    BeginRequest(epoch, nowMs);
    std::lock_guard<std::mutex> guard(mutex_);
    return pendingId_;
}

void DeviousDevices::Impl::BeginRequest(const Epoch& epoch, std::int64_t nowMs) {
    std::uint64_t token = 0;
    std::vector<StableFormKey> actors;
    {
        std::lock_guard<std::mutex> guard(mutex_);
        if (shuttingDown_ || !active_ || pendingToken_ != 0) {
            return;
        }
        lastNowMs_ = std::max(lastNowMs_, nowMs);
        token = ++generation_;
        pendingToken_ = token;
        pendingStartedAtMs_ = nowMs;
        actors = requestedActors_;
        gate_ = ExternalGate::Waiting;
    }
    VmValue payload;
    payload.type = VmValueType::String;
    payload.stringValue = BuildRequestPayload(epoch, actors);
    std::vector<VmValue> arguments;
    arguments.push_back(std::move(payload));
    const RequestId id = calls_.SubmitGlobal(
        options_.script, options_.function, std::move(arguments),
        [weak = weak_from_this(), token](PapyrusCallResult result) {
            if (const auto state = weak.lock()) state->HandleResult(token, std::move(result));
        });
    {
        std::lock_guard<std::mutex> guard(mutex_);
        if (pendingToken_ == token) {
            pendingId_ = id;
        }
    }
}

void DeviousDevices::Impl::HandleResult(std::uint64_t token, PapyrusCallResult result) {
    Epoch expected;
    std::vector<StableFormKey> requested;
    bool failed = false;
    PapyrusCallKind failureKind = result.kind;
    {
        std::lock_guard<std::mutex> guard(mutex_);
        if (token == 0 || token != pendingToken_) {
            return;
        }
        expected = epoch_;
        requested = requestedActors_;
        if (result.kind != PapyrusCallKind::Ready || result.status != Status::Ready) {
            failed = true;
        }
    }
    if (failed) {
        HandleFailure(token, failureKind);
        return;
    }
    if (result.values.size() != 1 || result.values.front().type != VmValueType::String) {
        HandleFailure(token, PapyrusCallKind::Failed);
        return;
    }

    DdngSnapshot parsed;
    std::string parseError;
    bool valid = ParseDdngWire(result.values.front().stringValue, expected, parsed,
                               parseError);
    if (valid && parsed.availability == ExternalAvailability::Available) {
        std::vector<StableFormKey> parsedActors;
        parsedActors.reserve(parsed.actors.size());
        for (const DdngActorState& actor : parsed.actors) {
            parsedActors.push_back(actor.actor);
        }
        if (!SameActors(parsedActors, requested)) {
            valid = false;
        } else if (parsed.provider.apiVersion != options_.requiredApiVersion) {
            parsed.availability = ExternalAvailability::Incompatible;
        }
    }
    const bool currentEpoch = calls_.IsEpochCurrent(expected);
    if (!currentEpoch) {
        Completion staleCompletion;
        DdngSnapshot staleSnapshot;
        {
            std::lock_guard<std::mutex> guard(mutex_);
            if (token != pendingToken_) {
                return;
            }
            pendingToken_ = 0;
            pendingId_ = {};
            retryAtMs_ = -1;
            dirty_ = true;
            active_ = false;
            gate_ = ExternalGate::Stale;
            staleSnapshot = snapshot_;
            staleCompletion = std::move(completion_);
        }
        if (staleCompletion) {
            staleCompletion(staleSnapshot, ExternalGate::Stale);
        }
        return;
    }
    if (!valid) {
        Completion failedCompletion;
        DdngSnapshot failedSnapshot;
        {
            std::lock_guard<std::mutex> guard(mutex_);
            if (token != pendingToken_) {
                return;
            }
            pendingToken_ = 0;
            pendingId_ = {};
            ++attempts_;
            if (attempts_ >= options_.timing.maxAttempts) {
                gate_ = ExternalGate::Failed;
                retryAtMs_ = -1;
                failedSnapshot = snapshot_;
                failedCompletion = std::move(completion_);
            } else {
                gate_ = ExternalGate::Waiting;
                retryAtMs_ = lastNowMs_ + options_.timing.retryIntervalMs;
            }
        }
        if (failedCompletion) {
            failedCompletion(failedSnapshot, ExternalGate::Failed);
        }
        return;
    }

    Completion completion;
    Completion staleCompletion;
    DdngSnapshot snapshot;
    DdngSnapshot staleSnapshot;
    ExternalGate gate = ExternalGate::Waiting;
    bool staleAtCommit = false;
    {
        std::lock_guard<std::mutex> guard(mutex_);
        if (token != pendingToken_) {
            return;
        }
        pendingToken_ = 0;
        pendingId_ = {};
        retryAtMs_ = -1;
        if (!currentEpoch) {
            attempts_ = 0;
            active_ = false;
            dirty_ = true;
            gate_ = ExternalGate::Stale;
            staleAtCommit = true;
            staleSnapshot = snapshot_;
            staleCompletion = std::move(completion_);
        } else {
            snapshot_ = std::move(parsed);
            snapshot_.epoch = expected;
            dirty_ = false;
            if (snapshot_.availability == ExternalAvailability::Available) {
                attempts_ = 0;
                gate_ = ExternalGate::Ready;
                completion = std::move(completion_);
            } else if (snapshot_.availability == ExternalAvailability::Unknown) {
                ++attempts_;
                if (attempts_ >= options_.timing.maxAttempts) {
                    gate_ = ExternalGate::Failed;
                    retryAtMs_ = -1;
                    completion = std::move(completion_);
                } else {
                    gate_ = ExternalGate::Waiting;
                    retryAtMs_ = lastNowMs_ + options_.timing.retryIntervalMs;
                }
            } else {
                attempts_ = 0;
                gate_ = GateForAvailability(snapshot_.availability);
                completion = std::move(completion_);
            }
            gate = gate_;
            snapshot = snapshot_;
        }
    }
    if (staleAtCommit) {
        if (staleCompletion) {
            staleCompletion(staleSnapshot, ExternalGate::Stale);
        }
        return;
    }
    if (completion) {
        completion(snapshot, gate);
    }
}

void DeviousDevices::Impl::HandleFailure(std::uint64_t token, PapyrusCallKind kind) {
    Completion completion;
    DdngSnapshot snapshot;
    ExternalGate gate = ExternalGate::Waiting;
    {
        std::lock_guard<std::mutex> guard(mutex_);
        if (token == 0 || token != pendingToken_) {
            return;
        }
        pendingToken_ = 0;
        pendingId_ = {};
        if (kind == PapyrusCallKind::Stale) {
            gate_ = ExternalGate::Stale;
            retryAtMs_ = -1;
            dirty_ = true;
            active_ = false;
            completion = std::move(completion_);
        } else if (attempts_ + 1 >= options_.timing.maxAttempts) {
            attempts_ = options_.timing.maxAttempts;
            gate_ = ExternalGate::Failed;
            retryAtMs_ = -1;
            completion = std::move(completion_);
        } else {
            ++attempts_;
            gate_ = ExternalGate::Waiting;
            retryAtMs_ = lastNowMs_ + options_.timing.retryIntervalMs;
        }
        gate = gate_;
        snapshot = snapshot_;
    }
    if (completion) {
        completion(snapshot, gate);
    }
}

void DeviousDevices::Impl::OnEquipmentChanged(const Epoch& epoch) {
    RequestId id;
    Completion completion;
    DdngSnapshot snapshot;
    {
        std::lock_guard<std::mutex> guard(mutex_);
        if (!active_ || epoch != epoch_) {
            return;
        }
        dirty_ = true;
        gate_ = ExternalGate::Changed;
        pendingToken_ = 0;
        id = pendingId_;
        pendingId_ = {};
        attempts_ = 0;
        retryAtMs_ = 0;
        snapshot = snapshot_;
        completion = std::move(completion_);
    }
    if (id) {
        calls_.Cancel(id);
    }
    if (completion) {
        completion(snapshot, ExternalGate::Changed);
    }
}

void DeviousDevices::Impl::Advance(std::int64_t nowMs) {
    RequestId cancelId;
    bool start = false;
    Epoch epoch;
    Completion failedCompletion;
    DdngSnapshot failedSnapshot;
    {
        std::lock_guard<std::mutex> guard(mutex_);
        lastNowMs_ = std::max(lastNowMs_, nowMs);
        if (pendingToken_ != 0 && options_.timing.timeoutMs > 0 &&
            nowMs - pendingStartedAtMs_ >= options_.timing.timeoutMs) {
            cancelId = pendingId_;
            pendingToken_ = 0;
            pendingId_ = {};
            ++attempts_;
            if (attempts_ >= options_.timing.maxAttempts) {
                gate_ = ExternalGate::Failed;
                retryAtMs_ = -1;
                failedSnapshot = snapshot_;
                failedCompletion = std::move(completion_);
            } else {
                gate_ = ExternalGate::Waiting;
                retryAtMs_ = lastNowMs_ + options_.timing.retryIntervalMs;
            }
        }
        if (pendingToken_ == 0 && active_ && retryAtMs_ >= 0 && nowMs >= retryAtMs_) {
            retryAtMs_ = -1;
            epoch = epoch_;
            start = true;
        }
    }
    if (cancelId) {
        calls_.Cancel(cancelId);
    }
    if (failedCompletion) {
        failedCompletion(failedSnapshot, ExternalGate::Failed);
    }
    if (start) {
        BeginRequest(epoch, nowMs);
    }
}

ExternalGate DeviousDevices::Impl::Gate() const {
    std::lock_guard<std::mutex> guard(mutex_);
    return gate_;
}

DdngSnapshot DeviousDevices::Impl::Snapshot() const {
    std::lock_guard<std::mutex> guard(mutex_);
    return snapshot_;
}

std::uint64_t DeviousDevices::Impl::Revision() const {
    std::lock_guard<std::mutex> guard(mutex_);
    return snapshot_.equipmentRevision;
}

std::uint32_t DeviousDevices::Impl::Attempts() const {
    std::lock_guard<std::mutex> guard(mutex_);
    return attempts_;
}

void DeviousDevices::Impl::Cancel() {
    RequestId id;
    Completion completion;
    DdngSnapshot snapshot;
    {
        std::lock_guard<std::mutex> guard(mutex_);
        active_ = false;
        gate_ = ExternalGate::Closed;
        pendingToken_ = 0;
        id = pendingId_;
        pendingId_ = {};
        retryAtMs_ = -1;
        snapshot = snapshot_;
        completion = std::move(completion_);
    }
    if (id) {
        calls_.Cancel(id);
    }
    if (completion) {
        completion(snapshot, ExternalGate::Closed);
    }
}

void DeviousDevices::Impl::Reset() {
    Cancel();
    std::lock_guard<std::mutex> guard(mutex_);
    snapshot_ = {};
    requestedActors_.clear();
    dirty_ = true;
    gate_ = ExternalGate::Changed;
    attempts_ = 0;
    epoch_ = {};
}

DeviousDevices::DeviousDevices(PapyrusCalls& calls, DdngOptions options)
    : impl_(std::make_shared<Impl>(calls, std::move(options))) {}

DeviousDevices::~DeviousDevices() { impl_->Shutdown(); }

RequestId DeviousDevices::RequestState(const Epoch& epoch, const std::vector<StableFormKey>& actors, std::int64_t nowMs, Completion completion) {
    const auto state = impl_;
    return state->RequestState(epoch, actors, nowMs, std::move(completion));
}

void DeviousDevices::OnEquipmentChanged(const Epoch& epoch) {
    const auto state = impl_;
    return state->OnEquipmentChanged(epoch);
}

void DeviousDevices::Advance(std::int64_t nowMs) {
    const auto state = impl_;
    return state->Advance(nowMs);
}

ExternalGate DeviousDevices::Gate() const {
    const auto state = impl_;
    return state->Gate();
}

DdngSnapshot DeviousDevices::Snapshot() const {
    const auto state = impl_;
    return state->Snapshot();
}

std::uint64_t DeviousDevices::Revision() const {
    const auto state = impl_;
    return state->Revision();
}

std::uint32_t DeviousDevices::Attempts() const {
    const auto state = impl_;
    return state->Attempts();
}

std::string DeviousDevices::BuildRequestPayload(const Epoch& epoch, const std::vector<StableFormKey>& actors) const {
    const auto state = impl_;
    return state->BuildRequestPayload(epoch, actors);
}

void DeviousDevices::Cancel() {
    const auto state = impl_;
    return state->Cancel();
}

void DeviousDevices::Reset() {
    const auto state = impl_;
    return state->Reset();
}

}
