#pragma once

#include "slsf/core/gameplay/AnimationClassifier.h"
#include "slsf/integrations/ExternalState.h"
#include "slsf/integrations/PapyrusCalls.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace slsf::integrations {

enum class PPlusInteraction : std::uint8_t {
    pStimulation = 0,
    aAnimObjFace,
    pAnimObjFace,
    pSuckingToes,
    pGrinding,
    pSkullfuck,
    aHandJob,
    aFootJob,
    aBoobJob,
    bKissing,
    aSuckingToes,
    pFacial,
    aOral,
    aLickingShaft,
    aDeepthroat,
    pVaginal,
    pAnal,
    aFacial,
    aGrinding,
    pHandJob,
    pFootJob,
    pBoobJob,
    pLickingShaft,
    pOral,
    pDeepthroat,
    aSkullfuck,
    aVaginal,
    aAnal,
};

inline constexpr std::size_t kPPlusInteractionCount = 28;

[[nodiscard]] const char* PPlusInteractionName(PPlusInteraction value) noexcept;

enum class PPlusTagScope : std::uint8_t {
    Thread = 0,
    Scene,
    Stage,
};

[[nodiscard]] const char* PPlusTagScopeName(PPlusTagScope value) noexcept;

struct PPlusTag {
    std::string name;
    PPlusTagScope scope{PPlusTagScope::Stage};
};

struct PPlusActorState {
    StableFormKey actor;
    std::string name;
    gameplay::AnimationRace race{gameplay::AnimationRace::Unknown};
    bool isPlayer{};
    bool female{};
    bool strapon{};
    bool victim{};
    bool aggressor{};
    bool dominant{};
    bool submissive{};
    bool masochist{};
    std::uint32_t interactions{};
    std::uint32_t interactionUnknown{};
    std::uint32_t roleUnknown{};
    std::optional<std::uint8_t> sexCode;
    Fact playerInteraction;

    [[nodiscard]] gameplay::Tri InteractionState(PPlusInteraction value) const noexcept;
    [[nodiscard]] bool HasInteraction(PPlusInteraction value) const noexcept;
};

struct PPlusSceneState {
    ExternalAvailability availability{ExternalAvailability::Unknown};
    ExternalProviderInfo provider;
    Epoch epoch{};
    bool active{};
    std::uint32_t stage{};
    std::uint32_t stageCount{};
    std::string sceneId;
    std::string stageId;
    Fact leadIn;
    StableFormKey animation;
    std::string animationName;
    std::vector<PPlusTag> tags;
    std::vector<PPlusActorState> actors;

    [[nodiscard]] std::vector<std::string> TagsAtScope(PPlusTagScope scope) const;
    [[nodiscard]] std::vector<std::string> ClassificationTags() const;
};

[[nodiscard]] bool ParsePPlusSceneWire(std::string_view wire, const Epoch& expected,
                                       PPlusSceneState& out, std::string& error);

struct PPlusApplyResult {
    bool applied{};
    std::string detail;
};

[[nodiscard]] PPlusApplyResult ApplyPPlusToAnimation(const PPlusSceneState& state,
                                                     gameplay::AnimationFacts& facts);

struct PPlusOptions {
    ExternalTimingPolicy timing;
    std::string script{"SLSF_Reloaded_PPlusProbe"};
    std::string function{"CollectScene"};
    std::uint32_t requiredApiVersion{1};
};

class SexLabPPlus {
public:
    using Completion = std::function<void(const PPlusSceneState&, ExternalGate)>;

    SexLabPPlus(PapyrusCalls& calls, PPlusOptions options = {});
    ~SexLabPPlus();

    SexLabPPlus(const SexLabPPlus&) = delete;
    SexLabPPlus& operator=(const SexLabPPlus&) = delete;

    RequestId OnSceneEvent(std::string_view event, const Epoch& epoch, std::int64_t nowMs,
                           Completion completion = {});
    RequestId RequestScene(const Epoch& epoch, std::int64_t nowMs,
                           Completion completion = {});
    void SetThreadId(std::int32_t threadId);
    [[nodiscard]] std::int32_t ThreadId() const;
    RequestId OnSceneNotification(std::string_view event, std::string_view threadId,
                                  const Epoch& epoch, std::int64_t nowMs,
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
    struct Impl;
    std::shared_ptr<Impl> impl_;
};

}
