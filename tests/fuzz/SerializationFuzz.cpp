#include "slsf/persistence/Serialization.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <span>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    const auto bytes = std::span<const std::byte>(reinterpret_cast<const std::byte*>(data), size);
    const auto decoded = slsf::persistence::DecodeSaveRecord(bytes);
    const auto inspected = slsf::persistence::InspectSaveRecord(bytes);
    if (!decoded) {
        // Header-only inspection is a second parser entry point and must also
        // stay total on arbitrary buffers.
        (void)inspected;
        return 0;
    }

    const auto encoded = slsf::persistence::EncodeSaveRecord(*decoded);
    if (!encoded) {
        std::abort();
    }
    const auto roundtrip = slsf::persistence::DecodeSaveRecord(*encoded);
    if (!roundtrip || *roundtrip != *decoded) {
        std::abort();
    }
    const auto roundtripInfo = slsf::persistence::InspectSaveRecord(*encoded);
    if (!roundtripInfo || roundtripInfo->payloadBytes !=
                              encoded->size() - slsf::persistence::kSaveEnvelopeHeaderBytes) {
        std::abort();
    }
    return 0;
}
