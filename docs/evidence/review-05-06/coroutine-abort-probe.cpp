#include "slsf/runtime/Dispatcher.h"
#include <stdexcept>
#include <utility>

int main() {
    slsf::runtime::Dispatcher dispatcher;
    slsf::State state{};
    if (!dispatcher.Attach(slsf::Epoch{}, {}, std::move(state))) return 2;
    auto flow = dispatcher.SpawnFlow([&dispatcher]() -> slsf::runtime::Dispatcher::Flow {
        slsf::runtime::Dispatcher::Delay wait{dispatcher, 0.0, slsf::runtime::ClockDomain::Active};
        co_await wait;
        throw std::runtime_error("probe coroutine exception");
    });
    dispatcher.Step();
    return flow.Done() ? 0 : 3;
}