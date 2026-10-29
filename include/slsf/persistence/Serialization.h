#pragma once

#include "slsf/core/persistence/PersistentState.h"
#include "slsf/core/Types.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace slsf::persistence {

// Native save record identity.  SKSE stores the 4-character type and version
// outside the payload; the envelope repeats them so an offline parser can
// reject a foreign/truncated record without knowing the SKSE file format.
inline constexpr std::uint32_t kSaveRecordType = 0x534C5356u; // 'SLSV'
inline constexpr std::uint16_t kSaveRecordVersion = 1;
inline constexpr std::uint32_t kSaveEnvelopeMagic = 0x534C5332u; // 'SLS2'
inline constexpr std::uint16_t kSaveEnvelopeVersion = 1;
inline constexpr std::size_t kSaveEnvelopeHeaderBytes = 20;
inline constexpr std::size_t kSaveRecordMaxBytes = 64u * 1024u * 1024u;

struct SaveRecordInfo {
    std::uint32_t recordType{};
    std::uint32_t recordVersion{};
    std::uint32_t codecVersion{};
    std::uint32_t payloadBytes{};
    std::uint32_t checksum{};
};

// Frames the versioned StateCodec payload with the save record identity and a
// checksum.  Encoding is performed after CaptureState has returned, outside
// the dispatcher lock.
Result<std::vector<std::byte>> EncodeSaveRecord(const PersistentState& state);

// Validates the envelope, size, checksum, record type/version and then the
// StateCodec payload.  Every failure is returned; no partial state escapes.
Result<PersistentState> DecodeSaveRecord(std::span<const std::byte> bytes);

// Header-only inspection used for diagnostics and version gates.  It does not
// validate the payload checksum beyond the header's declared length.
Result<SaveRecordInfo> InspectSaveRecord(std::span<const std::byte> bytes);

} // namespace slsf::persistence
