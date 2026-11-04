#include <catch2/catch_test_macros.hpp>

#include "slsf/core/persistence/PersistentState.h"
#include "slsf/core/persistence/StateCodec.h"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <limits>
#include <random>
#include <string>
#include <vector>

TEST_CASE("v2 decoding rejects custom metadata that cannot be saved", "[game-ready][codec]") {
    slsf::PersistentState state;
    state.codecVersion = slsf::kPreviousPersistentStateCodecVersion;
    state.customRegions.push_back({"X", {"custom.esp", 1}});
    auto bytes = slsf::EncodeState(state);
    REQUIRE(bytes);
    (*bytes)[26] = std::byte{0};
    (*bytes)[27] = std::byte{0};
    bytes->erase(bytes->begin() + 28);
    REQUIRE_FALSE(slsf::DecodeState(*bytes));
}

namespace {

#ifndef SLSF_FIXTURE_DIR
#define SLSF_FIXTURE_DIR "tests/fixtures"
#endif

std::vector<std::byte> ReadFixture(const char* name) {
    std::ifstream file(std::string(SLSF_FIXTURE_DIR) + "/serialization/v1/" + name,
                       std::ios::binary);
    REQUIRE(file.good());
    std::vector<char> raw((std::istreambuf_iterator<char>(file)),
                          std::istreambuf_iterator<char>());
    std::vector<std::byte> bytes;
    bytes.reserve(raw.size());
    for (const char value : raw) {
        bytes.push_back(std::byte{static_cast<unsigned char>(value)});
    }
    return bytes;
}

void PutU16(std::vector<std::byte>& bytes, std::size_t offset, std::uint16_t value) {
    bytes[offset] = static_cast<std::byte>(value & 0xFFu);
    bytes[offset + 1] = static_cast<std::byte>((value >> 8) & 0xFFu);
}

void PutU32(std::vector<std::byte>& bytes, std::size_t offset, std::uint32_t value) {
    for (std::size_t i = 0; i < 4; ++i) {
        bytes[offset + i] = static_cast<std::byte>((value >> (i * 8)) & 0xFFu);
    }
}

std::vector<std::byte> MinimalHeader() {
    std::vector<std::byte> bytes(38, std::byte{0});
    PutU32(bytes, 0, slsf::kStateCodecMagic);
    PutU16(bytes, 4, static_cast<std::uint16_t>(slsf::kStateCodecVersion));
    PutU32(bytes, 6, slsf::kPersistentStateSchemaVersion);
    return bytes;
}

std::vector<std::byte> MinimalHeaderV1() {
    std::vector<std::byte> bytes = MinimalHeader();
    bytes.resize(26);
    PutU16(bytes, 4, static_cast<std::uint16_t>(slsf::kLegacyStateCodecVersion));
    return bytes;
}

slsf::PersistentState FixtureState() {
    slsf::PersistentState state;
    state.schemaVersion = slsf::kPersistentStateSchemaVersion;
    state.codecVersion = slsf::kLegacyStateCodecVersion;

    slsf::LocationPersistentState location;
    location.key = slsf::StableRegionKey{"plugin.esp", 0x1234};
    for (std::size_t i = 0; i < slsf::kFameSlotCount; ++i) {
        location.local[i] = static_cast<double>(i) * 1.5 - 3.25;
        location.remote[i] = static_cast<double>(i) * 0.25;
        location.protectionUntilDay[i] = static_cast<double>(i) + 0.5;
        location.flags[i] = static_cast<std::uint8_t>((i & 1u) == 0 ? 0x11 : 0x22);
    }
    location.settledAt = 12345.75;
    location.anonymity = 7;
    location.auxiliaryState = {1, 2, 3, 4};
    state.locations.push_back(location);

    state.providerFlags.push_back(slsf::ProviderFlagState{
        "provider.one", slsf::FameCategory::Cuck, true, 9});
    state.customRegions.push_back(slsf::CustomRegionState{
        "Hideout", slsf::StableRegionKey{"custom.esp", 0xABC}});
    state.graphEdits.push_back(slsf::GraphEditState{
        slsf::StableRegionKey{"a.esp", 1}, slsf::StableRegionKey{"b.esp", 2}, 0.375, true});
    state.settings.push_back(slsf::SettingState{"fame.scale", 1.25});
    state.settings.push_back(slsf::SettingState{"empty", 0.0});
    return state;
}

}

TEST_CASE("state codec preserves save-owned registration and spread settings", "[codec][game-ready]") {
    slsf::PersistentState state;
    state.namedProviders = {{"example.esp", 0x2000001}};
    state.spreadChances = {{{"Skyrim.esm", 0x1234}, 73}};
    const auto encoded = slsf::EncodeState(state);
    REQUIRE(encoded);
    const auto decoded = slsf::DecodeState(*encoded);
    REQUIRE(decoded);
    CHECK(*decoded == state);
    state.codecVersion = slsf::kPreviousPersistentStateCodecVersion;
    CHECK_FALSE(slsf::EncodeState(state));
    state.namedProviders.clear();
    state.spreadChances.clear();
    const auto previous = slsf::EncodeState(state);
    REQUIRE(previous);
    CHECK(previous->size() == 26);
    CHECK(slsf::DecodeState(*previous) == state);
}

TEST_CASE("state codec encodes canonical empty payloads", "[codec]") {
    const std::vector<std::byte> expectedV2 = MinimalHeader();
    const auto encodedV2 = slsf::EncodeState(slsf::PersistentState{});
    REQUIRE(encodedV2.has_value());
    REQUIRE(*encodedV2 == expectedV2);

    const std::vector<std::byte> expectedV1 = MinimalHeaderV1();
    REQUIRE(ReadFixture("empty.bin") == expectedV1);
    auto legacyEmpty = slsf::PersistentState{};
    legacyEmpty.codecVersion = slsf::kLegacyStateCodecVersion;
    const auto encodedV1 = slsf::EncodeState(legacyEmpty);
    REQUIRE(encodedV1.has_value());
    REQUIRE(*encodedV1 == expectedV1);

    const auto decodedV1 = slsf::DecodeState(expectedV1);
    REQUIRE(decodedV1.has_value());
    REQUIRE(*decodedV1 == legacyEmpty);
}

TEST_CASE("state codec roundtrips the full v1 fixture", "[codec]") {
    const auto expected = FixtureState();
    const auto encoded = slsf::EncodeState(expected);
    REQUIRE(encoded.has_value());
    REQUIRE(ReadFixture("minimal.bin") == *encoded);

    const auto decoded = slsf::DecodeState(*encoded);
    REQUIRE(decoded.has_value());
    REQUIRE(*decoded == expected);
    REQUIRE(decoded->locations[0].local[3] == 1.25);
    REQUIRE(decoded->locations[0].protectionUntilDay[24] == 24.5);
    REQUIRE(decoded->locations[0].key.plugin == "plugin.esp");
    REQUIRE(decoded->providerFlags[0].category == slsf::FameCategory::Cuck);

    const auto reencoded = slsf::EncodeState(*decoded);
    REQUIRE(reencoded.has_value());
    REQUIRE(*reencoded == *encoded);
}

TEST_CASE("state codec keys providers by name and category", "[codec]") {
    auto state = FixtureState();
    state.providerFlags.push_back(slsf::ProviderFlagState{
        "provider.one", slsf::FameCategory::Nasty, false, 2});
    const auto encoded = slsf::EncodeState(state);
    REQUIRE(encoded.has_value());
    const auto decoded = slsf::DecodeState(*encoded);
    REQUIRE(decoded.has_value());
    REQUIRE(decoded->providerFlags.size() == 2);

    state.providerFlags.push_back(state.providerFlags[0]);
    REQUIRE_FALSE(slsf::EncodeState(state).has_value());

    state.providerFlags.pop_back();
    state.providerFlags[1].provider.clear();
    REQUIRE_FALSE(slsf::EncodeState(state).has_value());
}

TEST_CASE("state codec rejects missing plugins and preserves exact finite values", "[codec]") {
    slsf::PersistentState state;
    slsf::LocationPersistentState location;
    location.key = slsf::StableRegionKey{"", 0};
    location.local[0] = -0.0;
    location.local[1] = std::numeric_limits<double>::denorm_min();
    location.local[2] = std::numeric_limits<double>::max();
    location.local[3] = std::numeric_limits<double>::lowest();
    location.remote[0] = 0.1 + 0.2;
    location.protectionUntilDay[0] = -123.875;
    location.settledAt = -1.0e-300;
    state.locations.push_back(location);
    REQUIRE_FALSE(slsf::EncodeState(state).has_value());

    state.locations[0].key.plugin = "plugin.esp";
    const auto encoded = slsf::EncodeState(state);
    REQUIRE(encoded.has_value());
    const auto decoded = slsf::DecodeState(*encoded);
    REQUIRE(decoded.has_value());
    REQUIRE(*decoded == state);
    REQUIRE(std::bit_cast<std::uint64_t>(decoded->locations[0].local[0]) ==
            std::bit_cast<std::uint64_t>(-0.0));
}

TEST_CASE("state codec random finite states roundtrip", "[codec]") {
    std::mt19937_64 rng(1170);
    std::uniform_int_distribution<int> countDistribution(0, 8);
    std::uniform_int_distribution<int> wordDistribution(0, 1000000);
    std::uniform_real_distribution<double> fractionDistribution(-1000.0, 1000.0);

    for (std::uint32_t iteration = 0; iteration < 500; ++iteration) {
        slsf::PersistentState state;
        state.codecVersion = slsf::kLegacyStateCodecVersion;
        const auto locationCount = countDistribution(rng);
        for (int i = 0; i < locationCount; ++i) {
            slsf::LocationPersistentState location;
            location.key = slsf::StableRegionKey{
                "plugin" + std::to_string(wordDistribution(rng)) + ".esp",
                static_cast<std::uint32_t>(wordDistribution(rng))};
            for (std::size_t slot = 0; slot < slsf::kFameSlotCount; ++slot) {
                location.local[slot] = fractionDistribution(rng) + static_cast<double>(slot) / 64.0;
                location.remote[slot] = -fractionDistribution(rng);
                location.protectionUntilDay[slot] = fractionDistribution(rng) / 3.0;
                location.flags[slot] = static_cast<std::uint8_t>(wordDistribution(rng) & 0xFF);
            }
            location.settledAt = fractionDistribution(rng);
            location.anonymity = static_cast<std::uint8_t>(wordDistribution(rng) & 0xFF);
            for (auto& value : location.auxiliaryState) {
                value = static_cast<std::uint8_t>(wordDistribution(rng) & 0xFF);
            }
            state.locations.push_back(location);
        }

        const auto providerCount = countDistribution(rng);
        for (int i = 0; i < providerCount; ++i) {
            state.providerFlags.push_back(slsf::ProviderFlagState{
                "provider" + std::to_string(i),
                static_cast<slsf::FameCategory>(wordDistribution(rng) % slsf::kFameCategoryCount),
                (wordDistribution(rng) & 1) != 0,
                static_cast<std::uint8_t>(wordDistribution(rng) & 0xFF)});
        }

        const auto regionCount = countDistribution(rng);
        for (int i = 0; i < regionCount; ++i) {
            state.customRegions.push_back(slsf::CustomRegionState{
                "region" + std::to_string(i),
                slsf::StableRegionKey{"custom.esp", static_cast<std::uint32_t>(i + 1)}});
        }

        const auto editCount = countDistribution(rng);
        for (int i = 0; i < editCount; ++i) {
            state.graphEdits.push_back(slsf::GraphEditState{
                slsf::StableRegionKey{"from.esp", static_cast<std::uint32_t>(i + 1)},
                slsf::StableRegionKey{"to.esp", static_cast<std::uint32_t>(i + 1)},
                fractionDistribution(rng),
                (wordDistribution(rng) & 1) != 0});
        }

        const auto settingCount = countDistribution(rng);
        for (int i = 0; i < settingCount; ++i) {
            state.settings.push_back(
                slsf::SettingState{"setting" + std::to_string(i), fractionDistribution(rng)});
        }

        const auto encoded = slsf::EncodeState(state);
        REQUIRE(encoded.has_value());
        const auto decoded = slsf::DecodeState(*encoded);
        REQUIRE(decoded.has_value());
        REQUIRE(*decoded == state);
    }
}

TEST_CASE("state codec rejects invalid source states", "[codec]") {
    auto state = FixtureState();
    state.schemaVersion = 2;
    REQUIRE_FALSE(slsf::EncodeState(state).has_value());

    state = FixtureState();
    state.locations[0].local[5] = std::numeric_limits<double>::quiet_NaN();
    REQUIRE_FALSE(slsf::EncodeState(state).has_value());

    state = FixtureState();
    state.locations[0].remote[6] = std::numeric_limits<double>::infinity();
    REQUIRE_FALSE(slsf::EncodeState(state).has_value());

    state = FixtureState();
    state.locations[0].protectionUntilDay[7] = -std::numeric_limits<double>::infinity();
    REQUIRE_FALSE(slsf::EncodeState(state).has_value());

    state = FixtureState();
    state.locations[0].settledAt = std::numeric_limits<double>::quiet_NaN();
    REQUIRE_FALSE(slsf::EncodeState(state).has_value());

    state = FixtureState();
    state.locations.push_back(state.locations[0]);
    REQUIRE_FALSE(slsf::EncodeState(state).has_value());

    state = FixtureState();
    state.providerFlags.push_back(state.providerFlags[0]);
    REQUIRE_FALSE(slsf::EncodeState(state).has_value());

    state = FixtureState();
    state.providerFlags[0].category = slsf::FameCategory::Count;
    REQUIRE_FALSE(slsf::EncodeState(state).has_value());

    state = FixtureState();
    state.customRegions.push_back(state.customRegions[0]);
    REQUIRE_FALSE(slsf::EncodeState(state).has_value());

    state = FixtureState();
    state.graphEdits[0].retention = std::numeric_limits<double>::quiet_NaN();
    REQUIRE_FALSE(slsf::EncodeState(state).has_value());

    state = FixtureState();
    state.settings.push_back(state.settings[0]);
    REQUIRE_FALSE(slsf::EncodeState(state).has_value());

    state = FixtureState();
    state.settings[0].value = std::numeric_limits<double>::infinity();
    REQUIRE_FALSE(slsf::EncodeState(state).has_value());

    state = FixtureState();
    state.locations[0].key.plugin.assign(slsf::kStateCodecMaxStringBytes + 1, 'x');
    REQUIRE_FALSE(slsf::EncodeState(state).has_value());

    state = FixtureState();
    state.providerFlags[0].provider.assign(slsf::kStateCodecMaxStringBytes + 1, 'x');
    REQUIRE_FALSE(slsf::EncodeState(state).has_value());
}

TEST_CASE("state codec v2 roundtrips contributions and rejects legacy sections", "[codec]") {
    slsf::PersistentState state;
    state.codecVersion = slsf::kPreviousPersistentStateCodecVersion;
    state.locations.push_back(slsf::LocationPersistentState{
        slsf::StableRegionKey{"region.esp", 3}, {}, {}, {}, {}, 5.0, 6.0, 1, {}});
    state.locations[0].local[static_cast<std::size_t>(slsf::FameCategory::Nasty)] = 12.5;
    state.contributions.push_back(
        slsf::ProviderContributionState{42, slsf::CategoryMask::FromCategory(
                                                slsf::FameCategory::Cuck)});

    const auto encoded = slsf::EncodeState(state);
    REQUIRE(encoded.has_value());
    REQUIRE((*encoded)[4] == std::byte{2});
    REQUIRE((*encoded)[5] == std::byte{0});
    const auto decoded = slsf::DecodeState(*encoded);
    REQUIRE(decoded.has_value());
    REQUIRE(*decoded == state);
    REQUIRE(decoded->locations[0].remoteSettledAt == 6.0);
    const auto reencoded = slsf::EncodeState(*decoded);
    REQUIRE(reencoded.has_value());
    REQUIRE(*reencoded == *encoded);

    auto withSettings = state;
    withSettings.settings.push_back(slsf::SettingState{"fame.scale", 2.0});
    REQUIRE_FALSE(slsf::EncodeState(withSettings).has_value());

    auto withGraphEdits = state;
    withGraphEdits.graphEdits.push_back(slsf::GraphEditState{
        slsf::StableRegionKey{"a.esp", 1}, slsf::StableRegionKey{"b.esp", 2}, 0.5, true});
    REQUIRE_FALSE(slsf::EncodeState(withGraphEdits).has_value());

    auto withEmptyCustomKey = state;
    withEmptyCustomKey.customRegions.push_back(
        slsf::CustomRegionState{"hideout", slsf::StableRegionKey{"", 3}});
    REQUIRE_FALSE(slsf::EncodeState(withEmptyCustomKey).has_value());

    auto emptyCustomKeyBytes = MinimalHeader();
    emptyCustomKeyBytes.resize(26);
    PutU16(emptyCustomKeyBytes, 4, slsf::kPreviousPersistentStateCodecVersion);
    PutU16(emptyCustomKeyBytes, 16, 1);
    for (const char ch : std::string("hideout")) {
        emptyCustomKeyBytes.push_back(std::byte{static_cast<unsigned char>(ch)});
    }
    // The string length must precede the name.
    emptyCustomKeyBytes.insert(emptyCustomKeyBytes.end() - 7,
                               {std::byte{7}, std::byte{0}});
    emptyCustomKeyBytes.push_back(std::byte{0});
    emptyCustomKeyBytes.push_back(std::byte{0});
    for (std::size_t i = 0; i < 4; ++i) {
        emptyCustomKeyBytes.push_back(std::byte{static_cast<unsigned char>((3u >> (i * 8)) & 0xFFu)});
    }
    const auto emptyCustomDecoded = slsf::DecodeState(emptyCustomKeyBytes);
    REQUIRE_FALSE(emptyCustomDecoded);
    REQUIRE(emptyCustomDecoded.error().code == slsf::Error::Code::InvalidArgument);

    auto legacyHeader = MinimalHeader();
    PutU32(legacyHeader, 22, 1);
    REQUIRE(slsf::DecodeState(legacyHeader).error().code ==
            slsf::Error::Code::InvalidArgument);

    slsf::PersistentState invalid = state;
    invalid.contributions[0].registrationId = 0;
    REQUIRE_FALSE(slsf::EncodeState(invalid).has_value());
    invalid = state;
    invalid.contributions[0].categories.Clear();
    const auto emptyRegistration = slsf::EncodeState(invalid);
    REQUIRE(emptyRegistration);
    REQUIRE(slsf::DecodeState(*emptyRegistration) == invalid);
    invalid = state;
    invalid.contributions.push_back(invalid.contributions[0]);
    REQUIRE_FALSE(slsf::EncodeState(invalid).has_value());

    invalid = state;
    invalid.locations[0].remoteSettledAt = std::numeric_limits<double>::quiet_NaN();
    REQUIRE_FALSE(slsf::EncodeState(invalid).has_value());
}

TEST_CASE("state codec reports malformed headers", "[codec]") {
    const auto encoded = slsf::EncodeState(FixtureState());
    REQUIRE(encoded.has_value());

    REQUIRE_FALSE(slsf::DecodeState(std::span<const std::byte>{}).has_value());

    auto bytes = MinimalHeader();
    bytes[0] = std::byte{0};
    REQUIRE_FALSE(slsf::DecodeState(bytes).has_value());
    REQUIRE(slsf::DecodeState(bytes).error().code == slsf::Error::Code::InvalidArgument);

    bytes = MinimalHeader();
    PutU16(bytes, 4, 4);
    REQUIRE(slsf::DecodeState(bytes).error().code == slsf::Error::Code::Unsupported);

    bytes = MinimalHeader();
    PutU32(bytes, 6, 2);
    REQUIRE(slsf::DecodeState(bytes).error().code == slsf::Error::Code::Unsupported);

    bytes = MinimalHeader();
    PutU16(bytes, 10, 0xFFFFu);
    REQUIRE(slsf::DecodeState(bytes).error().code == slsf::Error::Code::InvalidArgument);

    auto huge = MinimalHeader();
    PutU16(huge, 10, 0);
    PutU32(huge, 12, static_cast<std::uint32_t>(slsf::kStateCodecMaxProviders + 1));
    REQUIRE(slsf::DecodeState(huge).error().code == slsf::Error::Code::OutOfRange);

    auto trailing = *encoded;
    trailing.push_back(std::byte{0});
    REQUIRE_FALSE(slsf::DecodeState(trailing).has_value());
}

TEST_CASE("state codec rejects every truncation of valid payload", "[codec]") {
    const auto encoded = slsf::EncodeState(FixtureState());
    REQUIRE(encoded.has_value());
    for (std::size_t size = 0; size < encoded->size(); ++size) {
        const auto decoded = slsf::DecodeState(std::span<const std::byte>(encoded->data(), size));
        REQUIRE_FALSE(decoded.has_value());
    }
}

TEST_CASE("state codec validates enum and boolean fields", "[codec]") {
    auto provider = MinimalHeaderV1();
    PutU32(provider, 12, 1);
    provider.push_back(std::byte{1});
    provider.push_back(std::byte{0});
    provider.push_back(std::byte{'p'});
    provider.push_back(std::byte{25});
    provider.push_back(std::byte{1});
    provider.push_back(std::byte{0});
    REQUIRE(slsf::DecodeState(provider).error().code == slsf::Error::Code::InvalidArgument);

    provider[29] = std::byte{0};
    provider[30] = std::byte{2};
    REQUIRE(slsf::DecodeState(provider).error().code == slsf::Error::Code::InvalidArgument);

    provider[29] = std::byte{0};
    provider[30] = std::byte{1};
    const auto decodedProvider = slsf::DecodeState(provider);
    REQUIRE(decodedProvider.has_value());
    REQUIRE(decodedProvider->providerFlags[0].provider == "p");
    REQUIRE(decodedProvider->providerFlags[0].category == slsf::FameCategory::Whore);
    REQUIRE(decodedProvider->providerFlags[0].enabled);

    auto edit = MinimalHeaderV1();
    PutU32(edit, 18, 1);
    const auto appendKey = [](std::vector<std::byte>& bytes, char name, std::uint32_t id) {
        bytes.push_back(std::byte{1});
        bytes.push_back(std::byte{0});
        bytes.push_back(std::byte{static_cast<unsigned char>(name)});
        for (std::size_t i = 0; i < 4; ++i) {
            bytes.push_back(std::byte{static_cast<unsigned char>((id >> (i * 8)) & 0xFFu)});
        }
    };
    appendKey(edit, 'a', 1);
    appendKey(edit, 'b', 2);
    constexpr std::array<std::uint8_t, 8> retention = {0, 0, 0, 0, 0, 0, 0xE0, 0x3F};
    for (const auto value : retention) {
        edit.push_back(std::byte{value});
    }
    edit.push_back(std::byte{2});
    REQUIRE(slsf::DecodeState(edit).error().code == slsf::Error::Code::InvalidArgument);

    edit.back() = std::byte{1};
    const auto decodedEdit = slsf::DecodeState(edit);
    REQUIRE(decodedEdit.has_value());
    REQUIRE(decodedEdit->graphEdits[0].enabled);
    REQUIRE(decodedEdit->graphEdits[0].retention == std::bit_cast<double>(0x3FE0000000000000ull));
}
