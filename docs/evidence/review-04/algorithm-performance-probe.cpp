#include "slsf/core/gameplay/Witness.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <new>

namespace {
std::uint64_t allocations = 0;

class FixedRandom final : public slsf::IRandom {
public:
    std::int32_t UniformInt(std::int32_t lo, std::int32_t hi) override {
        return hi;
    }
};

__attribute__((noinline)) bool HotTest(const slsf::CategoryMask& mask,
                                       slsf::FameCategory category) {
    return mask.Test(category);
}
} // namespace

void* operator new(std::size_t size) {
    ++allocations;
    if (void* memory = std::malloc(size)) {
        return memory;
    }
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) {
    ++allocations;
    if (void* memory = std::malloc(size)) {
        return memory;
    }
    throw std::bad_alloc();
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

int main() {
    using namespace slsf;
    using namespace slsf::gameplay;
    constexpr std::size_t batchSize = 10000;

    FameGainRollFacts facts;
    facts.status = Status::Ready;
    facts.validLocation = Fact::Known(true);
    facts.anonymous = Fact::Known(false);
    facts.eligible.fill(Fact::Known(true));
    facts.currentFame.fill(FameValue::Known(0));

    FixedRandom rng;
    FameGainRoller roller(rng);
    std::size_t selectedTotal = 0;
    const std::uint64_t before = allocations;
    const auto start = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < batchSize; ++i) {
        const FameGainRollResult result = roller.Run(facts);
        selectedTotal += result.selection.selectedMask.Count();
    }
    const auto stop = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                             stop - start)
                             .count();
    const std::uint64_t usedAllocations = allocations - before;
    CategoryMask hotMask;
    for (const FameCategory category : kEspFameCategoryOrder) {
        hotMask.Set(category);
    }
    constexpr std::size_t lookupPasses = 1000000;
    std::size_t hitCount = 0;
    const auto lookupStart = std::chrono::steady_clock::now();
    for (std::size_t pass = 0; pass < lookupPasses; ++pass) {
        for (std::size_t index = 0; index < kFameCategoryCount; ++index) {
            hitCount += HotTest(hotMask, FameCategoryFromIndex(index));
        }
    }
    const auto lookupStop = std::chrono::steady_clock::now();
    const auto lookupElapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                   lookupStop - lookupStart)
                                   .count();
    std::cout << "workload=10000 FameGainRoller calls, 25 eligible categories, wanted=count"
              << '\n';
    std::cout << "selected_total=" << selectedTotal << '\n';
    std::cout << "allocation_total=" << usedAllocations << '\n';
    std::cout << "allocations_per_call="
              << static_cast<double>(usedAllocations) / batchSize << '\n';
    std::cout << "elapsed_us=" << elapsed << '\n';
    std::cout << "events_per_second="
              << (elapsed > 0 ? batchSize * 1000000.0 / elapsed : 0.0) << '\n';
    std::cout << "category_mask_low25=0x" << std::hex
              << CategoryMask::kValidBits << std::dec << '\n';
    std::cout << "hot_lookup_workload=1000000 full-mask scans x 25 sequential categories"
              << '\n';
    std::cout << "hot_lookup_hits=" << hitCount << '\n';
    std::cout << "hot_lookup_elapsed_ns=" << lookupElapsed << '\n';
    std::cout << "hot_category_tests_per_second="
              << (lookupElapsed > 0
                      ? lookupPasses * kFameCategoryCount * 1000000000.0 /
                            lookupElapsed
                      : 0.0)
              << '\n';
    return selectedTotal == batchSize * kFameCategoryCount &&
                   hitCount == lookupPasses * kFameCategoryCount
               ? 0
               : 1;
}
