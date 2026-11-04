#pragma once

#include "slsf/compat/ModEventBridge.h"
#include "slsf/core/Contracts.h"
#include "slsf/core/Types.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace RE {
namespace BSScript {
namespace Internal {
class VirtualMachine;
}
}
}

namespace slsf {
namespace api {
class PublicApi;
}
namespace compat {

class ModEventBridge;

class VmNatives {
public:
    using ApiProvider = std::function<slsf::api::PublicApi*()>;

    static void BindApiProvider(ApiProvider provider);
    static void BindBridge(ModEventBridge* bridge);
    static void BindEpochProbe(std::function<Epoch()> current,
                               std::function<bool(Epoch)> valid);

    static bool RegisterPapyrusNatives(RE::BSScript::Internal::VirtualMachine* vm);

    static void DeliverReply(const std::string& eventName,
                             const std::vector<ModEventValue>& values);
    static void DeliverReplyNow(const std::string& eventName,
                                const std::vector<ModEventValue>& values);
    static void KickFrozenProbe();
    static void PublishOutputs();
    static slsf::api::PublicApi* Api();
    static ModEventBridge* Bridge();

    struct Counters {
        std::uint64_t nativeCalls{};
        std::uint64_t taskletCalls{};
        std::uint64_t sinkEvents{};
        std::uint64_t repliesScheduled{};
        std::uint64_t repliesSent{};
        std::uint64_t implFallbacks{};
    };
    static const Counters& Stats() noexcept;
    static void ResetStats() noexcept;
};

}
}
