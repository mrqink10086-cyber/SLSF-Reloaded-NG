#include "slsf/runtime/ClockService.h"

#include <cstdio>

int main() {
    slsf::runtime::ClockService clock;
    clock.SetWallSource([&clock] {
        std::fputs("wall_supplier=enter\n", stderr);
        std::fflush(stderr);
        return clock.Real().value;
    });
    (void)clock.Tick();
    return 0;
}
