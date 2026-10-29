#include "slsf/core/location/LocationIndex.h"
#include "slsf/core/reputation/ReputationState.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iomanip>
#include <immintrin.h>
#include <intrin.h>
#include <iostream>
#include <numeric>
#include <queue>
#include <random>
#include <string>
#include <vector>

// Standalone research probe: no plugin code or production algorithms are changed.
namespace {
volatile double sink = 0;

template<class F> double Median(F&& fn, int rounds = 7) {
    fn();
    std::vector<double> samples;
    for (int i = 0; i < rounds; ++i) {
        const auto start = std::chrono::steady_clock::now();
        const double result = fn();
        const auto stop = std::chrono::steady_clock::now();
        sink = result;
        samples.push_back(std::chrono::duration<double, std::micro>(stop - start).count());
    }
    std::sort(samples.begin(), samples.end());
    return samples[samples.size() / 2];
}

void Row(const char* name, std::size_t size, double baseline, double candidate, const char* unit) {
    std::cout << name << ',' << size << ',' << baseline << ',' << candidate << ','
              << baseline / candidate << ',' << unit << '\n';
}

__declspec(noinline) std::optional<slsf::LocationId> BinaryLocation(
    const slsf::LocationIndexSnapshot& index, const slsf::StableFormKey& key) {
    const auto it = std::lower_bound(index.records.begin(), index.records.end(), key,
        [](const slsf::LocationRecord& record, const slsf::StableFormKey& value) {
            return record.formId < value;
        });
    if (it == index.records.end() || it->formId != key) return std::nullopt;
    return slsf::LocationId(static_cast<std::uint32_t>(it - index.records.begin()));
}

__declspec(noinline) std::optional<slsf::LocationId> LinearLocation(
    const slsf::LocationIndexSnapshot& index, const slsf::StableFormKey& key) {
    for (std::size_t i = 0; i < index.records.size(); ++i) {
        if (index.records[i].formId == key) {
            return slsf::LocationId(static_cast<std::uint32_t>(i));
        }
    }
    return std::nullopt;
}

void Locations() {
    std::mt19937 rng(1170);
    for (const std::size_t n : {256u, 4096u}) {
        slsf::LocationIndexSnapshot index;
        for (std::size_t i = 0; i < n; ++i) {
            slsf::LocationRecord record;
            record.formId = {"locations.esp", static_cast<std::uint32_t>(i * 2)};
            index.records.push_back(std::move(record));
        }
        std::vector<slsf::StableFormKey> queries;
        for (int i = 0; i < 4096; ++i) {
            const auto key = slsf::StableFormKey{"locations.esp", rng() % static_cast<unsigned>(n * 2)};
            queries.push_back(key);
            if (slsf::FindLocation(index, key) != BinaryLocation(index, key)) std::abort();
            if (slsf::FindLocation(index, key) != LinearLocation(index, key)) std::abort();
        }
        const auto linear = Median([&] {
            double total = 0;
            for (const auto& key : queries) total += LinearLocation(index, key).value_or(slsf::LocationId(0)).value;
            return total;
        });
        const auto binary = Median([&] {
            double total = 0;
            for (const auto& key : queries) total += slsf::FindLocation(index, key).value_or(slsf::LocationId(0)).value;
            return total;
        });
        Row("location_lookup_linear_vs_binary", n, linear * 1000 / queries.size(),
            binary * 1000 / queries.size(), "ns/query");
    }
}

__declspec(noinline) void WalkOwners(const std::vector<std::uint32_t>& parent,
    const std::vector<std::uint32_t>& candidate, std::vector<std::uint32_t>& out) {
    for (std::uint32_t i = 0; i < parent.size(); ++i) {
        auto best = candidate[i];
        auto p = parent[i];
        while (p != UINT32_MAX) { best = std::min(best, candidate[p]); p = parent[p]; }
        out[i] = best;
    }
}

__declspec(noinline) void PrefixOwners(const std::vector<std::uint32_t>& parent,
    const std::vector<std::uint32_t>& candidate, std::vector<std::uint32_t>& out) {
    // Synthetic inputs are in parent-before-child order; production uses DFS preorder.
    for (std::uint32_t i = 0; i < parent.size(); ++i)
        out[i] = parent[i] == UINT32_MAX ? candidate[i] : std::min(candidate[i], out[parent[i]]);
}

void Owners() {
    constexpr std::size_t n = 4096;
    for (const bool chain : {false, true}) {
        std::vector<std::uint32_t> parent(n), candidate(n, UINT32_MAX), a(n), b(n);
        for (std::uint32_t i = 0; i < n; ++i) {
            parent[i] = i == 0 ? UINT32_MAX : (chain ? i - 1 : (i - 1) / 4);
            if (i % 31 == 0) candidate[i] = 1000 - i % 997;
        }
        WalkOwners(parent, candidate, a);
        PrefixOwners(parent, candidate, b);
        if (a != b) std::abort();
        const int repeats = chain ? 4 : 200;
        auto run = [&](auto fn, auto& out) {
            return Median([&] {
                double total = 0;
                for (int r = 0; r < repeats; ++r) { fn(parent, candidate, out); total += out[r % n]; }
                return total;
            }) / repeats;
        };
        const auto walk = run(WalkOwners, a);
        const auto prefix = run(PrefixOwners, b);
        Row(chain ? "owner_kernel_chain" : "owner_kernel_4ary", n, walk, prefix, "us/bind-kernel");
    }
}

struct Deadline {
    std::uint64_t due;
    std::uint64_t period;
    std::uint32_t id;
    friend bool operator>(const Deadline& a, const Deadline& b) {
        return a.due > b.due || (a.due == b.due && a.id > b.id);
    }
};

__declspec(noinline) double ScanDeadlines(const std::vector<Deadline>& input) {
    auto tasks = input;
    double checksum = 0;
    for (std::uint64_t now = 1; now <= 10000; ++now) {
        for (auto& task : tasks) if (task.due <= now) {
            checksum += task.id + 1;
            task.due += task.period;
        }
    }
    return checksum;
}

__declspec(noinline) double HeapDeadlines(const std::vector<Deadline>& input) {
    std::priority_queue<Deadline, std::vector<Deadline>, std::greater<>> tasks(
        std::greater<>{}, input);
    double checksum = 0;
    for (std::uint64_t now = 1; now <= 10000; ++now) {
        while (!tasks.empty() && tasks.top().due <= now) {
            auto task = tasks.top(); tasks.pop();
            checksum += task.id + 1;
            task.due += task.period;
            tasks.push(task);
        }
    }
    return checksum;
}

void Deadlines() {
    for (const std::size_t n : {32u, 1024u}) {
        std::vector<Deadline> input;
        for (std::uint32_t i = 0; i < n; ++i) {
            const std::uint64_t period = 17 + (i * 997) % 30000;
            input.push_back({period, period, i});
        }
        if (ScanDeadlines(input) != HeapDeadlines(input)) std::abort();
        Row("deadline_scan_vs_heap", n,
            Median([&] { return ScanDeadlines(input); }),
            Median([&] { return HeapDeadlines(input); }), "us/10000-wakes");
    }
}

__declspec(noinline) slsf::PublicView PublicOnly(const slsf::PublishedState& state) {
    slsf::PublicView out;
    out.revision = state.revision;
    out.epoch = state.epoch;
    out.fame = state.fame;
    out.flags = state.flags;
    return out;
}

void Publish() {
    for (const std::size_t n : {21u, 512u, 4096u}) {
        slsf::State state;
        state.accounts.resize(n);
        state.accountKeys.resize(n);
        state.locationAccounts.resize(n);
        for (std::size_t i = 0; i < n; ++i) {
            state.accountKeys[i] = slsf::StableRegionKey{"probe.esp",
                                                        static_cast<std::uint32_t>(i)};
            state.locationAccounts[i] = slsf::RegionId(static_cast<std::uint32_t>(i));
            for (std::size_t c = 0; c < 32; ++c) {
                state.accounts[i].local[c] = 70;
                state.accounts[i].remote[c] = 80;
                state.accounts[i].flags[c] = 1;
            }
            slsf::RecomputeAccount(state.accounts[i]);
        }
        const auto published = slsf::MakePublishedState(state);
        const auto smallCheck = PublicOnly(published);
        for (std::size_t i = 0; i < n; ++i) {
            for (std::size_t c = 0; c < slsf::kFameCategoryCount; ++c) {
                if (published.fame[i * 32 + c] != 80 || smallCheck.fame[i * 32 + c] != 80) {
                    std::cerr << "publish mismatch n=" << n << " i=" << i
                              << " c=" << c << " value=" << published.fame[i * 32 + c] << "\n";
                    std::abort();
                }
            }
        }
        const auto captured = slsf::CaptureState(state);
        if (!captured) {
            std::cerr << "capture failed: " << captured.error().message << "\n";
            std::abort();
        }
        if (captured->locations.size() != n) {
            std::cerr << "capture size " << captured->locations.size() << " n=" << n << "\n";
            std::abort();
        }
        const auto slots = n * 32;
        constexpr int repeats = 200;
        const auto full = Median([&] {
            double total = 0;
            for (int r = 0; r < repeats; ++r) {
                const auto out = slsf::CaptureState(state);
                total += out->locations[r % n].local[r % 32];
            }
            return total;
        }) / repeats;
        const auto small = Median([&] {
            double total = 0;
            for (int r = 0; r < repeats; ++r) {
                const auto out = slsf::MakePublishedState(state);
                total += out.fame[r % slots];
            }
            return total;
        }) / repeats;
        Row("publish_capture_vs_narrow", n, full, small, "us/publish");
    }
}

struct Edge { std::uint32_t source; double weight; };
struct Graph { std::vector<std::uint32_t> offsets; std::vector<Edge> edges; };

__declspec(noinline) void Scalar(const Graph& g, const std::vector<double>& l,
    const std::vector<double>& r, std::vector<double>& out) {
    for (std::size_t t = 0; t + 1 < g.offsets.size(); ++t) {
        std::fill_n(out.data() + t * 32, 32, 0.0);
        for (auto e = g.offsets[t]; e < g.offsets[t + 1]; ++e) {
            const auto edge = g.edges[e];
            #pragma loop(no_vector)
            for (std::size_t c = 0; c < 25; ++c) {
                const auto candidate = std::max(l[edge.source * 32 + c], r[edge.source * 32 + c]) * edge.weight;
                if (candidate >= 5.0) out[t * 32 + c] = std::max(out[t * 32 + c], candidate);
            }
        }
    }
}

__declspec(noinline) void AutoVector(const Graph& g, const std::vector<double>& l,
    const std::vector<double>& r, std::vector<double>& out) {
    for (std::size_t t = 0; t + 1 < g.offsets.size(); ++t) {
        std::fill_n(out.data() + t * 32, 32, 0.0);
        for (auto e = g.offsets[t]; e < g.offsets[t + 1]; ++e) {
            const auto edge = g.edges[e];
            for (std::size_t c = 0; c < 25; ++c) {
                const auto candidate = std::max(l[edge.source * 32 + c], r[edge.source * 32 + c]) * edge.weight;
                if (candidate >= 5.0) out[t * 32 + c] = std::max(out[t * 32 + c], candidate);
            }
        }
    }
}

template<bool Precomputed>
__declspec(noinline) void Vector(const Graph& g, const std::vector<double>& l,
    const std::vector<double>& r, std::vector<double>& out, std::vector<double>& f) {
    if constexpr (Precomputed) {
        for (std::size_t i = 0; i < l.size(); i += 4)
            _mm256_storeu_pd(f.data() + i, _mm256_max_pd(_mm256_loadu_pd(l.data() + i), _mm256_loadu_pd(r.data() + i)));
    }
    const auto cutoff = _mm256_set1_pd(5.0);
    const auto tail = _mm256_castsi256_pd(_mm256_set_epi64x(0, 0, 0, -1LL));
    for (std::size_t t = 0; t + 1 < g.offsets.size(); ++t) {
        __m256d acc[8];
        for (auto& value : acc) value = _mm256_setzero_pd();
        for (auto e = g.offsets[t]; e < g.offsets[t + 1]; ++e) {
            const auto edge = g.edges[e];
            const auto weight = _mm256_set1_pd(edge.weight);
            for (int block = 0; block < 8; ++block) {
                const auto offset = edge.source * 32 + block * 4;
                const auto source = Precomputed ? _mm256_loadu_pd(f.data() + offset) :
                    _mm256_max_pd(_mm256_loadu_pd(l.data() + offset), _mm256_loadu_pd(r.data() + offset));
                auto value = _mm256_mul_pd(source, weight);
                value = _mm256_and_pd(value, _mm256_cmp_pd(value, cutoff, _CMP_GE_OQ));
                if (block == 6) value = _mm256_and_pd(value, tail);
                if (block == 7) value = _mm256_setzero_pd();
                acc[block] = _mm256_max_pd(acc[block], value);
            }
        }
        for (int block = 0; block < 8; ++block) _mm256_storeu_pd(out.data() + t * 32 + block * 4, acc[block]);
    }
}

void Diffusion() {
    std::mt19937 rng(1170);
    for (const std::size_t n : {21u, 128u, 1024u}) {
        Graph g;
        for (std::size_t t = 0; t < n; ++t) {
            g.offsets.push_back(static_cast<std::uint32_t>(g.edges.size()));
            for (int k = 0; k < 4; ++k) {
                auto s = (t + 1 + rng() % (n - 1)) % n;
                g.edges.push_back({static_cast<std::uint32_t>(s), 0.2 + 0.1 * k});
            }
        }
        g.offsets.push_back(static_cast<std::uint32_t>(g.edges.size()));
        std::vector<double> l(n * 32), r(n * 32), a(n * 32), b(n * 32), f(n * 32);
        for (std::size_t i = 0; i < l.size(); ++i) { l[i] = rng() % 151; r[i] = rng() % 151; }
        Scalar(g, l, r, a);
        AutoVector(g, l, r, b);
        if (a != b) std::abort();
        Vector<false>(g, l, r, b, f);
        if (a != b) std::abort();
        Vector<true>(g, l, r, b, f);
        if (a != b) std::abort();
        const int repeats = static_cast<int>(100000 / n);
        auto measure = [&](auto fn) {
            return Median([&] {
                double total = 0;
                for (int k = 0; k < repeats; ++k) { fn(); total += b[k % b.size()]; }
                return total;
            }) / repeats;
        };
        const auto scalar = measure([&] { Scalar(g, l, r, b); });
        const auto automatic = measure([&] { AutoVector(g, l, r, b); });
        const auto vector = measure([&] { Vector<false>(g, l, r, b, f); });
        const auto precomputed = measure([&] { Vector<true>(g, l, r, b, f); });
        Row("diffusion_scalar_vs_avx2", n, scalar, vector, "us/pass");
        Row("diffusion_plain_cpp_vs_avx2", n, automatic, vector, "us/pass");
        Row("diffusion_avx2_vs_f_precompute", n, vector, precomputed, "us/pass");
    }
}
}

int main() {
    std::cout << std::fixed << std::setprecision(3);
    int regs[4];
    char brand[49]{};
    for (int i = 0; i < 3; ++i) { __cpuid(regs, 0x80000002 + i); std::copy_n(reinterpret_cast<char*>(regs), 16, brand + i * 16); }
    std::cout << "cpu," << brand << "\ncompiler,MSVC " << _MSC_FULL_VER << "\nseed,1170\nrounds,7\n";
    std::cout << "case,size,baseline,candidate,speedup,unit\n";
    std::cerr << "stage Locations\n"; Locations();
    std::cerr << "stage Owners\n"; Owners();
    std::cerr << "stage Deadlines\n"; Deadlines();
    std::cerr << "stage Publish\n"; Publish();
    std::cerr << "stage Diffusion\n"; Diffusion();
    std::cerr << "stage done\n";
    std::cout << "checks,all_equal\n";
}
