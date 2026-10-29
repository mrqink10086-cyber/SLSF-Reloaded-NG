#include "slsf/core/reputation/DiffusionGraph.h"
#include "slsf/interfaces/WakeSource.h"
#include "slsf/runtime/Dispatcher.h"

#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>

namespace {
using namespace slsf;
using namespace slsf::runtime;
using slsf::time::ClockDomain;

void Require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

class MainQueue {
public:
    void AddTask(std::function<void()> task) { tasks_.push_back(std::move(task)); }
    void Pump() {
        while (!tasks_.empty()) {
            auto task = std::move(tasks_.front());
            tasks_.pop_front();
            task();
        }
    }
private:
    std::deque<std::function<void()>> tasks_;
};

class OfflineWake final : public IWakeSource {
public:
    explicit OfflineWake(MainQueue& queue) : queue_(queue) {}
    bool Start(WakeCallback callback) override {
        callback_ = std::make_shared<WakeCallback>(std::move(callback));
        return true;
    }
    void Stop() override { callback_.reset(); }
    void Emit(GameTime game) {
        queue_.AddTask([weak = std::weak_ptr<WakeCallback>(callback_), game] {
            if (auto callback = weak.lock()) { (*callback)(game); }
        });
    }
private:
    MainQueue& queue_;
    std::shared_ptr<WakeCallback> callback_;
};

State InitialState() {
    State state;
    state.accounts.resize(1);
    state.accountKeys = {{"offline.esp", 1}};
    state.locationAccounts = {RegionId{0}};
    return state;
}
}

int main() {
    std::mutex monitorMutex;
    std::condition_variable monitor;
    bool finished = false;
    std::thread watchdog([&] {
        std::unique_lock lock(monitorMutex);
        if (!monitor.wait_for(lock, std::chrono::seconds(10), [&] { return finished; })) {
            std::cerr << "FAIL: owner thread did not return within 10 seconds\n";
            std::abort();
        }
    });

    MainQueue queue;
    OfflineWake wake(queue);
    Dispatcher dispatcher;
    DiffusionGraph graph;
    auto parameters = LegacyDefaultParameters();
    Configuration configuration;
    RuntimeBindings bindings{&graph, &parameters, &configuration};
    Require(dispatcher.Attach(Epoch{1, 0, 0}, bindings, InitialState()), "attach");
    double wall = 0;
    dispatcher.Clocks().SetWallSource([&] { return wall; });
    const auto owner = std::this_thread::get_id();
    int wakeCalls = 0;
    Require(wake.Start([&](GameTime game) {
        Require(std::this_thread::get_id() == owner, "wake owner thread");
        ++wakeCalls;
        dispatcher.WakeTick(game);
    }), "wake start");

    int completions = 0;
    auto wait = [&](double seconds, ClockDomain domain) -> Dispatcher::Flow {
        if (co_await Dispatcher::Delay{dispatcher, seconds, domain}) { ++completions; }
    };
    auto longWait = dispatcher.SpawnFlow([&] { return wait(1000000, ClockDomain::Real); });
    auto realWait = dispatcher.SpawnFlow([&] { return wait(10, ClockDomain::Real); });
    auto activeWait = dispatcher.SpawnFlow([&] { return wait(10, ClockDomain::Active); });
    auto gameWait = dispatcher.SpawnFlow([&] { return wait(10, ClockDomain::Game); });
    int heartbeats = 0;
    double gameSeconds = 0;
    double maxPumpMicros = 0;
    constexpr int frames = 3600;
    for (int frame = 1; frame <= frames; ++frame) {
        wall = frame / 60.0;
        const bool menu = frame <= 1200;
        dispatcher.Clocks().SetMenuOpen(menu);
        if (!menu) { gameSeconds += 1.0 / 60; }
        wake.Emit(GameTime::FromSeconds(gameSeconds));
        queue.AddTask([&] {
            Require(std::this_thread::get_id() == owner, "heartbeat owner thread");
            ++heartbeats;
        });
        const auto before = std::chrono::steady_clock::now();
        queue.Pump();
        maxPumpMicros = std::max(maxPumpMicros, std::chrono::duration<double, std::micro>(
            std::chrono::steady_clock::now() - before).count());
        if (frame == 900) {
            Require(realWait.Done(), "Real runs through menu");
            Require(!activeWait.Done() && !gameWait.Done(), "menu parks Active and Game");
        }
    }
    Require(heartbeats == frames && wakeCalls == frames, "every heartbeat runs");
    Require(completions == 3 && !longWait.Done(), "long wait stays parked");
    Require(longWait.CancelWait() && longWait.Done(), "cancel resumes abort once");
    auto replacedWait = dispatcher.SpawnFlow([&] { return wait(1000000, ClockDomain::Real); });
    int stale = 0;
    dispatcher.Schedule(0, ClockDomain::Real, [&] {
        dispatcher.Detach();
        Require(dispatcher.Attach(Epoch{2, 0, 0}, bindings, InitialState()), "reattach");
    });
    dispatcher.Schedule(0, ClockDomain::Real, [&] { ++stale; });
    wake.Emit(GameTime::FromSeconds(gameSeconds));
    queue.Pump();
    Require(replacedWait.Done() && stale == 0 && completions == 3, "session invalidation");
    const auto beforeStop = wakeCalls;
    wake.Emit(GameTime{});
    wake.Stop();
    queue.Pump();
    Require(wakeCalls == beforeStop, "Stop drops queued wake");

    std::cout << "heartbeat=" << heartbeats << " frames=" << frames
              << " simulated_seconds=" << wall << " max_pump_us=" << maxPumpMicros
              << " long_wait_seconds=1000000 completions=" << completions
              << " stale_callbacks=" << stale << "\nOFFLINE_WAKE_PROBE=PASS\n";
    {
        std::lock_guard lock(monitorMutex);
        finished = true;
    }
    monitor.notify_one();
    watchdog.join();
}
