#include "slsf/integrations/ExternalMods.h"

#include "ExternalWire.h"

#include <algorithm>
#include <utility>

namespace slsf::integrations {
namespace {

using detail::EscapeField;
using detail::ParseAvailability;
using detail::ParseBool;
using detail::ParseDouble;
using detail::ParseHex32;
using detail::ParseLocalFormId;
using detail::ParseInt64;
using detail::ParseUint32;
using detail::SplitWireLine;
using detail::SplitWireLines;

bool IsKnownModName(std::string_view name, ExternalMod& out) noexcept {
    for (std::size_t index = 0; index < kExternalModCount; ++index) {
        const ExternalMod mod = static_cast<ExternalMod>(index);
        if (name == ExternalModName(mod)) {
            out = mod;
            return true;
        }
    }
    return false;
}

}

const char* ExternalModName(ExternalMod value) noexcept {
    switch (value) {
    case ExternalMod::PublicWhore: return "PublicWhore";
    case ExternalMod::FillHerUp: return "FillHerUp";
    case ExternalMod::FertilityMode: return "FertilityMode";
    case ExternalMod::EstrusChaurus: return "EstrusChaurus";
    case ExternalMod::EstrusSpider: return "EstrusSpider";
    case ExternalMod::HentaiPregnancy: return "HentaiPregnancy";
    case ExternalMod::FameComments: return "FameComments";
    case ExternalMod::Bimbos: return "Bimbos";
    }
    return "Unknown";
}

const char* ExternalModEsp(ExternalMod value) noexcept {
    switch (value) {
    case ExternalMod::PublicWhore: return "Public Whore.esp";
    case ExternalMod::FillHerUp: return "sr_FillHerUp.esp";
    case ExternalMod::FertilityMode: return "Fertility Mode 3 Fixes and Updates.esp";
    case ExternalMod::EstrusChaurus: return "EstrusChaurus.esp";
    case ExternalMod::EstrusSpider: return "EstrusSpider.esp";
    case ExternalMod::HentaiPregnancy: return "HentaiPregnancy.esm";
    case ExternalMod::FameComments: return "SLSFFameComments.esp";
    case ExternalMod::Bimbos: return "CustomComments.esp";
    }
    return "";
}

const ExternalModStatus* ExternalSceneSnapshot::FindMod(ExternalMod mod) const {
    const auto found = std::find_if(mods.begin(), mods.end(),
                                    [&](const ExternalModStatus& item) {
                                        return item.mod == mod;
                                    });
    return found == mods.end() ? nullptr : &*found;
}

const ExternalActorModState* ExternalSceneSnapshot::FindActor(
    const StableFormKey& actor) const {
    const auto found = std::find_if(actors.begin(), actors.end(),
                                    [&](const ExternalActorModState& item) {
                                        return item.actor == actor;
                                    });
    return found == actors.end() ? nullptr : &*found;
}

bool ParseExternalWire(std::string_view wire, const Epoch& expected,
                       ExternalSceneSnapshot& out, std::string& error) {
    error.clear();
    ExternalSceneSnapshot parsed;
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
    bool playerSeen = false;
    bool publicWhoreSeen = false;
    std::array<bool, kExternalModCount> modSeen{};
    for (std::string_view line : lines) {
        if (!SplitWireLine(line, fields)) {
            error = "wire line is malformed";
            return false;
        }
        if (!header) {
            if (fields.size() != 1 || fields[0] != "SLSFEX1") {
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
        } else if (key == "player") {
            if (playerSeen || fields.size() != 2 ||
                !ParseBool(fields[1], parsed.playerPresent)) {
                error = "player flag is invalid";
                return false;
            }
            playerSeen = true;
        } else if (key == "publicwhore") {
            if (publicWhoreSeen || fields.size() != 2 ||
                !ParseBool(fields[1], parsed.publicWhoreActive)) {
                error = "public whore flag is invalid";
                return false;
            }
            publicWhoreSeen = true;
        } else if (key == "mod") {
            ExternalMod mod{};
            if (fields.size() != 4 || !IsKnownModName(fields[1], mod)) {
                error = "mod line is invalid";
                return false;
            }
            const auto index = static_cast<std::size_t>(mod);
            if (modSeen[index]) {
                error = "duplicate mod line";
                return false;
            }
            ExternalModStatus status;
            status.mod = mod;
            if (!ParseAvailability(fields[2], status.availability)) {
                error = "mod availability is invalid";
                return false;
            }
            status.version = std::move(fields[3]);
            parsed.mods.push_back(std::move(status));
            modSeen[index] = true;
        } else if (key == "actor") {
            if (fields.size() != 10 && fields.size() != 12) {
                error = "actor line has the wrong field count";
                return false;
            }
            ExternalActorModState actor;
            if (fields[1].empty()) {
                error = "actor plugin is empty";
                return false;
            }
            actor.actor.plugin = std::move(fields[1]);
            if (!ParseLocalFormId(fields[2], actor.actor.localFormId) ||
                !ParseBool(fields[3], actor.isFemale) ||
                !ParseBool(fields[4], actor.fertilityModeBreeder) ||
                !ParseBool(fields[5], actor.hentaiPregnant) ||
                !ParseBool(fields[6], actor.estrusChaurusBreeder) ||
                !ParseBool(fields[7], actor.estrusSpiderBreeder) ||
                !ParseBool(fields[8], actor.inflationKnown) ||
                !ParseDouble(fields[9], actor.inflation)) {
                error = "actor state is invalid";
                return false;
            }
            if (parsed.actors.size() >= kMaxExternalActors) {
                error = "too many actors";
                return false;
            }
            if (fields.size() == 12) {
                for (std::size_t i = 10; i < 12; ++i) {
                    if (fields[i] == "unknown") continue;
                    std::int64_t rank = 0;
                    if (!ParseInt64(fields[i], rank) || rank < -128 || rank > 127) {
                        error = "faction rank is invalid";
                        return false;
                    }
                    (i == 10 ? actor.fertilityModeRank : actor.hentaiPregnancyRank) = static_cast<int>(rank);
                }
            }
            for (const ExternalActorModState& existing : parsed.actors) {
                if (existing.actor == actor.actor) {
                    error = "duplicate actor line";
                    return false;
                }
            }
            if (!actor.inflationKnown) {
                actor.inflation = 0.0;
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
    if (!(parsed.epoch == expected)) {
        error = "wire epoch does not match the requested scene";
        return false;
    }
    if (parsed.availability == ExternalAvailability::Available &&
        (!playerSeen || !publicWhoreSeen || !providerSeen)) {
        error = "available wire is missing required provider state";
        return false;
    }
    if (parsed.availability == ExternalAvailability::Available &&
        !std::all_of(modSeen.begin(), modSeen.end(), [](bool seen) { return seen; })) {
        error = "available wire must describe every external mod";
        return false;
    }
    out = std::move(parsed);
    return true;
}

bool ExternalModAvailable(const ExternalSceneSnapshot& snapshot, ExternalMod mod) {
    if (snapshot.availability != ExternalAvailability::Available) {
        return false;
    }
    const ExternalModStatus* status = snapshot.FindMod(mod);
    return status != nullptr && status->availability == ExternalAvailability::Available;
}

Fact ExternalProviderFact(const ExternalSceneSnapshot& snapshot) {
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

Fact ExternalModFact(const ExternalSceneSnapshot& snapshot, ExternalMod mod) {
    if (snapshot.availability != ExternalAvailability::Available) {
        return ExternalProviderFact(snapshot);
    }
    const ExternalModStatus* status = snapshot.FindMod(mod);
    if (status == nullptr) {
        return Fact{FactState::Missing};
    }
    switch (status->availability) {
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

Fact ExternalPublicWhoreFact(const ExternalSceneSnapshot& snapshot) {
    if (snapshot.availability != ExternalAvailability::Available) {
        return ExternalProviderFact(snapshot);
    }
    const ExternalModStatus* status = snapshot.FindMod(ExternalMod::PublicWhore);
    if (status == nullptr) {
        return Fact{FactState::Missing};
    }
    switch (status->availability) {
    case ExternalAvailability::Available:
        return Fact::Known(snapshot.publicWhoreActive);
    case ExternalAvailability::Missing:
        return Fact::Known(false);
    case ExternalAvailability::Incompatible:
        return Fact{FactState::Incompatible};
    case ExternalAvailability::Unknown:
        return Fact{FactState::Unknown};
    }
    return Fact{FactState::Unknown};
}

struct ExternalMods::Impl : std::enable_shared_from_this<ExternalMods::Impl> {
public:
    using Completion =
        std::function<void(const ExternalSceneSnapshot&, ExternalGate)>;

    Impl(PapyrusCalls& calls, ExternalModsOptions options = {});
    void Shutdown();


    RequestId RequestState(const Epoch& epoch, const std::vector<StableFormKey>& actors,
                           std::int64_t nowMs, Completion completion = {});
    void OnStateChanged(const Epoch& epoch);
    void OnModEvent(std::string_view eventName, std::int64_t nowMs);
    void Advance(std::int64_t nowMs);

    [[nodiscard]] ExternalGate Gate() const;
    [[nodiscard]] ExternalSceneSnapshot Snapshot() const;
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
    ExternalModsOptions options_;
    mutable std::mutex mutex_;
    ExternalSceneSnapshot snapshot_;
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

ExternalMods::Impl::Impl(PapyrusCalls& calls, ExternalModsOptions options)
    : calls_(calls), options_(std::move(options)) {}

void ExternalMods::Impl::Shutdown() {
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

std::string ExternalMods::Impl::BuildRequestPayload(
    const Epoch& epoch, const std::vector<StableFormKey>& actors) const {
    std::string payload = "SLSFEXQ1\nversion\t1\nepoch\t";
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

bool ExternalMods::Impl::SameActors(const std::vector<StableFormKey>& lhs,
                              const std::vector<StableFormKey>& rhs) const {
    return lhs.size() == rhs.size() &&
           std::is_permutation(lhs.begin(), lhs.end(), rhs.begin());
}

RequestId ExternalMods::Impl::RequestState(const Epoch& epoch,
                                     const std::vector<StableFormKey>& actors,
                                     std::int64_t nowMs, Completion completion) {
    if (!calls_.IsEpochCurrent(epoch)) {
        RequestId id;
        Completion pendingCompletion;
        Completion newCompletion;
        ExternalSceneSnapshot staleSnapshot;
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
    ExternalSceneSnapshot ready;
    ExternalGate readyGate = ExternalGate::Changed;
    bool alreadyReady = false;
    bool inFlight = false;
    RequestId supersededId;
    Completion supersededCompletion;
    ExternalSceneSnapshot supersededSnapshot;
    Completion replacedCompletion;
    ExternalSceneSnapshot replacedSnapshot;
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
            snapshot_ = ExternalSceneSnapshot{};
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

void ExternalMods::Impl::BeginRequest(const Epoch& epoch, std::int64_t nowMs) {
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

void ExternalMods::Impl::HandleResult(std::uint64_t token, PapyrusCallResult result) {
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

    ExternalSceneSnapshot parsed;
    std::string parseError;
    bool valid = ParseExternalWire(result.values.front().stringValue, expected, parsed,
                                   parseError);
    if (valid && parsed.availability == ExternalAvailability::Available) {
        std::vector<StableFormKey> parsedActors;
        parsedActors.reserve(parsed.actors.size());
        for (const ExternalActorModState& actor : parsed.actors) {
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
        ExternalSceneSnapshot staleSnapshot;
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
        ExternalSceneSnapshot failedSnapshot;
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
    ExternalSceneSnapshot snapshot;
    ExternalSceneSnapshot staleSnapshot;
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
            if (snapshot_.availability == ExternalAvailability::Unknown) {
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

void ExternalMods::Impl::HandleFailure(std::uint64_t token, PapyrusCallKind kind) {
    Completion completion;
    ExternalSceneSnapshot snapshot;
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

void ExternalMods::Impl::OnStateChanged(const Epoch& epoch) {
    RequestId id;
    Completion completion;
    ExternalSceneSnapshot snapshot;
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

void ExternalMods::Impl::OnModEvent(std::string_view eventName, std::int64_t nowMs) {
    if (eventName.empty()) {
        return;
    }
    Epoch epoch;
    bool active = false;
    {
        std::lock_guard<std::mutex> guard(mutex_);
        lastNowMs_ = std::max(lastNowMs_, nowMs);
        epoch = epoch_;
        active = active_;
    }
    if (active) {
        OnStateChanged(epoch);
    }
}

void ExternalMods::Impl::Advance(std::int64_t nowMs) {
    RequestId cancelId;
    bool start = false;
    Epoch epoch;
    Completion failedCompletion;
    ExternalSceneSnapshot failedSnapshot;
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

ExternalGate ExternalMods::Impl::Gate() const {
    std::lock_guard<std::mutex> guard(mutex_);
    return gate_;
}

ExternalSceneSnapshot ExternalMods::Impl::Snapshot() const {
    std::lock_guard<std::mutex> guard(mutex_);
    return snapshot_;
}

std::uint32_t ExternalMods::Impl::Attempts() const {
    std::lock_guard<std::mutex> guard(mutex_);
    return attempts_;
}

void ExternalMods::Impl::Cancel() {
    RequestId id;
    Completion completion;
    ExternalSceneSnapshot snapshot;
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

void ExternalMods::Impl::Reset() {
    Cancel();
    std::lock_guard<std::mutex> guard(mutex_);
    snapshot_ = {};
    requestedActors_.clear();
    dirty_ = true;
    gate_ = ExternalGate::Changed;
    attempts_ = 0;
    epoch_ = {};
}

ExternalMods::ExternalMods(PapyrusCalls& calls, ExternalModsOptions options)
    : impl_(std::make_shared<Impl>(calls, std::move(options))) {}

ExternalMods::~ExternalMods() { impl_->Shutdown(); }

RequestId ExternalMods::RequestState(const Epoch& epoch, const std::vector<StableFormKey>& actors, std::int64_t nowMs, Completion completion) {
    const auto state = impl_;
    return state->RequestState(epoch, actors, nowMs, std::move(completion));
}

void ExternalMods::OnStateChanged(const Epoch& epoch) {
    const auto state = impl_;
    return state->OnStateChanged(epoch);
}

void ExternalMods::OnModEvent(std::string_view eventName, std::int64_t nowMs) {
    const auto state = impl_;
    return state->OnModEvent(eventName, nowMs);
}

void ExternalMods::Advance(std::int64_t nowMs) {
    const auto state = impl_;
    return state->Advance(nowMs);
}

ExternalGate ExternalMods::Gate() const {
    const auto state = impl_;
    return state->Gate();
}

ExternalSceneSnapshot ExternalMods::Snapshot() const {
    const auto state = impl_;
    return state->Snapshot();
}

std::uint32_t ExternalMods::Attempts() const {
    const auto state = impl_;
    return state->Attempts();
}

std::string ExternalMods::BuildRequestPayload(const Epoch& epoch, const std::vector<StableFormKey>& actors) const {
    const auto state = impl_;
    return state->BuildRequestPayload(epoch, actors);
}

void ExternalMods::Cancel() {
    const auto state = impl_;
    return state->Cancel();
}

void ExternalMods::Reset() {
    const auto state = impl_;
    return state->Reset();
}

}
