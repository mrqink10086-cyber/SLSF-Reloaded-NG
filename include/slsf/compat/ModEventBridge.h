#pragma once

#include "slsf/core/Types.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace slsf::compat {

enum class ModEventType : std::uint8_t {
    None = 0,
    Bool,
    Int,
    Float,
    String,
    Form,
};

const char* ModEventTypeToString(ModEventType type) noexcept;

struct ModEventValue {
    ModEventType type = ModEventType::None;
    bool boolValue = false;
    std::int64_t intValue = 0;
    double floatValue = 0.0;
    std::string stringValue;
    StableFormKey formValue;
    bool hasForm = false;
    std::vector<std::string> stringArray;
    std::vector<StableFormKey> formArray;
    std::uint64_t scriptHandle{};
    std::string scriptName;
};

struct ModEvent {
    std::string eventName;
    std::string handler;
    std::vector<std::string> parameterNames;
    std::vector<ModEventValue> values;

    const ModEventValue* Find(const std::string& name) const;
    bool BoolValue(const std::string& name, bool fallback = false) const;
    std::int64_t IntValue(const std::string& name, std::int64_t fallback = 0) const;
    double FloatValue(const std::string& name, double fallback = 0.0) const;
    std::string StringValue(const std::string& name,
                            const std::string& fallback = "") const;
    const StableFormKey* FormValue(const std::string& name) const;
};

struct ModEventParameter {
    std::string name;
    ModEventType type = ModEventType::None;
    std::optional<ModEventValue> defaultValue;
};

struct ModEventContract {
    std::string event;
    std::string handler;
    std::vector<ModEventParameter> parameters;
};

class ModEventBridge {
public:
    using EventHandler = std::function<void(const ModEvent&)>;
    using ReplySink =
        std::function<void(const std::string&, const std::vector<ModEventValue>&)>;

    static const std::vector<ModEventContract>& InputContracts();
    static const std::vector<ModEventContract>& ReplyContracts();
    static std::size_t InputCount() noexcept;
    static std::size_t ReplyCount() noexcept;

    const ModEventContract* FindInput(const std::string& eventName) const noexcept;
    const ModEventContract* FindReply(const std::string& eventName) const noexcept;

    Result<ModEvent> ParseInput(const std::string& eventName,
                                const std::vector<ModEventValue>& values) const;

    Result<ModEvent> ParseReply(const std::string& eventName,
                                const std::vector<ModEventValue>& values) const;

    bool ValidateReply(const ModEvent& reply) const;

    bool RegisterHandler(const std::string& eventName, EventHandler handler);

    Result<ModEvent> Dispatch(const std::string& eventName,
                              const std::vector<ModEventValue>& values);

    void SetReplySink(ReplySink sink);
    bool SendReply(const ModEvent& reply);

    std::size_t HandlerCount() const noexcept;

private:
    mutable std::mutex handlersMutex_;
    std::unordered_map<std::string, EventHandler> handlers_;
    mutable std::mutex sinkMutex_;
    ReplySink replySink_;
};

}
