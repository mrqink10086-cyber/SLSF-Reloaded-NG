#include "slsf/runtime/Dispatcher.h"

#include <cstdio>

int main() {
    slsf::runtime::Dispatcher dispatcher;
    if (!dispatcher.Attach(slsf::Epoch{1, 1, 1}, slsf::runtime::RuntimeBindings{},
                           slsf::State{})) {
        return 2;
    }

    bool firstRan = false;
    bool staleRan = false;
    dispatcher.Schedule(0.0, slsf::runtime::ClockDomain::Real, [&] {
        firstRan = true;
        dispatcher.Detach();
        if (!dispatcher.Attach(slsf::Epoch{2, 1, 1}, slsf::runtime::RuntimeBindings{},
                               slsf::State{})) {
            std::fprintf(stderr, "reattach failed\n");
        }
    });
    dispatcher.Schedule(0.0, slsf::runtime::ClockDomain::Real, [&] {
        staleRan = dispatcher.CurrentEpoch().session == 2;
    });

    dispatcher.Step();
    std::printf("first_ran=%d stale_callback_ran_in_new_session=%d\n", firstRan, staleRan);
    return firstRan && staleRan ? 0 : 1;
}
