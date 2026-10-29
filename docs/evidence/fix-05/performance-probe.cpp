#include "slsf/runtime/Dispatcher.h"
#include "slsf/core/reputation/DiffusionGraph.h"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <vector>

using namespace slsf;
using namespace slsf::runtime;
using namespace slsf::time;

template<class F> double Median(F run) {
    std::vector<double> samples;
    for (int sample = 0; sample < 7; ++sample) { samples.push_back(run()); }
    std::sort(samples.begin(), samples.end());
    return samples[3];
}
template<class F> double Time(F run) {
    const auto start = std::chrono::steady_clock::now();
    run();
    return std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - start).count();
}

int main() {
    std::uint64_t checksum = 0;
    for (const std::size_t count : {100, 1000, 5000}) {
        const double ns = Median([&] {
            Dispatcher dispatcher;
            DiffusionGraph graph;
            auto parameters = LegacyDefaultParameters();
            Configuration configuration;
            State state;
            state.accounts.resize(1);
            state.accountKeys = {{"perf.esp", 1}};
            state.locationAccounts = {RegionId{0}};
            if (!dispatcher.Attach(Epoch{1, 0, 0}, RuntimeBindings{&graph, &parameters, &configuration}, state)) { std::abort(); }
            for (std::size_t i = 0; i < count; ++i) {
                dispatcher.Post([&, i] { checksum = checksum * 31 + i; });
            }
            return Time([&] { dispatcher.Step(); });
        });
        std::cout << "step events=" << count << " median_ns=" << ns << '\n';
    }
    for (const std::size_t count : {128, 1024, 8192}) {
        Scheduler scheduler;
        for (std::size_t i = 0; i < count; ++i) { scheduler.Schedule(ClockDomain::Game, 1000, [] {}); }
        const double ns = Median([&] {
            return Time([&] {
                for (int i = 0; i < 5000; ++i) {
#ifdef FIX05_BASELINE
                    checksum += scheduler.DueCount(ClockDomain::Game, 0) != 0;
#else
                    checksum += scheduler.HasDue(ClockDomain::Game, 0);
#endif
                }
            }) / 5000;
        });
        std::cout << "due live=" << count << " median_ns=" << ns << '\n';
    }
    for (const std::size_t count : {128, 8192, 65535}) {
        PublishedState view;
        view.regionIds.resize(count);
        view.fame.resize(count * kFameSlotCount);
        view.flags.resize(count * kFameSlotCount);
        for (std::size_t i = 0; i < count; ++i) { view.regionIds[i] = RegionId{static_cast<std::uint32_t>(i)}; }
        view.locationAccounts = {RegionId{static_cast<std::uint32_t>(count - 1)}};
        const auto slot = static_cast<std::size_t>(FameCategory::Whore);
        view.fame[(count - 1) * kFameSlotCount + slot] = 42;
        const QueryRequest request{QueryRequest::Kind::Fame, LocationId{0}, FameCategory::Whore};
        const double ns = Median([&] {
            return Time([&] {
                for (int i = 0; i < 1000; ++i) {
                    auto result = Query(view, request);
                    if (!result || result->fame.front() != 42) { std::abort(); }
                    checksum += static_cast<std::uint64_t>(result->fame.front());
                }
            }) / 1000;
        });
        std::cout << "query rows=" << count << " median_ns=" << ns << '\n';
    }
    std::cout << "checksum=" << checksum << '\n';
}
