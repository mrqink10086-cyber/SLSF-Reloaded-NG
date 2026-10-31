#include "slsf/integrations/SexLabPPlus.h"
#include "slsf/integrations/DeviousDevices.h"
#include "slsf/integrations/ExternalMods.h"
#include <algorithm>
#include <iostream>
#include <utility>

using namespace slsf;
using namespace slsf::integrations;

class Port final : public IVmPort {
public:
    struct Call { VmRequest request; VmCompletion completion; };
    std::vector<Call> calls;
    RequestId Submit(VmRequest request, VmCompletion completion) override {
        calls.push_back({std::move(request), std::move(completion)});
        return RequestId{calls.size()};
    }
    void RequestCancellation(RequestId) override {}
    void Complete(std::size_t index, std::string wire) {
        VmResult result;
        result.epoch = calls.at(index).request.epoch;
        result.status = Status::Ready;
        VmValue value;
        value.type = VmValueType::String;
        value.stringValue = std::move(wire);
        result.values.push_back(std::move(value));
        auto done = std::move(calls.at(index).completion);
        done(std::move(result));
    }
};

class Random final : public IRandom {
public:
    std::int32_t UniformInt(std::int32_t lo, std::int32_t) override { return lo; }
};

std::string Scene(std::string id = "14", std::string known = "0",
                  std::string unknown = "0", std::string tags = {}) {
    return "SLSFPP1\nversion\t1\navailability\tAvailable\n"
           "provider\tSLSF_Reloaded_PPlusProbe\t1.0.0\t1\n"
           "epoch\t1\t2\t3\nactive\t1\nstage\t1\t3\n"
           "animation\tSexLab.esm\t1234\tAnimation\n" + tags +
           "actor\tSkyrim.esm\t" + id + "\tPlayer\tother\t80\t" + known + "\t" + unknown +
           "\nactor\tSkyrim.esm\t15\tPartner\tother\t0\t0\t0\n";
}

gameplay::AnimationFacts Facts() {
    gameplay::AnimationFacts facts;
    facts.anonymous = Fact::Known(false);
    facts.validLocation = Fact::Known(true);
    facts.startGraceElapsed = Fact::Known(true);
    facts.sceneEnded = Fact::Known(false);
    facts.bodySearch = Fact::Known(false);
    facts.leadIn = Fact::Known(false);
    facts.allowForeplayFame = Fact::Known(false);
    facts.sexWorker = Fact::Known(false);
    facts.publicWhore = Fact::Known(false);
    facts.submissiveDefault = Fact::Known(false);
    facts.dominantDefault = Fact::Known(false);
    facts.canGainBoundFame = Fact::Known(false);
    facts.victimsAreMasochist = Fact::Known(false);
    facts.allowLikeFameWhenRaped = Fact::Known(false);
    facts.allowBestialityWhenRaped = Fact::Known(false);
    facts.deviousDevicesInstalled = Fact::Known(false);
    return facts;
}

bool HasTag(const gameplay::AnimationFacts& facts, std::string_view tag) {
    return std::find(facts.tags.begin(), facts.tags.end(), tag) != facts.tags.end();
}

int main() {
    const Epoch epoch{1, 2, 3};
    std::string error;
    Port port;
    PapyrusCalls calls(port, [epoch] { return epoch; }, PapyrusCallOptions{});
    SexLabPPlus adapter(calls);
    adapter.OnSceneEvent("SexLabAnimationStart", epoch, 0);
    port.Complete(0, Scene("14", "0", "0", "tag\tstage\tLeadIn\n"));
    const auto leadId = adapter.OnSceneEvent("SexLabLeadInEnd", epoch, 10);
    std::cout << "lead_in_end returned_request=" << leadId.value
              << " request_count=" << port.calls.size()
              << " ready=" << (adapter.Gate() == ExternalGate::Ready)
              << " old_stage=" << adapter.Snapshot().stage << '\n';
    adapter.OnSceneEvent("SexLabStageStart", epoch, 11);
    std::cout << "synthetic_stage_start request_count=" << port.calls.size()
              << " waiting=" << (adapter.Gate() == ExternalGate::Waiting) << '\n';

    for (const std::string unknown : {"0", "800000"}) {
        PPlusSceneState state;
        const bool parsed = ParsePPlusSceneWire(Scene("14", "0", unknown), epoch, state, error);
        auto facts = Facts();
        const auto applied = ApplyPPlusToAnimation(state, facts);
        for (auto& actor : facts.actors) {
            actor.relationshipKnown = Fact::Known(true);
            actor.relationshipRank = 0;
        }
        Random random;
        const auto result = gameplay::AnimationClassifier{}.Classify(facts, random);
        const bool oralCategory = std::find(result.categories.begin(), result.categories.end(),
                                           FameCategory::Oral) != result.categories.end();
        std::cout << "oral unknown_mask=" << unknown << " parsed=" << parsed
                  << " tri=" << static_cast<int>(state.actors.at(0).InteractionState(PPlusInteraction::pOral))
                  << " applied=" << applied.applied << " tag=" << HasTag(facts, "Oral")
                  << " ready=" << result.IsReady()
                  << " kind=" << static_cast<int>(result.kind)
                  << " category=" << oralCategory << " category_count=" << result.categories.size() << '\n';
    }

    PPlusSceneState facial;
    ParsePPlusSceneWire(Scene("14", "20000"), epoch, facial, error);
    auto facts = Facts();
    const auto applied = ApplyPPlusToAnimation(facial, facts);
    std::cout << "facial known=" << facial.actors.at(0).HasInteraction(PPlusInteraction::aFacial)
              << " applied=" << applied.applied << " tag=" << HasTag(facts, "Facial") << '\n';
    facts.playerIsFemale = Fact::Known(true);
    facial.active = false;
    const auto rejected = ApplyPPlusToAnimation(facial, facts);
    std::cout << "inactive applied=" << rejected.applied
              << " old_player_fact=" << facts.playerIsFemale.IsKnownTrue() << '\n';

    for (const std::string id : {"0", "FFFFFFFF"}) {
        PPlusSceneState pplus;
        DdngSnapshot dd;
        ExternalSceneSnapshot external;
        const std::string ddWire = "SLSFDD1\nversion\t1\navailability\tAvailable\nprovider\tDevious Devices\t3.0.0\t2\nepoch\t1\t2\t3\nrevision\t7\nactor\tSkyrim.esm\t" + id + "\t0\t0\n";
        const std::string exWire = "SLSFEX1\nversion\t1\navailability\tAvailable\nprovider\tSLSF_Reloaded_ExternalProbe\t1.0.0\t1\nepoch\t1\t2\t3\nplayer\t1\npublicwhore\t0\nactor\tSkyrim.esm\t" + id + "\t0\t0\t0\t0\t0\t1\t0\n";
        const bool p = ParsePPlusSceneWire(Scene(id), epoch, pplus, error);
        const bool d = ParseDdngWire(ddWire, epoch, dd, error);
        const bool e = ParseExternalWire(exWire, epoch, external, error);
        std::cout << "invalid_id=" << id << " pplus=" << p << " dd=" << d << " external=" << e << '\n';
        if (e) {
            const Fact missing = ExternalModFact(external, ExternalMod::FillHerUp);
            std::cout << "omitted_mod available=" << (external.availability == ExternalAvailability::Available)
                      << " fact_state=" << static_cast<int>(missing.state)
                      << " known_false=" << missing.IsKnownFalse() << '\n';
        }
    }
}
