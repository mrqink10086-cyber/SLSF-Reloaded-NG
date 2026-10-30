#pragma once

#include "slsf/core/Types.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace slsf {

enum class VmValueType : std::uint8_t {
    None = 0,
    Bool,
    Int,
    Float,
    String,
    Form,
};

enum class VmTargetKind : std::uint8_t {
    Global = 0,
    Instance,
};

struct VmValue {
    VmValueType type{VmValueType::None};
    bool boolValue{};
    std::int64_t intValue{};
    double floatValue{};
    std::string stringValue;
    StableFormKey formValue;
};

struct VmRequest {
    Epoch epoch;
    VmTargetKind target{VmTargetKind::Global};
    StableFormKey instance{};
    std::string script;
    std::string function;
    std::vector<VmValue> arguments;
};

struct VmResult {
    Epoch epoch;
    Status status{Status::NotReady};
    std::vector<VmValue> values;
    std::string error;
};

using VmCompletion = std::function<void(VmResult)>;

class IVmPort {
public:
    virtual ~IVmPort() = default;
    virtual RequestId Submit(VmRequest request, VmCompletion completion) = 0;

    virtual void RequestCancellation(RequestId) {}
};

}
