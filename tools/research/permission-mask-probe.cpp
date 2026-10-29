#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <immintrin.h>
#include <iostream>
#include <numeric>
#include <random>
#include <vector>

// Synthetic category-set operations only; this does not execute gameplay or RNG rules.
namespace {
constexpr std::uint32_t valid = (std::uint32_t{1} << 25) - 1;
using Bytes = std::array<std::uint8_t, 32>;
using Values = std::array<double, 32>;
struct ByteInput { Bytes allow{}, enabled{}, observed{}, ready{}; };
struct MaskInput { std::uint32_t allow{}, enabled{}, observed{}, ready{}; };
volatile double sink = 0;

template<class F> double Median(F fn) {
    fn();
    std::array<double, 7> times{};
    for (auto& t : times) {
        const auto start = std::chrono::steady_clock::now();
        sink = fn();
        t = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start).count();
    }
    std::sort(times.begin(), times.end());
    return times[3];
}

__declspec(noinline) std::uint64_t CombineBytes(const std::vector<ByteInput>& in) {
    std::uint64_t sum = 0;
    for (const auto& r : in) {
        std::uint32_t m = 0;
        for (unsigned c = 0; c < 25; ++c)
            if (r.allow[c] && r.enabled[c] && r.observed[c] && r.ready[c]) m |= std::uint32_t{1} << c;
        sum += m;
    }
    return sum;
}
__declspec(noinline) std::uint64_t CombineMasks(const std::vector<MaskInput>& in) {
    std::uint64_t sum = 0;
    for (const auto& r : in) sum += r.allow & r.enabled & r.observed & r.ready & valid;
    return sum;
}

__declspec(noinline) void ByteUpdate(const std::vector<Values>& in, const std::vector<Bytes>& flags,
                                     const Values& delta, std::vector<Values>& out) {
    for (std::size_t r = 0; r < in.size(); ++r) {
        out[r] = in[r];
        for (unsigned c = 0; c < 25; ++c) if (flags[r][c]) out[r][c] = in[r][c] + delta[c];
    }
}
__declspec(noinline) void SparseUpdate(const std::vector<Values>& in, const std::vector<std::uint32_t>& flags,
                                       const Values& delta, std::vector<Values>& out) {
    for (std::size_t r = 0; r < in.size(); ++r) {
        out[r] = in[r];
        for (auto bits = flags[r] & valid; bits; bits &= bits - 1) {
            const auto c = std::countr_zero(bits);
            out[r][c] = in[r][c] + delta[c];
        }
    }
}

alignas(32) const auto laneMasks = [] {
    std::array<std::array<std::int64_t, 4>, 16> table{};
    for (unsigned n = 0; n < 16; ++n)
        for (unsigned c = 0; c < 4; ++c) table[n][c] = (n & (1u << c)) ? -1 : 0;
    return table;
}();

__declspec(noinline) void VectorUpdate(const std::vector<Values>& in, const std::vector<std::uint32_t>& flags,
                                       const Values& delta, std::vector<Values>& out) {
    for (std::size_t r = 0; r < in.size(); ++r) {
        const auto bits = flags[r] & valid;
        if (bits == 0) { out[r] = in[r]; continue; }
        for (unsigned c = 0; c < 32; c += 4) {
            const auto old = _mm256_loadu_pd(in[r].data() + c);
            const auto mask = _mm256_castsi256_pd(_mm256_load_si256(
                reinterpret_cast<const __m256i*>(laneMasks[(bits >> c) & 15].data())));
            const auto next = _mm256_add_pd(old, _mm256_loadu_pd(delta.data() + c));
            _mm256_storeu_pd(out[r].data() + c, _mm256_blendv_pd(old, next, mask));
        }
    }
}

void Probe(std::size_t rows, std::mt19937& rng) {
    const int repeat = rows == 21 ? 20000 : 1000;
    std::vector<ByteInput> byteInput(rows);
    std::vector<MaskInput> maskInput(rows);
    for (std::size_t r = 0; r < rows; ++r) {
        auto& m = maskInput[r];
        m = {rng() & valid, rng() & valid, rng() & valid, rng() & valid};
        for (unsigned c = 0; c < 25; ++c) {
            byteInput[r].allow[c] = (m.allow >> c) & 1;
            byteInput[r].enabled[c] = (m.enabled >> c) & 1;
            byteInput[r].observed[c] = (m.observed >> c) & 1;
            byteInput[r].ready[c] = (m.ready >> c) & 1;
        }
    }
    for (std::size_t r = 0; r < rows; ++r) {
        std::uint32_t expected = 0;
        const auto& b = byteInput[r];
        for (unsigned c = 0; c < 25; ++c)
            if (b.allow[c] && b.enabled[c] && b.observed[c] && b.ready[c]) expected |= 1u << c;
        const auto& m = maskInput[r];
        if (expected != (m.allow & m.enabled & m.observed & m.ready & valid)) std::abort();
    }
    if (CombineBytes(byteInput) != CombineMasks(maskInput)) std::abort();
    const auto bt = Median([&] { double sum = 0; for (int i = 0; i < repeat; ++i) sum += static_cast<double>(CombineBytes(byteInput)); return sum; });
    const auto mt = Median([&] { double sum = 0; for (int i = 0; i < repeat; ++i) sum += static_cast<double>(CombineMasks(maskInput)); return sum; });
    std::cout << "combine," << rows << ",mixed," << bt / repeat << ',' << mt / repeat << ",0,us-per-pass\n";

    Values delta{};
    for (unsigned c = 0; c < 25; ++c) delta[c] = (c + 1) * 0.125;
    std::vector<Values> input(rows), reference(rows), sparse(rows), vector(rows);
    for (auto& row : input) for (auto& v : row) v = (rng() % 1200) * 0.125;
    for (unsigned count : {0u, 2u, 12u, 25u}) {
        std::vector<Bytes> bytes(rows);
        std::vector<std::uint32_t> masks(rows);
        for (std::size_t r = 0; r < rows; ++r) {
            std::array<unsigned, 25> categories{};
            std::iota(categories.begin(), categories.end(), 0);
            std::shuffle(categories.begin(), categories.end(), rng);
            for (unsigned i = 0; i < count; ++i) { bytes[r][categories[i]] = 1; masks[r] |= 1u << categories[i]; }
        }
        ByteUpdate(input, bytes, delta, reference);
        SparseUpdate(input, masks, delta, sparse);
        VectorUpdate(input, masks, delta, vector);
        if (reference != sparse || reference != vector) std::abort();
        const auto time = [&](auto&& update, auto& out) {
            return Median([&] {
                double sum = 0;
                for (int i = 0; i < repeat; ++i) { update(); sum += out[static_cast<std::size_t>(i) % rows][i % 25]; }
                return sum;
            }) / repeat;
        };
        const auto b = time([&] { ByteUpdate(input, bytes, delta, reference); }, reference);
        const auto s = time([&] { SparseUpdate(input, masks, delta, sparse); }, sparse);
        const auto v = time([&] { VectorUpdate(input, masks, delta, vector); }, vector);
        std::cout << "update," << rows << ',' << count << ',' << b << ',' << s << ',' << v << ",us-per-pass\n";
    }
}
}

int main() {
    std::mt19937 rng(1170);
    std::cout << "compiler,MSVC," << _MSC_FULL_VER << "\nseed,1170\n";
    std::cout << "operation,rows,set_bits,byte_loop,packed_or_sparse,avx2,unit\n" << std::fixed << std::setprecision(6);
    for (auto rows : {21u, 1024u}) Probe(rows, rng);
}
