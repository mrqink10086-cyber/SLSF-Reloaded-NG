#include "slsf/runtime/ClockService.h"

#include "slsf/core/Types.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <limits>
#include <memory>

namespace {

using slsf::GameTime;
using slsf::runtime::ClockService;
using slsf::runtime::ClockSnapshot;

// Fully deterministic wall clock: the test hands out seconds manually.
class FakeWall {
public:
    explicit FakeWall(double start = 0.0) : now_(start) {}

    ClockService WallService() {
        return ClockService{[this] { return now_; }};
    }

    void Advance(double seconds) { now_ += seconds; }

private:
    double now_;
};

} // namespace

TEST_CASE("clock supplier can query clock state and rebind its source", "[fix05]") {
    ClockService clocks;
    double wall = 0;
    clocks.SetWallSource([&] { return wall + clocks.Real().value * 0; });
    clocks.Tick();
    wall = 2;
    clocks.SetMenuOpen(true);
    CHECK(clocks.Real().value == 2);
    CHECK(clocks.Active().value == 2);
    clocks.SetWallSource([&] {
        clocks.SetWallSource([] { return 100.0; });
        return 50.0;
    });
    CHECK(clocks.Tick().real.value == 2);
    CHECK(clocks.Tick().real.value == 2);
    int released = 0;
    auto capture = std::shared_ptr<int>(new int(0), [&](int* value) {
        CHECK(clocks.Real().value == 2);
        ++released;
        delete value;
    });
    clocks.SetWallSource([capture] { return 100.0; });
    capture.reset();
    clocks.SetWallSource([] { return 101.0; });
    CHECK(released == 1);
}

TEST_CASE("clock service anchors on the first wall sample") {
    FakeWall wall{100.0};
    auto clocks = wall.WallService();

    auto snapshot = clocks.Tick();
    CHECK(snapshot.real.value == Catch::Approx(0.0));
    CHECK(snapshot.active.value == Catch::Approx(0.0));

    wall.Advance(0.5);
    snapshot = clocks.Tick();
    CHECK(snapshot.real.value == Catch::Approx(0.5));
    CHECK(snapshot.active.value == Catch::Approx(0.5));
}

TEST_CASE("clock service accumulates active time only outside menus") {
    // MenuModeTimer vs Timer legacy split: the active clock freezes while
    // the engine adapter reports open menus; the real clock keeps running.
    FakeWall wall;
    auto clocks = wall.WallService();
    (void)clocks.Tick(); // anchor on the manual provider before measuring
    wall.Advance(2.0);
    CHECK(clocks.Tick().active.value == Catch::Approx(2.0));

    // Menu opens (pause menu): real keeps advancing, active does not.
    clocks.SetMenuOpen(true);
    REQUIRE(clocks.MenuOpen());
    wall.Advance(5.0);
    auto snapshot = clocks.Tick();
    CHECK(snapshot.real.value == Catch::Approx(7.0));
    CHECK(snapshot.active.value == Catch::Approx(2.0));

    // Repeated ticks while open must not double-count the open interval.
    wall.Advance(1.0);
    snapshot = clocks.Tick();
    CHECK(snapshot.real.value == Catch::Approx(8.0));
    CHECK(snapshot.active.value == Catch::Approx(2.0));

    // Menu closes (or load finishes): active resumes from where it froze.
    clocks.SetMenuOpen(false);
    REQUIRE_FALSE(clocks.MenuOpen());
    wall.Advance(3.0);
    snapshot = clocks.Tick();
    CHECK(snapshot.real.value == Catch::Approx(11.0));
    CHECK(snapshot.active.value == Catch::Approx(5.0));
}

TEST_CASE("clock service rejects game-clock rewinds and follows fresh pushes") {
    FakeWall wall;
    auto clocks = wall.WallService();

    clocks.RecordGameTime(GameTime::FromDays(10.0));
    CHECK(clocks.Game().AsDays() == Catch::Approx(10.0));

    // Stale pushes (out-of-order wake producers, load screen races) are
    // ignored until the session resets.
    clocks.RecordGameTime(GameTime::FromDays(9.0));
    CHECK(clocks.Game().AsDays() == Catch::Approx(10.0));
    clocks.RecordGameTime(GameTime::FromDays(10.0));
    CHECK(clocks.Game().AsDays() == Catch::Approx(10.0));
    clocks.RecordGameTime(GameTime::FromDays(11.5));
    CHECK(clocks.Game().AsDays() == Catch::Approx(11.5));

    // New game / new session: everything starts over, including wall time.
    clocks.Reset();
    CHECK(clocks.Game().AsDays() == Catch::Approx(0.0));
    wall.Advance(4.0);
    const auto snapshot = clocks.Tick();
    CHECK(snapshot.game.AsDays() == Catch::Approx(0.0));
    CHECK(snapshot.real.value == Catch::Approx(0.0)); // re-anchors after reset
    wall.Advance(2.0);
    CHECK(clocks.Tick().real.value == Catch::Approx(2.0));
}

TEST_CASE("clock service menu state survives ticks without wall samples") {
    FakeWall wall;
    auto clocks = wall.WallService();

    clocks.Tick(); // anchor
    clocks.SetMenuOpen(true);
    const auto snapshot = clocks.Tick(); // no elapsed wall time
    CHECK(snapshot.real.value == Catch::Approx(0.0));
    CHECK(snapshot.active.value == Catch::Approx(0.0));
    CHECK(clocks.MenuOpen());
}

TEST_CASE("clock service attributes the interval before a menu transition") {
    // Wall time between two ticks belongs to the state active *then*, not
    // the state at the next Tick.  Transitions settle the interval immediately.
    FakeWall wall;
    auto clocks = wall.WallService();
    (void)clocks.Tick(); // anchor at t=0

    wall.Advance(4.0);          // 4 s outside the menu, no Tick in between
    clocks.SetMenuOpen(true);   // the transition must credit those 4 s
    CHECK(clocks.Real().value == Catch::Approx(4.0));
    CHECK(clocks.Active().value == Catch::Approx(4.0));

    wall.Advance(3.0);          // menu open: real keeps counting
    clocks.SetMenuOpen(false);
    CHECK(clocks.Real().value == Catch::Approx(7.0));
    CHECK(clocks.Active().value == Catch::Approx(4.0));
}

TEST_CASE("clock service drops non-finite wall samples and re-anchors") {
    double wall = 0.0;
    bool poison = false;
    ClockService clocks{[&wall, &poison] {
        return poison ? std::numeric_limits<double>::infinity() : wall;
    }};

    (void)clocks.Tick(); // anchor at 0
    wall = 2.0;
    CHECK(clocks.Tick().real.value == Catch::Approx(2.0));

    poison = true;
    CHECK(clocks.Tick().real.value == Catch::Approx(2.0)); // interval dropped

    poison = false;
    wall = 5.0;
    CHECK(clocks.Tick().real.value == Catch::Approx(2.0)); // re-anchor, no delta
    wall = 7.0;
    CHECK(clocks.Tick().real.value == Catch::Approx(4.0));
}
