#include "slsf/runtime/Dispatcher.h"
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <vector>

using slsf::runtime::ClockDomain;
using slsf::runtime::Dispatcher;

struct Observation {
    std::size_t id;
    bool elapsed;
};

Dispatcher::Flow WaitAndRecord(Dispatcher& dispatcher, std::size_t id,
                               std::vector<Observation>& observations,
                               double seconds) {
    Dispatcher::Delay wait{dispatcher, seconds, ClockDomain::Real};
    const bool elapsed = co_await wait;
    observations.push_back(Observation{id, elapsed});
}

std::uint64_t Checksum(const std::vector<Observation>& observations) {
    std::uint64_t hash = 14695981039346656037ull;
    for (const auto& observation : observations) {
        hash = (hash ^ static_cast<std::uint64_t>(observation.id)) * 1099511628211ull;
        hash = (hash ^ static_cast<std::uint64_t>(observation.elapsed)) * 1099511628211ull;
    }
    return hash;
}

bool InExpectedOrder(const std::vector<Observation>& observations,
                     std::size_t count, bool expectedElapsed) {
    if (observations.size() != count) return false;
    for (std::size_t i = 0; i < count; ++i) {
        if (observations[i].id != i || observations[i].elapsed != expectedElapsed) return false;
    }
    return true;
}

void RunExpiry(std::size_t count) {
    Dispatcher dispatcher;
    double wall = 500.0;
    dispatcher.Clocks().SetWallSource([&wall] { return wall; });
    if (!dispatcher.Attach(slsf::Epoch{1, 0, 0}, slsf::runtime::RuntimeBindings{}, slsf::State{})) {
        std::cout << "expiry attach_failed n=" << count << '\n';
        return;
    }
    std::vector<Observation> observations;
    observations.reserve(count);
    std::vector<Dispatcher::Flow> flows;
    flows.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        flows.push_back(dispatcher.SpawnFlow([&dispatcher, &observations, i]() -> Dispatcher::Flow {
            return WaitAndRecord(dispatcher, i, observations, 1.0);
        }));
    }
    wall += 1.0;
    const auto start = std::chrono::steady_clock::now();
    dispatcher.Step();
    const auto stop = std::chrono::steady_clock::now();
    const auto elapsedUs = std::chrono::duration_cast<std::chrono::microseconds>(stop - start).count();
    std::cout << "expiry n=" << count
              << " completed=" << observations.size()
              << " order_ok=" << InExpectedOrder(observations, count, true)
              << " checksum=0x" << std::hex << Checksum(observations) << std::dec
              << " elapsed_us=" << elapsedUs << '\n';
}

void RunCancel(std::size_t count) {
    Dispatcher dispatcher;
    double wall = 900.0;
    dispatcher.Clocks().SetWallSource([&wall] { return wall; });
    if (!dispatcher.Attach(slsf::Epoch{2, 0, 0}, slsf::runtime::RuntimeBindings{}, slsf::State{})) {
        std::cout << "cancel attach_failed n=" << count << '\n';
        return;
    }
    std::vector<Observation> observations;
    observations.reserve(count);
    std::vector<Dispatcher::Flow> flows;
    flows.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        flows.push_back(dispatcher.SpawnFlow([&dispatcher, &observations, i]() -> Dispatcher::Flow {
            return WaitAndRecord(dispatcher, i, observations, 1000000.0);
        }));
    }
    const auto start = std::chrono::steady_clock::now();
    bool allCancelled = true;
    for (auto& flow : flows) allCancelled = flow.CancelWait() && allCancelled;
    const auto stop = std::chrono::steady_clock::now();
    const auto elapsedUs = std::chrono::duration_cast<std::chrono::microseconds>(stop - start).count();
    std::cout << "cancel n=" << count
              << " cancelled=" << allCancelled
              << " completed=" << observations.size()
              << " order_ok=" << InExpectedOrder(observations, count, false)
              << " checksum=0x" << std::hex << Checksum(observations) << std::dec
              << " elapsed_us=" << elapsedUs << '\n';
}

int main() {
    for (int trial = 0; trial < 5; ++trial) {
        for (const std::size_t count : {100u, 1000u, 5000u}) {
            RunExpiry(count);
            RunCancel(count);
        }
    }
}
