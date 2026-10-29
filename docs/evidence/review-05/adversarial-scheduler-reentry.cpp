#include "slsf/core/time/Scheduler.h"

#include <cstdio>
#include <memory>

struct ReenterOnDestroy {
    slsf::time::Scheduler* scheduler{};
    explicit ReenterOnDestroy(slsf::time::Scheduler* value) : scheduler(value) {}
    ~ReenterOnDestroy() {
        std::fputs("capture_destructor=enter\n", stderr);
        std::fflush(stderr);
        (void)scheduler->PendingCount();
        std::fputs("capture_destructor=returned\n", stderr);
        std::fflush(stderr);
    }
};

int main() {
    slsf::time::Scheduler scheduler;
    auto owner = std::make_shared<ReenterOnDestroy>(&scheduler);
    auto token = scheduler.Schedule(slsf::time::ClockDomain::Real, 1.0,
                                    [capture = owner] {});
    owner.reset();
    return scheduler.Cancel(token) ? 0 : 1;
}
