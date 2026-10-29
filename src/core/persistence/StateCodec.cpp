#include "slsf/core/persistence/StateCodec.h"

#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace slsf {
namespace {

constexpr std::size_t kLocationMinBytesV1 =
    2 + 4 + (kFameSlotCount * sizeof(double) * 3) + kFameSlotCount + sizeof(double) + 1 + 4;
constexpr std::size_t kLocationMinBytesV2 = kLocationMinBytesV1 + sizeof(double);
constexpr std::size_t kProviderMinBytesV1 = 2 + 1 + 1 + 1;
constexpr std::size_t kProviderMinBytesV2 = sizeof(std::uint64_t) + sizeof(std::uint32_t);
constexpr std::size_t kCustomRegionMinBytes = 2 + 2 + 4;
constexpr std::size_t kGraphEditMinBytes = 2 + 4 + 2 + 4 + sizeof(double) + 1;
constexpr std::size_t kSettingMinBytes = 2 + sizeof(double);

class Reader {
public:
    explicit Reader(std::span<const std::byte> bytes) : bytes_(bytes) {}

    bool Ok() const { return ok_; }
    const Error& GetError() const { return error_; }
    std::size_t Remaining() const { return bytes_.size() - offset_; }

    bool U8(std::uint8_t& value) {
        if (!Need(1)) {
            return false;
        }
        value = std::to_integer<std::uint8_t>(bytes_[offset_]);
        ++offset_;
        return true;
    }

    bool U16(std::uint16_t& value) {
        if (!Need(2)) {
            return false;
        }
        value = static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(bytes_[offset_])) |
                static_cast<std::uint16_t>(
                    static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(bytes_[offset_ + 1]))
                    << 8);
        offset_ += 2;
        return true;
    }

    bool U32(std::uint32_t& value) {
        if (!Need(4)) {
            return false;
        }
        value = 0;
        for (std::size_t i = 0; i < 4; ++i) {
            value |= static_cast<std::uint32_t>(
                         std::to_integer<std::uint8_t>(bytes_[offset_ + i]))
                     << (i * 8);
        }
        offset_ += 4;
        return true;
    }

    bool U64(std::uint64_t& value) {
        if (!Need(8)) {
            return false;
        }
        value = 0;
        for (std::size_t i = 0; i < 8; ++i) {
            value |= static_cast<std::uint64_t>(
                         std::to_integer<std::uint8_t>(bytes_[offset_ + i]))
                     << (i * 8);
        }
        offset_ += 8;
        return true;
    }

    bool Double(double& value) {
        std::uint64_t raw = 0;
        if (!U64(raw)) {
            return false;
        }
        value = std::bit_cast<double>(raw);
        return true;
    }

    bool String(std::string& value, std::size_t maxBytes) {
        std::uint16_t size = 0;
        if (!U16(size)) {
            return false;
        }
        if (size > maxBytes) {
            return FailBytes("string exceeds configured limit");
        }
        if (!Need(size)) {
            return false;
        }
        value.assign(reinterpret_cast<const char*>(bytes_.data() + offset_), size);
        offset_ += size;
        return true;
    }

    bool Key(std::string& plugin, std::uint32_t& localFormId) {
        return String(plugin, kStateCodecMaxStringBytes) && U32(localFormId);
    }

    bool Done() {
        if (!ok_) {
            return false;
        }
        if (offset_ != bytes_.size()) {
            return FailBytes("trailing bytes after state");
        }
        return true;
    }

private:
    bool Need(std::size_t count) {
        if (!ok_) {
            return false;
        }
        if (count > bytes_.size() - offset_) {
            return FailBytes("truncated state");
        }
        return true;
    }

    bool FailBytes(std::string_view message) {
        if (ok_) {
            ok_ = false;
            error_ = Error{Error::Code::InvalidArgument, std::string(message)};
        }
        return false;
    }

    std::span<const std::byte> bytes_;
    std::size_t offset_{};
    bool ok_{true};
    Error error_{};
};

void PutU16(std::vector<std::byte>& out, std::uint16_t value) {
    out.push_back(static_cast<std::byte>(value & 0xFFu));
    out.push_back(static_cast<std::byte>((value >> 8) & 0xFFu));
}

void PutU32(std::vector<std::byte>& out, std::uint32_t value) {
    for (std::size_t i = 0; i < 4; ++i) {
        out.push_back(static_cast<std::byte>((value >> (i * 8)) & 0xFFu));
    }
}

void PutU64(std::vector<std::byte>& out, std::uint64_t value) {
    for (std::size_t i = 0; i < 8; ++i) {
        out.push_back(static_cast<std::byte>((value >> (i * 8)) & 0xFFu));
    }
}

void PutDouble(std::vector<std::byte>& out, double value) {
    PutU64(out, std::bit_cast<std::uint64_t>(value));
}

void PutString(std::vector<std::byte>& out, std::string_view value) {
    PutU16(out, static_cast<std::uint16_t>(value.size()));
    for (const char ch : value) {
        out.push_back(static_cast<std::byte>(static_cast<unsigned char>(ch)));
    }
}

void PutKey(std::vector<std::byte>& out, std::string_view plugin, std::uint32_t localFormId) {
    PutString(out, plugin);
    PutU32(out, localFormId);
}

bool ValidString(std::string_view value) {
    return value.size() <= kStateCodecMaxStringBytes;
}

}

Result<std::vector<std::byte>> EncodeState(const PersistentState& state) {
    if (state.schemaVersion != kPersistentStateSchemaVersion) {
        return Fail(Error::Code::Unsupported, "unsupported persistent state schema");
    }
    if (state.codecVersion != kStateCodecVersion &&
        state.codecVersion != kLegacyStateCodecVersion) {
        return Fail(Error::Code::Unsupported, "unsupported state codec version");
    }
    const bool legacy = state.codecVersion == kLegacyStateCodecVersion;
    if (legacy) {
        if (!state.contributions.empty()) {
            return Fail(Error::Code::InvalidArgument,
                        "v1 payload cannot carry compiled provider contributions");
        }
    } else if (!state.providerFlags.empty() || !state.graphEdits.empty() ||
               !state.settings.empty()) {
        // Version 2 keeps settings and graph edits in cross-save JSON instead of this payload.
        return Fail(Error::Code::InvalidArgument,
                    "v2 payload cannot carry legacy settings or graph edits");
    }
    if (state.locations.size() > kStateCodecMaxLocations) {
        return Fail(Error::Code::OutOfRange, "too many persisted locations");
    }
    if (state.providerFlags.size() > kStateCodecMaxProviders ||
        state.contributions.size() > kStateCodecMaxProviders) {
        return Fail(Error::Code::OutOfRange, "too many persisted providers");
    }
    if (state.customRegions.size() > kStateCodecMaxCustomRegions) {
        return Fail(Error::Code::OutOfRange, "too many persisted custom regions");
    }
    if (state.graphEdits.size() > kStateCodecMaxGraphEdits) {
        return Fail(Error::Code::OutOfRange, "too many persisted graph edits");
    }
    if (state.settings.size() > kStateCodecMaxSettings) {
        return Fail(Error::Code::OutOfRange, "too many persisted settings");
    }

    std::set<StableRegionKey> locationKeys;
    for (const auto& location : state.locations) {
        if (!ValidString(location.key.plugin)) {
            return Fail(Error::Code::OutOfRange, "location plugin string is too long");
        }
        if (location.key.plugin.empty()) {
            return Fail(Error::Code::InvalidArgument, "location key has no plugin");
        }
        if (!locationKeys.emplace(location.key).second) {
            return Fail(Error::Code::InvalidArgument, "duplicate location key");
        }
        for (const double value : location.local) {
            if (!std::isfinite(value)) {
                return Fail(Error::Code::InvalidArgument, "location has a non-finite local value");
            }
        }
        for (const double value : location.remote) {
            if (!std::isfinite(value)) {
                return Fail(Error::Code::InvalidArgument, "location has a non-finite remote value");
            }
        }
        for (const double value : location.protectionUntilDay) {
            if (!std::isfinite(value)) {
                return Fail(Error::Code::InvalidArgument, "location has a non-finite protection");
            }
        }
        if (!std::isfinite(location.settledAt)) {
            return Fail(Error::Code::InvalidArgument, "location has a non-finite settled time");
        }
        if (!legacy && !std::isfinite(location.remoteSettledAt)) {
            return Fail(Error::Code::InvalidArgument,
                        "location has a non-finite remote settled time");
        }
    }
    std::set<std::pair<std::string, FameCategory>> providerKeys;
    for (const auto& provider : state.providerFlags) {
        if (!ValidString(provider.provider) || provider.provider.empty()) {
            return Fail(Error::Code::OutOfRange, "provider string is invalid");
        }
        if (!IsValidFameCategory(provider.category)) {
            return Fail(Error::Code::InvalidArgument, "provider has an invalid category");
        }
        if (!providerKeys.emplace(provider.provider, provider.category).second) {
            return Fail(Error::Code::InvalidArgument, "duplicate provider entry");
        }
    }
    std::set<std::uint64_t> contributionIds;
    for (const auto& contribution : state.contributions) {
        if (contribution.registrationId == 0) {
            return Fail(Error::Code::InvalidArgument, "provider contribution has no registration");
        }
        if (!contributionIds.emplace(contribution.registrationId).second) {
            return Fail(Error::Code::InvalidArgument, "duplicate provider contribution");
        }
    }
    std::set<StableRegionKey> customRegionKeys;
    for (const auto& region : state.customRegions) {
        if (!ValidString(region.name) || !ValidString(region.key.plugin)) {
            return Fail(Error::Code::OutOfRange, "custom region string is too long");
        }
        if (region.key.plugin.empty()) {
            return Fail(Error::Code::InvalidArgument, "custom region key has no plugin");
        }
        if (!customRegionKeys.emplace(region.key).second) {
            return Fail(Error::Code::InvalidArgument, "duplicate custom region key");
        }
    }
    for (const auto& edit : state.graphEdits) {
        if (!ValidString(edit.from.plugin) || !ValidString(edit.to.plugin)) {
            return Fail(Error::Code::OutOfRange, "graph edit string is too long");
        }
        if (!std::isfinite(edit.retention)) {
            return Fail(Error::Code::InvalidArgument, "graph edit has a non-finite retention");
        }
    }
    std::set<std::string> settingKeys;
    for (const auto& setting : state.settings) {
        if (!ValidString(setting.key)) {
            return Fail(Error::Code::OutOfRange, "setting string is too long");
        }
        if (!settingKeys.emplace(setting.key).second) {
            return Fail(Error::Code::InvalidArgument, "duplicate setting key");
        }
        if (!std::isfinite(setting.value)) {
            return Fail(Error::Code::InvalidArgument, "setting has a non-finite value");
        }
    }

    const std::size_t providerCount =
        legacy ? state.providerFlags.size() : state.contributions.size();
    std::vector<std::byte> out;
    out.reserve(26 + (state.locations.size() * kLocationMinBytesV2));
    PutU32(out, kStateCodecMagic);
    PutU16(out, static_cast<std::uint16_t>(state.codecVersion));
    PutU32(out, state.schemaVersion);
    PutU16(out, static_cast<std::uint16_t>(state.locations.size()));
    PutU32(out, static_cast<std::uint32_t>(providerCount));
    PutU16(out, static_cast<std::uint16_t>(state.customRegions.size()));
    PutU32(out, legacy ? static_cast<std::uint32_t>(state.graphEdits.size()) : 0u);
    PutU32(out, legacy ? static_cast<std::uint32_t>(state.settings.size()) : 0u);

    for (const auto& location : state.locations) {
        PutKey(out, location.key.plugin, location.key.localFormId);
        for (const double value : location.local) {
            PutDouble(out, value);
        }
        for (const double value : location.remote) {
            PutDouble(out, value);
        }
        for (const double value : location.protectionUntilDay) {
            PutDouble(out, value);
        }
        for (const std::uint8_t value : location.flags) {
            out.push_back(static_cast<std::byte>(value));
        }
        PutDouble(out, location.settledAt);
        if (!legacy) {
            PutDouble(out, location.remoteSettledAt);
        }
        out.push_back(static_cast<std::byte>(location.anonymity));
        for (const std::uint8_t value : location.auxiliaryState) {
            out.push_back(static_cast<std::byte>(value));
        }
    }

    if (legacy) {
        for (const auto& provider : state.providerFlags) {
            PutString(out, provider.provider);
            out.push_back(static_cast<std::byte>(static_cast<std::uint8_t>(provider.category)));
            out.push_back(static_cast<std::byte>(provider.enabled ? 1 : 0));
            out.push_back(static_cast<std::byte>(provider.phase));
        }
    } else {
        for (const auto& contribution : state.contributions) {
            PutU64(out, contribution.registrationId);
            PutU32(out, contribution.categories.Bits());
        }
    }

    for (const auto& region : state.customRegions) {
        PutString(out, region.name);
        PutKey(out, region.key.plugin, region.key.localFormId);
    }

    for (const auto& edit : state.graphEdits) {
        PutKey(out, edit.from.plugin, edit.from.localFormId);
        PutKey(out, edit.to.plugin, edit.to.localFormId);
        PutDouble(out, edit.retention);
        out.push_back(static_cast<std::byte>(edit.enabled ? 1 : 0));
    }

    for (const auto& setting : state.settings) {
        PutString(out, setting.key);
        PutDouble(out, setting.value);
    }

    return out;
}

Result<PersistentState> DecodeState(std::span<const std::byte> bytes) {
    Reader reader(bytes);

    std::uint32_t magic = 0;
    std::uint16_t codecVersion = 0;
    std::uint32_t schemaVersion = 0;
    std::uint16_t locationCount = 0;
    std::uint32_t providerCount = 0;
    std::uint16_t customRegionCount = 0;
    std::uint32_t graphEditCount = 0;
    std::uint32_t settingCount = 0;

    if (!reader.U32(magic)) {
        return std::unexpected(reader.GetError());
    }
    if (magic != kStateCodecMagic) {
        return Fail(Error::Code::InvalidArgument, "bad state magic");
    }
    if (!reader.U16(codecVersion)) {
        return std::unexpected(reader.GetError());
    }
    if (codecVersion != kStateCodecVersion && codecVersion != kLegacyStateCodecVersion) {
        return Fail(Error::Code::Unsupported, "unsupported state codec version");
    }
    const bool legacy = codecVersion == kLegacyStateCodecVersion;
    if (!reader.U32(schemaVersion)) {
        return std::unexpected(reader.GetError());
    }
    if (schemaVersion != kPersistentStateSchemaVersion) {
        return Fail(Error::Code::Unsupported, "unsupported persistent state schema");
    }
    if (!reader.U16(locationCount) || !reader.U32(providerCount) ||
        !reader.U16(customRegionCount) || !reader.U32(graphEditCount) ||
        !reader.U32(settingCount)) {
        return std::unexpected(reader.GetError());
    }

    if (locationCount > kStateCodecMaxLocations || providerCount > kStateCodecMaxProviders ||
        customRegionCount > kStateCodecMaxCustomRegions || graphEditCount > kStateCodecMaxGraphEdits ||
        settingCount > kStateCodecMaxSettings) {
        return Fail(Error::Code::OutOfRange, "state element count exceeds limit");
    }
    if (!legacy && (graphEditCount != 0 || settingCount != 0)) {
        return Fail(Error::Code::InvalidArgument,
                    "v2 payload carries legacy settings or graph edits");
    }
    const std::size_t providerMinBytes =
        legacy ? kProviderMinBytesV1 : kProviderMinBytesV2;
    const std::size_t locationMinBytes =
        legacy ? kLocationMinBytesV1 : kLocationMinBytesV2;
    // Sections are decoded in order, so claim their minimum bytes against one
    // shared budget instead of testing each against the same remaining size.
    std::size_t budget = reader.Remaining();
    const auto claim = [&budget](std::size_t count, std::size_t minBytes) {
        if (count > budget / minBytes) {
            return false;
        }
        budget -= count * minBytes;
        return true;
    };
    if (!claim(locationCount, locationMinBytes) ||
        !claim(providerCount, providerMinBytes) ||
        !claim(customRegionCount, kCustomRegionMinBytes) ||
        !claim(graphEditCount, kGraphEditMinBytes) ||
        !claim(settingCount, kSettingMinBytes)) {
        return Fail(Error::Code::InvalidArgument, "state element count exceeds payload");
    }

    PersistentState state;
    state.schemaVersion = schemaVersion;
    state.codecVersion = codecVersion;
    state.locations.resize(locationCount);
    state.customRegions.resize(customRegionCount);
    if (legacy) {
        state.providerFlags.resize(providerCount);
        state.graphEdits.resize(graphEditCount);
        state.settings.resize(settingCount);
    } else {
        state.contributions.resize(providerCount);
    }

    for (auto& location : state.locations) {
        if (!reader.Key(location.key.plugin, location.key.localFormId)) {
            return std::unexpected(reader.GetError());
        }
        if (location.key.plugin.empty()) {
            return Fail(Error::Code::InvalidArgument, "location key has no plugin");
        }
        for (double& value : location.local) {
            if (!reader.Double(value)) {
                return std::unexpected(reader.GetError());
            }
        }
        for (double& value : location.remote) {
            if (!reader.Double(value)) {
                return std::unexpected(reader.GetError());
            }
        }
        for (double& value : location.protectionUntilDay) {
            if (!reader.Double(value)) {
                return std::unexpected(reader.GetError());
            }
        }
        for (std::uint8_t& value : location.flags) {
            if (!reader.U8(value)) {
                return std::unexpected(reader.GetError());
            }
        }
        if (!reader.Double(location.settledAt)) {
            return std::unexpected(reader.GetError());
        }
        if (!legacy && !reader.Double(location.remoteSettledAt)) {
            return std::unexpected(reader.GetError());
        }
        if (!reader.U8(location.anonymity)) {
            return std::unexpected(reader.GetError());
        }
        for (std::uint8_t& value : location.auxiliaryState) {
            if (!reader.U8(value)) {
                return std::unexpected(reader.GetError());
            }
        }
    }

    if (legacy) {
        for (auto& provider : state.providerFlags) {
            if (!reader.String(provider.provider, kStateCodecMaxStringBytes)) {
                return std::unexpected(reader.GetError());
            }
            std::uint8_t category = 0;
            std::uint8_t enabled = 0;
            if (!reader.U8(category) || !reader.U8(enabled) || !reader.U8(provider.phase)) {
                return std::unexpected(reader.GetError());
            }
            if (category >= kFameCategoryCount) {
                return Fail(Error::Code::InvalidArgument, "provider has an invalid category");
            }
            if (enabled > 1) {
                return Fail(Error::Code::InvalidArgument, "provider has an invalid enabled flag");
            }
            provider.category = static_cast<FameCategory>(category);
            provider.enabled = enabled != 0;
        }
    } else {
        for (auto& contribution : state.contributions) {
            std::uint32_t bits = 0;
            if (!reader.U64(contribution.registrationId) || !reader.U32(bits)) {
                return std::unexpected(reader.GetError());
            }
            if (contribution.registrationId == 0) {
                return Fail(Error::Code::InvalidArgument,
                            "provider contribution has no registration");
            }
            const auto categories = CategoryMask::FromBits(bits);
            if (!categories) {
                return Fail(Error::Code::InvalidArgument,
                            "provider contribution has invalid categories");
            }
            contribution.categories = *categories;
        }
    }

    for (auto& region : state.customRegions) {
        if (!reader.String(region.name, kStateCodecMaxStringBytes) ||
            !reader.Key(region.key.plugin, region.key.localFormId)) {
            return std::unexpected(reader.GetError());
        }
    }

    for (auto& edit : state.graphEdits) {
        if (!reader.Key(edit.from.plugin, edit.from.localFormId) ||
            !reader.Key(edit.to.plugin, edit.to.localFormId) || !reader.Double(edit.retention)) {
            return std::unexpected(reader.GetError());
        }
        std::uint8_t enabled = 0;
        if (!reader.U8(enabled)) {
            return std::unexpected(reader.GetError());
        }
        if (enabled > 1) {
            return Fail(Error::Code::InvalidArgument, "graph edit has an invalid enabled flag");
        }
        edit.enabled = enabled != 0;
    }

    for (auto& setting : state.settings) {
        if (!reader.String(setting.key, kStateCodecMaxStringBytes) ||
            !reader.Double(setting.value)) {
            return std::unexpected(reader.GetError());
        }
    }

    std::set<StableRegionKey> locationKeys;
    for (const auto& location : state.locations) {
        if (!locationKeys.emplace(location.key).second) {
            return Fail(Error::Code::InvalidArgument, "duplicate location key");
        }
        for (const double value : location.local) {
            if (!std::isfinite(value)) {
                return Fail(Error::Code::InvalidArgument, "location has a non-finite local value");
            }
        }
        for (const double value : location.remote) {
            if (!std::isfinite(value)) {
                return Fail(Error::Code::InvalidArgument, "location has a non-finite remote value");
            }
        }
        for (const double value : location.protectionUntilDay) {
            if (!std::isfinite(value)) {
                return Fail(Error::Code::InvalidArgument, "location has a non-finite protection");
            }
        }
        if (!std::isfinite(location.settledAt)) {
            return Fail(Error::Code::InvalidArgument, "location has a non-finite settled time");
        }
        if (!legacy && !std::isfinite(location.remoteSettledAt)) {
            return Fail(Error::Code::InvalidArgument,
                        "location has a non-finite remote settled time");
        }
    }

    std::set<std::pair<std::string, FameCategory>> providerKeys;
    for (const auto& provider : state.providerFlags) {
        if (provider.provider.empty()) {
            return Fail(Error::Code::InvalidArgument, "provider string is empty");
        }
        if (!providerKeys.emplace(provider.provider, provider.category).second) {
            return Fail(Error::Code::InvalidArgument, "duplicate provider entry");
        }
    }

    std::set<std::uint64_t> contributionIds;
    for (const auto& contribution : state.contributions) {
        if (contribution.registrationId == 0) {
            return Fail(Error::Code::InvalidArgument, "provider contribution has no registration");
        }
        if (!contributionIds.emplace(contribution.registrationId).second) {
            return Fail(Error::Code::InvalidArgument, "duplicate provider contribution");
        }
    }

    std::set<StableRegionKey> customRegionKeys;
    for (const auto& region : state.customRegions) {
        if (region.key.plugin.empty()) {
            return Fail(Error::Code::InvalidArgument, "custom region key has no plugin");
        }
        if (!customRegionKeys.emplace(region.key).second) {
            return Fail(Error::Code::InvalidArgument, "duplicate custom region key");
        }
    }

    for (const auto& edit : state.graphEdits) {
        if (!std::isfinite(edit.retention)) {
            return Fail(Error::Code::InvalidArgument, "graph edit has a non-finite retention");
        }
    }

    std::set<std::string> settingKeys;
    for (const auto& setting : state.settings) {
        if (!settingKeys.emplace(setting.key).second) {
            return Fail(Error::Code::InvalidArgument, "duplicate setting key");
        }
        if (!std::isfinite(setting.value)) {
            return Fail(Error::Code::InvalidArgument, "setting has a non-finite value");
        }
    }

    if (!reader.Done()) {
        return std::unexpected(reader.GetError());
    }
    return state;
}

}
