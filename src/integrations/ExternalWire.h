#pragma once

#include "slsf/integrations/ExternalState.h"

#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace slsf::integrations::detail {

inline constexpr std::size_t kMaxWireBytes = 256 * 1024;
inline constexpr std::size_t kMaxWireLines = 4096;
inline constexpr std::size_t kMaxWireFields = 32;
inline constexpr std::size_t kMaxWireFieldBytes = 8192;

[[nodiscard]] inline std::string_view TrimAscii(std::string_view text) noexcept {
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t' ||
                             text.front() == '\r' || text.front() == '\n')) {
        text.remove_prefix(1);
    }
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t' ||
                             text.back() == '\r' || text.back() == '\n')) {
        text.remove_suffix(1);
    }
    return text;
}

[[nodiscard]] inline bool UnescapeField(std::string_view raw, std::string& out) {
    out.clear();
    out.reserve(raw.size());
    for (std::size_t index = 0; index < raw.size(); ++index) {
        const char value = raw[index];
        if (value != '\\') {
            out.push_back(value);
            continue;
        }
        if (index + 1 >= raw.size()) {
            return false;
        }
        const char escape = raw[++index];
        switch (escape) {
        case '\\':
            out.push_back('\\');
            break;
        case 't':
            out.push_back('\t');
            break;
        case 'n':
            out.push_back('\n');
            break;
        case 'r':
            out.push_back('\r');
            break;
        default:
            return false;
        }
    }
    return true;
}

[[nodiscard]] inline std::string EscapeField(std::string_view value) {
    std::string out;
    out.reserve(value.size());
    for (const char item : value) {
        switch (item) {
        case '\\':
            out += "\\\\";
            break;
        case '\t':
            out += "\\t";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        default:
            out.push_back(item);
            break;
        }
    }
    return out;
}

[[nodiscard]] inline bool SplitWireLine(std::string_view line,
                                        std::vector<std::string>& fields) {
    fields.clear();
    while (true) {
        const std::size_t tab = line.find('\t');
        const std::string_view raw = line.substr(0, tab);
        if (raw.size() > kMaxWireFieldBytes) {
            return false;
        }
        if (fields.size() >= kMaxWireFields) {
            return false;
        }
        std::string decoded;
        if (!UnescapeField(raw, decoded)) {
            return false;
        }
        fields.push_back(std::move(decoded));
        if (tab == std::string_view::npos) {
            return true;
        }
        line.remove_prefix(tab + 1);
    }
}

[[nodiscard]] inline bool SplitWireLines(std::string_view wire,
                                         std::vector<std::string_view>& lines) {
    lines.clear();
    if (wire.size() > kMaxWireBytes) {
        return false;
    }
    while (!wire.empty()) {
        const std::size_t newline = wire.find('\n');
        std::string_view line = wire.substr(0, newline);
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }
        if (!line.empty()) {
            if (lines.size() >= kMaxWireLines) {
                return false;
            }
            lines.push_back(line);
        }
        if (newline == std::string_view::npos) {
            break;
        }
        wire.remove_prefix(newline + 1);
    }
    return !lines.empty();
}

template <class T>
[[nodiscard]] inline bool ParseInteger(std::string_view text, T& out) {
    text = TrimAscii(text);
    if (text.empty()) {
        return false;
    }
    T value{};
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size()) {
        return false;
    }
    out = value;
    return true;
}

[[nodiscard]] inline bool ParseInt64(std::string_view text, std::int64_t& out) {
    return ParseInteger<std::int64_t>(text, out);
}

[[nodiscard]] inline bool ParseUint32(std::string_view text, std::uint32_t& out) {
    return ParseInteger<std::uint32_t>(text, out);
}

[[nodiscard]] inline bool ParseHex32(std::string_view text, std::uint32_t& out) {
    text = TrimAscii(text);
    if (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        text.remove_prefix(2);
    }
    if (text.empty() || text.size() > 8) {
        return false;
    }
    std::uint32_t value{};
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value, 16);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size()) {
        return false;
    }
    out = value;
    return true;
}

[[nodiscard]] inline bool ParseLocalFormId(std::string_view text, std::uint32_t& out) {
    return ParseHex32(text, out) && out != 0 && out <= 0x00ffffff;
}

[[nodiscard]] inline bool ParseBool(std::string_view text, bool& out) {
    text = TrimAscii(text);
    if (text == "1" || text == "true" || text == "True") {
        out = true;
        return true;
    }
    if (text == "0" || text == "false" || text == "False") {
        out = false;
        return true;
    }
    return false;
}

[[nodiscard]] inline bool ParseBoolOrUnknown(std::string_view text, bool& out,
                                             bool& known) {
    text = TrimAscii(text);
    if (text == "-1" || text == "unknown" || text == "Unknown") {
        known = false;
        out = false;
        return true;
    }
    known = true;
    return ParseBool(text, out);
}

[[nodiscard]] inline bool ParseDouble(std::string_view text, double& out) {
    text = TrimAscii(text);
    if (text.empty()) {
        return false;
    }
    double value{};
    const auto result =
        std::from_chars(text.data(), text.data() + text.size(), value,
                        std::chars_format::general);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size() ||
        !std::isfinite(value) || std::abs(value) > 1.0e9) {
        return false;
    }
    out = value;
    return true;
}

[[nodiscard]] inline bool ParseAvailability(std::string_view text,
                                            ExternalAvailability& out) {
    text = TrimAscii(text);
    if (text == "available" || text == "Available") {
        out = ExternalAvailability::Available;
        return true;
    }
    if (text == "missing" || text == "Missing") {
        out = ExternalAvailability::Missing;
        return true;
    }
    if (text == "incompatible" || text == "Incompatible") {
        out = ExternalAvailability::Incompatible;
        return true;
    }
    if (text == "unknown" || text == "Unknown") {
        out = ExternalAvailability::Unknown;
        return true;
    }
    return false;
}

[[nodiscard]] inline bool ParseFactBool(std::string_view text, Fact& out) {
    bool value = false;
    bool known = false;
    if (!ParseBoolOrUnknown(text, value, known)) {
        return false;
    }
    out = known ? Fact::Known(value) : Fact{FactState::Unknown};
    return true;
}

[[nodiscard]] inline std::string MakeProviderText(const ExternalProviderInfo& info) {
    return EscapeField(info.name) + "\t" + EscapeField(info.version) + "\t" +
           std::to_string(info.apiVersion);
}

}
