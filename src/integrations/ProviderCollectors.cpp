#include "slsf/integrations/ProviderCollectors.h"
#include "ExternalWire.h"
#include <algorithm>
#include <charconv>
#include <set>
#include <stdexcept>

namespace slsf::integrations {
std::vector<std::uint32_t> EncodeCollectorContinuations(std::span<const CollectorContinuation> calls) {
    if (calls.size() > kMaxCollectorCalls) throw std::length_error("too many collector continuations");
    std::vector<std::uint32_t> words{static_cast<std::uint32_t>(calls.size())};
    for (const auto& call : calls) {
        words.push_back(call.stack);
        words.push_back(call.actors ? 1 : 0);
    }
    return words;
}
bool DecodeCollectorContinuations(std::span<const std::uint32_t> words,
                                  std::vector<CollectorContinuation>& out) {
    if (words.empty() || words[0] > kMaxCollectorCalls || words.size() != 1 + words[0] * 2) return false;
    std::vector<CollectorContinuation> calls;
    std::set<std::uint32_t> stacks;
    for (std::size_t i = 1; i < words.size(); i += 2) {
        if (words[i + 1] > 1 || !stacks.insert(words[i]).second) return false;
        calls.push_back({words[i], words[i + 1] == 1});
    }
    out = std::move(calls);
    return true;
}
std::string WireEscape(std::string_view text) {
    std::string out;
    for (const auto ch : text) {
        switch (ch) {
        case '\\': out += "\\\\"; break;
        case '\t': out += "\\t"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        default: out += ch;
        }
    }
    return out;
}
std::string WireHex(std::uint32_t value) {
    std::array<char, 8> buffer;
    const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value, 16);
    return {buffer.data(), result.ptr};
}
bool ParseCollectorRequest(std::string_view wire, std::string_view header, CollectorRequest& out) {
    std::vector<std::string_view> lines;
    if (!detail::SplitWireLines(wire, lines) || lines.size() < 3 || lines.front() != header) return false;
    CollectorRequest parsed;
    bool version = false, epoch = false;
    std::vector<std::string> fields;
    for (std::size_t i = 1; i < lines.size(); ++i) {
        if (!detail::SplitWireLine(lines[i], fields) || fields.empty()) return false;
        if (fields[0] == "version" && !version && fields.size() == 2 && fields[1] == "1") version = true;
        else if (fields[0] == "epoch" && !epoch && fields.size() == 4) {
            std::int64_t values[3]{};
            for (int n = 0; n < 3; ++n)
                if (!detail::ParseInt64(fields[n+1], values[n]) || values[n] < 0) return false;
            parsed.epoch = {static_cast<std::uint64_t>(values[0]), static_cast<std::uint64_t>(values[1]), static_cast<std::uint64_t>(values[2])};
            epoch = true;
        } else if (fields[0] == "actor" && fields.size() == 3 && !fields[1].empty()) {
            StableFormKey key{fields[1], 0};
            if (!detail::ParseUint32(fields[2], key.localFormId) || key.localFormId == 0 || key.localFormId > 0xffffff || parsed.actors.size() >= kMaxExternalActors || std::find(parsed.actors.begin(), parsed.actors.end(), key) != parsed.actors.end()) return false;
            parsed.actors.push_back(std::move(key));
        } else return false;
    }
    if (!version || !epoch) return false;
    out = std::move(parsed);
    return true;
}
}
