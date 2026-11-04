#include <catch2/catch_test_macros.hpp>
#include "slsf/boundary/GameplayState.h"
#include "slsf/compat/StorageOutputQueue.h"

TEST_CASE("storage outputs serialize revisions and retire abandoned sessions", "[game-ready][boundary]") {
    using namespace slsf;
    using namespace slsf::compat;
    StorageOutputQueue queue;
    const Epoch first{1, 2, 3}, second{2, 2, 3};
    queue.Submit({{first, "a", "key1", {1}}, {first, "a", "key2", {1}}});
    const auto a = queue.Take();
    REQUIRE(a);
    queue.Submit({{first, "b", "key1", {2}}});
    queue.Submit({{first, "c", "key1", {3}}, {first, "c", "key2", {3}}});
    REQUIRE_FALSE(queue.Take());
    REQUIRE(queue.Current("a", first));
    queue.Complete(a->request);
    const auto c = queue.Take();
    REQUIRE(c);
    REQUIRE(c->values == std::vector<std::int32_t>{3});
    REQUIRE(c->key == "key1");
    queue.Complete(a->request);
    REQUIRE_FALSE(queue.Take());
    queue.Submit({{second, "d", "key1", {4}}});
    const auto d = queue.Take();
    REQUIRE(d);
    REQUIRE(d->request != c->request);
    REQUIRE_FALSE(queue.Current("c", first));
    queue.Complete(c->request);
    REQUIRE(queue.Current("d", second));
    REQUIRE_FALSE(queue.Take());
    queue.Complete(d->request);
    REQUIRE_FALSE(queue.Take());
}

TEST_CASE("recognition restore preserves an equal-timestamp resident and transitions once", "[game-ready][boundary]") {
    using namespace slsf;
    using namespace slsf::boundary;
    RegionSnapshot regions;
    regions.regions = {{"Skyrim.esm", 1}, {"Skyrim.esm", 2}};
    std::vector<RegionRecognitionState> saved{{regions.regions[0], 14, 10, 10, false, false, true}};
    gameplay::AnonymityRules rules;
    std::optional<RegionId> current;
    RestoreRecognition(rules, current, saved, regions);
    REQUIRE(current == RegionId{0});
    TransitionRecognition(rules, current, RegionId{0}, GameTime{11});
    REQUIRE(rules.Find(RegionId{0})->lastEnter.days == 10);
    REQUIRE(rules.Find(RegionId{0})->recognitionDays == 14);
    REQUIRE_FALSE(rules.Find(RegionId{0})->localAnonymous);
    TransitionRecognition(rules, current, RegionId{1}, GameTime{11});
    REQUIRE(current == RegionId{1});
    const auto captured = CaptureRecognition(rules, current, regions);
    REQUIRE(captured.size() == 2);
    REQUIRE_FALSE(captured[0].resident);
    REQUIRE(captured[1].resident);
    REQUIRE(captured[1].lastEnter == 11);
    TransitionRecognition(rules, current, RegionId{1}, GameTime{12});
    REQUIRE(rules.Find(RegionId{1})->lastEnter.days == 11);
    RestoreRecognition(rules, current, captured, regions);
    TransitionRecognition(rules, current, RegionId{1}, GameTime{13});
    REQUIRE(rules.Find(RegionId{1})->lastEnter.days == 11);
}
