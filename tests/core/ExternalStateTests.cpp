#include "slsf/integrations/DeviousDevices.h"
#include "slsf/integrations/ExternalMods.h"
#include "slsf/integrations/SexLabPPlus.h"

#include "slsf/integrations/PapyrusCalls.h"
#include "slsf/interfaces/VmPort.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <atomic>
#include <barrier>
#include <memory>
#include <stdexcept>
#include <thread>
#include "slsf/integrations/ProviderCollectors.h"
#include <cstdint>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace {

using slsf::Epoch;
using slsf::IVmPort;
using slsf::PapyrusCalls;
using slsf::PapyrusCallOptions;
using slsf::RequestId;
using slsf::StableFormKey;
using slsf::Status;
using slsf::VmCompletion;
using slsf::VmRequest;
using slsf::VmResult;
using slsf::VmValue;
using slsf::VmValueType;

class TestPort final : public IVmPort {
public:
    struct Pending {
        VmRequest request;
        VmCompletion completion;
    };

    RequestId Submit(VmRequest request, VmCompletion completion) override {
        const RequestId id{nextId_++};
        pending_.push_back(Pending{std::move(request), std::move(completion)});
        return id;
    }

    void Complete(RequestId id, Status status, std::vector<VmValue> values = {}) {
        CompleteWithEpoch(id, status, pending_[static_cast<std::size_t>(id.value - 1)].request.epoch,
                          std::move(values));
    }

    void CompleteWithEpoch(RequestId id, Status status, Epoch epoch,
                           std::vector<VmValue> values = {}) {
        Pending& entry = pending_[static_cast<std::size_t>(id.value - 1)];
        VmResult result;
        result.epoch = epoch;
        result.status = status;
        result.values = std::move(values);
        VmCompletion completion = std::move(entry.completion);
        entry.completion = nullptr;
        if (completion) {
            completion(std::move(result));
        }
    }

    std::size_t Count() const { return pending_.size(); }
    const VmRequest* At(std::size_t index) const {
        return index < pending_.size() ? &pending_[index].request : nullptr;
    }

    void RequestCancellation(RequestId) override {}

private:
    std::vector<Pending> pending_;
    std::uint64_t nextId_{1};
};

VmValue StringVm(std::string value) {
    VmValue out;
    out.type = VmValueType::String;
    out.stringValue = std::move(value);
    return out;
}

struct EpochBox {
    Epoch current{1, 2, 3};
};

PapyrusCalls::EpochProbe Probe(EpochBox& box) {
    return [&box] { return box.current; };
}

const StableFormKey kPlayer{"SLSF Reloaded.esp", 0x80A};
const StableFormKey kNpc{"Skyrim.esm", 0x1D15B};

std::string PPlusWire(std::string_view availability = "Available",
                      std::string_view providerVersion = "1.0.0",
                      std::uint32_t api = 1) {
    std::string wire = "SLSFPP1\nversion\t1\navailability\t";
    wire += availability;
    wire += "\nprovider\tSLSF_Reloaded_PPlusProbe\t";
    wire += providerVersion;
    wire += "\t";
    wire += std::to_string(api);
    wire += "\nepoch\t1\t2\t3\nactive\t1\nstage\t2\t4\n";
    wire += "animation\tSexLab.esp\t1234\tMyAnimation\n";
    if (std::string_view(availability) == "Available") {
        wire += "tag\tscene\toral\n";
        wire += "tag\tstage\taggressive\n";
        wire += "tag\tstage\tORGY\n";
        wire += "tag\tthread\tLeadIn\n";
        wire += "actor\tSLSF Reloaded.esp\t80A\tPlayer\tother\t81\t8000\t10000\n";
        wire += "actor\tSkyrim.esm\t1D15B\tNpc\tkhajiit\t0\t1000\t0\n";
    }
    return wire;
}

std::string DdngWire(std::uint32_t revision = 7) {
    std::string wire = "SLSFDD1\nversion\t1\navailability\tAvailable\n";
    wire += "provider\tDevious Devices\t3.0.0\t2\n";
    wire += "epoch\t1\t2\t3\nrevision\t";
    wire += std::to_string(revision);
    wire += "\nactor\tSLSF Reloaded.esp\t80A\t44\t1805\n";
    return wire;
}

std::string ExternalWire(std::string_view availability = "Available") {
    std::string wire = "SLSFEX1\nversion\t1\navailability\t";
    wire += availability;
    wire += "\nprovider\tSLSF_Reloaded_ExternalProbe\t1.0.0\t1\n";
    wire += "epoch\t1\t2\t3\nplayer\t1\npublicwhore\t1\n";
    wire += "mod\tPublicWhore\tAvailable\t2.0.0\n";
    wire += "mod\tFillHerUp\tMissing\t\n";
    wire += "mod\tFertilityMode\tMissing\t\n";
    wire += "mod\tEstrusChaurus\tMissing\t\n";
    wire += "mod\tEstrusSpider\tMissing\t\n";
    wire += "mod\tHentaiPregnancy\tMissing\t\n";
    wire += "mod\tFameComments\tAvailable\t1.0.0\n";
    wire += "mod\tBimbos\tMissing\t\n";
    if (std::string_view(availability) == "Available") {
        wire += "actor\tSLSF Reloaded.esp\t80A\t1\t1\t0\t0\t0\t1\t12.5\n";
    }
    return wire;
}

}

TEST_CASE("Group08 rejects invalid stable keys and incomplete available mod snapshots", "[g08-fixes]") {
    using namespace slsf::integrations;
    for (const std::string id : {"0", "FFFFFFFF", "1000000"}) {
        std::string error;
        auto pp = PPlusWire();
        pp.replace(pp.find("\t80A\t"), 5, "\t" + id + "\t");
        PPlusSceneState scene;
        REQUIRE_FALSE(ParsePPlusSceneWire(pp, {1,2,3}, scene, error));
        auto dd = DdngWire();
        dd.replace(dd.find("\t80A\t"), 5, "\t" + id + "\t");
        DdngSnapshot devices;
        REQUIRE_FALSE(ParseDdngWire(dd, {1,2,3}, devices, error));
        auto ex = ExternalWire();
        ex.replace(ex.find("\t80A\t"), 5, "\t" + id + "\t");
        ExternalSceneSnapshot external;
        REQUIRE_FALSE(ParseExternalWire(ex, {1,2,3}, external, error));
    }
    auto incomplete = ExternalWire();
    incomplete.erase(incomplete.find("mod\tFillHerUp"), std::string("mod\tFillHerUp\tMissing\t\n").size());
    ExternalSceneSnapshot external;
    std::string error;
    REQUIRE_FALSE(ParseExternalWire(incomplete, {1,2,3}, external, error));
    auto ranks = ExternalWire();
    ranks.insert(ranks.size() - 1, "\t61\t3");
    REQUIRE(ParseExternalWire(ranks, {1,2,3}, external, error));
    REQUIRE(external.actors[0].fertilityModeRank == 61);
    REQUIRE(external.actors[0].hentaiPregnancyRank == 3);
}

TEST_CASE("Group08 collectors preserve request identity and reject ambiguous requests", "[g08-fixes]") {
    using namespace slsf::integrations;
    const std::string request = "SLSFDDQ1\nversion\t1\nepoch\t4294967300\t2\t3\nactor\tPlugin\\tName.esp\t4095\n";
    CollectorRequest parsed;
    REQUIRE(ParseCollectorRequest(request, "SLSFDDQ1", parsed));
    REQUIRE(parsed.epoch.session == 4294967300ULL);
    REQUIRE(parsed.actors[0] == StableFormKey{"Plugin\tName.esp", 4095});
    REQUIRE(WireEscape("a\tb\nc\\d") == "a\\tb\\nc\\\\d");
    REQUIRE(WireHex(0xffffff) == "ffffff");
    for (const auto& invalid : {request + "actor\tPlugin\\tName.esp\t4095\n",
                               request + "version\t1\n",
                               request + "actor\tOther.esp\t0\n",
                               request + "actor\tOther.esp\t16777216\n"}) {
        REQUIRE_FALSE(ParseCollectorRequest(invalid, "SLSFDDQ1", parsed));
        REQUIRE(parsed.actors.size() == 1);
    }
    REQUIRE_FALSE(ParseCollectorRequest(request, "SLSFEXQ1", parsed));
}

TEST_CASE("Group08 propagates unknown P+ interactions into classification", "[g08-fixes]") {
    using slsf::Fact;
    using namespace slsf::gameplay;
    using namespace slsf::integrations;
    struct Random final : slsf::IRandom { std::int32_t UniformInt(std::int32_t lo, std::int32_t) override { return lo; } } rng;
    PPlusSceneState scene;
    std::string error;
    REQUIRE(ParsePPlusSceneWire(PPlusWire(), {1,2,3}, scene, error));
    scene.tags.clear();
    scene.leadIn = Fact::Known(false);
    for (auto& actor : scene.actors) { actor.interactions = 0; actor.interactionUnknown = 0; actor.playerInteraction = Fact::Known(false); }
    auto classify = [&] {
        AnimationFacts facts;
        for (auto* field : {&facts.anonymous, &facts.sceneEnded, &facts.bodySearch,
             &facts.allowForeplayFame, &facts.sexWorker, &facts.publicWhore,
             &facts.submissiveDefault, &facts.dominantDefault, &facts.canGainBoundFame,
             &facts.victimsAreMasochist, &facts.allowLikeFameWhenRaped,
             &facts.allowBestialityWhenRaped, &facts.deviousDevicesInstalled}) *field = Fact::Known(false);
        facts.validLocation = Fact::Known(true);
        facts.startGraceElapsed = Fact::Known(true);
        REQUIRE(ApplyPPlusToAnimation(scene, facts).applied);
        for (auto& actor : facts.actors) { actor.relationshipKnown = Fact::Known(true); actor.relationshipRank = 0; }
        return AnimationClassifier{}.Classify(facts, rng);
    };
    REQUIRE(classify().IsReady());
    scene.actors[0].interactionUnknown = 1u << static_cast<unsigned>(PPlusInteraction::pOral);
    REQUIRE_FALSE(classify().IsReady());
    scene.actors[1].interactions = 1u << static_cast<unsigned>(PPlusInteraction::aOral);
    REQUIRE(classify().IsReady());
    scene.actors[1].interactions = 0;
    scene.tags.push_back({"Oral", PPlusTagScope::Stage});
    REQUIRE_FALSE(classify().IsReady());
    scene.tags.push_back({"Oral", PPlusTagScope::Scene});
    REQUIRE(classify().IsReady());
}

TEST_CASE("Group08 resets rejected projections without changing category policy", "[g08-fixes]") {
    using namespace slsf::integrations;
    using namespace slsf::gameplay;
    PPlusSceneState scene;
    std::string error;
    REQUIRE(ParsePPlusSceneWire(PPlusWire(), {1,2,3}, scene, error));
    scene.actors[0].roleUnknown = 0x5f;
    scene.actors[0].submissive = true;
    AnimationFacts facts;
    REQUIRE(ApplyPPlusToAnimation(scene, facts).applied);
    REQUIRE_FALSE(facts.playerIsFemale.Value().has_value());
    REQUIRE_FALSE(facts.usingStrapon.Value().has_value());
    REQUIRE_FALSE(facts.victim.Value().has_value());
    REQUIRE_FALSE(facts.aggressor.Value().has_value());
    scene.actors[0].interactions = 1u << static_cast<unsigned>(PPlusInteraction::aFacial);
    REQUIRE(ApplyPPlusToAnimation(scene, facts).applied);
    REQUIRE(std::find(facts.tags.begin(), facts.tags.end(), "Facial") != facts.tags.end());
    scene.active = false;
    REQUIRE_FALSE(ApplyPPlusToAnimation(scene, facts).applied);
    REQUIRE(facts.actors.empty());
    REQUIRE(facts.tags.empty());
    REQUIRE_FALSE(facts.playerIsFemale.Value().has_value());
    scene.active = true;
    scene.actors[1].isPlayer = true;
    REQUIRE_FALSE(ApplyPPlusToAnimation(scene, facts).applied);
}

TEST_CASE("Group08 collector continuation records reject corrupt and ambiguous stacks", "[g08-fixes]") {
    using namespace slsf::integrations;
    const std::vector<CollectorContinuation> calls{{0, false}, {UINT32_MAX, true}};
    auto words = EncodeCollectorContinuations(calls);
    std::vector<CollectorContinuation> decoded;
    REQUIRE(DecodeCollectorContinuations(words, decoded));
    REQUIRE(decoded == calls);
    for (const auto& bad : std::vector<std::vector<std::uint32_t>>{
             {}, {1}, {2, 0, 0}, {1, 0, 2}, {2, 4, 0, 4, 1}, {4097}}) {
        REQUIRE_FALSE(DecodeCollectorContinuations(bad, decoded));
        REQUIRE(decoded == calls);
    }
    REQUIRE(DecodeCollectorContinuations(std::vector<std::uint32_t>{0}, decoded));
    REQUIRE(decoded.empty());
    std::vector<CollectorContinuation> maximum(kMaxCollectorCalls);
    for (std::size_t i = 0; i < maximum.size(); ++i) maximum[i] = {static_cast<std::uint32_t>(i), i % 2 == 0};
    REQUIRE(DecodeCollectorContinuations(EncodeCollectorContinuations(maximum), decoded));
    REQUIRE(decoded == maximum);
    maximum.push_back({5000, false});
    REQUIRE_THROWS_AS(EncodeCollectorContinuations(maximum), std::length_error);
}

TEST_CASE("Group08 keeps victim roles and stage tags separate from legacy category policy", "[g08-fixes]") {
    using slsf::Fact;
    using slsf::FameCategory;
    using namespace slsf::gameplay;
    using namespace slsf::integrations;
    struct Random final : slsf::IRandom { std::int32_t UniformInt(std::int32_t lo, std::int32_t) override { return lo; } } rng;
    PPlusSceneState scene;
    std::string error;
    REQUIRE(ParsePPlusSceneWire(PPlusWire(), {1,2,3}, scene, error));
    scene.tags.clear();
    scene.leadIn = Fact::Known(false);
    scene.actors[0].victim = false;
    scene.actors[0].submissive = false;
    scene.actors[0].female = false;
    bool submissiveDefault = true;
    for (auto& actor : scene.actors) { actor.interactions = 0; actor.interactionUnknown = 0; actor.playerInteraction = Fact::Known(false); }
    auto classify = [&] {
        AnimationFacts facts;
        for (auto* field : {&facts.anonymous, &facts.sceneEnded, &facts.bodySearch,
             &facts.allowForeplayFame, &facts.sexWorker, &facts.publicWhore,
             &facts.dominantDefault, &facts.canGainBoundFame, &facts.victimsAreMasochist,
             &facts.allowLikeFameWhenRaped, &facts.allowBestialityWhenRaped,
             &facts.deviousDevicesInstalled}) *field = Fact::Known(false);
        facts.submissiveDefault = Fact::Known(submissiveDefault);
        facts.validLocation = Fact::Known(true);
        facts.startGraceElapsed = Fact::Known(true);
        REQUIRE(ApplyPPlusToAnimation(scene, facts).applied);
        REQUIRE(facts.victim.IsKnownFalse());
        for (auto& actor : facts.actors) { actor.relationshipKnown = Fact::Known(true); actor.relationshipRank = 0; }
        return AnimationClassifier{}.Classify(facts, rng);
    };
    const auto baseline = classify();
    REQUIRE(baseline.kind == AnimationResultKind::Analyzed);
    REQUIRE(std::find(baseline.categories.begin(), baseline.categories.end(), FameCategory::Submissive) != baseline.categories.end());
    submissiveDefault = false;
    const auto noDefault = classify();
    REQUIRE(std::find(noDefault.categories.begin(), noDefault.categories.end(), FameCategory::Submissive) == noDefault.categories.end());
    submissiveDefault = true;
    scene.tags = {{"BodySearch", PPlusTagScope::Stage}, {"Aggressive", PPlusTagScope::Stage}};
    REQUIRE(classify().categories == baseline.categories);
    REQUIRE(classify().kind == baseline.kind);
    REQUIRE(scene.TagsAtScope(PPlusTagScope::Stage).size() == 2);
    scene.tags.push_back({"Aggressive", PPlusTagScope::Scene});
    REQUIRE(classify().categories != baseline.categories);
    scene.tags.push_back({"BodySearch", PPlusTagScope::Scene});
    REQUIRE(classify().kind == AnimationResultKind::Skipped);
}

TEST_CASE("P+ preference follows player partner contacts in mixed scenes", "[partner-interaction][pplus]") {
    using slsf::Fact;
    using slsf::FameCategory;
    using namespace slsf::gameplay;
    using namespace slsf::integrations;
    struct Random final : slsf::IRandom { std::int32_t UniformInt(std::int32_t lo, std::int32_t) override { return lo; } } rng;
    const bool femalePlayer = GENERATE(false, true);
    const bool sameSexContact = GENERATE(false, true);
    const auto wire = std::string("SLSFPP1\nversion\t1\navailability\tAvailable\nprovider\tSexLabPPlus\t2.18.1\t1\nepoch\t1\t2\t3\nactive\ttrue\nstage\t0\t1\nscene\tscene\tstage\tfalse\n") +
        "actor\tSkyrim.esm\t14\tPlayer\tother\t" + (femalePlayer ? "81" : "80") + "\t0\t0\t0\t" + (femalePlayer ? "1" : "0") + "\tfalse\n" +
        "actor\tSkyrim.esm\t15\tSame\tother\t" + (femalePlayer ? "1" : "0") + "\t0\t0\t0\t" + (femalePlayer ? "1" : "0") + "\t" + (sameSexContact ? "true" : "false") + "\n" +
        "actor\tSkyrim.esm\t16\tOpposite\tother\t" + (femalePlayer ? "0" : "1") + "\t0\t0\t0\t" + (femalePlayer ? "0" : "1") + "\ttrue\n";
    PPlusSceneState scene;
    std::string error;
    REQUIRE(ParsePPlusSceneWire(wire, {1,2,3}, scene, error));
    auto classify = [&] {
        AnimationFacts facts;
        for (auto* field : {&facts.anonymous, &facts.sceneEnded, &facts.bodySearch,
             &facts.allowForeplayFame, &facts.sexWorker, &facts.publicWhore,
             &facts.submissiveDefault, &facts.dominantDefault, &facts.canGainBoundFame,
             &facts.victimsAreMasochist, &facts.allowLikeFameWhenRaped,
             &facts.allowBestialityWhenRaped, &facts.deviousDevicesInstalled}) *field = Fact::Known(false);
        facts.validLocation = Fact::Known(true);
        facts.startGraceElapsed = Fact::Known(true);
        REQUIRE(ApplyPPlusToAnimation(scene, facts).applied);
        for (auto& actor : facts.actors) { actor.relationshipKnown = Fact::Known(true); actor.relationshipRank = 0; }
        return AnimationClassifier{}.Classify(facts, rng);
    };
    const auto result = classify();
    REQUIRE(result.IsReady());
    const auto has = [&](FameCategory category) { return std::find(result.categories.begin(), result.categories.end(), category) != result.categories.end(); };
    REQUIRE(has(femalePlayer ? FameCategory::LikesWomen : FameCategory::LikesMen) == sameSexContact);
    REQUIRE(has(femalePlayer ? FameCategory::LikesMen : FameCategory::LikesWomen));
    scene.actors[1].playerInteraction = Fact{};
    REQUIRE_FALSE(classify().IsReady());
    scene.actors[1].playerInteraction = Fact::Known(false);
    scene.actors[1].roleUnknown |= 1;
    REQUIRE(classify().IsReady());
    scene.actors[1].playerInteraction = Fact::Known(true);
    REQUIRE_FALSE(classify().IsReady());
}

TEST_CASE("P+ legacy and invalid partner contact data never infer contacts", "[partner-interaction][pplus]") {
    using namespace slsf::integrations;
    using namespace slsf::gameplay;
    PPlusSceneState scene;
    std::string error;
    REQUIRE(ParsePPlusSceneWire(PPlusWire(), {1,2,3}, scene, error));
    AnimationFacts facts;
    REQUIRE(ApplyPPlusToAnimation(scene, facts).applied);
    REQUIRE(TriOf(facts.malePartnerInteraction) == Tri::Unknown);
    REQUIRE(facts.femalePartnerInteraction.IsKnownFalse());
    for (const auto contact : {"unknown", "false", "true", "bad"}) {
        auto wire = PPlusWire();
        const auto pos = wire.find("\t1000\t0\n");
        wire.replace(pos, 8, std::string("\t1000\t0\t0\t0\t") + contact + "\n");
        if (std::string_view(contact) == "bad") {
            REQUIRE_FALSE(ParsePPlusSceneWire(wire, {1,2,3}, scene, error));
            REQUIRE(error == "actor player interaction is invalid");
        } else {
            REQUIRE(ParsePPlusSceneWire(wire, {1,2,3}, scene, error));
            REQUIRE(ApplyPPlusToAnimation(scene, facts).applied);
            REQUIRE(TriOf(facts.malePartnerInteraction) == (std::string_view(contact) == "unknown" ? Tri::Unknown : std::string_view(contact) == "true" ? Tri::True : Tri::False));
        }
    }
    scene.active = false;
    REQUIRE_FALSE(ApplyPPlusToAnimation(scene, facts).applied);
    REQUIRE_FALSE(facts.malePartnerInteraction.Value().has_value());
    REQUIRE_FALSE(facts.femalePartnerInteraction.Value().has_value());
}

TEST_CASE("Group08 notification selects a real thread and preserves wide epoch arguments", "[g08-fixes]") {
    TestPort port;
    EpochBox epochs;
    PapyrusCalls calls(port, Probe(epochs), {});
    slsf::integrations::SexLabPPlus adapter(calls);
    REQUIRE_FALSE(adapter.OnSceneNotification("SexLabAnimationStart", "bad", epochs.current, 0));
    REQUIRE_FALSE(adapter.OnSceneNotification("bogus", "12", epochs.current, 0));
    auto id = adapter.OnSceneNotification("SexLabAnimationStart", "12", epochs.current, 0);
    REQUIRE(id);
    REQUIRE(port.At(0)->arguments[0].intValue == 12);
    REQUIRE_FALSE(adapter.OnSceneNotification("SexLabAnimationEnd", "13", epochs.current, 1));
    REQUIRE(adapter.Active());
    port.Complete(id, Status::Ready, {StringVm(PPlusWire())});
    REQUIRE(adapter.Gate() == slsf::integrations::ExternalGate::Ready);
    REQUIRE_FALSE(adapter.OnSceneNotification("SexLabLeadInEnd", "13", epochs.current, 2));
    REQUIRE(port.Count() == 1);
    epochs.current.session = 4294967300ULL;
    REQUIRE(adapter.OnSceneNotification("SexLabAnimationStart", "12", epochs.current, 3));
    REQUIRE(port.At(1)->arguments[1].type == slsf::VmValueType::String);
    REQUIRE(port.At(1)->arguments[1].stringValue == "4294967300");
}

TEST_CASE("Group08 retains P+ sex codes without folding other sexes into male counts", "[g08-fixes]") {
    using namespace slsf::integrations;
    auto wire = PPlusWire();
    wire.replace(wire.find("\t10000\n"), 7, "\t10000\t50\t4\n");
    wire.replace(wire.find("\t1000\t0\n"), 8, "\t1000\t0\t50\t2\n");
    PPlusSceneState scene;
    std::string error;
    REQUIRE(ParsePPlusSceneWire(wire, {1,2,3}, scene, error));
    REQUIRE(scene.actors[0].sexCode == 4);
    REQUIRE(scene.actors[1].sexCode == 2);
    slsf::gameplay::AnimationFacts facts;
    REQUIRE(ApplyPPlusToAnimation(scene, facts).applied);
    REQUIRE(facts.playerIsFemale.IsKnownTrue());
    REQUIRE(facts.maleCount.value == 0);
    REQUIRE(facts.femaleCount.value == 0);
    scene.actors[0].sexCode = 1;
    scene.actors[1].sexCode = 0;
    REQUIRE(ApplyPPlusToAnimation(scene, facts).applied);
    REQUIRE(facts.maleCount.value == 1);
    REQUIRE(facts.femaleCount.value == 1);
}

TEST_CASE("Group08 adapter state survives concurrent wrapper destruction during completion", "[g08-fixes]") {
    using namespace slsf::integrations;
    auto verify = [](auto factory, auto request, std::string wire) {
        TestPort port;
        EpochBox epochs;
        PapyrusCalls calls(port, Probe(epochs), {});
        auto adapter = factory(calls);
        std::barrier rendezvous(2);
        std::atomic<int> completed{};
        const auto id = request(*adapter, [&](const auto&, ExternalGate) {
            ++completed;
            rendezvous.arrive_and_wait();
            rendezvous.arrive_and_wait();
        });
        std::thread callback([&] { port.Complete(id, Status::Ready, {StringVm(wire)}); });
        rendezvous.arrive_and_wait();
        adapter.reset();
        rendezvous.arrive_and_wait();
        callback.join();
        REQUIRE(completed == 1);
    };
    SECTION("P+") {
        verify([](auto& calls) { return std::make_unique<SexLabPPlus>(calls); },
               [](auto& a, auto done) { return a.RequestScene({1,2,3}, 0, done); }, PPlusWire());
    }
    SECTION("DDNG") {
        verify([](auto& calls) { return std::make_unique<DeviousDevices>(calls); },
               [](auto& a, auto done) { return a.RequestState({1,2,3}, {kPlayer}, 0, done); }, DdngWire());
    }
    SECTION("external") {
        verify([](auto& calls) { return std::make_unique<ExternalMods>(calls); },
               [](auto& a, auto done) { return a.RequestState({1,2,3}, {kPlayer}, 0, done); }, ExternalWire());
    }
}

TEST_CASE("Group08 isolates VM and epoch exceptions with a single terminal outcome", "[g08-fixes]") {
    struct Port final : IVmPort {
        VmCompletion pending;
        bool throwSubmit{true};
        RequestId Submit(VmRequest, VmCompletion done) override {
            if (throwSubmit) throw std::runtime_error("submit");
            pending = std::move(done);
            return RequestId{1};
        }
        void RequestCancellation(RequestId) override { throw std::runtime_error("cancel"); }
    } port;
    bool broken = false;
    PapyrusCalls calls(port, [&] { if (broken) throw std::runtime_error("epoch"); return Epoch{}; }, {});
    int outcomes = 0;
    REQUIRE_FALSE(calls.SubmitGlobal("Probe", "Collect", {}, [&](auto result) {
        ++outcomes;
        REQUIRE(result.kind == slsf::PapyrusCallKind::Failed);
    }));
    REQUIRE(outcomes == 1);
    port.throwSubmit = false;
    auto id = calls.SubmitGlobal("Probe", "Collect", {}, [&](auto) { ++outcomes; });
    REQUIRE(id);
    REQUIRE_NOTHROW(calls.Cancel(id));
    REQUIRE(outcomes == 2);
    broken = true;
    REQUIRE_FALSE(calls.IsEpochCurrent({}));
    REQUIRE_FALSE(calls.SubmitGlobal("Probe", "Collect", {}, [&](auto) { ++outcomes; }));
    REQUIRE(outcomes == 3);
    REQUIRE(calls.InFlight() == 0);
}

TEST_CASE("P+ wire keeps only classification scopes and canonicalizes tags") {
    slsf::integrations::PPlusSceneState state;
    std::string error;
    REQUIRE(slsf::integrations::ParsePPlusSceneWire(PPlusWire(), Epoch{1, 2, 3}, state,
                                                    error));
    REQUIRE(error.empty());
    REQUIRE(state.actors.size() == 2);
    REQUIRE(state.actors[0].isPlayer);
    REQUIRE(state.actors[0].HasInteraction(slsf::integrations::PPlusInteraction::pVaginal));
    REQUIRE(state.actors[1].HasInteraction(slsf::integrations::PPlusInteraction::aOral));
    REQUIRE(state.actors[1].InteractionState(
                slsf::integrations::PPlusInteraction::pAnal) == slsf::gameplay::Tri::False);

    const std::vector<std::string> tags = state.ClassificationTags();
    REQUIRE(tags == std::vector<std::string>{"Oral"});
    REQUIRE(state.TagsAtScope(slsf::integrations::PPlusTagScope::Thread) ==
            std::vector<std::string>{"LeadIn"});
}

TEST_CASE("P+ wire rejects a mismatched scene epoch") {
    slsf::integrations::PPlusSceneState state;
    std::string error;
    REQUIRE_FALSE(slsf::integrations::ParsePPlusSceneWire(
        PPlusWire(), Epoch{1, 2, 4}, state, error));
    REQUIRE(error.find("epoch") != std::string::npos);
}

TEST_CASE("P+ facts use the validated scene actors and interactions") {
    slsf::integrations::PPlusSceneState state;
    std::string error;
    REQUIRE(slsf::integrations::ParsePPlusSceneWire(PPlusWire(), Epoch{1, 2, 3}, state,
                                                    error));
    slsf::gameplay::AnimationFacts facts;
    const auto applied = slsf::integrations::ApplyPPlusToAnimation(state, facts);
    REQUIRE(applied.applied);
    REQUIRE(facts.actors.size() == 2);
    REQUIRE(facts.actors[0].isPlayer);
    REQUIRE(facts.actors[1].race == slsf::gameplay::AnimationRace::Khajiit);
    REQUIRE(facts.maleCount.value == 1);
    REQUIRE(facts.femaleCount.value == 1);
    REQUIRE(facts.playerIsFemale.IsKnownTrue());
    REQUIRE(facts.tags == std::vector<std::string>{"Oral"});
}

TEST_CASE("P+ adapter retries within the bounded policy and gates readiness") {
    TestPort port;
    EpochBox box;
    PapyrusCalls calls(port, Probe(box), PapyrusCallOptions{});
    slsf::integrations::PPlusOptions options;
    options.timing.timeoutMs = 1000;
    options.timing.retryIntervalMs = 10;
    options.timing.maxAttempts = 3;
    slsf::integrations::SexLabPPlus adapter(calls, options);

    int completions = 0;
    slsf::integrations::ExternalGate seenGate = slsf::integrations::ExternalGate::Waiting;
    const RequestId id = adapter.RequestScene(Epoch{1, 2, 3}, 0,
                                              [&](const auto&, auto gate) {
                                                  ++completions;
                                                  seenGate = gate;
                                              });
    REQUIRE(id.value != 0);
    REQUIRE(adapter.Gate() == slsf::integrations::ExternalGate::Waiting);
    REQUIRE(port.At(0)->arguments.size() == 4);
    REQUIRE(port.At(0)->arguments[0].intValue == -1);
    REQUIRE(port.At(0)->arguments[1].stringValue == "1");
    REQUIRE(port.At(0)->script == "SLSF_Reloaded_PPlusProbe");

    port.Complete(id, Status::NotReady);
    REQUIRE(adapter.Attempts() == 1);
    REQUIRE(adapter.Gate() == slsf::integrations::ExternalGate::Waiting);

    adapter.Advance(10);
    REQUIRE(port.Count() == 2);
    port.Complete(RequestId{2}, Status::Ready, {StringVm("bad")});
    REQUIRE(adapter.Attempts() == 2);
    REQUIRE(completions == 0);

    adapter.Advance(20);
    REQUIRE(port.Count() == 3);
    port.Complete(RequestId{3}, Status::Ready, {StringVm(PPlusWire())});
    REQUIRE(completions == 1);
    REQUIRE(seenGate == slsf::integrations::ExternalGate::Ready);
    REQUIRE(adapter.Gate() == slsf::integrations::ExternalGate::Ready);
    REQUIRE(adapter.Attempts() == 0);
    REQUIRE(adapter.Snapshot().actors.size() == 2);
}

TEST_CASE("P+ adapter reports stale results and does not score them") {
    TestPort port;
    EpochBox box;
    PapyrusCalls calls(port, Probe(box), PapyrusCallOptions{});
    slsf::integrations::SexLabPPlus adapter(calls, {});

    int completions = 0;
    slsf::integrations::ExternalGate seenGate = slsf::integrations::ExternalGate::Waiting;
    const RequestId id = adapter.RequestScene(Epoch{1, 2, 3}, 0,
                                              [&](const auto&, auto gate) {
                                                  ++completions;
                                                  seenGate = gate;
                                              });
    box.current = Epoch{1, 2, 4};
    port.Complete(id, Status::Ready, {StringVm(PPlusWire())});
    REQUIRE(completions == 1);
    REQUIRE(seenGate == slsf::integrations::ExternalGate::Stale);
    REQUIRE(adapter.Gate() == slsf::integrations::ExternalGate::Stale);
    REQUIRE(slsf::integrations::BlocksNewScoring(adapter.Gate()));
}

TEST_CASE("P+ stage changes refetch instead of serving a cached scene") {
    TestPort port;
    EpochBox box;
    PapyrusCalls calls(port, Probe(box), PapyrusCallOptions{});
    slsf::integrations::SexLabPPlus adapter(calls, {});
    const RequestId first = adapter.RequestScene(Epoch{1, 2, 3}, 0, {});
    port.Complete(first, Status::Ready, {StringVm(PPlusWire())});
    REQUIRE(adapter.Gate() == slsf::integrations::ExternalGate::Ready);

    const RequestId second =
        adapter.OnSceneEvent("StageChange", Epoch{1, 2, 3}, 5, {});
    REQUIRE(second.value != 0);
    REQUIRE(port.Count() == 2);
    port.Complete(second, Status::Ready, {StringVm(PPlusWire())});
    REQUIRE(adapter.Gate() == slsf::integrations::ExternalGate::Ready);
}

TEST_CASE("DDNG wire applies device and bondage masks to the requested actor") {
    slsf::integrations::DdngSnapshot snapshot;
    std::string error;
    REQUIRE(slsf::integrations::ParseDdngWire(DdngWire(), Epoch{1, 2, 3}, snapshot,
                                              error));
    REQUIRE(snapshot.equipmentRevision == 7);
    REQUIRE(snapshot.provider.apiVersion == 2);
    REQUIRE(snapshot.actors.size() == 1);

    slsf::gameplay::AnimationDeviousFacts facts;
    const auto applied = slsf::integrations::ApplyDdngToAnimation(snapshot, kPlayer, facts);
    REQUIRE(applied.applied);
    REQUIRE(facts.hood.IsKnownTrue());
    REQUIRE(facts.belt.IsKnownTrue());
    REQUIRE(facts.gag.IsKnownTrue());
    REQUIRE(facts.gagPanel.IsKnownFalse());
    REQUIRE(facts.boots.IsKnownTrue());
    REQUIRE(facts.blindfold.IsKnownTrue());
    REQUIRE(facts.lockable.IsKnownFalse());
    REQUIRE(facts.harness.IsKnownFalse());
    REQUIRE(facts.heavyBondage.IsKnownFalse());
    REQUIRE(facts.AcceptedLockables().size() == 17);
}

TEST_CASE("DDNG bondage flags synthesize heavy bondage and mittens") {
    std::string wire = DdngWire();
    const std::string line = "actor\tSLSF Reloaded.esp\t80A\t44\t1805\n";
    const std::size_t at = wire.find(line);
    REQUIRE(at != std::string::npos);
    wire.replace(at, line.size(), "actor\tSLSF Reloaded.esp\t80A\t82\t0\n");

    slsf::integrations::DdngSnapshot snapshot;
    std::string error;
    REQUIRE(slsf::integrations::ParseDdngWire(wire, Epoch{1, 2, 3}, snapshot, error));

    slsf::gameplay::AnimationDeviousFacts facts;
    REQUIRE(slsf::integrations::ApplyDdngToAnimation(snapshot, kPlayer, facts).applied);
    REQUIRE(facts.heavyBondage.IsKnownTrue());
    REQUIRE(facts.gloves.IsKnownTrue());
    REQUIRE(facts.belt.IsKnownFalse());
}

TEST_CASE("DDNG wire rejects the bondage sentinel bit") {
    std::string wire = DdngWire();
    const std::string line = "actor\tSLSF Reloaded.esp\t80A\t44\t1805\n";
    const std::size_t at = wire.find(line);
    REQUIRE(at != std::string::npos);
    wire.replace(at, line.size(), "actor\tSLSF Reloaded.esp\t80A\t200\t1805\n");
    slsf::integrations::DdngSnapshot snapshot;
    std::string error;
    REQUIRE_FALSE(slsf::integrations::ParseDdngWire(wire, Epoch{1, 2, 3}, snapshot,
                                                    error));
}

TEST_CASE("DDNG adapter serves the validated snapshot and refetches equipment changes") {
    TestPort port;
    EpochBox box;
    PapyrusCalls calls(port, Probe(box), PapyrusCallOptions{});
    slsf::integrations::DeviousDevices adapter(calls, {});

    int completions = 0;
    const RequestId first =
        adapter.RequestState(Epoch{1, 2, 3}, {kPlayer}, 0,
                             [&](const auto&, auto gate) {
                                 ++completions;
                                 REQUIRE(gate ==
                                         slsf::integrations::ExternalGate::Ready);
                             });
    REQUIRE(first.value != 0);
    REQUIRE(adapter.BuildRequestPayload(Epoch{1, 2, 3}, {kPlayer}).find("SLSFDDQ1") == 0);
    port.Complete(first, Status::Ready, {StringVm(DdngWire(7))});
    REQUIRE(completions == 1);
    REQUIRE(adapter.Gate() == slsf::integrations::ExternalGate::Ready);
    REQUIRE(adapter.Revision() == 7);

    adapter.OnEquipmentChanged(Epoch{1, 2, 3});
    REQUIRE(adapter.Gate() == slsf::integrations::ExternalGate::Changed);
    int refreshed = 0;
    const RequestId second =
        adapter.RequestState(Epoch{1, 2, 3}, {kPlayer}, 0,
                             [&](const auto&, auto gate) {
                                 ++refreshed;
                                 REQUIRE(gate == slsf::integrations::ExternalGate::Ready);
                             });
    REQUIRE(second.value != 0);
    REQUIRE(port.Count() == 2);
    port.Complete(second, Status::Ready, {StringVm(DdngWire(8))});
    REQUIRE(adapter.Revision() == 8);
    REQUIRE(refreshed == 1);
    REQUIRE(completions == 1);
}

TEST_CASE("DDNG equipment changes outside the active scene are ignored") {
    TestPort port;
    EpochBox box;
    PapyrusCalls calls(port, Probe(box), PapyrusCallOptions{});
    slsf::integrations::DeviousDevices adapter(calls, {});

    const RequestId id = adapter.RequestState(Epoch{1, 2, 3}, {kPlayer}, 0, {});
    REQUIRE(id.value != 0);
    adapter.OnEquipmentChanged(Epoch{1, 2, 4});
    REQUIRE(adapter.Gate() == slsf::integrations::ExternalGate::Waiting);

    port.Complete(id, Status::Ready, {StringVm(DdngWire(7))});
    REQUIRE(adapter.Gate() == slsf::integrations::ExternalGate::Ready);
    adapter.OnEquipmentChanged(Epoch{1, 2, 4});
    REQUIRE(adapter.Gate() == slsf::integrations::ExternalGate::Ready);
    REQUIRE(adapter.Revision() == 7);

    adapter.Cancel();
    adapter.OnEquipmentChanged(Epoch{1, 2, 3});
    REQUIRE(adapter.Gate() == slsf::integrations::ExternalGate::Closed);
}

TEST_CASE("DDNG actor outside the snapshot leaves facts unknown") {
    slsf::integrations::DdngSnapshot snapshot;
    std::string error;
    REQUIRE(slsf::integrations::ParseDdngWire(DdngWire(), Epoch{1, 2, 3}, snapshot,
                                              error));
    slsf::gameplay::AnimationDeviousFacts facts;
    const auto applied = slsf::integrations::ApplyDdngToAnimation(snapshot, kNpc, facts);
    REQUIRE_FALSE(applied.applied);
    REQUIRE(facts.gag.state == slsf::FactState::Unknown);
}

TEST_CASE("external wire exposes module and per-actor state") {
    slsf::integrations::ExternalSceneSnapshot snapshot;
    std::string error;
    REQUIRE(slsf::integrations::ParseExternalWire(ExternalWire(), Epoch{1, 2, 3},
                                                  snapshot, error));
    REQUIRE(snapshot.playerPresent);
    REQUIRE(snapshot.publicWhoreActive);
    REQUIRE(snapshot.mods.size() == 8);
    REQUIRE(slsf::integrations::ExternalModAvailable(
        snapshot, slsf::integrations::ExternalMod::PublicWhore));
    REQUIRE_FALSE(slsf::integrations::ExternalModAvailable(
        snapshot, slsf::integrations::ExternalMod::FertilityMode));
    REQUIRE(slsf::integrations::ExternalProviderFact(snapshot).IsKnownTrue());
    REQUIRE(slsf::integrations::ExternalPublicWhoreFact(snapshot).IsKnownTrue());
    REQUIRE(slsf::integrations::ExternalModFact(
                snapshot, slsf::integrations::ExternalMod::Bimbos).IsKnownFalse());

    const auto* actor = snapshot.FindActor(kPlayer);
    REQUIRE(actor != nullptr);
    REQUIRE(actor->isFemale);
    REQUIRE(actor->fertilityModeBreeder);
    REQUIRE_FALSE(actor->hentaiPregnant);
    REQUIRE(actor->inflationKnown);
    REQUIRE(actor->inflation == 12.5);
}

TEST_CASE("external wire rejects duplicate modules and non-finite numbers") {
    slsf::integrations::ExternalSceneSnapshot snapshot;
    std::string error;
    std::string wire = ExternalWire();
    wire += "mod\tPublicWhore\tAvailable\t2.0.0\n";
    REQUIRE_FALSE(slsf::integrations::ParseExternalWire(wire, Epoch{1, 2, 3}, snapshot,
                                                        error));
    REQUIRE(error.find("duplicate") != std::string::npos);

    wire = ExternalWire();
    const std::string replacement = "\t12.5\n";
    const std::size_t at = wire.find(replacement);
    REQUIRE(at != std::string::npos);
    wire.replace(at, replacement.size(), "\tnan\n");
    REQUIRE_FALSE(slsf::integrations::ParseExternalWire(wire, Epoch{1, 2, 3}, snapshot,
                                                        error));
}

TEST_CASE("external adapter retries on timeout and gates new scoring while missing") {
    TestPort port;
    EpochBox box;
    PapyrusCalls calls(port, Probe(box), PapyrusCallOptions{});
    slsf::integrations::ExternalModsOptions options;
    options.timing.timeoutMs = 100;
    options.timing.retryIntervalMs = 50;
    options.timing.maxAttempts = 3;
    slsf::integrations::ExternalMods adapter(calls, options);

    int completions = 0;
    const RequestId first =
        adapter.RequestState(Epoch{1, 2, 3}, {kPlayer}, 0,
                             [&](const auto&, auto) { ++completions; });
    REQUIRE(first.value != 0);
    adapter.Advance(100);
    REQUIRE(adapter.Attempts() == 1);
    REQUIRE(adapter.Gate() == slsf::integrations::ExternalGate::Waiting);
    adapter.Advance(149);
    REQUIRE(port.Count() == 1);
    adapter.Advance(150);
    REQUIRE(port.Count() == 2);
    port.Complete(RequestId{2}, Status::Ready, {StringVm(ExternalWire("Missing"))});
    REQUIRE(completions == 1);
    REQUIRE(adapter.Gate() == slsf::integrations::ExternalGate::Missing);
    REQUIRE(slsf::integrations::BlocksNewScoring(adapter.Gate()));
    REQUIRE_FALSE(slsf::integrations::ExternalProviderFact(adapter.Snapshot()).IsKnownTrue());
}

TEST_CASE("external state changes invalidate a ready snapshot") {
    TestPort port;
    EpochBox box;
    PapyrusCalls calls(port, Probe(box), PapyrusCallOptions{});
    slsf::integrations::ExternalMods adapter(calls, {});
    const RequestId first = adapter.RequestState(Epoch{1, 2, 3}, {kPlayer}, 0, {});
    port.Complete(first, Status::Ready, {StringVm(ExternalWire())});
    REQUIRE(adapter.Gate() == slsf::integrations::ExternalGate::Ready);

    adapter.OnModEvent("SLSF_External_Changed", 50);
    REQUIRE(adapter.Gate() == slsf::integrations::ExternalGate::Changed);
    adapter.Advance(50);
    REQUIRE(port.Count() == 2);
    port.Complete(RequestId{2}, Status::Ready, {StringVm(ExternalWire())});
    REQUIRE(adapter.Gate() == slsf::integrations::ExternalGate::Ready);
}

TEST_CASE("external state changes outside the active scene are ignored") {
    TestPort port;
    EpochBox box;
    PapyrusCalls calls(port, Probe(box), PapyrusCallOptions{});
    slsf::integrations::ExternalMods adapter(calls, {});

    const RequestId id = adapter.RequestState(Epoch{1, 2, 3}, {kPlayer}, 0, {});
    REQUIRE(id.value != 0);
    adapter.OnStateChanged(Epoch{1, 2, 4});
    REQUIRE(adapter.Gate() == slsf::integrations::ExternalGate::Waiting);

    port.Complete(id, Status::Ready, {StringVm(ExternalWire())});
    REQUIRE(adapter.Gate() == slsf::integrations::ExternalGate::Ready);
    adapter.OnStateChanged(Epoch{1, 2, 4});
    REQUIRE(adapter.Gate() == slsf::integrations::ExternalGate::Ready);
}

TEST_CASE("external mod events invalidate an in-flight request") {
    TestPort port;
    EpochBox box;
    PapyrusCalls calls(port, Probe(box), PapyrusCallOptions{});
    slsf::integrations::ExternalMods adapter(calls, {});

    int completions = 0;
    adapter.RequestState(Epoch{1, 2, 3}, {kPlayer}, 0,
                         [&](const auto&, auto) { ++completions; });
    adapter.OnModEvent("SLSF_External_Changed", 1);
    REQUIRE(adapter.Gate() == slsf::integrations::ExternalGate::Changed);
    REQUIRE(completions == 1);
    adapter.Advance(1);
    REQUIRE(port.Count() == 2);
}

TEST_CASE("external adapter rejects another scene epoch") {
    slsf::integrations::ExternalSceneSnapshot snapshot;
    std::string error;
    REQUIRE_FALSE(slsf::integrations::ParseExternalWire(ExternalWire(), Epoch{1, 2, 4},
                                                        snapshot, error));
    REQUIRE(error.find("epoch") != std::string::npos);
}

TEST_CASE("P+ interaction flags synthesize classification scope tags") {
    std::string wire = PPlusWire();
    const std::string oralTag = "tag\tscene\toral\n";
    const std::string oralMask = "\t1000\t0\n";
    const std::string analMask = "\t08000000\t0\n";
    const std::size_t tagAt = wire.find(oralTag);
    REQUIRE(tagAt != std::string::npos);
    wire.erase(tagAt, oralTag.size());
    const std::size_t maskAt = wire.find(oralMask);
    REQUIRE(maskAt != std::string::npos);
    wire.replace(maskAt, oralMask.size(), analMask);

    slsf::integrations::PPlusSceneState state;
    std::string error;
    REQUIRE(slsf::integrations::ParsePPlusSceneWire(wire, Epoch{1, 2, 3}, state, error));
    slsf::gameplay::AnimationFacts facts;
    REQUIRE(slsf::integrations::ApplyPPlusToAnimation(state, facts).applied);
    REQUIRE(std::find(facts.tags.begin(), facts.tags.end(), "Anal") != facts.tags.end());
    REQUIRE(std::find(facts.tags.begin(), facts.tags.end(), "Oral") == facts.tags.end());
}

TEST_CASE("P+ wire rejects duplicate singleton lines") {
    std::string wire = PPlusWire();
    const std::string anchor = "availability\tAvailable";
    const std::size_t at = wire.find(anchor);
    REQUIRE(at != std::string::npos);
    wire.insert(at, "availability\tAvailable\n");

    slsf::integrations::PPlusSceneState state;
    std::string error;
    REQUIRE_FALSE(slsf::integrations::ParsePPlusSceneWire(wire, Epoch{1, 2, 3}, state,
                                                          error));
    REQUIRE(error.find("availability") != std::string::npos);
}

TEST_CASE("P+ adapter invokes the completion once after malformed responses") {
    TestPort port;
    EpochBox box;
    PapyrusCalls calls(port, Probe(box), PapyrusCallOptions{});
    slsf::integrations::PPlusOptions options;
    options.timing.timeoutMs = 100;
    options.timing.retryIntervalMs = 10;
    options.timing.maxAttempts = 2;
    slsf::integrations::SexLabPPlus adapter(calls, options);

    int completions = 0;
    slsf::integrations::ExternalGate seenGate = slsf::integrations::ExternalGate::Waiting;
    const RequestId first = adapter.RequestScene(Epoch{1, 2, 3}, 0,
                                                 [&](const auto&, auto gate) {
                                                     ++completions;
                                                     seenGate = gate;
                                                 });
    REQUIRE(first.value != 0);
    port.Complete(first, Status::Ready, {StringVm(PPlusWire("Bogus"))});
    REQUIRE(completions == 0);
    REQUIRE(adapter.Gate() == slsf::integrations::ExternalGate::Waiting);

    adapter.Advance(10);
    REQUIRE(port.Count() == 2);
    port.Complete(RequestId{2}, Status::Ready, {StringVm(PPlusWire("Bogus"))});
    REQUIRE(completions == 1);
    REQUIRE(seenGate == slsf::integrations::ExternalGate::Failed);
    adapter.Advance(1000);
    REQUIRE(port.Count() == 2);
}

TEST_CASE("P+ transient transport failure keeps the completion for the final attempt") {
    TestPort port;
    EpochBox box;
    PapyrusCalls calls(port, Probe(box), PapyrusCallOptions{});
    slsf::integrations::PPlusOptions options;
    options.timing.timeoutMs = 100;
    options.timing.retryIntervalMs = 10;
    options.timing.maxAttempts = 2;
    slsf::integrations::SexLabPPlus adapter(calls, options);

    int completions = 0;
    slsf::integrations::ExternalGate seenGate = slsf::integrations::ExternalGate::Waiting;
    const RequestId first = adapter.RequestScene(
        Epoch{1, 2, 3}, 0, [&](const auto&, auto gate) {
            ++completions;
            seenGate = gate;
        });
    port.Complete(first, Status::Invalid);
    REQUIRE(completions == 0);
    REQUIRE(adapter.Gate() == slsf::integrations::ExternalGate::Waiting);

    adapter.Advance(10);
    REQUIRE(port.Count() == 2);
    port.Complete(RequestId{2}, Status::Ready, {StringVm(PPlusWire())});
    REQUIRE(completions == 1);
    REQUIRE(seenGate == slsf::integrations::ExternalGate::Ready);
}

TEST_CASE("P+ repeated requests keep the bounded attempt budget") {
    TestPort port;
    EpochBox box;
    PapyrusCalls calls(port, Probe(box), PapyrusCallOptions{});
    slsf::integrations::PPlusOptions options;
    options.timing.timeoutMs = 100;
    options.timing.retryIntervalMs = 10;
    options.timing.maxAttempts = 2;
    slsf::integrations::SexLabPPlus adapter(calls, options);

    int completions = 0;
    slsf::integrations::ExternalGate seenGate = slsf::integrations::ExternalGate::Waiting;
    const RequestId first = adapter.RequestScene(Epoch{1, 2, 3}, 0,
                                                 [&](const auto&, auto gate) {
                                                     ++completions;
                                                     seenGate = gate;
                                                 });
    const RequestId repeated = adapter.RequestScene(Epoch{1, 2, 3}, 0, {});
    REQUIRE(repeated.value == first.value);
    REQUIRE(port.Count() == 1);

    port.Complete(first, Status::Ready, {StringVm(PPlusWire("Bogus"))});
    REQUIRE(adapter.Attempts() == 1);
    const RequestId resumed = adapter.RequestScene(Epoch{1, 2, 3}, 0, {});
    REQUIRE(resumed.value != 0);
    REQUIRE(port.Count() == 2);
    port.Complete(resumed, Status::Ready, {StringVm(PPlusWire("Bogus"))});
    REQUIRE(completions == 1);
    REQUIRE(seenGate == slsf::integrations::ExternalGate::Failed);

    const RequestId terminal = adapter.RequestScene(Epoch{1, 2, 3}, 0,
                                                    [&](const auto&, auto gate) {
                                                        ++completions;
                                                        seenGate = gate;
                                                    });
    REQUIRE(terminal.value == 0);
    REQUIRE(port.Count() == 2);
    REQUIRE(completions == 2);
    REQUIRE(seenGate == slsf::integrations::ExternalGate::Failed);
}

TEST_CASE("DDNG repeated requests keep the bounded attempt budget") {
    TestPort port;
    EpochBox box;
    PapyrusCalls calls(port, Probe(box), PapyrusCallOptions{});
    slsf::integrations::DdngOptions options;
    options.timing.timeoutMs = 100;
    options.timing.retryIntervalMs = 10;
    options.timing.maxAttempts = 2;
    slsf::integrations::DeviousDevices adapter(calls, options);

    int completions = 0;
    const RequestId first = adapter.RequestState(
        Epoch{1, 2, 3}, {kPlayer}, 0,
        [&](const auto&, auto) { ++completions; });
    const RequestId repeated =
        adapter.RequestState(Epoch{1, 2, 3}, {kPlayer}, 0, {});
    REQUIRE(repeated.value == first.value);
    REQUIRE(port.Count() == 1);

    adapter.Advance(100);
    REQUIRE(adapter.Attempts() == 1);
    const RequestId resumed = adapter.RequestState(Epoch{1, 2, 3}, {kPlayer}, 100, {});
    REQUIRE(resumed.value != 0);
    REQUIRE(port.Count() == 2);
    adapter.Advance(210);
    REQUIRE(completions == 1);
    REQUIRE(adapter.Gate() == slsf::integrations::ExternalGate::Failed);

    const RequestId terminal =
        adapter.RequestState(Epoch{1, 2, 3}, {kPlayer}, 220,
                             [&](const auto&, auto) { ++completions; });
    REQUIRE(terminal.value == 0);
    REQUIRE(port.Count() == 2);
    REQUIRE(completions == 2);
}

TEST_CASE("external repeated requests keep the bounded attempt budget") {
    TestPort port;
    EpochBox box;
    PapyrusCalls calls(port, Probe(box), PapyrusCallOptions{});
    slsf::integrations::ExternalModsOptions options;
    options.timing.timeoutMs = 100;
    options.timing.retryIntervalMs = 10;
    options.timing.maxAttempts = 2;
    slsf::integrations::ExternalMods adapter(calls, options);

    int completions = 0;
    const RequestId first = adapter.RequestState(
        Epoch{1, 2, 3}, {kPlayer}, 0,
        [&](const auto&, auto) { ++completions; });
    const RequestId repeated =
        adapter.RequestState(Epoch{1, 2, 3}, {kPlayer}, 0, {});
    REQUIRE(repeated.value == first.value);
    REQUIRE(port.Count() == 1);

    adapter.Advance(100);
    REQUIRE(adapter.Attempts() == 1);
    const RequestId resumed = adapter.RequestState(Epoch{1, 2, 3}, {kPlayer}, 100, {});
    REQUIRE(resumed.value != 0);
    REQUIRE(port.Count() == 2);
    adapter.Advance(210);
    REQUIRE(completions == 1);
    REQUIRE(adapter.Gate() == slsf::integrations::ExternalGate::Failed);

    const RequestId terminal =
        adapter.RequestState(Epoch{1, 2, 3}, {kPlayer}, 220,
                             [&](const auto&, auto) { ++completions; });
    REQUIRE(terminal.value == 0);
    REQUIRE(port.Count() == 2);
    REQUIRE(completions == 2);
}

TEST_CASE("DDNG wire rejects duplicate providers and actors") {
    std::string providerWire = DdngWire();
    const std::size_t providerAt = providerWire.find("provider\tDevious Devices");
    REQUIRE(providerAt != std::string::npos);
    providerWire.insert(providerAt, "provider\tDevious Devices\t3.0.0\t2\n");
    slsf::integrations::DdngSnapshot snapshot;
    std::string error;
    REQUIRE_FALSE(slsf::integrations::ParseDdngWire(providerWire, Epoch{1, 2, 3}, snapshot,
                                                    error));

    std::string actorWire = DdngWire();
    const std::string actorLine = "actor\tSLSF Reloaded.esp\t80A\t44\t1805\n";
    const std::size_t actorAt = actorWire.find(actorLine);
    REQUIRE(actorAt != std::string::npos);
    actorWire.insert(actorAt + actorLine.size(), actorLine);
    REQUIRE_FALSE(slsf::integrations::ParseDdngWire(actorWire, Epoch{1, 2, 3}, snapshot,
                                                    error));
}

TEST_CASE("P+ stale request completes both waiters without submitting") {
    TestPort port;
    EpochBox box;
    PapyrusCalls calls(port, Probe(box), PapyrusCallOptions{});
    slsf::integrations::SexLabPPlus adapter(calls, {});

    int pending = 0;
    int fresh = 0;
    const RequestId first = adapter.RequestScene(
        Epoch{1, 2, 3}, 0, [&](const auto&, auto) { ++pending; });
    box.current = Epoch{1, 2, 4};
    const RequestId stale = adapter.RequestScene(
        Epoch{1, 2, 3}, 5, [&](const auto&, auto gate) {
            ++fresh;
            REQUIRE(gate == slsf::integrations::ExternalGate::Stale);
        });

    REQUIRE(stale.value == 0);
    REQUIRE(pending == 1);
    REQUIRE(fresh == 1);
    REQUIRE_FALSE(adapter.Active());
    REQUIRE(adapter.Gate() == slsf::integrations::ExternalGate::Stale);
    REQUIRE(port.Count() == 1);
    port.Complete(first, Status::Ready, {StringVm(PPlusWire())});
    REQUIRE(adapter.Gate() == slsf::integrations::ExternalGate::Stale);
}

TEST_CASE("P+ end event for an old epoch cannot close the current scene") {
    TestPort port;
    EpochBox box{1, 2, 4};
    PapyrusCalls calls(port, Probe(box), PapyrusCallOptions{});
    slsf::integrations::SexLabPPlus adapter(calls, {});

    const RequestId id = adapter.RequestScene(Epoch{1, 2, 4}, 0, {});
    adapter.OnSceneEnded(Epoch{1, 2, 3});
    REQUIRE(adapter.Active());
    REQUIRE(adapter.Gate() == slsf::integrations::ExternalGate::Waiting);
    port.Complete(id, Status::Ready,
                  {StringVm("SLSFPP1\nversion\t1\navailability\tAvailable\n"
                            "provider\tSLSF_Reloaded_PPlusProbe\t1.0.0\t1\n"
                            "epoch\t1\t2\t4\nactive\t1\nstage\t0\t1\n"
                            "actor\tSLSF Reloaded.esp\t80A\tPlayer\tother\t81\t8000\t0\n")});
    REQUIRE(adapter.Gate() == slsf::integrations::ExternalGate::Ready);
    adapter.OnSceneEnded(Epoch{1, 2, 4});
    REQUIRE_FALSE(adapter.Active());
    REQUIRE(adapter.Gate() == slsf::integrations::ExternalGate::Closed);
}

TEST_CASE("P+ stale request for an older scene cannot replace the current request") {
    TestPort port;
    EpochBox box;
    PapyrusCalls calls(port, Probe(box), PapyrusCallOptions{});
    slsf::integrations::SexLabPPlus adapter(calls, {});

    const Epoch firstEpoch{1, 2, 3};
    const Epoch currentEpoch{1, 2, 4};
    const RequestId first = adapter.RequestScene(firstEpoch, 0, {});
    port.Complete(first, Status::Ready, {StringVm(PPlusWire())});
    REQUIRE(adapter.Gate() == slsf::integrations::ExternalGate::Ready);

    box.current = currentEpoch;
    const RequestId current = adapter.RequestScene(currentEpoch, 10, {});
    REQUIRE(current.value != 0);

    int staleCompletions = 0;
    const RequestId stale = adapter.RequestScene(
        firstEpoch, 20, [&](const auto&, auto gate) {
            ++staleCompletions;
            REQUIRE(gate == slsf::integrations::ExternalGate::Stale);
        });
    REQUIRE(stale.value == 0);
    REQUIRE(staleCompletions == 1);
    REQUIRE(adapter.Active());
    REQUIRE(adapter.Gate() == slsf::integrations::ExternalGate::Waiting);
    REQUIRE(port.Count() == 2);

    std::string wire = PPlusWire();
    const std::string epochLine = "epoch\t1\t2\t3";
    wire.replace(wire.find(epochLine), epochLine.size(), "epoch\t1\t2\t4");
    port.Complete(current, Status::Ready, {StringVm(wire)});
    REQUIRE(adapter.Gate() == slsf::integrations::ExternalGate::Ready);
    REQUIRE(adapter.Snapshot().epoch == currentEpoch);
}

TEST_CASE("P+ events for an older scene cannot dirty the current scene") {
    TestPort port;
    EpochBox box;
    PapyrusCalls calls(port, Probe(box), PapyrusCallOptions{});
    slsf::integrations::SexLabPPlus adapter(calls, {});

    const RequestId id = adapter.RequestScene(Epoch{1, 2, 3}, 0, {});
    port.Complete(id, Status::Ready, {StringVm(PPlusWire())});
    REQUIRE(adapter.Gate() == slsf::integrations::ExternalGate::Ready);
    const std::size_t requests = port.Count();

    REQUIRE(adapter.OnSceneEvent("StageChange", Epoch{1, 2, 4}, 20, {}).value == 0);
    REQUIRE(adapter.Gate() == slsf::integrations::ExternalGate::Ready);
    REQUIRE(adapter.Active());
    REQUIRE(port.Count() == requests);
}

TEST_CASE("P+ closed scene does not reopen for the same epoch") {
    TestPort port;
    EpochBox box;
    PapyrusCalls calls(port, Probe(box), PapyrusCallOptions{});
    slsf::integrations::SexLabPPlus adapter(calls, {});

    const RequestId id = adapter.RequestScene(Epoch{1, 2, 3}, 0, {});
    port.Complete(id, Status::Ready, {StringVm(PPlusWire())});
    adapter.Cancel();
    REQUIRE(adapter.Gate() == slsf::integrations::ExternalGate::Closed);

    int completions = 0;
    const RequestId reopened = adapter.RequestScene(
        Epoch{1, 2, 3}, 10, [&](const auto&, auto gate) {
            ++completions;
            REQUIRE(gate == slsf::integrations::ExternalGate::Closed);
        });
    REQUIRE(reopened.value == 0);
    REQUIRE(completions == 1);
    REQUIRE(adapter.Gate() == slsf::integrations::ExternalGate::Closed);
    REQUIRE(port.Count() == 1);
}

TEST_CASE("P+ provider inactive frame closes the scene") {
    TestPort port;
    EpochBox box;
    PapyrusCalls calls(port, Probe(box), PapyrusCallOptions{});
    slsf::integrations::SexLabPPlus adapter(calls, {});

    const RequestId id = adapter.RequestScene(Epoch{1, 2, 3}, 0, {});
    std::string wire = PPlusWire();
    const std::string active = "active\t1\n";
    const std::size_t at = wire.find(active);
    REQUIRE(at != std::string::npos);
    wire.replace(at, active.size(), "active\t0\n");
    port.Complete(id, Status::Ready, {StringVm(wire)});

    REQUIRE(adapter.Gate() == slsf::integrations::ExternalGate::Closed);
    REQUIRE_FALSE(adapter.Active());
    int completions = 0;
    REQUIRE(adapter.RequestScene(Epoch{1, 2, 3}, 10,
                                 [&](const auto&, auto gate) {
                                     ++completions;
                                     REQUIRE(gate == slsf::integrations::ExternalGate::Closed);
                                 }).value == 0);
    REQUIRE(completions == 1);
    REQUIRE(port.Count() == 1);
}

TEST_CASE("DDNG new scene supersedes an old in-flight request once") {
    TestPort port;
    EpochBox box;
    PapyrusCalls calls(port, Probe(box), PapyrusCallOptions{});
    slsf::integrations::DeviousDevices adapter(calls, {});

    int oldCompletions = 0;
    const RequestId old = adapter.RequestState(
        Epoch{1, 2, 3}, {kPlayer}, 0, [&](const auto&, auto gate) {
            ++oldCompletions;
            REQUIRE(gate == slsf::integrations::ExternalGate::Stale);
        });
    box.current = Epoch{1, 2, 4};
    const RequestId current =
        adapter.RequestState(Epoch{1, 2, 4}, {kPlayer}, 10, {});
    REQUIRE(current.value != 0);
    REQUIRE(oldCompletions == 1);
    REQUIRE(port.Count() == 2);

    port.Complete(old, Status::Ready, {StringVm(DdngWire())});
    REQUIRE(adapter.Gate() == slsf::integrations::ExternalGate::Waiting);
    REQUIRE(adapter.Snapshot().actors.empty());

    std::string wire = DdngWire(8);
    const std::string epochLine = "epoch\t1\t2\t3";
    wire.replace(wire.find(epochLine), epochLine.size(), "epoch\t1\t2\t4");
    port.Complete(current, Status::Ready, {StringVm(wire)});
    REQUIRE(adapter.Gate() == slsf::integrations::ExternalGate::Ready);
    REQUIRE(adapter.Revision() == 8);
}

TEST_CASE("DDNG closed scene does not reopen for the same epoch") {
    TestPort port;
    EpochBox box;
    PapyrusCalls calls(port, Probe(box), PapyrusCallOptions{});
    slsf::integrations::DeviousDevices adapter(calls, {});

    const RequestId id = adapter.RequestState(Epoch{1, 2, 3}, {kPlayer}, 0, {});
    port.Complete(id, Status::Ready, {StringVm(DdngWire())});
    adapter.Cancel();
    REQUIRE(adapter.Gate() == slsf::integrations::ExternalGate::Closed);

    int completions = 0;
    const RequestId reopened = adapter.RequestState(
        Epoch{1, 2, 3}, {kPlayer}, 10, [&](const auto&, auto gate) {
            ++completions;
            REQUIRE(gate == slsf::integrations::ExternalGate::Closed);
        });
    REQUIRE(reopened.value == 0);
    REQUIRE(completions == 1);
    REQUIRE(adapter.Gate() == slsf::integrations::ExternalGate::Closed);
    REQUIRE(port.Count() == 1);
}

TEST_CASE("external stale request cannot replace the current request") {
    TestPort port;
    EpochBox box;
    PapyrusCalls calls(port, Probe(box), PapyrusCallOptions{});
    slsf::integrations::ExternalMods adapter(calls, {});

    const RequestId first = adapter.RequestState(Epoch{1, 2, 3}, {kPlayer}, 0, {});
    port.Complete(first, Status::Ready, {StringVm(ExternalWire())});
    REQUIRE(adapter.Gate() == slsf::integrations::ExternalGate::Ready);

    box.current = Epoch{1, 2, 4};
    const RequestId current =
        adapter.RequestState(Epoch{1, 2, 4}, {kPlayer}, 10, {});
    REQUIRE(current.value != 0);

    int staleCompletions = 0;
    const RequestId stale = adapter.RequestState(
        Epoch{1, 2, 3}, {kPlayer}, 20, [&](const auto&, auto gate) {
            ++staleCompletions;
            REQUIRE(gate == slsf::integrations::ExternalGate::Stale);
        });
    REQUIRE(stale.value == 0);
    REQUIRE(staleCompletions == 1);
    REQUIRE(adapter.Gate() == slsf::integrations::ExternalGate::Waiting);
    REQUIRE(port.Count() == 2);
}

TEST_CASE("P+ wire rejects interaction mask bits outside the ABI") {
    std::string wire = PPlusWire();
    const std::string line = "\t8000\t10000\n";
    const std::size_t at = wire.find(line);
    REQUIRE(at != std::string::npos);
    wire.replace(at, line.size(), "\t80000000\t0\n");
    slsf::integrations::PPlusSceneState state;
    std::string error;
    REQUIRE_FALSE(slsf::integrations::ParsePPlusSceneWire(wire, Epoch{1, 2, 3}, state,
                                                          error));
    REQUIRE(error.find("interaction") != std::string::npos);
}

TEST_CASE("DDNG chastity state contributes belt and bra facts") {
    std::string wire = DdngWire();
    const std::string line = "actor\tSLSF Reloaded.esp\t80A\t44\t1805\n";
    const std::size_t at = wire.find(line);
    REQUIRE(at != std::string::npos);
    wire.replace(at, line.size(), "actor\tSLSF Reloaded.esp\t80A\t28\t0\n");

    slsf::integrations::DdngSnapshot snapshot;
    std::string error;
    REQUIRE(slsf::integrations::ParseDdngWire(wire, Epoch{1, 2, 3}, snapshot, error));
    slsf::gameplay::AnimationDeviousFacts facts;
    REQUIRE(slsf::integrations::ApplyDdngToAnimation(snapshot, kPlayer, facts).applied);
    REQUIRE(facts.belt.IsKnownTrue());
    REQUIRE(facts.bra.IsKnownTrue());
    REQUIRE(facts.gag.IsKnownFalse());
}

TEST_CASE("DDNG wire rejects unknown keys") {
    std::string wire = DdngWire();
    wire += "bogus\t1\n";
    slsf::integrations::DdngSnapshot snapshot;
    std::string error;
    REQUIRE_FALSE(slsf::integrations::ParseDdngWire(wire, Epoch{1, 2, 3}, snapshot,
                                                    error));
    REQUIRE(error.find("unknown") != std::string::npos);
}

TEST_CASE("external wire requires the public whore flag when available") {
    std::string wire = ExternalWire();
    const std::size_t at = wire.find("publicwhore\t1\n");
    REQUIRE(at != std::string::npos);
    wire.erase(at, std::string("publicwhore\t1\n").size());
    slsf::integrations::ExternalSceneSnapshot snapshot;
    std::string error;
    REQUIRE_FALSE(slsf::integrations::ParseExternalWire(wire, Epoch{1, 2, 3}, snapshot,
                                                        error));
    REQUIRE(error.find("required") != std::string::npos);
}

TEST_CASE("external facts keep missing modules distinct from unavailable providers") {
    slsf::integrations::ExternalSceneSnapshot available;
    std::string error;
    REQUIRE(slsf::integrations::ParseExternalWire(ExternalWire(), Epoch{1, 2, 3},
                                                  available, error));
    REQUIRE(slsf::integrations::ExternalModFact(
                available, slsf::integrations::ExternalMod::FertilityMode).IsKnownFalse());
    REQUIRE(slsf::integrations::ExternalModFact(
                available, slsf::integrations::ExternalMod::Bimbos).IsKnownFalse());

    slsf::integrations::ExternalSceneSnapshot missing;
    REQUIRE(slsf::integrations::ParseExternalWire(ExternalWire("Missing"), Epoch{1, 2, 3},
                                                  missing, error));
    REQUIRE(slsf::integrations::ExternalModFact(
                missing, slsf::integrations::ExternalMod::PublicWhore).IsKnownFalse());
    REQUIRE(slsf::integrations::ExternalPublicWhoreFact(missing).IsKnownFalse());
}

TEST_CASE("public whore fact preserves an incompatible module state") {
    std::string wire = ExternalWire();
    const std::string line = "mod\tPublicWhore\tAvailable\t2.0.0\n";
    const std::size_t at = wire.find(line);
    REQUIRE(at != std::string::npos);
    wire.replace(at, line.size(), "mod\tPublicWhore\tIncompatible\t2.0.0\n");
    wire.replace(wire.find("publicwhore\t1\n"), std::string("publicwhore\t1\n").size(),
                 "publicwhore\t0\n");

    slsf::integrations::ExternalSceneSnapshot snapshot;
    std::string error;
    REQUIRE(slsf::integrations::ParseExternalWire(wire, Epoch{1, 2, 3}, snapshot, error));
    const slsf::Fact fact = slsf::integrations::ExternalPublicWhoreFact(snapshot);
    REQUIRE_FALSE(fact.IsKnownTrue());
    REQUIRE_FALSE(fact.IsKnownFalse());
}

TEST_CASE("external wire rejects duplicate public whore lines") {
    std::string wire = ExternalWire();
    const std::size_t at = wire.find("publicwhore\t1\n");
    REQUIRE(at != std::string::npos);
    wire.insert(at, "publicwhore\t1\n");
    slsf::integrations::ExternalSceneSnapshot snapshot;
    std::string error;
    REQUIRE_FALSE(slsf::integrations::ParseExternalWire(wire, Epoch{1, 2, 3}, snapshot,
                                                        error));
}


TEST_CASE("external wire rejects duplicate player and actor lines") {
    std::string wire = ExternalWire();
    const std::size_t playerAt = wire.find("player\t1\n");
    REQUIRE(playerAt != std::string::npos);
    wire.insert(playerAt, "player\t1\n");
    slsf::integrations::ExternalSceneSnapshot snapshot;
    std::string error;
    REQUIRE_FALSE(slsf::integrations::ParseExternalWire(wire, Epoch{1, 2, 3}, snapshot,
                                                        error));
}

TEST_CASE("P+ replaced waiter receives changed exactly once") {
    TestPort port;
    EpochBox box;
    PapyrusCalls calls(port, Probe(box), PapyrusCallOptions{});
    slsf::integrations::SexLabPPlus adapter(calls, {});

    int firstCount = 0;
    int secondCount = 0;
    slsf::integrations::ExternalGate firstGate = slsf::integrations::ExternalGate::Waiting;
    slsf::integrations::ExternalGate secondGate = slsf::integrations::ExternalGate::Waiting;
    const RequestId first = adapter.RequestScene(
        Epoch{1, 2, 3}, 0, [&](const auto&, auto gate) {
            ++firstCount;
            firstGate = gate;
        });
    const RequestId second = adapter.RequestScene(
        Epoch{1, 2, 3}, 1, [&](const auto&, auto gate) {
            ++secondCount;
            secondGate = gate;
        });
    REQUIRE(second.value == first.value);
    REQUIRE(firstCount == 1);
    REQUIRE(firstGate == slsf::integrations::ExternalGate::Changed);
    REQUIRE(port.Count() == 1);

    port.Complete(second, Status::Ready, {StringVm(PPlusWire())});
    REQUIRE(firstCount == 1);
    REQUIRE(secondCount == 1);
    REQUIRE(secondGate == slsf::integrations::ExternalGate::Ready);
}

TEST_CASE("P+ new scene start supersedes an active scene") {
    TestPort port;
    EpochBox box;
    PapyrusCalls calls(port, Probe(box), PapyrusCallOptions{});
    slsf::integrations::SexLabPPlus adapter(calls, {});
    const RequestId first = adapter.RequestScene(Epoch{1, 2, 3}, 0, {});
    port.Complete(first, Status::Ready, {StringVm(PPlusWire())});
    REQUIRE(adapter.Active());

    box.current = Epoch{1, 2, 4};
    const RequestId second =
        adapter.OnSceneEvent("AnimationStart", Epoch{1, 2, 4}, 10, {});
    REQUIRE(second.value != 0);
    REQUIRE(port.Count() == 2);
    REQUIRE(adapter.Active());
    REQUIRE(adapter.Snapshot().actors.empty());

    std::string wire = PPlusWire();
    const std::size_t at = wire.find("epoch\t1\t2\t3");
    REQUIRE(at != std::string::npos);
    wire.replace(at, std::string("epoch\t1\t2\t3").size(), "epoch\t1\t2\t4");
    port.Complete(second, Status::Ready, {StringVm(wire)});
    REQUIRE(adapter.Gate() == slsf::integrations::ExternalGate::Ready);
    REQUIRE(adapter.Snapshot().epoch == Epoch{1, 2, 4});
}

TEST_CASE("P+ facial interaction does not synthesize oral") {
    slsf::integrations::PPlusSceneState state;
    std::string error;
    REQUIRE(slsf::integrations::ParsePPlusSceneWire(PPlusWire(), Epoch{1, 2, 3}, state,
                                                    error));
    state.tags.clear();
    for (auto& actor : state.actors) {
        actor.interactions = 0;
    }
    state.actors[1].interactions =
        1u << static_cast<unsigned>(slsf::integrations::PPlusInteraction::aFacial);
    slsf::gameplay::AnimationFacts facts;
    REQUIRE(slsf::integrations::ApplyPPlusToAnimation(state, facts).applied);
    REQUIRE(std::find(facts.tags.begin(), facts.tags.end(), "Oral") == facts.tags.end());
}

TEST_CASE("P+ LeadInEnd does not read the previous scene as ready") {
    TestPort port;
    EpochBox box;
    PapyrusCalls calls(port, Probe(box), PapyrusCallOptions{});
    slsf::integrations::SexLabPPlus adapter(calls, {});
    const RequestId first = adapter.RequestScene(Epoch{1, 2, 3}, 0, {});
    port.Complete(first, Status::Ready, {StringVm(PPlusWire())});
    REQUIRE(adapter.Gate() == slsf::integrations::ExternalGate::Ready);

    const RequestId leadIn =
        adapter.OnSceneEvent("SexLabLeadInEnd", Epoch{1, 2, 3}, 5, {});
    REQUIRE(leadIn.value != 0);
    REQUIRE(port.Count() == 2);
    REQUIRE(adapter.Gate() == slsf::integrations::ExternalGate::Waiting);
    port.Complete(leadIn, Status::Ready, {StringVm(PPlusWire())});
    REQUIRE(adapter.Gate() == slsf::integrations::ExternalGate::Waiting);
    adapter.Advance(255);
    REQUIRE(port.Count() == 3);
    std::string main = PPlusWire();
    main.replace(main.find("stage\t2\t4"), 9, "stage\t3\t4");
    port.Complete(RequestId{3}, Status::Ready, {StringVm(main)});
    REQUIRE(adapter.Gate() == slsf::integrations::ExternalGate::Ready);
    REQUIRE(adapter.Snapshot().stage == 3);
}

TEST_CASE("P+ unknown availability retries within the bounded policy") {
    TestPort port;
    EpochBox box;
    PapyrusCalls calls(port, Probe(box), PapyrusCallOptions{});
    slsf::integrations::PPlusOptions options;
    options.timing.timeoutMs = 100;
    options.timing.retryIntervalMs = 10;
    options.timing.maxAttempts = 2;
    slsf::integrations::SexLabPPlus adapter(calls, options);

    int completions = 0;
    slsf::integrations::ExternalGate seenGate = slsf::integrations::ExternalGate::Waiting;
    const RequestId first = adapter.RequestScene(
        Epoch{1, 2, 3}, 0, [&](const auto&, auto gate) {
            ++completions;
            seenGate = gate;
        });
    port.Complete(first, Status::Ready, {StringVm(PPlusWire("Unknown"))});
    REQUIRE(adapter.Attempts() == 1);
    REQUIRE(adapter.Gate() == slsf::integrations::ExternalGate::Waiting);
    REQUIRE(completions == 0);

    adapter.Advance(10);
    REQUIRE(port.Count() == 2);
    port.Complete(RequestId{2}, Status::Ready, {StringVm(PPlusWire("Unknown"))});
    REQUIRE(adapter.Attempts() == 2);
    REQUIRE(adapter.Gate() == slsf::integrations::ExternalGate::Failed);
    REQUIRE(completions == 1);
    REQUIRE(seenGate == slsf::integrations::ExternalGate::Failed);
    adapter.Advance(1000);
    REQUIRE(port.Count() == 2);
}

TEST_CASE("P+ adapter notifies a replaced waiter when a retry is reclaimed") {
    TestPort port;
    EpochBox box;
    PapyrusCalls calls(port, Probe(box), PapyrusCallOptions{});
    slsf::integrations::PPlusOptions options;
    options.timing.timeoutMs = 100;
    options.timing.retryIntervalMs = 10;
    options.timing.maxAttempts = 3;
    slsf::integrations::SexLabPPlus adapter(calls, options);

    int oldCompletions = 0;
    int newCompletions = 0;
    const RequestId first = adapter.RequestScene(
        Epoch{1, 2, 3}, 0, [&](const auto&, auto) { ++oldCompletions; });
    port.Complete(first, Status::Invalid);
    REQUIRE(oldCompletions == 0);
    REQUIRE(adapter.Gate() == slsf::integrations::ExternalGate::Waiting);

    const RequestId replacement = adapter.RequestScene(
        Epoch{1, 2, 3}, 0, [&](const auto&, auto) { ++newCompletions; });
    REQUIRE(replacement.value != 0);
    REQUIRE(oldCompletions == 1);
    port.Complete(replacement, Status::Ready, {StringVm(PPlusWire())});
    REQUIRE(newCompletions == 1);
}

TEST_CASE("Papyrus result epoch mismatch is reported stale") {
    TestPort port;
    EpochBox box;
    PapyrusCalls calls(port, Probe(box), PapyrusCallOptions{});
    slsf::integrations::SexLabPPlus adapter(calls, {});
    int completions = 0;
    slsf::integrations::ExternalGate gate = slsf::integrations::ExternalGate::Waiting;
    const RequestId id = adapter.RequestScene(
        Epoch{1, 2, 3}, 0, [&](const auto&, auto value) {
            ++completions;
            gate = value;
        });
    port.CompleteWithEpoch(id, Status::Ready, Epoch{1, 2, 4}, {StringVm(PPlusWire())});
    REQUIRE(completions == 1);
    REQUIRE(gate == slsf::integrations::ExternalGate::Stale);
    REQUIRE_FALSE(adapter.Active());
}

TEST_CASE("DDNG replaced waiter receives changed exactly once") {
    TestPort port;
    EpochBox box;
    PapyrusCalls calls(port, Probe(box), PapyrusCallOptions{});
    slsf::integrations::DeviousDevices adapter(calls, {});
    int firstCount = 0;
    int secondCount = 0;
    slsf::integrations::ExternalGate firstGate = slsf::integrations::ExternalGate::Waiting;
    const RequestId first = adapter.RequestState(
        Epoch{1, 2, 3}, {kPlayer}, 0, [&](const auto&, auto gate) {
            ++firstCount;
            firstGate = gate;
        });
    const RequestId second = adapter.RequestState(
        Epoch{1, 2, 3}, {kPlayer}, 1, [&](const auto&, auto) { ++secondCount; });
    REQUIRE(second.value == first.value);
    REQUIRE(firstCount == 1);
    REQUIRE(firstGate == slsf::integrations::ExternalGate::Changed);
    port.Complete(second, Status::Ready, {StringVm(DdngWire())});
    REQUIRE(firstCount == 1);
    REQUIRE(secondCount == 1);
}

TEST_CASE("DDNG anal chastity contributes a belt fact") {
    slsf::integrations::DdngSnapshot snapshot;
    std::string error;
    REQUIRE(slsf::integrations::ParseDdngWire(DdngWire(), Epoch{1, 2, 3}, snapshot, error));
    REQUIRE(snapshot.actors.size() == 1);
    snapshot.actors[0].devices = 0;
    snapshot.actors[0].bondage = slsf::integrations::ddng_bondage::ChastifiedAnal;
    slsf::gameplay::AnimationDeviousFacts facts;
    REQUIRE(slsf::integrations::ApplyDdngToAnimation(snapshot, kPlayer, facts).applied);
    REQUIRE(facts.belt.IsKnownTrue());
}

TEST_CASE("external replaced waiter receives changed exactly once") {
    TestPort port;
    EpochBox box;
    PapyrusCalls calls(port, Probe(box), PapyrusCallOptions{});
    slsf::integrations::ExternalMods adapter(calls, {});
    int firstCount = 0;
    int secondCount = 0;
    slsf::integrations::ExternalGate firstGate = slsf::integrations::ExternalGate::Waiting;
    const RequestId first = adapter.RequestState(
        Epoch{1, 2, 3}, {kPlayer}, 0, [&](const auto&, auto gate) {
            ++firstCount;
            firstGate = gate;
        });
    const RequestId second = adapter.RequestState(
        Epoch{1, 2, 3}, {kPlayer}, 1, [&](const auto&, auto) { ++secondCount; });
    REQUIRE(second.value == first.value);
    REQUIRE(firstCount == 1);
    REQUIRE(firstGate == slsf::integrations::ExternalGate::Changed);
    port.Complete(second, Status::Ready, {StringVm(ExternalWire())});
    REQUIRE(firstCount == 1);
    REQUIRE(secondCount == 1);
}

TEST_CASE("wire parsers handle deterministic mutated input") {
    std::mt19937 rng(1170);
    auto mutate = [&rng](std::string base) {
        const std::size_t operations = 1 + (rng() % 8);
        for (std::size_t index = 0; index < operations; ++index) {
            if (base.empty()) {
                base.push_back(static_cast<char>(rng() % 256));
                continue;
            }
            const std::size_t at = rng() % base.size();
            switch (rng() % 3) {
            case 0:
                base[at] = static_cast<char>(rng() % 256);
                break;
            case 1:
                base.insert(base.begin() + static_cast<std::ptrdiff_t>(at),
                            static_cast<char>(rng() % 256));
                break;
            default:
                base.erase(base.begin() + static_cast<std::ptrdiff_t>(at));
                break;
            }
        }
        return base;
    };

    for (std::size_t index = 0; index < 2000; ++index) {
        slsf::integrations::PPlusSceneState pplus;
        std::string pplusError;
        (void)slsf::integrations::ParsePPlusSceneWire(mutate(PPlusWire()), Epoch{1, 2, 3},
                                                      pplus, pplusError);
        REQUIRE(pplus.actors.size() <= 64);

        slsf::integrations::DdngSnapshot ddng;
        std::string ddngError;
        (void)slsf::integrations::ParseDdngWire(mutate(DdngWire()), Epoch{1, 2, 3}, ddng,
                                                ddngError);
        REQUIRE(ddng.actors.size() <= 64);

        slsf::integrations::ExternalSceneSnapshot external;
        std::string externalError;
        (void)slsf::integrations::ParseExternalWire(mutate(ExternalWire()), Epoch{1, 2, 3},
                                                    external, externalError);
        REQUIRE(external.actors.size() <= 64);
    }
}
