#pragma once

#include "slsf/core/Types.h"

#include <functional>
#include <memory>
#include <mutex>

namespace slsf::runtime {

struct ClockSnapshot {
    GameTime game;
    RealSeconds real;
    ActiveSeconds active;
};

class ClockService {
public:
    // Monotonic seconds supplier.  The first call anchors; calls may run on
    // the executor thread.  Defaults to std::steady_clock; tests inject a
    // fake clock.
    using WallNowFn = std::function<double()>;

    explicit ClockService(WallNowFn wallNow = {});

    // Push a fresh engine game-time reading.  Non-monotonic readings are
    // ignored (session reset clears the record via Reset()).
    void RecordGameTime(GameTime game);

    // Menu blocking for the Active domain.  The engine adapter decides the
    // menu-type set (main menu, pause menus, loading screens).  A transition
    // settles the elapsed interval under the old state immediately, so the
    // Active attribution does not depend on Tick cadence while a menu is
    // open.
    void SetMenuOpen(bool open);
    bool MenuOpen() const;

    ClockSnapshot Tick();

    // Rebind the wall source (tests and probes inject fake clocks).  The
    // next Tick re-anchors; accumulated domains are left alone so an
    // adapter can switch providers inside one session.
    void SetWallSource(WallNowFn wallNow);

    GameTime Game() const;
    RealSeconds Real() const;
    ActiveSeconds Active() const;

    // Session reset: clears accumulators, the wall anchor and the game record.
    // The engine-owned menu state is preserved (a load behind an open menu
    // keeps excluding that time).
    void Reset();

private:
    static double SteadyWallSeconds() noexcept;
    void AccumulateLocked(double sample);

    std::shared_ptr<const WallNowFn> wallNow_;
    std::uint64_t sourceGeneration_{0};
    bool wallAnchored_{false};
    double lastWall_{0.0};
    double real_{0.0};
    double active_{0.0};
    GameTime game_{};
    bool gameSet_{false};
    bool menuOpen_{false};
    mutable std::mutex mutex_;
};

} // namespace slsf::runtime
