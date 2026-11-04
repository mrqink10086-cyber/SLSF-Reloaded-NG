#include "slsf/io/JsonTransfer.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <limits>

namespace slsf::io {
namespace {

constexpr std::size_t kJsonMaxDepth = 64;

void SetError(std::string* error, std::string_view message) {
    if (error != nullptr) {
        error->assign(message);
    }
}

std::string ByteMessage(std::string_view message, std::size_t offset) {
    std::string out;
    out.reserve(message.size() + 16);
    out.append(message);
    out.append(" (at byte ");
    out.append(std::to_string(offset));
    out.push_back(')');
    return out;
}

class Parser {
public:
    explicit Parser(std::string_view text) : text_(text) {}

    [[nodiscard]] JsonPtr Parse(std::string* error) {
        SkipWhitespace();
        JsonPtr root = ParseValue(error);
        if (root != nullptr) {
            SkipWhitespace();
            if (cursor_ < text_.size()) {
                FailAt(error, "expected end of document");
                return nullptr;
            }
        }
        return root;
    }

    [[nodiscard]] std::size_t FailureOffset() const noexcept { return failureOffset_; }

private:
    static void ContextError(std::string* error, const std::string& context) {
        if (error != nullptr) {
            error->append(" in " + context);
        }
    }

    void FailAt(std::string* error, std::string_view message) {
        if (error != nullptr && !error->empty()) {
            return;
        }
        failureOffset_ = cursor_;
        SetError(error, message);
    }

    void SkipWhitespace() {
        while (cursor_ < text_.size()) {
            const char c = text_[cursor_];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                ++cursor_;
            } else {
                break;
            }
        }
    }

    bool Consume(char expected) {
        if (cursor_ < text_.size() && text_[cursor_] == expected) {
            ++cursor_;
            return true;
        }
        return false;
    }

    bool Literal(std::string_view word) {
        if (text_.size() - cursor_ < word.size()) {
            return false;
        }
        if (text_.compare(cursor_, word.size(), word) != 0) {
            return false;
        }
        cursor_ += word.size();
        return true;
    }

    static std::string DescribeByte(char c) {
        char buffer[32];
        const auto result =
            std::to_chars(buffer, buffer + sizeof(buffer), static_cast<unsigned char>(c));
        if (c >= 32 && c < 127) {
            return std::string("unexpected '") + c + "'";
        }
        return std::string("unexpected byte ") + std::string(buffer, result.ptr);
    }

    [[nodiscard]] bool ReserveDepth(std::string* error, std::string_view exhausted) {
        if (depth_ >= kJsonMaxDepth) {
            FailAt(error, exhausted);
            return false;
        }
        ++depth_;
        return true;
    }

    void ReleaseDepth() noexcept { --depth_; }

    JsonPtr ParseValue(std::string* error) {
        if (cursor_ >= text_.size()) {
            FailAt(error, "unexpected end of document");
            return nullptr;
        }
        const char c = text_[cursor_];
        switch (c) {
        case '{':
            return ParseObject(error);
        case '[':
            return ParseArray(error);
        case '"':
            return ParseString(error);
        case 't':
            if (Literal("true")) return JsonValue::MakeBool(true);
            FailAt(error, "invalid literal");
            return nullptr;
        case 'f':
            if (Literal("false")) return JsonValue::MakeBool(false);
            FailAt(error, "invalid literal");
            return nullptr;
        case 'n':
            if (Literal("null")) return JsonValue::MakeNull();
            FailAt(error, "invalid literal");
            return nullptr;
        default:
            break;
        }
        if (c == '-' || (c >= '0' && c <= '9')) {
            return ParseNumber(error);
        }
        FailAt(error, DescribeByte(c));
        return nullptr;
    }

    JsonPtr ParseObject(std::string* error) {
        if (!ReserveDepth(error, "object nesting too deep")) {
            return nullptr;
        }
        ++cursor_;
        auto value = std::make_shared<JsonValue>(JsonKind::Object);
        SkipWhitespace();
        if (Consume('}')) {
            ReleaseDepth();
            return value;
        }
        while (cursor_ < text_.size()) {
            if (text_[cursor_] != '"') {
                FailAt(error, "expected object key");
                ReleaseDepth();
                return nullptr;
            }
            JsonPtr key = ParseString(error);
            if (key == nullptr) {
                ContextError(error, "object key");
                ReleaseDepth();
                return nullptr;
            }
            SkipWhitespace();
            if (!Consume(':')) {
                FailAt(error, "expected ':' after object key");
                ReleaseDepth();
                return nullptr;
            }
            SkipWhitespace();
            JsonPtr member = ParseValue(error);
            if (member == nullptr) {
                ContextError(error, "member '" + key->StringAt("") + "'");
                ReleaseDepth();
                return nullptr;
            }
            value->AttachMember(key->StringAt(""), std::move(member));
            SkipWhitespace();
            if (Consume(',')) {
                SkipWhitespace();
                continue;
            }
            if (Consume('}')) {
                ReleaseDepth();
                return value;
            }
            FailAt(error, "expected ',' or '}' in object");
            ReleaseDepth();
            return nullptr;
        }
        FailAt(error, "unterminated object");
        ReleaseDepth();
        return nullptr;
    }

    JsonPtr ParseArray(std::string* error) {
        if (!ReserveDepth(error, "array nesting too deep")) {
            return nullptr;
        }
        ++cursor_;
        auto value = std::make_shared<JsonValue>(JsonKind::Array);
        SkipWhitespace();
        if (Consume(']')) {
            ReleaseDepth();
            return value;
        }
        while (cursor_ > 0 && cursor_ < text_.size()) {
            JsonPtr member = ParseValue(error);
            if (member == nullptr) {
                ContextError(error, "array element " + std::to_string(value->ArraySize()));
                ReleaseDepth();
                return nullptr;
            }
            value->AppendArrayItem(std::move(member));
            SkipWhitespace();
            if (Consume(',')) {
                SkipWhitespace();
                continue;
            }
            if (Consume(']')) {
                ReleaseDepth();
                return value;
            }
            FailAt(error, "expected ',' or ']' in array");
            ReleaseDepth();
            return nullptr;
        }
        FailAt(error, "unterminated array");
        ReleaseDepth();
        return nullptr;
    }

    JsonPtr ParseString(std::string* error) {
        ++cursor_;
        std::string decoded;
        while (cursor_ < text_.size()) {
            const char c = text_[cursor_];
            if (static_cast<unsigned char>(c) < 0x20) {
                FailAt(error, "unescaped control character in string");
                return nullptr;
            }
            if (c == '"') {
                ++cursor_;
                return JsonValue::MakeString(decoded);
            }
            if (c != '\\') {
                decoded.push_back(c);
                ++cursor_;
                continue;
            }
            ++cursor_;
            if (cursor_ >= text_.size()) {
                FailAt(error, "truncated escape sequence");
                return nullptr;
            }
            const char escape = text_[cursor_++];
            switch (escape) {
            case '"': decoded.push_back('"'); break;
            case '\\': decoded.push_back('\\'); break;
            case '/': decoded.push_back('/'); break;
            case 'b': decoded.push_back('\b'); break;
            case 'f': decoded.push_back('\f'); break;
            case 'n': decoded.push_back('\n'); break;
            case 'r': decoded.push_back('\r'); break;
            case 't': decoded.push_back('\t'); break;
            case 'u':
                if (!DecodeUnicodeEscape(decoded, error)) {
                    return nullptr;
                }
                break;
            default:
                FailAt(error, "invalid escape sequence");
                return nullptr;
            }
        }
        FailAt(error, "unterminated string");
        return nullptr;
    }

    [[nodiscard]] bool AppendCodePoint(std::string& out, std::uint32_t code) {
        if (code < 0x80u) {
            out.push_back(static_cast<char>(code));
            return true;
        }
        if (code < 0x800u) {
            out.push_back(static_cast<char>(0xC0u | (code >> 6)));
            out.push_back(static_cast<char>(0x80u | (code & 0x3Fu)));
            return true;
        }
        if (code <= 0xFFFFu) {
            out.push_back(static_cast<char>(0xE0u | (code >> 12)));
            out.push_back(static_cast<char>(0x80u | ((code >> 6) & 0x3Fu)));
            out.push_back(static_cast<char>(0x80u | (code & 0x3Fu)));
            return true;
        }
        out.push_back(static_cast<char>(0xF0u | (code >> 18)));
        out.push_back(static_cast<char>(0x80u | ((code >> 12) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | ((code >> 6) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | (code & 0x3Fu)));
        return true;
    }

    [[nodiscard]] bool ReadHex4(std::uint32_t& value) {
        if (text_.size() - cursor_ < 4) {
            return false;
        }
        value = 0;
        for (std::size_t index = 0; index < 4; ++index) {
            const char digit = text_[cursor_ + index];
            std::uint32_t nibble;
            if (digit >= '0' && digit <= '9') {
                nibble = static_cast<std::uint32_t>(digit - '0');
            } else if (digit >= 'a' && digit <= 'f') {
                nibble = static_cast<std::uint32_t>(digit - 'a' + 10);
            } else if (digit >= 'A' && digit <= 'F') {
                nibble = static_cast<std::uint32_t>(digit - 'A' + 10);
            } else {
                return false;
            }
            value = (value << 4) | nibble;
        }
        cursor_ += 4;
        return true;
    }

    [[nodiscard]] bool DecodeUnicodeEscape(std::string& out, std::string* error) {
        std::uint32_t code = 0;
        if (!ReadHex4(code)) {
            FailAt(error, "invalid \\u escape");
            return false;
        }
        if (code >= 0xD800u && code <= 0xDBFFu) {
            if (text_.size() - cursor_ < 6 || text_[cursor_] != '\\' || text_[cursor_ + 1] != 'u') {
                FailAt(error, "high surrogate without low surrogate");
                return false;
            }
            const std::size_t save = cursor_;
            cursor_ += 2;
            std::uint32_t low = 0;
            if (!ReadHex4(low) || low < 0xDC00u || low > 0xDFFFu) {
                cursor_ = save;
                FailAt(error, "lone high surrogate");
                return false;
            }
            code = 0x10000u + ((code - 0xD800u) << 10) + (low - 0xDC00u);
        } else if (code >= 0xDC00u && code <= 0xDFFFu) {
            FailAt(error, "lone low surrogate");
            return false;
        }
        return AppendCodePoint(out, code);
    }

    JsonPtr ParseNumber(std::string* error) {
        const std::size_t begin = cursor_;
        Consume('-');
        if (cursor_ >= text_.size() || text_[cursor_] < '0' || text_[cursor_] > '9') {
            FailAt(error, "invalid number");
            return nullptr;
        }
        if (text_[cursor_] == '0') {
            ++cursor_;
        } else {
            while (cursor_ < text_.size() && text_[cursor_] >= '0' && text_[cursor_] <= '9') {
                ++cursor_;
            }
        }
        if (Consume('.')) {
            if (cursor_ >= text_.size() || text_[cursor_] < '0' || text_[cursor_] > '9') {
                FailAt(error, "missing fraction digits");
                return nullptr;
            }
            while (cursor_ < text_.size() && text_[cursor_] >= '0' && text_[cursor_] <= '9') {
                ++cursor_;
            }
        }
        if (cursor_ < text_.size() && (text_[cursor_] == 'e' || text_[cursor_] == 'E')) {
            ++cursor_;
            if (cursor_ < text_.size() && (text_[cursor_] == '+' || text_[cursor_] == '-')) {
                ++cursor_;
            }
            if (cursor_ >= text_.size() || text_[cursor_] < '0' || text_[cursor_] > '9') {
                FailAt(error, "missing exponent digits");
                return nullptr;
            }
            while (cursor_ < text_.size() && text_[cursor_] >= '0' && text_[cursor_] <= '9') {
                ++cursor_;
            }
        }
        std::string numberText(text_.substr(begin, cursor_ - begin));
        double value = 0;
        const auto result = std::from_chars(numberText.data(),
                                            numberText.data() + numberText.size(), value);
        if (result.ec == std::errc() && result.ptr == numberText.data() + numberText.size()) {
            if (!std::isfinite(value)) {
                FailAt(error, "number out of range");
                return nullptr;
            }
            return JsonValue::MakeNumber(value);
        }
        FailAt(error, "invalid number");
        return nullptr;
    }

    std::string_view text_;
    std::size_t cursor_{0};
    std::size_t depth_{0};
    std::size_t failureOffset_{0};
};

class Writer {
public:
    explicit Writer(int indent) : indent_(indent) {}

    void Write(const JsonValue& value) {
        switch (value.Kind()) {
        case JsonKind::Null:
            out_.append("null");
            break;
        case JsonKind::Bool:
            out_.append(value.BoolAt(false) ? "true" : "false");
            break;
        case JsonKind::Number:
            out_.append(NumberText(value.NumberAt(0.0)));
            break;
        case JsonKind::String:
            WriteString(value.StringAt(""));
            break;
        case JsonKind::Array:
            WriteArray(value);
            break;
        case JsonKind::Object:
            WriteObject(value);
            break;
        }
    }

    void WriteChild(const JsonPtr& value) {
        if (value == nullptr) {
            out_.append("null");
            return;
        }
        Write(*value);
    }

    [[nodiscard]] std::string Take() { return std::move(out_); }

private:
    void Newline() {
        out_.push_back('\n');
        out_.append(static_cast<std::size_t>(level_ * indent_), ' ');
    }

    void WriteArray(const JsonValue& value) {
        if (value.ArraySize() == 0) {
            out_.append("[]");
            return;
        }
        out_.push_back('[');
        const bool pretty = indent_ > 0;
        ++level_;
        const std::size_t count = value.ArraySize();
        for (std::size_t index = 0; index < count; ++index) {
            if (pretty) {
                Newline();
            }
            WriteChild(value.ArrayAt(index));
            if (index + 1 < count) {
                out_.push_back(',');
            }
        }
        --level_;
        if (pretty) {
            Newline();
        }
        out_.push_back(']');
    }

    void WriteObject(const JsonValue& value) {
        const auto& members = value.ObjectMembers();
        if (members.empty()) {
            out_.append("{}");
            return;
        }
        out_.push_back('{');
        const bool pretty = indent_ > 0;
        ++level_;
        const std::size_t count = members.size();
        for (std::size_t index = 0; index < count; ++index) {
            if (pretty) {
                Newline();
            }
            WriteString(members[index].first);
            out_.append(": ");
            WriteChild(members[index].second);
            if (index + 1 < count) {
                out_.push_back(',');
            }
        }
        --level_;
        if (pretty) {
            Newline();
        }
        out_.push_back('}');
    }

    void WriteString(std::string_view text) {
        out_.push_back('"');
        for (const char c : text) {
            switch (c) {
            case '"': out_.append("\\\""); break;
            case '\\': out_.append("\\\\"); break;
            case '\b': out_.append("\\b"); break;
            case '\f': out_.append("\\f"); break;
            case '\n': out_.append("\\n"); break;
            case '\r': out_.append("\\r"); break;
            case '\t': out_.append("\\t"); break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    static constexpr char kDigits[] = "0123456789abcdef";
                    out_.append("\\u00");
                    out_.push_back(kDigits[(static_cast<unsigned char>(c) >> 4) & 0xF]);
                    out_.push_back(kDigits[static_cast<unsigned char>(c) & 0xF]);
                } else {
                    out_.push_back(c);
                }
                break;
            }
        }
        out_.push_back('"');
    }

    [[nodiscard]] static std::string NumberText(double value) {
        char buffer[64];
        const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
        if (result.ec != std::errc()) {
            return "0";
        }
        return std::string(buffer, static_cast<std::size_t>(result.ptr - buffer));
    }

    std::string out_;
    int indent_;
    int level_{0};
};

}

JsonPtr JsonValue::Parse(std::string_view text, std::size_t maxBytes, std::string* error) {
    if (text.size() > maxBytes) {
        SetError(error, ByteMessage("document exceeds size limit", text.size()));
        return nullptr;
    }
    if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF &&
        static_cast<unsigned char>(text[1]) == 0xBB &&
        static_cast<unsigned char>(text[2]) == 0xBF) {
        text.remove_prefix(3);
    }
    std::string message;
    Parser parser(text);
    JsonPtr result = parser.Parse(&message);
    if (result == nullptr) {
        SetError(error, ByteMessage(message.empty() ? "parse failed" : message,
                                    parser.FailureOffset()));
        return nullptr;
    }
    return result;
}

std::string JsonValue::Write(const JsonValue& value, int prettyIndent) {
    Writer writer(prettyIndent);
    writer.Write(value);
    return writer.Take();
}

std::string JsonValue::WriteNull() {
    return "null";
}

JsonPtr JsonValue::MakeNull() {
    return std::make_shared<JsonValue>(JsonKind::Null);
}

JsonPtr JsonValue::MakeBool(bool b) {
    auto value = std::make_shared<JsonValue>(JsonKind::Bool);
    value->bool_ = b;
    return value;
}

JsonPtr JsonValue::MakeNumber(double d) {
    if (!std::isfinite(d)) {
        return nullptr;
    }
    auto value = std::make_shared<JsonValue>(JsonKind::Number);
    value->number_ = d == 0.0 ? 0.0 : d;
    return value;
}

JsonPtr JsonValue::MakeString(std::string_view s) {
    auto value = std::make_shared<JsonValue>(JsonKind::String);
    value->string_.assign(s);
    return value;
}

JsonPtr JsonValue::MakeArray() {
    return std::make_shared<JsonValue>(JsonKind::Array);
}

JsonPtr JsonValue::MakeObject() {
    return std::make_shared<JsonValue>(JsonKind::Object);
}

bool JsonValue::BoolAt(bool missing) const noexcept {
    return kind_ == JsonKind::Bool ? bool_ : missing;
}

double JsonValue::NumberAt(double missing) const noexcept {
    return kind_ == JsonKind::Number ? number_ : missing;
}

std::string JsonValue::StringAt(std::string_view missing) const {
    return kind_ == JsonKind::String ? string_ : std::string(missing);
}

std::int64_t JsonValue::IntAt(std::int64_t missing) const noexcept {
    if (kind_ != JsonKind::Number || !std::isfinite(number_)) {
        return missing;
    }
    if (number_ >= static_cast<double>(std::numeric_limits<std::int64_t>::max())) {
        return std::numeric_limits<std::int64_t>::max();
    }
    if (number_ <= static_cast<double>(std::numeric_limits<std::int64_t>::min())) {
        return std::numeric_limits<std::int64_t>::min();
    }
    return static_cast<std::int64_t>(number_);
}

bool JsonValue::HasMember(std::string_view name) const noexcept {
    return MemberAt(name) != nullptr;
}

void JsonValue::AttachMember(std::string key, JsonPtr value) {
    if (value == nullptr) {
        value = MakeNull();
    }
    for (auto& member : object_) {
        if (member.first == key) {
            member.second = std::move(value);
            return;
        }
    }
    object_.emplace_back(std::move(key), std::move(value));
}

void JsonValue::AppendArrayItem(JsonPtr value) {
    if (value == nullptr) {
        value = MakeNull();
    }
    array_.push_back(std::move(value));
}

JsonPtr JsonValue::MemberAt(std::string_view name) const noexcept {
    if (kind_ != JsonKind::Object) {
        return nullptr;
    }
    for (const auto& member : object_) {
        if (member.first == name) {
            return member.second;
        }
    }
    return nullptr;
}

double LegacyDocument::GetInt(std::string_view key, double missing) const {
    const JsonPtr group = Group("int");
    if (group == nullptr) {
        return missing;
    }
    const JsonPtr value = group->MemberAt(ToLowerAscii(key));
    return value != nullptr ? value->NumberAt(missing) : missing;
}

double LegacyDocument::GetFloat(std::string_view key, double missing) const {
    const JsonPtr group = Group("float");
    if (group == nullptr) {
        return missing;
    }
    const JsonPtr value = group->MemberAt(ToLowerAscii(key));
    return value != nullptr ? value->NumberAt(missing) : missing;
}

std::string LegacyDocument::GetString(std::string_view key, std::string_view missing) const {
    const JsonPtr group = Group("string");
    if (group == nullptr) {
        return std::string(missing);
    }
    const JsonPtr value = group->MemberAt(ToLowerAscii(key));
    return value != nullptr && value->IsString() ? value->StringAt(missing)
                                                 : std::string(missing);
}

std::string LegacyDocument::GetForm(std::string_view key, std::string_view missing) const {
    const JsonPtr group = Group("form");
    if (group == nullptr) {
        return std::string(missing);
    }
    const JsonPtr value = group->MemberAt(ToLowerAscii(key));
    return value != nullptr && value->IsString() ? value->StringAt(missing)
                                                 : std::string(missing);
}

bool LegacyDocument::HasKey(std::string_view type, std::string_view key) const {
    const JsonPtr group = Group(type);
    return group != nullptr && group->HasMember(ToLowerAscii(key));
}

void LegacyDocument::SetInt(std::string_view key, double value) {
    JsonPtr number = JsonValue::MakeNumber(value);
    EnsureGroup("int").AttachMember(ToLowerAscii(key), std::move(number));
}

void LegacyDocument::SetFloat(std::string_view key, double value) {
    JsonPtr number = JsonValue::MakeNumber(value);
    EnsureGroup("float").AttachMember(ToLowerAscii(key), std::move(number));
}

void LegacyDocument::SetString(std::string_view key, std::string_view value) {
    JsonPtr text = JsonValue::MakeString(value);
    EnsureGroup("string").AttachMember(ToLowerAscii(key), std::move(text));
}

void LegacyDocument::SetForm(std::string_view key, std::string_view value) {
    JsonPtr text = JsonValue::MakeString(value);
    EnsureGroup("form").AttachMember(ToLowerAscii(key), std::move(text));
}

JsonPtr LegacyDocument::Group(std::string_view type) const {
    for (const auto& group : mutables_) {
        if (group.name == type) {
            return group.values;
        }
    }
    return root_ != nullptr ? root_->MemberAt(type) : nullptr;
}

JsonValue& LegacyDocument::EnsureGroup(std::string_view type) {
    for (auto& group : mutables_) {
        if (group.name == type) {
            return const_cast<JsonValue&>(*group.values);
        }
    }
    MutableGroup group;
    group.name.assign(type);
    const JsonPtr existing = root_ != nullptr ? root_->MemberAt(type) : nullptr;
    if (existing != nullptr && existing->IsObject()) {
        group.values = std::make_shared<JsonValue>(*existing);
    } else {
        group.values = std::make_shared<JsonValue>(JsonKind::Object);
    }
    mutables_.push_back(std::move(group));
    return const_cast<JsonValue&>(*mutables_.back().values);
}

JsonPtr LegacyDocument::ToJsonPtr(const LegacyDocument& doc) {
    auto merged = std::make_shared<JsonValue>(JsonKind::Object);
    for (const char* group : {"int", "float", "string", "form"}) {
        JsonPtr values = doc.Group(group);
        merged->AttachMember(group, values != nullptr ? values : JsonValue::MakeObject());
    }
    if (doc.root_ != nullptr && doc.root_->IsObject()) {
        for (const auto& member : doc.root_->ObjectMembers()) {
            if (member.first == "int" || member.first == "float" ||
                member.first == "string" || member.first == "form") {
                continue;
            }
            merged->AttachMember(member.first, member.second);
        }
    }
    return merged;
}

Result<LegacyDocument> LegacyDocument::FromValue(const JsonPtr& root) {
    if (root == nullptr || root->IsNull()) {
        return std::unexpected{Error{Error::Code::InvalidArgument, "legacy document is null"}};
    }
    if (!root->IsObject()) {
        return std::unexpected{Error{Error::Code::InvalidArgument,
                                     "legacy document root is not an object"}};
    }
    for (const char* group : {"int", "float", "string", "form"}) {
        const JsonPtr member = root->MemberAt(group);
        if (member != nullptr && !member->IsObject()) {
            return std::unexpected{Error{Error::Code::InvalidArgument,
                                         std::string(group) + " group is not an object"}};
        }
    }
    LegacyDocument doc;
    doc.root_ = root;
    return doc;
}

std::string ToLowerAscii(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (const char c : text) {
        out.push_back(c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c);
    }
    return out;
}

namespace {

[[nodiscard]] int ClampInt(double value) noexcept {
    if (!std::isfinite(value) || value <= -2147483648.0) {
        return -2147483648;
    }
    if (value >= 2147483647.0) {
        return 2147483647;
    }
    return static_cast<int>(std::llround(value));
}

}

Result<LegacyImport> ExtractLegacyImport(const JsonPtr& root,
                                         const std::vector<std::string>& flagNames) {
    if (root == nullptr || !root->IsObject()) {
        return std::unexpected{Error{Error::Code::InvalidArgument,
                                     "legacy document root is not an object"}};
    }
    const JsonPtr intGroup = root->MemberAt("int");
    if (intGroup != nullptr && !intGroup->IsObject()) {
        return std::unexpected{Error{Error::Code::InvalidArgument, "int group is not an object"}};
    }
    const JsonPtr stringGroup = root->MemberAt("string");
    if (stringGroup != nullptr && !stringGroup->IsObject()) {
        return std::unexpected{Error{Error::Code::InvalidArgument,
                                     "string group is not an object"}};
    }

    LegacyImport import;
    if (intGroup != nullptr) {
        constexpr int kMaxImportedEntries = 4096;
        for (const auto& member : intGroup->ObjectMembers()) {
            if (import.fame.size() + import.spread.size() >=
                static_cast<std::size_t>(kMaxImportedEntries)) {
                break;
            }
            const JsonPtr value = member.second;
            if (value == nullptr || !value->IsNumber()) {
                continue;
            }
            const int number = ClampInt(value->NumberAt(0.0));
            const std::size_t dot = member.first.find('.');
            if (dot == std::string::npos || dot == 0 || dot + 1 == member.first.size()) {
                continue;
            }
            const std::string location = member.first.substr(0, dot);
            const std::string field = member.first.substr(dot + 1);
            if (field == "famespreadchance") {
                import.spread.push_back(LegacyImport::SpreadEntry{location, number});
                continue;
            }
            const auto category = ParseFameCategory(field);
            if (category.has_value()) {
                import.fame.push_back(
                    LegacyImport::FameEntry{location, *category, number});
            }
        }
    }

    std::size_t modCount = 0;
    if (intGroup != nullptr) {
        const JsonPtr total = intGroup->MemberAt("modtotal");
        modCount = total != nullptr && total->IsNumber()
                       ? static_cast<std::size_t>(std::clamp<std::int64_t>(
                             total->IntAt(0), 0, static_cast<std::int64_t>(256)))
                       : 0;
    }
    if (modCount != 0 && stringGroup != nullptr) {
        constexpr int kMaxModIndex = 256;
        std::vector<std::optional<std::string>> indexed(modCount);
        for (const auto& member : stringGroup->ObjectMembers()) {
            if (!member.first.starts_with("modindex[") || member.first.back() != ']') {
                continue;
            }
            const std::string_view inside(
                member.first.data() + std::string_view("modindex[").size(),
                member.first.size() - std::string_view("modindex[").size() - 1);
            std::int64_t index = -1;
            const auto [ptr, ec] =
                std::from_chars(inside.data(), inside.data() + inside.size(), index);
            if (ec != std::errc() || ptr != inside.data() + inside.size() ||
                index < 0 || index >= kMaxModIndex ||
                static_cast<std::size_t>(index) >= modCount) {
                continue;
            }
            const JsonPtr value = member.second;
            if (value == nullptr || !value->IsString()) {
                continue;
            }
            const std::string name = value->StringAt("");
            if (name.empty()) {
                continue;
            }
            indexed[static_cast<std::size_t>(index)] = name;
        }
        for (const auto& name : indexed) {
            if (name.has_value()) {
                import.mods.push_back(*name);
            }
        }
        for (const auto& mod : import.mods) {
            LegacyImport::ModFlags entry;
            entry.mod = mod;
            for (const auto& flag : flagNames) {
                const std::string key = ToLowerAscii(mod) + "." + ToLowerAscii(flag);
                const JsonPtr value = intGroup != nullptr ? intGroup->MemberAt(key) : nullptr;
                entry.flags[flag] = value != nullptr && value->IsNumber() &&
                                    value->IntAt(0) != 0;
            }
            import.modFlags.push_back(std::move(entry));
        }
    }
    const auto forms = root->MemberAt("form");
    const auto locationTotal = intGroup ? intGroup->MemberAt("locationtotal") : nullptr;
    const auto locationCount = locationTotal ? locationTotal->IntAt(0) : 0;
    if (locationCount < 0 || locationCount > 65535) {
        return Fail(Error::Code::InvalidArgument, "invalid imported custom location count");
    }
    for (std::int64_t i = 0; i < locationCount; ++i) {
        const auto suffix = std::to_string(i);
        const auto name = stringGroup ? stringGroup->MemberAt("location_string_" + suffix) : nullptr;
        const auto form = forms ? forms->MemberAt("location_form_" + suffix) : nullptr;
        if (!name || !name->IsString() || name->StringAt("").empty() || !form || !form->IsString()) {
            return Fail(Error::Code::InvalidArgument, "incomplete imported custom location");
        }
        const auto identity = form->StringAt("");
        const auto separator = identity.find('|');
        if (separator == std::string::npos || separator + 1 == identity.size()) {
            return Fail(Error::Code::InvalidArgument, "invalid imported location identity");
        }
        auto id = std::string_view(identity).substr(0, separator);
        if (id.starts_with("0x") || id.starts_with("0X")) id.remove_prefix(2);
        std::uint32_t localId{};
        const auto parsed = std::from_chars(id.data(), id.data() + id.size(), localId, 16);
        if (parsed.ec != std::errc{} || parsed.ptr != id.data() + id.size() || localId == 0 || localId > 0x00FFFFFFu) {
            return Fail(Error::Code::InvalidArgument, "invalid imported local form ID");
        }
        import.customRegions.push_back({name->StringAt(""), {identity.substr(separator + 1), localId}});
    }
    return import;
}

}
