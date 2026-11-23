#include <catch2/catch_test_macros.hpp>
#include "slsf/boundary/GameplayState.h"
#include "slsf/compat/StorageOutputQueue.h"

TEST_CASE("storage outputs serialize revisions and retire abandoned sessions", "[game-ready][boundary]") {
    using namespace slsf;
    using namespace slsf::compat;
    StorageOutputQueue queue;
    const Epoch first{1, 2, 3}, second{2, 2, 3};
    queue.Submit({{first, "key1", {1}}, {first, "key2", {1}}});
    const auto a = queue.Take();
    REQUIRE(a);
    queue.Submit({{first, "key1", {2}}});
    queue.Submit({{first, "key1", {3}}, {first, "key2", {3}}});
    REQUIRE_FALSE(queue.Take());
    REQUIRE(queue.Current(std::to_string(a->request), first));
    queue.Complete(a->request, true);
    const auto c = queue.Take();
    REQUIRE(c);
    REQUIRE(c->values == std::vector<std::int32_t>{3});
    REQUIRE(c->key == "key2");
    queue.Complete(a->request, true);
    REQUIRE_FALSE(queue.Take());
    queue.Submit({{second, "key1", {4}}});
    const auto d = queue.Take();
    REQUIRE(d);
    REQUIRE(d->request != c->request);
    REQUIRE_FALSE(queue.Current(std::to_string(c->request), first));
    REQUIRE_FALSE(queue.Current(std::to_string(c->request), second));
    queue.Complete(c->request, true);
    REQUIRE(queue.Current(std::to_string(d->request), second));
    REQUIRE_FALSE(queue.Take());
    queue.Complete(d->request, true);
    REQUIRE_FALSE(queue.Take());
}

TEST_CASE("storage refreshes keep progress and write the newest value for each key", "[game-ready][boundary]") {
    using namespace slsf;
    using namespace slsf::compat;
    StorageOutputQueue queue;
    const Epoch epoch{1, 2, 3};
    queue.Submit({{epoch, "first", {1}}, {epoch, "second", {2}}, {epoch, "third", {3}}});
    const auto first = queue.Take();
    REQUIRE(first);
    CHECK(first->key == "first");
    queue.Submit({{epoch, "first", {1}}, {epoch, "second", {20}}, {epoch, "third", {3}}});
    queue.Complete(first->request, true);
    const auto second = queue.Take();
    REQUIRE(second);
    CHECK(second->key == "second");
    CHECK(second->values == std::vector<std::int32_t>{20});
    queue.Submit({{epoch, "first", {1}}, {epoch, "second", {21}}, {epoch, "third", {30}}});
    queue.Complete(second->request, true);
    const auto third = queue.Take();
    REQUIRE(third);
    CHECK(third->key == "third");
    CHECK(third->values == std::vector<std::int32_t>{30});
    queue.Complete(third->request, true);
    const auto latest = queue.Take();
    REQUIRE(latest);
    CHECK(latest->key == "second");
    CHECK(latest->values == std::vector<std::int32_t>{21});
    queue.Complete(latest->request, true);
    queue.Submit({{epoch, "first", {1}}, {epoch, "second", {21}}, {epoch, "third", {30}}});
    REQUIRE_FALSE(queue.Take());
}

TEST_CASE("storage failures do not mark a partial list as committed", "[game-ready][boundary]") {
    using namespace slsf;
    using namespace slsf::compat;
    StorageOutputQueue queue;
    const Epoch epoch{1, 0, 0};
    queue.Submit({{epoch, "key", {1}}});
    const auto original = queue.Take();
    REQUIRE(original);
    queue.Complete(original->request, true);
    queue.Submit({{epoch, "key", {2}}});
    const auto failed = queue.Take();
    REQUIRE(failed);
    queue.Complete(failed->request, false);
    queue.Submit({{epoch, "key", {1}}});
    const auto retry = queue.Take();
    REQUIRE(retry);
    CHECK(retry->values == std::vector<std::int32_t>{1});
    CHECK_FALSE(queue.Current(std::to_string(failed->request), epoch));
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
TEST_CASE("unregistered recognition memory is not persisted or restored on return", "[game-ready][boundary]") {
    using namespace slsf;
    using namespace slsf::boundary;
    RegionSnapshot regions;
    regions.regions = {{"Skyrim.esm", 1}, {"TestCustom.esp", 2}};
    std::vector<RegionRecognitionState> saved{
        {regions.regions[0], 14, 5, 9, false, false, false},
        {regions.regions[1], 20, 10, 9, false, false, true},
    };
    gameplay::AnonymityRules rules;
    std::optional<RegionId> current;
    RestoreRecognition(rules, current, saved, regions);

    SECTION("removing a different location preserves the current region") {
        REQUIRE_FALSE(ForgetRecognition(rules, current, RegionId{0}));
        CHECK(current == RegionId{1});
        CHECK(rules.Find(RegionId{0}) == nullptr);
        CHECK(CaptureRecognition(rules, current, regions).size() == 1);
    }
    SECTION("removing the current location starts a fresh stay in the fallback region") {
        REQUIRE(ForgetRecognition(rules, current, RegionId{1}));
        CHECK_FALSE(current);
        TransitionRecognition(rules, current, RegionId{0}, GameTime{11});
        CHECK(current == RegionId{0});
        const auto captured = CaptureRecognition(rules, current, regions);
        REQUIRE(captured.size() == 1);
        CHECK(captured[0].key == regions.regions[0]);
        CHECK(captured[0].resident);
        CHECK(captured[0].lastEnter == 11);
        CHECK(rules.Find(RegionId{1}) == nullptr);

        TransitionRecognition(rules, current, RegionId{1}, GameTime{12});
        REQUIRE(rules.Find(RegionId{1}));
        CHECK(rules.Find(RegionId{1})->recognitionDays != 20);
    }
}
