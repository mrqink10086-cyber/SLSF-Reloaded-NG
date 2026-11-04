#include "slsf/integrations/SexLabPPlus.h"

#include "ExternalWire.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace slsf::integrations {
namespace {

using detail::ParseAvailability;
using detail::ParseBool;
using detail::ParseHex32;
using detail::ParseLocalFormId;
using detail::ParseInt64;
using detail::ParseUint32;
using detail::SplitWireLine;
using detail::SplitWireLines;

bool EndsWith(std::string_view text, std::string_view suffix) noexcept {
    return text.size() >= suffix.size() &&
           text.substr(text.size() - suffix.size()) == suffix;
}

char LowerAscii(char value) noexcept {
    return value >= 'A' && value <= 'Z' ? static_cast<char>(value - 'A' + 'a') : value;
}

std::string CanonicalTag(std::string_view name) {
    std::string lower;
    lower.reserve(name.size());
    for (const char value : name) {
        lower.push_back(LowerAscii(value));
    }
    if (lower == "oral") return "Oral";
    if (lower == "anal") return "Anal";
    if (lower == "orgy") return "Orgy";
    if (lower == "aggressive") return "Aggressive";
    if (lower == "leadin") return "LeadIn";
    if (lower == "bodysearch") return "BodySearch";
    if (lower == "creature") return "Creature";
    if (lower == "bestiality") return "Bestiality";
    if (lower == "loving") return "Loving";
    if (lower == "hugging") return "Hugging";
    if (lower == "cuddling") return "Cuddling";
    if (lower == "kissing") return "Kissing";
    return std::string(name);
}

gameplay::AnimationRace ParseRace(std::string_view text) noexcept {
    if (text == "orc" || text == "Orc") {
        return gameplay::AnimationRace::Orc;
    }
    if (text == "khajiit" || text == "Khajiit") {
        return gameplay::AnimationRace::Khajiit;
    }
    if (text == "argonian" || text == "Argonian") {
        return gameplay::AnimationRace::Argonian;
    }
    if (text == "other" || text == "Other") {
        return gameplay::AnimationRace::Other;
    }
    return gameplay::AnimationRace::Unknown;
}

bool ParseScope(std::string_view text, PPlusTagScope& out) noexcept {
    if (text == "thread") {
        out = PPlusTagScope::Thread;
        return true;
    }
    if (text == "scene") {
        out = PPlusTagScope::Scene;
        return true;
    }
    if (text == "stage") {
        out = PPlusTagScope::Stage;
        return true;
    }
    return false;
}

bool ParseActor(std::vector<std::string>& fields, PPlusActorState& out,
                std::string& error) {
    if (fields.size() < 8 || fields.size() > 10) {
        error = "actor line has the wrong field count";
        return false;
    }
    out.actor.plugin = std::move(fields[1]);
    if (out.actor.plugin.empty()) {
        error = "actor plugin is empty";
        return false;
    }
    if (!ParseLocalFormId(fields[2], out.actor.localFormId)) {
        error = "actor form id is invalid";
        return false;
    }
    out.name = std::move(fields[3]);
    out.race = ParseRace(fields[4]);
    std::uint32_t flags = 0;
    if (!ParseHex32(fields[5], flags) || flags > 0xFFu) {
        error = "actor flags are invalid";
        return false;
    }
    out.isPlayer = (flags & 0x80u) != 0;
    out.masochist = (flags & 0x40u) != 0;
    out.submissive = (flags & 0x20u) != 0;
    out.dominant = (flags & 0x10u) != 0;
    out.aggressor = (flags & 0x08u) != 0;
    out.victim = (flags & 0x04u) != 0;
    out.strapon = (flags & 0x02u) != 0;
    out.female = (flags & 0x01u) != 0;
    if (fields.size() >= 9 && (!ParseHex32(fields[8], out.roleUnknown) || out.roleUnknown > 0x7f)) {
        error = "actor role unknown mask is invalid";
        return false;
    }
    if (fields.size() == 10 && fields[9] != "unknown") {
        std::uint32_t sex{};
        if (!ParseUint32(fields[9], sex) || sex > 4) {
            error = "actor sex code is invalid";
            return false;
        }
        out.sexCode = static_cast<std::uint8_t>(sex);
    } else if (fields.size() == 10) {
        out.roleUnknown |= 1;
    }
    if (!ParseHex32(fields[6], out.interactions)) {
        error = "interaction mask is invalid";
        return false;
    }
    if (!ParseHex32(fields[7], out.interactionUnknown)) {
        error = "unknown interaction mask is invalid";
        return false;
    }
    constexpr std::uint32_t kKnownInteractionMask =
        (1u << static_cast<std::uint32_t>(kPPlusInteractionCount)) - 1u;
    if ((out.interactions & ~kKnownInteractionMask) != 0 ||
        (out.interactionUnknown & ~kKnownInteractionMask) != 0 ||
        (out.interactions & out.interactionUnknown) != 0) {
        error = "interaction masks are inconsistent";
        return false;
    }
    return true;
}

}

const char* PPlusInteractionName(PPlusInteraction value) noexcept {
    switch (value) {
    case PPlusInteraction::pStimulation: return "pStimulation";
    case PPlusInteraction::aAnimObjFace: return "aAnimObjFace";
    case PPlusInteraction::pAnimObjFace: return "pAnimObjFace";
    case PPlusInteraction::pSuckingToes: return "pSuckingToes";
    case PPlusInteraction::pGrinding: return "pGrinding";
    case PPlusInteraction::pSkullfuck: return "pSkullfuck";
    case PPlusInteraction::aHandJob: return "aHandJob";
    case PPlusInteraction::aFootJob: return "aFootJob";
    case PPlusInteraction::aBoobJob: return "aBoobJob";
    case PPlusInteraction::bKissing: return "bKissing";
    case PPlusInteraction::aSuckingToes: return "aSuckingToes";
    case PPlusInteraction::pFacial: return "pFacial";
    case PPlusInteraction::aOral: return "aOral";
    case PPlusInteraction::aLickingShaft: return "aLickingShaft";
    case PPlusInteraction::aDeepthroat: return "aDeepthroat";
    case PPlusInteraction::pVaginal: return "pVaginal";
    case PPlusInteraction::pAnal: return "pAnal";
    case PPlusInteraction::aFacial: return "aFacial";
    case PPlusInteraction::aGrinding: return "aGrinding";
    case PPlusInteraction::pHandJob: return "pHandJob";
    case PPlusInteraction::pFootJob: return "pFootJob";
    case PPlusInteraction::pBoobJob: return "pBoobJob";
    case PPlusInteraction::pLickingShaft: return "pLickingShaft";
    case PPlusInteraction::pOral: return "pOral";
    case PPlusInteraction::pDeepthroat: return "pDeepthroat";
    case PPlusInteraction::aSkullfuck: return "aSkullfuck";
    case PPlusInteraction::aVaginal: return "aVaginal";
    case PPlusInteraction::aAnal: return "aAnal";
    }
    return "unknown";
}

const char* PPlusTagScopeName(PPlusTagScope value) noexcept {
    switch (value) {
    case PPlusTagScope::Thread: return "Thread";
    case PPlusTagScope::Scene: return "Scene";
    case PPlusTagScope::Stage: return "Stage";
    }
    return "Unknown";
}

gameplay::Tri PPlusActorState::InteractionState(PPlusInteraction value) const noexcept {
    const std::uint32_t bit = 1u << static_cast<std::uint32_t>(value);
    if ((interactionUnknown & bit) != 0) {
        return gameplay::Tri::Unknown;
    }
    return (interactions & bit) != 0 ? gameplay::Tri::True : gameplay::Tri::False;
}

bool PPlusActorState::HasInteraction(PPlusInteraction value) const noexcept {
    return InteractionState(value) == gameplay::Tri::True;
}

std::vector<std::string> PPlusSceneState::TagsAtScope(PPlusTagScope scope) const {
    std::vector<std::string> result;
    for (const PPlusTag& tag : tags) {
        if (tag.scope == scope && !tag.name.empty()) {
            result.push_back(tag.name);
        }
    }
    return result;
}

std::vector<std::string> PPlusSceneState::ClassificationTags() const {
    std::vector<std::string> result;
    for (const PPlusTag& tag : tags) {
        if (tag.scope == PPlusTagScope::Scene && !tag.name.empty()) {
            result.push_back(CanonicalTag(tag.name));
        }
    }
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

bool ParsePPlusSceneWire(std::string_view wire, const Epoch& expected,
                         PPlusSceneState& out, std::string& error) {
    error.clear();
    PPlusSceneState parsed;
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
    bool activeSeen = false;
    bool stageSeen = false;
    bool animationSeen = false;
    bool sceneSeen = false;
    for (std::string_view line : lines) {
        if (!SplitWireLine(line, fields)) {
            error = "wire line is malformed";
            return false;
        }
        if (!header) {
            if (fields.size() != 1 || fields[0] != "SLSFPP1") {
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
        } else if (key == "active") {
            if (activeSeen || fields.size() != 2 ||
                !ParseBool(fields[1], parsed.active)) {
                error = "active flag is invalid";
                return false;
            }
            activeSeen = true;
        } else if (key == "stage") {
            if (stageSeen || fields.size() != 3 ||
                !ParseUint32(fields[1], parsed.stage) ||
                !ParseUint32(fields[2], parsed.stageCount)) {
                error = "stage line is invalid";
                return false;
            }
            if (parsed.stageCount != 0 && parsed.stage >= parsed.stageCount) {
                error = "stage index is outside the stage range";
                return false;
            }
            stageSeen = true;
        } else if (key == "scene") {
            bool leadIn = false;
            if (sceneSeen || fields.size() != 4 || fields[1].empty() || fields[2].empty() ||
                !ParseBool(fields[3], leadIn)) {
                error = "scene identity is invalid";
                return false;
            }
            sceneSeen = true;
            parsed.sceneId = std::move(fields[1]);
            parsed.stageId = std::move(fields[2]);
            parsed.leadIn = Fact::Known(leadIn);
        } else if (key == "animation") {
            if (animationSeen || fields.size() != 4 || fields[1].empty() ||
                !ParseLocalFormId(fields[2], parsed.animation.localFormId)) {
                error = "animation line is invalid";
                return false;
            }
            animationSeen = true;
            parsed.animation.plugin = std::move(fields[1]);
            parsed.animationName = std::move(fields[3]);
        } else if (key == "tag") {
            PPlusTag tag;
            if (fields.size() != 3 || !ParseScope(fields[1], tag.scope) ||
                fields[2].empty()) {
                error = "tag line is invalid";
                return false;
            }
            tag.name = std::move(fields[2]);
            if (parsed.tags.size() >= kMaxExternalTags) {
                error = "too many tags";
                return false;
            }
            parsed.tags.push_back(std::move(tag));
        } else if (key == "actor") {
            PPlusActorState actor;
            if (!ParseActor(fields, actor, error)) {
                return false;
            }
            if (parsed.actors.size() >= kMaxExternalActors) {
                error = "too many actors";
                return false;
            }
            for (const PPlusActorState& existing : parsed.actors) {
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
    if (!header || !version || !availability || !epochSeen || !activeSeen || !stageSeen) {
        error = "wire is missing required fields";
        return false;
    }
    if (!(parsed.epoch == expected)) {
        error = "wire epoch does not match the requested scene";
        return false;
    }
    if (parsed.availability == ExternalAvailability::Available && !providerSeen) {
        error = "available wire is missing provider state";
        return false;
    }
    if (parsed.availability == ExternalAvailability::Available && parsed.active &&
        parsed.actors.empty()) {
        error = "active scene has no actors";
        return false;
    }
    if (parsed.availability == ExternalAvailability::Available && parsed.active &&
        std::count_if(parsed.actors.begin(), parsed.actors.end(), [](const auto& actor) {
            return actor.isPlayer;
        }) != 1) {
        error = "active scene must contain exactly one player";
        return false;
    }
    out = std::move(parsed);
    return true;
}

PPlusApplyResult ApplyPPlusToAnimation(const PPlusSceneState& state,
                                       gameplay::AnimationFacts& facts) {
    facts.playerIsFemale = Fact{};
    facts.usingStrapon = Fact{};
    facts.victim = Fact{};
    facts.aggressor = Fact{};
    facts.oralInteraction.reset();
    facts.analInteraction.reset();
    facts.actors.clear();
    facts.tags.clear();
    facts.maleCount = {};
    facts.femaleCount = {};
    facts.leadIn = state.leadIn;
    if (state.availability != ExternalAvailability::Available) {
        return {false, "provider is not available"};
    }
    if (!state.active) {
        return {false, "scene is not active"};
    }
    if (state.actors.empty()) {
        return {false, "scene has no validated actors"};
    }
    if (state.actors.size() > kMaxExternalActors) {
        return {false, "scene has too many actors"};
    }
    if (std::count_if(state.actors.begin(), state.actors.end(),
                     [](const auto& actor) { return actor.isPlayer; }) != 1) {
        return {false, "scene must contain exactly one player"};
    }

    const PPlusActorState* player = nullptr;
    std::vector<gameplay::AnimationActorFacts> actors;
    actors.reserve(state.actors.size());
    int maleCount = 0;
    int femaleCount = 0;
    bool sexesKnown = true;
    gameplay::Tri oral = gameplay::Tri::False;
    gameplay::Tri anal = gameplay::Tri::False;
    bool facial = false;
    for (const PPlusActorState& source : state.actors) {
        gameplay::AnimationActorFacts actor;
        actor.isPlayer = source.isPlayer;
        actor.race = source.race;
        actors.push_back(actor);
        if (source.isPlayer) {
            player = &source;
        }
        sexesKnown = sexesKnown && (source.roleUnknown & 1) == 0;
        if (source.sexCode) {
            maleCount += *source.sexCode == 0;
            femaleCount += *source.sexCode == 1;
        } else if (source.female) {
            ++femaleCount;
        } else {
            ++maleCount;
        }
        for (const auto interaction : {PPlusInteraction::aOral, PPlusInteraction::pOral,
                PPlusInteraction::aLickingShaft, PPlusInteraction::pLickingShaft,
                PPlusInteraction::aDeepthroat, PPlusInteraction::pDeepthroat}) {
            oral = gameplay::TriOr(oral, source.InteractionState(interaction));
        }
        anal = gameplay::TriOr(anal, gameplay::TriOr(source.InteractionState(PPlusInteraction::aAnal),
                                                   source.InteractionState(PPlusInteraction::pAnal)));
        facial = facial || source.HasInteraction(PPlusInteraction::aFacial) ||
                           source.HasInteraction(PPlusInteraction::pFacial);
    }

    facts.actors = std::move(actors);
    if (sexesKnown) {
        facts.maleCount = gameplay::ObservedNumber::Known(maleCount);
        facts.femaleCount = gameplay::ObservedNumber::Known(femaleCount);
    }
    facts.tags = state.ClassificationTags();
    const auto interactionFact = [](gameplay::Tri value) {
        return value == gameplay::Tri::Unknown ? Fact{} : Fact::Known(value == gameplay::Tri::True);
    };
    facts.oralInteraction = interactionFact(oral);
    facts.analInteraction = interactionFact(anal);
    if (oral == gameplay::Tri::True && std::find(facts.tags.begin(), facts.tags.end(), "Oral") ==
                    facts.tags.end()) {
        facts.tags.push_back("Oral");
    }
    if (anal == gameplay::Tri::True && std::find(facts.tags.begin(), facts.tags.end(), "Anal") ==
                    facts.tags.end()) {
        facts.tags.push_back("Anal");
    }
    if (facial && std::find(facts.tags.begin(), facts.tags.end(), "Facial") == facts.tags.end()) {
        facts.tags.push_back("Facial");
    }
    std::sort(facts.tags.begin(), facts.tags.end());
    if (player != nullptr) {
        facts.playerIsFemale = (player->roleUnknown & 1) ? Fact{} : Fact::Known(player->female);
        facts.usingStrapon = (player->roleUnknown & 2) ? Fact{} : Fact::Known(player->strapon);
        facts.victim = (player->roleUnknown & 4) ? Fact{} : Fact::Known(player->victim);
        facts.aggressor = (player->roleUnknown & 8) ? Fact{} : Fact::Known(player->aggressor);
    }
    return {true, {}};
}

struct SexLabPPlus::Impl : std::enable_shared_from_this<SexLabPPlus::Impl> {
public:
    using Completion = std::function<void(const PPlusSceneState&, ExternalGate)>;

    Impl(PapyrusCalls& calls, PPlusOptions options = {});
    void Shutdown();
    bool SelectNotificationThread(std::string_view event, std::int32_t id, const Epoch& epoch) {
        if (!calls_.IsEpochCurrent(epoch)) return false;
        const bool start = EndsWith(event, "AnimationStart") || EndsWith(event, "SceneStart");
        const bool transition = EndsWith(event, "LeadInEnd") || EndsWith(event, "StageStart") ||
            EndsWith(event, "StageChange") || EndsWith(event, "AnimationEnd") || EndsWith(event, "SceneEnd");
        if (!start && !transition) return false;
        std::lock_guard guard(mutex_);
        if (shuttingDown_ || (!start && id != threadId_)) return false;
        threadId_ = id;
        return true;
    }


    RequestId OnSceneEvent(std::string_view event, const Epoch& epoch, std::int64_t nowMs,
                           Completion completion = {});
    RequestId RequestScene(const Epoch& epoch, std::int64_t nowMs,
                           Completion completion = {});
    void Advance(std::int64_t nowMs);
    void OnSceneEnded(const Epoch& epoch);
    void MarkDirty();

    [[nodiscard]] ExternalGate Gate() const;
    [[nodiscard]] PPlusSceneState Snapshot() const;
    [[nodiscard]] bool Active() const;
    [[nodiscard]] std::uint32_t Attempts() const;

    void Cancel();
    void Reset();

private:
    struct Pending {
        RequestId id;
        std::uint64_t token{};
        Epoch epoch{};
        std::int64_t startedAtMs{};
    };

    void BeginRequest(const Epoch& epoch, std::int64_t nowMs, Completion completion);
    void HandleResult(std::uint64_t token, PapyrusCallResult result);
    void HandleFailure(std::uint64_t token, PapyrusCallKind kind);
    void CancelPending();
    [[nodiscard]] bool SameEpoch(const Epoch& lhs, const Epoch& rhs) const noexcept;

    PapyrusCalls& calls_;
    PPlusOptions options_;
    mutable std::mutex mutex_;
    PPlusSceneState snapshot_;
    ExternalGate gate_{ExternalGate::Waiting};
    Epoch sceneEpoch_{};
    bool active_{};
    bool dirty_{true};
    bool shuttingDown_{};
    std::uint64_t generation_{};
    std::uint64_t pendingToken_{};
    Pending pending_{};
    std::uint32_t attempts_{};
    std::int64_t retryAtMs_{-1};
    std::int64_t lastNowMs_{};
    Completion completion_;
    bool awaitingMainStage_{};
    PPlusSceneState transitionFrom_;
    std::int32_t threadId_{-1};
public:
    void SetThreadId(std::int32_t id) { std::lock_guard guard(mutex_); threadId_ = id; }
    std::int32_t ThreadId() const { std::lock_guard guard(mutex_); return threadId_; }
};

SexLabPPlus::Impl::Impl(PapyrusCalls& calls, PPlusOptions options)
    : calls_(calls), options_(std::move(options)) {}

void SexLabPPlus::Impl::Shutdown() {
    RequestId id;
    {
        std::lock_guard<std::mutex> guard(mutex_);
        shuttingDown_ = true;
        active_ = false;
        pendingToken_ = 0;
        id = pending_.id;
        pending_ = {};
        completion_ = {};
    }
    if (id) {
        calls_.Cancel(id);
    }
}

bool SexLabPPlus::Impl::SameEpoch(const Epoch& lhs, const Epoch& rhs) const noexcept {
    return lhs == rhs;
}

RequestId SexLabPPlus::Impl::OnSceneEvent(std::string_view event, const Epoch& epoch,
                                    std::int64_t nowMs, Completion completion) {
    const bool startEvent =
        EndsWith(event, "AnimationStart") || EndsWith(event, "SceneStart") ||
        EndsWith(event, "StageStart") || EndsWith(event, "StageChange") ||
        EndsWith(event, "LeadInEnd");
    if (!calls_.IsEpochCurrent(epoch)) return {};
    bool current = false;
    {
        std::lock_guard<std::mutex> guard(mutex_);
        if (active_) {
            current = SameEpoch(sceneEpoch_, epoch) ||
                      startEvent;
        } else {
            current = true;
        }
    }
    if (!current) {
        return {};
    }
    if (EndsWith(event, "AnimationEnd") || EndsWith(event, "SceneEnd")) {
        OnSceneEnded(epoch);
        return {};
    }
    if (startEvent) {
        {
            std::lock_guard<std::mutex> guard(mutex_);
            if (EndsWith(event, "LeadInEnd")) {
                transitionFrom_ = snapshot_;
                awaitingMainStage_ = true;
            } else if (EndsWith(event, "AnimationStart") || EndsWith(event, "SceneStart")) {
                awaitingMainStage_ = false;
            }
        }
        MarkDirty();
        return RequestScene(epoch, nowMs, std::move(completion));
    }
    return {};
}

RequestId SexLabPPlus::Impl::RequestScene(const Epoch& epoch, std::int64_t nowMs,
                                    Completion completion) {
    if (!calls_.IsEpochCurrent(epoch)) {
        RequestId id;
        Completion pendingCompletion;
        Completion newCompletion;
        PPlusSceneState staleState;
        bool isCurrentScene = false;
        {
            std::lock_guard<std::mutex> guard(mutex_);
            isCurrentScene = SameEpoch(sceneEpoch_, epoch);
            staleState = snapshot_;
            if (completion) {
                newCompletion = std::move(completion);
            }
            if (isCurrentScene) {
                active_ = false;
                dirty_ = true;
                gate_ = ExternalGate::Stale;
                snapshot_.active = false;
                pendingCompletion = std::move(completion_);
                id = pending_.id;
                pendingToken_ = 0;
                pending_ = {};
                retryAtMs_ = -1;
            }
        }
        if (id) {
            calls_.Cancel(id);
        }
        staleState.epoch = epoch;
        staleState.active = false;
        if (pendingCompletion) {
            pendingCompletion(staleState, ExternalGate::Stale);
        }
        if (newCompletion) {
            newCompletion(staleState, ExternalGate::Stale);
        }
        return {};
    }
    bool alreadyReady = false;
    bool inFlight = false;
    PPlusSceneState readyState;
    ExternalGate readyGate = ExternalGate::Waiting;
    RequestId supersededId;
    Completion supersededCompletion;
    PPlusSceneState supersededSnapshot;
    Completion replacedCompletion;
    PPlusSceneState replacedSnapshot;
    {
        std::lock_guard<std::mutex> guard(mutex_);
        if (shuttingDown_) {
            return {};
        }
        lastNowMs_ = std::max(lastNowMs_, nowMs);
        const bool sameScene = active_ && SameEpoch(sceneEpoch_, epoch);
        const bool closedScene =
            !active_ && SameEpoch(sceneEpoch_, epoch) && gate_ == ExternalGate::Closed;
        if (closedScene) {
            alreadyReady = true;
            readyState = snapshot_;
            readyGate = ExternalGate::Closed;
        } else if (sameScene && pendingToken_ != 0) {
            if (completion) {
                if (completion_) {
                    replacedCompletion = std::move(completion_);
                    replacedSnapshot = snapshot_;
                }
                completion_ = std::move(completion);
            }
            inFlight = true;
        } else if (sameScene && !dirty_ && gate_ == ExternalGate::Ready) {
            alreadyReady = true;
            readyState = snapshot_;
            readyGate = gate_;
        } else if (sameScene &&
                   (gate_ == ExternalGate::Failed ||
                    gate_ == ExternalGate::Missing ||
                    gate_ == ExternalGate::Incompatible ||
                    gate_ == ExternalGate::Closed)) {
            alreadyReady = true;
            readyState = snapshot_;
            readyGate = gate_;
        } else {
            if (completion && completion_) {
                replacedCompletion = std::move(completion_);
                replacedSnapshot = snapshot_;
            }
            if (!sameScene) {
                attempts_ = 0;
                if (pendingToken_ != 0) {
                    supersededId = pending_.id;
                    supersededCompletion = std::move(completion_);
                    supersededSnapshot = snapshot_;
                    pendingToken_ = 0;
                    pending_ = {};
                    retryAtMs_ = -1;
                }
            }
            sceneEpoch_ = epoch;
            active_ = true;
            dirty_ = true;
            retryAtMs_ = -1;
            gate_ = ExternalGate::Waiting;
            snapshot_ = PPlusSceneState{};
            snapshot_.epoch = epoch;
            snapshot_.active = false;
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
        return pending_.id;
    }
    if (alreadyReady) {
        if (completion) {
            completion(readyState, readyGate);
        }
        return {};
    }
    BeginRequest(epoch, nowMs, std::move(completion));
    std::lock_guard<std::mutex> guard(mutex_);
    return pending_.id;
}

void SexLabPPlus::Impl::BeginRequest(const Epoch& epoch, std::int64_t nowMs,
                               Completion completion) {
    std::uint64_t token = 0;
    {
        std::lock_guard<std::mutex> guard(mutex_);
        if (shuttingDown_ || !active_ || pendingToken_ != 0) {
            return;
        }
        lastNowMs_ = std::max(lastNowMs_, nowMs);
        token = ++generation_;
        pendingToken_ = token;
        pending_ = Pending{};
        pending_.token = token;
        pending_.epoch = epoch;
        pending_.startedAtMs = nowMs;
        if (completion) {
            completion_ = std::move(completion);
        }
        gate_ = ExternalGate::Waiting;
    }

    std::vector<VmValue> arguments;
    arguments.reserve(4);
    VmValue thread;
    thread.type = VmValueType::Int;
    { std::lock_guard guard(mutex_); thread.intValue = threadId_; }
    arguments.push_back(std::move(thread));
    VmValue session;
    session.type = VmValueType::String;
    session.stringValue = std::to_string(epoch.session);
    VmValue index;
    index.type = VmValueType::String;
    index.stringValue = std::to_string(epoch.index);
    VmValue scene;
    scene.type = VmValueType::String;
    scene.stringValue = std::to_string(epoch.scene);
    arguments.push_back(std::move(session));
    arguments.push_back(std::move(index));
    arguments.push_back(std::move(scene));

    const RequestId id = calls_.SubmitGlobal(
        options_.script, options_.function, std::move(arguments),
        [weak = weak_from_this(), token](PapyrusCallResult result) {
            if (const auto state = weak.lock()) state->HandleResult(token, std::move(result));
        });
    {
        std::lock_guard<std::mutex> guard(mutex_);
        if (pendingToken_ == token) {
            pending_.id = id;
        }
    }
}

void SexLabPPlus::Impl::HandleResult(std::uint64_t token, PapyrusCallResult result) {
    Epoch expected;
    bool failed = false;
    PapyrusCallKind failureKind = result.kind;
    {
        std::lock_guard<std::mutex> guard(mutex_);
        if (token == 0 || token != pendingToken_) {
            return;
        }
        expected = pending_.epoch;
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

    PPlusSceneState parsed;
    std::string parseError;
    if (!ParsePPlusSceneWire(result.values.front().stringValue, expected, parsed,
                             parseError)) {
        Completion failedCompletion;
        PPlusSceneState failedSnapshot;
        {
            std::lock_guard<std::mutex> guard(mutex_);
            if (token != pendingToken_) {
                return;
            }
            pendingToken_ = 0;
            pending_ = {};
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
    if (parsed.availability == ExternalAvailability::Available &&
        parsed.provider.apiVersion != options_.requiredApiVersion) {
        parsed.availability = ExternalAvailability::Incompatible;
        parsed.active = false;
    }

    Completion completion;
    Completion staleCompletion;
    PPlusSceneState snapshot;
    PPlusSceneState staleSnapshot;
    ExternalGate gate = ExternalGate::Waiting;
    bool staleAtCommit = false;
    const bool currentEpoch = calls_.IsEpochCurrent(expected);
    {
        std::lock_guard<std::mutex> guard(mutex_);
        if (token == 0 || token != pendingToken_) {
            return;
        }
        pendingToken_ = 0;
        pending_ = {};
        retryAtMs_ = -1;
        if (!currentEpoch) {
            attempts_ = 0;
            active_ = false;
            dirty_ = true;
            gate_ = ExternalGate::Stale;
            snapshot_.active = false;
            staleAtCommit = true;
            staleSnapshot = snapshot_;
            staleCompletion = std::move(completion_);
        } else {
            if (awaitingMainStage_ && parsed.availability == ExternalAvailability::Available && parsed.active) {
                const bool sameStage = parsed.sceneId == transitionFrom_.sceneId &&
                    parsed.stageId == transitionFrom_.stageId && parsed.stage == transitionFrom_.stage &&
                    parsed.animation == transitionFrom_.animation;
                if (sameStage || parsed.leadIn.IsKnownTrue()) {
                    parsed.availability = ExternalAvailability::Unknown;
                } else {
                    awaitingMainStage_ = false;
                }
            }
            snapshot_ = std::move(parsed);
            snapshot_.epoch = expected;
            dirty_ = false;
            if (snapshot_.availability == ExternalAvailability::Available) {
                attempts_ = 0;
                snapshot_.active = active_ && snapshot_.active;
                if (!snapshot_.active) {
                    active_ = false;
                }
                gate_ = snapshot_.active ? ExternalGate::Ready : ExternalGate::Closed;
                completion = std::move(completion_);
            } else if (snapshot_.availability == ExternalAvailability::Missing) {
                attempts_ = 0;
                snapshot_.active = false;
                gate_ = ExternalGate::Missing;
                completion = std::move(completion_);
            } else if (snapshot_.availability == ExternalAvailability::Incompatible) {
                attempts_ = 0;
                snapshot_.active = false;
                gate_ = ExternalGate::Incompatible;
                completion = std::move(completion_);
            } else {
                snapshot_.active = false;
                ++attempts_;
                if (attempts_ >= options_.timing.maxAttempts) {
                    gate_ = ExternalGate::Failed;
                    retryAtMs_ = -1;
                    completion = std::move(completion_);
                } else {
                    gate_ = ExternalGate::Waiting;
                    retryAtMs_ = lastNowMs_ + options_.timing.retryIntervalMs;
                }
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

void SexLabPPlus::Impl::HandleFailure(std::uint64_t token, PapyrusCallKind kind) {
    Completion completion;
    PPlusSceneState snapshot;
    ExternalGate gate = ExternalGate::Waiting;
    {
        std::lock_guard<std::mutex> guard(mutex_);
        if (token == 0 || token != pendingToken_) {
            return;
        }
        pendingToken_ = 0;
        pending_ = {};
        if (kind == PapyrusCallKind::Stale) {
            gate_ = ExternalGate::Stale;
            retryAtMs_ = -1;
            dirty_ = true;
            active_ = false;
            snapshot_.active = false;
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

void SexLabPPlus::Impl::Advance(std::int64_t nowMs) {
    RequestId cancelId;
    bool start = false;
    Completion failedCompletion;
    PPlusSceneState failedSnapshot;
    {
        std::lock_guard<std::mutex> guard(mutex_);
        lastNowMs_ = std::max(lastNowMs_, nowMs);
        if (pendingToken_ != 0 && options_.timing.timeoutMs > 0 &&
            nowMs - pending_.startedAtMs >= options_.timing.timeoutMs) {
            cancelId = pending_.id;
            pendingToken_ = 0;
            pending_ = {};
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
        Epoch epoch;
        {
            std::lock_guard<std::mutex> guard(mutex_);
            epoch = sceneEpoch_;
        }
        BeginRequest(epoch, nowMs, {});
    }
}

void SexLabPPlus::Impl::OnSceneEnded(const Epoch& epoch) {
    RequestId id;
    Completion completion;
    PPlusSceneState snapshot;
    {
        std::lock_guard<std::mutex> guard(mutex_);
        if (!SameEpoch(sceneEpoch_, epoch)) {
            return;
        }
        active_ = false;
        dirty_ = false;
        retryAtMs_ = -1;
        pendingToken_ = 0;
        id = pending_.id;
        pending_ = {};
        gate_ = ExternalGate::Closed;
        snapshot_.active = false;
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

void SexLabPPlus::Impl::MarkDirty() {
    RequestId id;
    Completion completion;
    PPlusSceneState snapshot;
    {
        std::lock_guard<std::mutex> guard(mutex_);
        if (!active_) {
            return;
        }
        dirty_ = true;
        gate_ = ExternalGate::Changed;
        pendingToken_ = 0;
        id = pending_.id;
        pending_ = {};
        attempts_ = 0;
        retryAtMs_ = 0;
        snapshot_.active = false;
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

ExternalGate SexLabPPlus::Impl::Gate() const {
    std::lock_guard<std::mutex> guard(mutex_);
    return gate_;
}

PPlusSceneState SexLabPPlus::Impl::Snapshot() const {
    std::lock_guard<std::mutex> guard(mutex_);
    return snapshot_;
}

bool SexLabPPlus::Impl::Active() const {
    std::lock_guard<std::mutex> guard(mutex_);
    return active_;
}

std::uint32_t SexLabPPlus::Impl::Attempts() const {
    std::lock_guard<std::mutex> guard(mutex_);
    return attempts_;
}

void SexLabPPlus::Impl::Cancel() {
    Epoch epoch;
    {
        std::lock_guard<std::mutex> guard(mutex_);
        epoch = sceneEpoch_;
    }
    OnSceneEnded(epoch);
}

void SexLabPPlus::Impl::Reset() {
    RequestId id;
    Completion completion;
    PPlusSceneState snapshot;
    {
        std::lock_guard<std::mutex> guard(mutex_);
        active_ = false;
        dirty_ = true;
        gate_ = ExternalGate::Waiting;
        pendingToken_ = 0;
        id = pending_.id;
        pending_ = {};
        snapshot_ = {};
        awaitingMainStage_ = false;
        transitionFrom_ = {};
        threadId_ = -1;
        snapshot = snapshot_;
        attempts_ = 0;
        retryAtMs_ = -1;
        completion = std::move(completion_);
    }
    if (id) {
        calls_.Cancel(id);
    }
    if (completion) {
        completion(snapshot, ExternalGate::Closed);
    }
}

SexLabPPlus::SexLabPPlus(PapyrusCalls& calls, PPlusOptions options)
    : impl_(std::make_shared<Impl>(calls, std::move(options))) {}

SexLabPPlus::~SexLabPPlus() { impl_->Shutdown(); }

void SexLabPPlus::SetThreadId(std::int32_t id) { const auto state = impl_; state->SetThreadId(id); }
std::int32_t SexLabPPlus::ThreadId() const { const auto state = impl_; return state->ThreadId(); }

RequestId SexLabPPlus::OnSceneNotification(std::string_view event, std::string_view threadId,
                                         const Epoch& epoch, std::int64_t nowMs,
                                         Completion completion) {
    std::int64_t id = -1;
    if (!ParseInt64(threadId, id) || id < 0 || id > 2147483647) return {};
    const auto state = impl_;
    if (!state->SelectNotificationThread(event, static_cast<std::int32_t>(id), epoch)) return {};
    return state->OnSceneEvent(event, epoch, nowMs, std::move(completion));
}

RequestId SexLabPPlus::OnSceneEvent(std::string_view event, const Epoch& epoch, std::int64_t nowMs, Completion completion) {
    const auto state = impl_;
    return state->OnSceneEvent(event, epoch, nowMs, std::move(completion));
}

RequestId SexLabPPlus::RequestScene(const Epoch& epoch, std::int64_t nowMs, Completion completion) {
    const auto state = impl_;
    return state->RequestScene(epoch, nowMs, std::move(completion));
}

void SexLabPPlus::Advance(std::int64_t nowMs) {
    const auto state = impl_;
    return state->Advance(nowMs);
}

void SexLabPPlus::OnSceneEnded(const Epoch& epoch) {
    const auto state = impl_;
    return state->OnSceneEnded(epoch);
}

void SexLabPPlus::MarkDirty() {
    const auto state = impl_;
    return state->MarkDirty();
}

ExternalGate SexLabPPlus::Gate() const {
    const auto state = impl_;
    return state->Gate();
}

PPlusSceneState SexLabPPlus::Snapshot() const {
    const auto state = impl_;
    return state->Snapshot();
}

bool SexLabPPlus::Active() const {
    const auto state = impl_;
    return state->Active();
}

std::uint32_t SexLabPPlus::Attempts() const {
    const auto state = impl_;
    return state->Attempts();
}

void SexLabPPlus::Cancel() {
    const auto state = impl_;
    return state->Cancel();
}

void SexLabPPlus::Reset() {
    const auto state = impl_;
    return state->Reset();
}

}
