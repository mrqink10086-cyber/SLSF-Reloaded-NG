#pragma once

#include <atomic>
#include <cstdint>

namespace slsf::runtime {

class SessionGate {
public:
    std::uint64_t Close() noexcept { return requested_.fetch_add(1) + 1; }
    std::uint64_t Generation() const noexcept { return requested_.load(); }
    void Open(std::uint64_t generation) noexcept { opened_.store(generation); }
    bool Closed() const noexcept { return opened_.load() != requested_.load(); }

private:
    std::atomic<std::uint64_t> requested_{0};
    std::atomic<std::uint64_t> opened_{0};
};

}
