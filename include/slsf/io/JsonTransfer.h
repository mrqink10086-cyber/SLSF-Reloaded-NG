#pragma once

#include "slsf/core/Types.h"

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace slsf::io {

enum class JsonKind : std::uint8_t {
    Null = 0,
    Bool,
    Number,
    String,
    Array,
    Object,
};

class JsonValue;
using JsonPtr = std::shared_ptr<const JsonValue>;

class JsonValue {
public:
    using Member = std::pair<std::string, JsonPtr>;
    using Members = std::vector<Member>;

    [[nodiscard]] static JsonPtr Parse(std::string_view text,
                                       std::size_t maxBytes,
                                       std::string* error = nullptr);

    [[nodiscard]] static std::string Write(const JsonValue& value, int prettyIndent = 2);
    [[nodiscard]] static std::string Write(const JsonPtr& value, int prettyIndent = 2) {
        return value ? Write(*value, prettyIndent) : WriteNull();
    }
    [[nodiscard]] static std::string WriteNull();

    [[nodiscard]] static JsonPtr MakeNull();
    [[nodiscard]] static JsonPtr MakeBool(bool b);
    [[nodiscard]] static JsonPtr MakeNumber(double d);
    [[nodiscard]] static JsonPtr MakeString(std::string_view s);
    [[nodiscard]] static JsonPtr MakeArray();
    [[nodiscard]] static JsonPtr MakeObject();

    [[nodiscard]] JsonKind Kind() const noexcept { return kind_; }
    [[nodiscard]] bool IsNull() const noexcept { return kind_ == JsonKind::Null; }
    [[nodiscard]] bool IsBool() const noexcept { return kind_ == JsonKind::Bool; }
    [[nodiscard]] bool IsNumber() const noexcept { return kind_ == JsonKind::Number; }
    [[nodiscard]] bool IsString() const noexcept { return kind_ == JsonKind::String; }
    [[nodiscard]] bool IsArray() const noexcept { return kind_ == JsonKind::Array; }
    [[nodiscard]] bool IsObject() const noexcept { return kind_ == JsonKind::Object; }

    [[nodiscard]] bool BoolAt(bool missing) const noexcept;
    [[nodiscard]] double NumberAt(double missing) const noexcept;
    [[nodiscard]] std::string StringAt(std::string_view missing) const;
    [[nodiscard]] std::int64_t IntAt(std::int64_t missing) const noexcept;

    [[nodiscard]] std::size_t ArraySize() const noexcept {
        return kind_ == JsonKind::Array ? array_.size() : 0;
    }
    [[nodiscard]] JsonPtr ArrayAt(std::size_t index) const noexcept {
        return kind_ == JsonKind::Array && index < array_.size() ? array_[index] : JsonPtr{};
    }
    void AppendArrayItem(JsonPtr value);

    [[nodiscard]] bool HasMember(std::string_view name) const noexcept;
    [[nodiscard]] JsonPtr MemberAt(std::string_view name) const noexcept;
    void AttachMember(std::string key, JsonPtr value);
    [[nodiscard]] const Members& ObjectMembers() const noexcept { return object_; }

    JsonValue() = default;
    explicit JsonValue(JsonKind kind) : kind_(kind) {}

private:
    JsonKind kind_{JsonKind::Null};
    bool bool_{};
    double number_{};
    std::string string_;
    std::vector<JsonPtr> array_;
    Members object_;
};

class LegacyDocument {
public:
    LegacyDocument() = default;

    [[nodiscard]] static Result<LegacyDocument> FromValue(const JsonPtr& root);
    [[nodiscard]] std::string Write() const { return JsonValue::Write(ToJsonPtr(*this), 0); }

    [[nodiscard]] static JsonPtr ToJsonPtr(const LegacyDocument& doc);

    [[nodiscard]] double GetInt(std::string_view key, double missing) const;
    [[nodiscard]] double GetFloat(std::string_view key, double missing) const;
    [[nodiscard]] std::string GetString(std::string_view key, std::string_view missing) const;
    [[nodiscard]] std::string GetForm(std::string_view key, std::string_view missing) const;
    [[nodiscard]] bool HasKey(std::string_view type, std::string_view key) const;

    void SetInt(std::string_view key, double value);
    void SetFloat(std::string_view key, double value);
    void SetString(std::string_view key, std::string_view value);
    void SetForm(std::string_view key, std::string_view value);

private:
    [[nodiscard]] JsonPtr Group(std::string_view type) const;
    JsonValue& EnsureGroup(std::string_view type);

    struct MutableGroup {
        std::string name;
        JsonPtr values;
    };

    JsonPtr root_;
    std::vector<MutableGroup> mutables_;
};

struct LegacyImport {
    struct FameEntry {
        std::string location;
        FameCategory category{FameCategory::Count};
        int value{0};
    };

    struct SpreadEntry {
        std::string location;
        int chance{0};
    };

    struct ModFlags {
        std::string mod;
        std::map<std::string, bool> flags;
    };

    std::vector<FameEntry> fame;
    std::vector<SpreadEntry> spread;
    std::vector<std::string> mods;
    std::vector<ModFlags> modFlags;
};

[[nodiscard]] Result<LegacyImport> ExtractLegacyImport(const JsonPtr& root,
                                                       const std::vector<std::string>& flagNames);

    std::string ToLowerAscii(std::string_view text);

} // namespace slsf::io
