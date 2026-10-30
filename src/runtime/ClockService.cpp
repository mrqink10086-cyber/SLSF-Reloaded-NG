#include "slsf/runtime/ClockService.h"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace slsf::runtime {

double ClockService::SteadyWallSeconds() noexcept {
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration<double>(now).count();
}
ClockService::ClockService(WallNowFn wallNow)
    : wallNow_(std::make_shared<const WallNowFn>(wallNow ? std::move(wallNow)
                                  : WallNowFn{+[]() { return SteadyWallSeconds(); }})) {}

void ClockService::RecordGameTime(GameTime game) {
    if (!std::isfinite(game.AsSeconds())) {
        return; // a poisoned sample would freeze the game domain forever
    }
    const std::lock_guard<std::mutex> guard(mutex_);
    if (!gameSet_ || game.AsSeconds() >= game_.AsSeconds()) {
        game_ = game;
        gameSet_ = true;
    }
    // Rewinds belong to a new session; the adapter calls Reset() there.
}
void ClockService::SetMenuOpen(bool open) {
    std::shared_ptr<const WallNowFn> source;
    std::uint64_t generation;
    {
        const std::lock_guard<std::mutex> guard(mutex_);
        if (open == menuOpen_) {
            return;
        }
        source = wallNow_;
        generation = sourceGeneration_;
    }
    const double sample = (*source)();
    const std::lock_guard<std::mutex> guard(mutex_);
    if (generation == sourceGeneration_) {
        AccumulateLocked(sample);
    }
    menuOpen_ = open;
}
bool ClockService::MenuOpen() const {
    const std::lock_guard<std::mutex> guard(mutex_);
    return menuOpen_;
}
void ClockService::SetWallSource(WallNowFn wallNow) {
    if (!wallNow) {
        wallNow = WallNowFn{+[]() { return SteadyWallSeconds(); }};
    }
    auto replacement = std::make_shared<const WallNowFn>(std::move(wallNow));
    {
        const std::lock_guard<std::mutex> guard(mutex_);
        wallNow_.swap(replacement);
        ++sourceGeneration_;
        wallAnchored_ = false;
    }
}
ClockSnapshot ClockService::Tick() {
    std::shared_ptr<const WallNowFn> source;
    std::uint64_t generation;
    {
        const std::lock_guard<std::mutex> guard(mutex_);
        source = wallNow_;
        generation = sourceGeneration_;
    }
    const double sample = (*source)();
    const std::lock_guard<std::mutex> guard(mutex_);
    if (generation == sourceGeneration_) {
        AccumulateLocked(sample);
    }
    return ClockSnapshot{game_, RealSeconds{real_}, ActiveSeconds{active_}};
}
void ClockService::AccumulateLocked(double sample) {
    if (!std::isfinite(sample)) {
        // A non-finite wall sample would poison both accumulators; drop the
        // interval and re-anchor at the next finite reading.
        wallAnchored_ = false;
    } else {
        const bool anchored = wallAnchored_;
        if (wallAnchored_) {
            const double delta = std::max(0.0, sample - lastWall_);
            real_ += delta;
            if (!menuOpen_) {
                active_ += delta;
            }
        }
        wallAnchored_ = true;
        lastWall_ = anchored ? std::max(lastWall_, sample) : sample;
    }
}
GameTime ClockService::Game() const {
    const std::lock_guard<std::mutex> guard(mutex_);
    return game_;
}
RealSeconds ClockService::Real() const {
    const std::lock_guard<std::mutex> guard(mutex_);
    return RealSeconds{real_};
}
ActiveSeconds ClockService::Active() const {
    const std::lock_guard<std::mutex> guard(mutex_);
    return ActiveSeconds{active_};
}
void ClockService::Reset() {
    const std::lock_guard<std::mutex> guard(mutex_);
    wallAnchored_ = false;
    lastWall_ = 0.0;
    real_ = 0.0;
    active_ = 0.0;
    game_ = GameTime{};
    gameSet_ = false;
    ++sourceGeneration_;
}
}