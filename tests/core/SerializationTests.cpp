#include <catch2/catch_test_macros.hpp>

#include "slsf/core/CategoryMask.h"
#include "slsf/core/persistence/PersistentState.h"
#include "slsf/persistence/Serialization.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <string>
#include <vector>

namespace {

using slsf::FameCategory;
using slsf::LocationPersistentState;
using slsf::PersistentState;
using slsf::ProviderContributionState;
using slsf::StableRegionKey;

PersistentState SampleState() {
    PersistentState state;
    state.codecVersion = slsf::kPersistentStateCodecVersion;
    for (std::uint32_t index = 0; index < 3; ++index) {
        LocationPersistentState location;
        location.key = StableRegionKey{"save.esp", 0x100 + index};
        for (std::size_t slot = 0; slot < slsf::kFameSlotCount; ++slot) {
            location.local[slot] = static_cast<double>(slot) + index * 0.25;
            location.remote[slot] = static_cast<double>(slot) * 0.5 - index;
            location.protectionUntilDay[slot] = static_cast<double>(slot) + 0.75;
            location.flags[slot] = static_cast<std::uint8_t>((slot + index) & 0x1Fu);
        }
        location.settledAt = 100.5 + index;
        location.remoteSettledAt = 200.25 + index;
        location.anonymity = static_cast<std::uint8_t>(index + 1);
        location.auxiliaryState = {1, 2, 3, static_cast<std::uint8_t>(index)};
        state.locations.push_back(location);
    }
    state.contributions.push_back(ProviderContributionState{
        0x1234,
        slsf::CategoryMask::FromCategory(FameCategory::Whore) |
            slsf::CategoryMask::FromCategory(FameCategory::Cuck),
    });
    state.customRegions.push_back(slsf::CustomRegionState{
        "Custom",
        StableRegionKey{"save.esp", 0x900},
    });
    return state;
}

std::uint32_t GetU16(const std::vector<std::byte>& bytes, std::size_t offset) {
    return static_cast<std::uint32_t>(bytes[offset]) |
           (static_cast<std::uint32_t>(bytes[offset + 1]) << 8u);
}

void PutU16(std::vector<std::byte>& bytes, std::size_t offset, std::uint16_t value) {
    bytes[offset] = static_cast<std::byte>(value & 0xFFu);
    bytes[offset + 1] = static_cast<std::byte>((value >> 8u) & 0xFFu);
}

void PutU32(std::vector<std::byte>& bytes, std::size_t offset, std::uint32_t value) {
    for (unsigned index = 0; index < 4; ++index) {
        bytes[offset + index] = static_cast<std::byte>((value >> (index * 8u)) & 0xFFu);
    }
}

} // namespace

TEST_CASE("save envelope roundtrips the complete persistent state") {
    const auto state = SampleState();
    auto encoded = slsf::persistence::EncodeSaveRecord(state);
    REQUIRE(encoded.has_value());
    REQUIRE(encoded->size() > slsf::persistence::kSaveEnvelopeHeaderBytes);

    const auto info = slsf::persistence::InspectSaveRecord(*encoded);
    REQUIRE(info.has_value());
    CHECK(info->recordType == slsf::persistence::kSaveRecordType);
    CHECK(info->recordVersion == slsf::persistence::kSaveRecordVersion);
    CHECK(info->codecVersion == slsf::kPersistentStateCodecVersion);
    CHECK(info->payloadBytes == encoded->size() - slsf::persistence::kSaveEnvelopeHeaderBytes);

    auto decoded = slsf::persistence::DecodeSaveRecord(*encoded);
    REQUIRE(decoded.has_value());
    CHECK(*decoded == state);
}

TEST_CASE("legacy v1 save payload is still readable") {
    auto state = SampleState();
    state.codecVersion = slsf::kLegacyPersistentStateCodecVersion;
    state.contributions.clear(); // v1 cannot carry compiled contributions
    for (auto& location : state.locations) {
        location.remoteSettledAt = 0.0; // v1 has no remote relaxation anchor
    }
    auto encoded = slsf::persistence::EncodeSaveRecord(state);
    REQUIRE(encoded.has_value());
    const auto info = slsf::persistence::InspectSaveRecord(*encoded);
    REQUIRE(info.has_value());
    CHECK(info->codecVersion == slsf::kLegacyPersistentStateCodecVersion);
    auto decoded = slsf::persistence::DecodeSaveRecord(*encoded);
    REQUIRE(decoded.has_value());
    CHECK(decoded->codecVersion == slsf::kLegacyPersistentStateCodecVersion);
    CHECK(decoded->locations == state.locations);
}

TEST_CASE("every truncation and checksum corruption is rejected") {
    auto encoded = slsf::persistence::EncodeSaveRecord(SampleState());
    REQUIRE(encoded.has_value());

    for (std::size_t size = 0; size < encoded->size(); ++size) {
        const std::span<const std::byte> prefix(encoded->data(), size);
        CHECK_FALSE(slsf::persistence::DecodeSaveRecord(prefix).has_value());
    }

    auto checksumBad = *encoded;
    checksumBad[16] ^= std::byte{0x01};
    CHECK_FALSE(slsf::persistence::DecodeSaveRecord(checksumBad).has_value());

    auto magicBad = *encoded;
    magicBad[0] ^= std::byte{0xFF};
    CHECK_FALSE(slsf::persistence::DecodeSaveRecord(magicBad).has_value());

    auto recordBad = *encoded;
    PutU32(recordBad, 4, slsf::persistence::kSaveRecordType + 1);
    CHECK_FALSE(slsf::persistence::DecodeSaveRecord(recordBad).has_value());

    auto recordVersionBad = *encoded;
    PutU16(recordVersionBad, 8,
           static_cast<std::uint16_t>(slsf::persistence::kSaveRecordVersion + 1));
    CHECK(slsf::persistence::DecodeSaveRecord(recordVersionBad).error().code ==
          slsf::Error::Code::Unsupported);

    auto envelopeVersionBad = *encoded;
    PutU16(envelopeVersionBad, 10,
           static_cast<std::uint16_t>(slsf::persistence::kSaveEnvelopeVersion + 1));
    CHECK(slsf::persistence::DecodeSaveRecord(envelopeVersionBad).error().code ==
          slsf::Error::Code::Unsupported);

    auto trailing = *encoded;
    trailing.push_back(std::byte{0});
    CHECK_FALSE(slsf::persistence::DecodeSaveRecord(trailing).has_value());
}

TEST_CASE("deterministic malformed envelopes never escape as partial state") {
    std::mt19937_64 random(1170);
    std::uniform_int_distribution<int> sizeDistribution(0, 256);
    std::uniform_int_distribution<int> byteDistribution(0, 255);
    for (int iteration = 0; iteration < 2000; ++iteration) {
        std::vector<std::byte> bytes(static_cast<std::size_t>(sizeDistribution(random)));
        for (auto& value : bytes) {
            value = static_cast<std::byte>(byteDistribution(random));
        }
        const auto decoded = slsf::persistence::DecodeSaveRecord(bytes);
        if (decoded) {
            // A random buffer cannot form a valid magic/checksum/payload by
            // construction here; reaching this branch would still be safe.
            CHECK((decoded->codecVersion == slsf::kPersistentStateCodecVersion ||
                   decoded->codecVersion == slsf::kLegacyPersistentStateCodecVersion));
        }
    }
}

TEST_CASE("random valid states survive encode/decode with seed 1170") {
    std::mt19937_64 random(1170);
    std::uniform_real_distribution<double> values(-1000.0, 1000.0);
    for (int iteration = 0; iteration < 64; ++iteration) {
        PersistentState state;
        state.codecVersion = slsf::kPersistentStateCodecVersion;
        const int locations = static_cast<int>(random() % 8);
        for (int index = 0; index < locations; ++index) {
            LocationPersistentState location;
            location.key = StableRegionKey{"random.esp",
                                           static_cast<std::uint32_t>(random() & 0x00FFFFFFu)};
            for (std::size_t slot = 0; slot < slsf::kFameSlotCount; ++slot) {
                location.local[slot] = values(random);
                location.remote[slot] = values(random);
                location.protectionUntilDay[slot] = values(random);
                location.flags[slot] = static_cast<std::uint8_t>(random() & 0xFFu);
            }
            location.settledAt = values(random);
            location.remoteSettledAt = values(random);
            location.anonymity = static_cast<std::uint8_t>(random() & 0xFFu);
            for (auto& value : location.auxiliaryState) {
                value = static_cast<std::uint8_t>(random() & 0xFFu);
            }
            state.locations.push_back(location);
        }
        std::sort(state.locations.begin(), state.locations.end(),
                  [](const auto& left, const auto& right) { return left.key < right.key; });
        state.locations.erase(
            std::unique(state.locations.begin(), state.locations.end(),
                        [](const auto& left, const auto& right) { return left.key == right.key; }),
            state.locations.end());

        const auto encoded = slsf::persistence::EncodeSaveRecord(state);
        REQUIRE(encoded.has_value());
        const auto decoded = slsf::persistence::DecodeSaveRecord(*encoded);
        const std::string decodeError = decoded ? std::string{} : decoded.error().message;
        INFO(decodeError);
        REQUIRE(decoded.has_value());
        CHECK(*decoded == state);
    }
}
