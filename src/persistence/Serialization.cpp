#include "slsf/persistence/Serialization.h"

#include "slsf/core/persistence/StateCodec.h"

#include <array>
#include <limits>

namespace slsf::persistence {
namespace {

constexpr std::uint32_t FnvOffset = 2166136261u;
constexpr std::uint32_t FnvPrime = 16777619u;

std::uint32_t Checksum(std::span<const std::byte> bytes) {
    std::uint32_t hash = FnvOffset;
    for (const std::byte value : bytes) {
        hash ^= static_cast<std::uint8_t>(value);
        hash *= FnvPrime;
    }
    return hash;
}
void PutU16(std::vector<std::byte>& out, std::uint16_t value) {
    out.push_back(static_cast<std::byte>(value & 0xFFu));
    out.push_back(static_cast<std::byte>((value >> 8u) & 0xFFu));
}
void PutU32(std::vector<std::byte>& out, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8) {
        out.push_back(static_cast<std::byte>((value >> shift) & 0xFFu));
    }
}
std::uint16_t GetU16(std::span<const std::byte> bytes, std::size_t offset) {
    return static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(bytes[offset]) |
        (static_cast<std::uint16_t>(bytes[offset + 1]) << 8u));
}
std::uint32_t GetU32(std::span<const std::byte> bytes, std::size_t offset) {
    std::uint32_t value = 0;
    for (unsigned index = 0; index < 4; ++index) {
        value |= static_cast<std::uint32_t>(bytes[offset + index]) << (index * 8u);
    }
    return value;
}
Result<std::span<const std::byte>> PayloadOf(std::span<const std::byte> bytes) {
    if (bytes.size() < kSaveEnvelopeHeaderBytes) {
        return Fail(Error::Code::InvalidArgument, "save record is shorter than its header");
    }
    if (bytes.size() > kSaveRecordMaxBytes) {
        return Fail(Error::Code::OutOfRange, "save record exceeds the configured maximum");
    }
    if (GetU32(bytes, 0) != kSaveEnvelopeMagic) {
        return Fail(Error::Code::InvalidArgument, "bad save envelope magic");
    }
    if (GetU32(bytes, 4) != kSaveRecordType) {
        return Fail(Error::Code::InvalidArgument, "unexpected save record type");
    }
    if (GetU16(bytes, 8) != kSaveRecordVersion) {
        return Fail(Error::Code::Unsupported, "unsupported save record version");
    }
    if (GetU16(bytes, 10) != kSaveEnvelopeVersion) {
        return Fail(Error::Code::Unsupported, "unsupported save envelope version");
    }
    const std::uint32_t payloadBytes = GetU32(bytes, 12);
    if (payloadBytes != bytes.size() - kSaveEnvelopeHeaderBytes) {
        return Fail(Error::Code::InvalidArgument, "save envelope payload length mismatch");
    }
    const auto payload = bytes.subspan(kSaveEnvelopeHeaderBytes);
    if (Checksum(payload) != GetU32(bytes, 16)) {
        return Fail(Error::Code::InvalidArgument, "save payload checksum mismatch");
    }
    return payload;
}
}
Result<SaveRecordInfo> InspectSaveRecord(std::span<const std::byte> bytes) {
    if (bytes.size() < kSaveEnvelopeHeaderBytes) {
        return Fail(Error::Code::InvalidArgument, "save record is shorter than its header");
    }
    if (bytes.size() > kSaveRecordMaxBytes) {
        return Fail(Error::Code::OutOfRange, "save record exceeds the configured maximum");
    }
    if (GetU32(bytes, 0) != kSaveEnvelopeMagic) {
        return Fail(Error::Code::InvalidArgument, "bad save envelope magic");
    }
    if (GetU32(bytes, 4) != kSaveRecordType) {
        return Fail(Error::Code::InvalidArgument, "unexpected save record type");
    }
    if (GetU16(bytes, 8) != kSaveRecordVersion) {
        return Fail(Error::Code::Unsupported, "unsupported save record version");
    }
    if (GetU16(bytes, 10) != kSaveEnvelopeVersion) {
        return Fail(Error::Code::Unsupported, "unsupported save envelope version");
    }
    const std::uint32_t payloadBytes = GetU32(bytes, 12);
    if (payloadBytes != bytes.size() - kSaveEnvelopeHeaderBytes) {
        return Fail(Error::Code::InvalidArgument, "save envelope payload length mismatch");
    }
    const auto payload = bytes.subspan(kSaveEnvelopeHeaderBytes);
    if (payload.size() < 6) {
        return Fail(Error::Code::InvalidArgument, "state payload is shorter than its header");
    }
    if (GetU32(payload, 0) != kStateCodecMagic) {
        return Fail(Error::Code::InvalidArgument, "bad state magic");
    }
    const std::uint16_t codecVersion = GetU16(payload, 4);
    if (codecVersion != kPersistentStateCodecVersion &&
        codecVersion != kLegacyPersistentStateCodecVersion) {
        return Fail(Error::Code::Unsupported, "unsupported state codec version");
    }
    return SaveRecordInfo{
        .recordType = kSaveRecordType,
        .recordVersion = kSaveRecordVersion,
        .codecVersion = codecVersion,
        .payloadBytes = payloadBytes,
        .checksum = GetU32(bytes, 16),
    };
}
Result<std::vector<std::byte>> EncodeSaveRecord(const PersistentState& state) {
    auto payload = EncodeState(state);
    if (!payload) {
        return std::unexpected(payload.error());
    }
    if (payload->size() > std::numeric_limits<std::uint32_t>::max()) {
        return Fail(Error::Code::OutOfRange, "state payload is too large for a save record");
    }
    if (payload->size() > kSaveRecordMaxBytes - kSaveEnvelopeHeaderBytes) {
        return Fail(Error::Code::OutOfRange, "state payload is too large for a save record");
    }

    std::vector<std::byte> out;
    out.reserve(kSaveEnvelopeHeaderBytes + payload->size());
    PutU32(out, kSaveEnvelopeMagic);
    PutU32(out, kSaveRecordType);
    PutU16(out, kSaveRecordVersion);
    PutU16(out, kSaveEnvelopeVersion);
    PutU32(out, static_cast<std::uint32_t>(payload->size()));
    PutU32(out, Checksum(*payload));
    out.insert(out.end(), payload->begin(), payload->end());
    return out;
}
Result<PersistentState> DecodeSaveRecord(std::span<const std::byte> bytes) {
    const auto payload = PayloadOf(bytes);
    if (!payload) {
        return std::unexpected(payload.error());
    }
    return DecodeState(*payload);
}
}