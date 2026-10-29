#include "slsf/core/persistence/StateCodec.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <span>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    const auto bytes = std::span<const std::byte>(reinterpret_cast<const std::byte*>(data), size);
    const auto decoded = slsf::DecodeState(bytes);
    if (!decoded) {
        return 0;
    }

    const auto encoded = slsf::EncodeState(*decoded);
    if (!encoded) {
        std::abort();
    }
    if (!std::ranges::equal(*encoded, bytes)) {
        std::abort();
    }
    const auto roundtrip = slsf::DecodeState(*encoded);
    if (!roundtrip || *roundtrip != *decoded) {
        std::abort();
    }
    return 0;
}
