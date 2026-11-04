#include "pch.h"
#include "slsf/boundary/EngineVm.h"
#include "slsf/engine/RecordLookup.h"
#include <RE/I/IFunctionArguments.h>
#include <RE/I/IStackCallbackFunctor.h>
#include <RE/P/PackUnpack.h>
#undef GetObject

namespace slsf::boundary {
namespace {
class Arguments final : public RE::BSScript::IFunctionArguments {
public:
    explicit Arguments(std::vector<VmValue> values) : values_(std::move(values)) {}
    bool operator()(RE::BSScrapArray<RE::BSScript::Variable>& target) const override {
        target.resize(values_.size());
        for (std::size_t i = 0; i < values_.size(); ++i) {
            const auto& value = values_[i];
            auto& out = target[i];
            switch (value.type) {
            case VmValueType::Bool: out.SetBool(value.boolValue); break;
            case VmValueType::Int: out.SetSInt(static_cast<std::int32_t>(value.intValue)); break;
            case VmValueType::Float: out.SetFloat(static_cast<float>(value.floatValue)); break;
            case VmValueType::String: out.SetString(value.stringValue); break;
            case VmValueType::Form:
                out.Pack(engine::LookupLoadedRecord<RE::TESForm>(RE::TESDataHandler::GetSingleton(), value.formValue));
                break;
            default: out.SetNone(); break;
            }
        }
        return true;
    }
private:
    std::vector<VmValue> values_;
};

class Completion final : public RE::BSScript::IStackCallbackFunctor {
public:
    Completion(Epoch epoch, EngineVm::Probe probe, VmCompletion done)
        : epoch_(epoch), probe_(std::move(probe)), done_(std::move(done)) {}
    void operator()(RE::BSScript::Variable result) override {
        VmResult out;
        out.epoch = epoch_;
        out.status = probe_(epoch_) ? Status::Ready : Status::Stale;
        if (out.status == Status::Ready) {
            VmValue value;
            if (result.IsString()) { value.type = VmValueType::String; value.stringValue = result.GetString(); }
            else if (result.IsBool()) { value.type = VmValueType::Bool; value.boolValue = result.GetBool(); }
            else if (result.IsInt()) { value.type = VmValueType::Int; value.intValue = result.GetSInt(); }
            else if (result.IsFloat()) { value.type = VmValueType::Float; value.floatValue = result.GetFloat(); }
            out.values.push_back(std::move(value));
        }
        if (auto* tasks = SKSE::GetTaskInterface()) tasks->AddTask([done = done_, probe = probe_, out = std::move(out)]() mutable {
            if (done && probe(out.epoch)) done(std::move(out));
        });
    }
    void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}
private:
    Epoch epoch_;
    EngineVm::Probe probe_;
    VmCompletion done_;
};
}

RequestId EngineVm::Submit(VmRequest request, VmCompletion completion) {
    const RequestId id{++next_};
    auto* tasks = SKSE::GetTaskInterface();
    if (!tasks) return {};
    tasks->AddTask([request = std::move(request), completion = std::move(completion), probe = probe_]() mutable {
        auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        if (!vm || !probe(request.epoch)) {
            if (completion) completion({request.epoch, Status::Stale, {}, "VM epoch closed"});
            return;
        }
        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback{
            new Completion(request.epoch, probe, completion)};
        bool sent{};
        auto* args = new Arguments(std::move(request.arguments));
        if (request.target == VmTargetKind::Global) {
            sent = vm->DispatchStaticCall(request.script, request.function, args, callback);
        } else if (auto* form = engine::LookupLoadedRecord<RE::TESForm>(RE::TESDataHandler::GetSingleton(), request.instance)) {
            auto* policy = vm->GetObjectHandlePolicy();
            const auto handle = policy->GetHandleForObject(static_cast<RE::VMTypeID>(form->GetFormType()), form);
            sent = vm->DispatchMethodCall(handle, request.script, request.function, args, callback);
        } else {
            delete args;
        }
        if (!sent && completion) completion({request.epoch, Status::NotReady, {}, "VM dispatch failed"});
    });
    return id;
}
}
