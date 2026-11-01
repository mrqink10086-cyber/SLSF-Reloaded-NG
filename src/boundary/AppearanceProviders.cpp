#include "pch.h"
#include "slsf/boundary/AppearanceProviders.h"

#include <atomic>
#include <optional>
#include <utility>
#include <vector>

namespace slsf::boundary {
namespace {
using namespace integrations;

struct JcRoot {
    std::uint32_t version;
    const void* (*query)(std::uint32_t);
};
struct JcReflection {
    std::uint32_t version;
    void* (*function)(const char*, const char*);
};
struct JcDomains {
    std::uint32_t version;
    void* (*defaultDomain)();
    void* (*namedDomain)(const char*);
};

class SkeePort final : public RaceMenuExchangePort {
public:
    void* Exchange() noexcept override {
        struct ExchangeMessage { void* map{}; } message;
        auto* messaging = SKSE::GetMessagingInterface();
        if (!messaging || !messaging->Dispatch(kSkeeInterfaceExchangeMessage, &message,
                                               sizeof(message), "skee")) return nullptr;
        return message.map;
    }
};

class JcPort final : public SlaveTatsPort {
public:
    void Initialize(const JcRoot* root) {
        if (ready_ || !root || root->version != 1 || !root->query) return;
        const auto* reflection = static_cast<const JcReflection*>(root->query(1));
        const auto* domains = static_cast<const JcDomains*>(root->query(2));
        if (!reflection || reflection->version != 1 || !reflection->function ||
            !domains || domains->version != 1 || !domains->defaultDomain) return;
        domain_ = domains->defaultDomain();
        if (!domain_) return;
        Bind(count_, *reflection, "count", "JArray");
        Bind(object_, *reflection, "object", "JArray");
        Bind(getObject_, *reflection, "getObj", "JArray");
        Bind(getString_, *reflection, "getStr", "JMap");
        Bind(getInt_, *reflection, "getInt", "JMap");
        Bind(retain_, *reflection, "retain", "JValue");
        Bind(release_, *reflection, "release", "JValue");
        if (!count_ || !object_ || !getObject_ || !getString_ || !getInt_ || !retain_ || !release_) return;
        table_.domain = this;
        table_.arrayCount = [](void* context, std::int32_t array) {
            auto& self = *static_cast<JcPort*>(context);
            return self.count_(self.domain_, array);
        };
        table_.arrayObject = [](void* context) {
            auto& self = *static_cast<JcPort*>(context);
            const auto object = self.object_(self.domain_);
            return object ? self.retain_(self.domain_, object, RE::BSFixedString{}) : 0;
        };
        table_.arrayGetObj = [](void* context, std::int32_t array, std::int32_t index, std::int32_t fallback) {
            auto& self = *static_cast<JcPort*>(context);
            return self.getObject_(self.domain_, array, index, fallback);
        };
        table_.mapGetStr = [](void* context, std::int32_t object, JcString key, JcString fallback) {
            auto& self = *static_cast<JcPort*>(context);
            auto value = self.getString_(self.domain_, object, self.String(key), self.String(fallback));
            self.results_.push_back(std::move(value));
            return JcString{kResultBit | self.results_.size()};
        };
        table_.mapGetInt = [](void* context, std::int32_t object, JcString key, std::int32_t fallback) {
            auto& self = *static_cast<JcPort*>(context);
            return self.getInt_(self.domain_, object, self.String(key), fallback);
        };
        table_.release = [](void* context, std::int32_t object) {
            auto& self = *static_cast<JcPort*>(context);
            return self.release_(self.domain_, object);
        };
        ready_ = true;
    }

    const JcTable* Table() noexcept override { return ready_ ? &table_ : nullptr; }
    JcString Intern(const char* text) noexcept override {
        for (std::size_t index = 0; index < keys_.size(); ++index) {
            if (keys_[index] == text) return JcString{index + 1};
        }
        keys_.emplace_back(text);
        return JcString{keys_.size()};
    }
    std::string Text(JcString value) noexcept override { return String(value).c_str(); }
    void* ResolveActor(std::uint32_t formId) noexcept override {
        return RE::TESForm::LookupByID<RE::Actor>(formId);
    }
    void ClearResults() { results_.clear(); }

private:
    template <class Function>
    static void Bind(Function& target, const JcReflection& reflection, const char* name, const char* type) {
        target = reinterpret_cast<Function>(reflection.function(name, type));
    }
    const RE::BSFixedString& String(JcString value) const {
        if (value.opaque & kResultBit) return results_.at((value.opaque & ~kResultBit) - 1);
        return keys_.at(value.opaque - 1);
    }
    static constexpr std::uint64_t kResultBit = std::uint64_t{1} << 63;
    void* domain_{};
    bool ready_{};
    JcTable table_{};
    std::vector<RE::BSFixedString> keys_;
    std::vector<RE::BSFixedString> results_;
    std::int32_t (*count_)(void*, std::int32_t){};
    std::int32_t (*object_)(void*){};
    std::int32_t (*getObject_)(void*, std::int32_t, std::int32_t, std::int32_t){};
    RE::BSFixedString (*getString_)(void*, std::int32_t, RE::BSFixedString, RE::BSFixedString){};
    std::int32_t (*getInt_)(void*, std::int32_t, RE::BSFixedString, std::int32_t){};
    std::int32_t (*retain_)(void*, std::int32_t, RE::BSFixedString){};
    std::int32_t (*release_)(void*, std::int32_t){};
};

struct Providers {
    SkeePort skee;
    JcPort jc;
    RaceMenu raceMenu{skee};
    SlaveTats slaveTats{jc};
    std::optional<Epoch> epoch;
    std::atomic<const JcRoot*> root{};
};

Providers& State() {
    static Providers state;
    return state;
}

void JcMessage(SKSE::MessagingInterface::Message* message) {
    if (!message || message->type != 1 || !message->data) return;
    const auto* root = static_cast<const JcRoot*>(message->data);
    if (root->version == 1) State().root.store(root);
}

void SlaveTatsMessage(SKSE::MessagingInterface::Message* message) {
    if (!message) return;
    std::ignore = State().slaveTats.ConsumeInterfaceMessage(message->type, message->data, message->dataLen);
}

void EnterEpoch(Providers& state, const Epoch& epoch) {
    if (state.epoch == epoch) return;
    state.raceMenu.OnSessionChanged(epoch);
    state.slaveTats.OnSessionChanged(epoch);
    state.epoch = epoch;
}
}

void SubscribeAppearanceProviders() {
    auto* messaging = SKSE::GetMessagingInterface();
    if (!messaging) return;
    const bool jc = messaging->RegisterListener("JContainers64", JcMessage);
    const bool tats = messaging->RegisterListener("SlaveTatsNG", SlaveTatsMessage);
    logs::info("appearance: PostLoad subscriptions JContainers64={} SlaveTatsNG={}", jc, tats);
}

void InitializeAppearanceProviders() {
    auto& state = State();
    state.jc.Initialize(state.root.load());
    logs::info("appearance: JC bound={} SlaveTats capability={}", state.jc.Table() != nullptr,
               static_cast<int>(state.slaveTats.Capability().state));
}

RaceMenuSnapshot QueryRaceMenu(const Epoch& epoch) {
    auto& state = State();
    EnterEpoch(state, epoch);
    state.raceMenu.MarkDirty(epoch);
    const auto result = state.raceMenu.Ensure(epoch);
    auto snapshot = state.raceMenu.Snapshot();
    if (result.gate != ExternalGate::Ready) snapshot.availability = ExternalAvailability::Unknown;
    return snapshot;
}

SlaveTatsSnapshot QuerySlaveTats(RE::Actor* actor, const Epoch& epoch) {
    auto& state = State();
    EnterEpoch(state, epoch);
    state.jc.Initialize(state.root.load());
    state.jc.ClearResults();
    const auto result = state.slaveTats.Collect(actor ? actor->GetFormID() : 0, epoch);
    auto snapshot = state.slaveTats.Snapshot();
    state.jc.ClearResults();
    if (result.gate != ExternalGate::Ready) snapshot.availability = ExternalAvailability::Unknown;
    return snapshot;
}
}
