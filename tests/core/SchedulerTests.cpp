#include "slsf/core/time/Scheduler.h"

#include "../reference/ReferenceScheduler.h"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <random>
#include <utility>
#include <vector>

namespace {

using slsf::time::ClockDomain;
using slsf::time::Scheduler;
using slsf::time::TimerToken;

} // namespace

TEST_CASE("scheduler same-deadline tasks fire in stable enqueue order") {
    Scheduler scheduler;
    std::vector<unsigned> firedOrder;
    std::vector<TimerToken> tokens;

    for (unsigned i = 0; i < 5; ++i) {
        tokens.push_back(scheduler.Schedule(ClockDomain::Real, 10.0, [&firedOrder, i] {
            firedOrder.push_back(i);
        }));
    }

    // Cancelling the middle task keeps the surviving order stable.
    REQUIRE(scheduler.Cancel(tokens[2]));

    const auto due = scheduler.AcquireDue(ClockDomain::Real, 10.0);
    REQUIRE(due.size() == 4);
    // Extraction hands bodies back; the caller runs them without holding
    // the scheduler lock (same contract as the executor step loop).
    for (const auto& task : due) {
        if (task.body) {
            task.body();
        }
    }
    REQUIRE(firedOrder == std::vector<unsigned>{0, 1, 3, 4});

    // Extraction retired every task even though the bodies were run here.
    REQUIRE(scheduler.PendingCount() == 0);
    REQUIRE(scheduler.PendingCount(ClockDomain::Real) == 0);
}

TEST_CASE("scheduler cancels are idempotent and invalidate stale tokens") {
    Scheduler scheduler;

    const auto token = scheduler.Schedule(ClockDomain::Active, 5.0, {});
    REQUIRE(token.generation != 0);
    REQUIRE(scheduler.Cancel(token));
    REQUIRE_FALSE(scheduler.Cancel(token)); // generation already bumped

    // The dead token never matches a freshly recycled slot again.
    const auto reused = scheduler.Schedule(ClockDomain::Active, 5.0, {});
    REQUIRE(reused.id == token.id); // LIFO slot reuse
    REQUIRE(reused.generation != token.generation);
    REQUIRE_FALSE(scheduler.Cancel(token));

    const auto due = scheduler.AcquireDue(ClockDomain::Active, 5.0);
    REQUIRE(due.size() == 1);
    REQUIRE(due.front().token == reused);
}

TEST_CASE("scheduler domains advance independently") {
    Scheduler scheduler;
    std::size_t gameRuns = 0;
    std::size_t realRuns = 0;
    std::size_t activeRuns = 0;

    std::ignore = scheduler.Schedule(ClockDomain::Game, 10.0, [&] { ++gameRuns; });
    const auto realToken = scheduler.Schedule(ClockDomain::Real, 2.0, [&] { ++realRuns; });
    const auto activeToken = scheduler.Schedule(ClockDomain::Active, 3.0, [&] { ++activeRuns; });

    // A paused domain (menus open, load screen) simply never advances.
    auto realDue = scheduler.AcquireDue(ClockDomain::Real, 100.0);
    REQUIRE(realDue.size() == 1);
    for (auto& task : realDue) {
        task.body();
    }
    REQUIRE(gameRuns == 0);
    REQUIRE(scheduler.AcquireDue(ClockDomain::Active, 2.9).empty()); // not yet due
    auto activeDue = scheduler.AcquireDue(ClockDomain::Active, 3.0);
    REQUIRE(activeDue.size() == 1);
    for (auto& task : activeDue) {
        task.body();
    }
    REQUIRE(scheduler.AcquireDue(ClockDomain::Game, 9.999).empty());
    auto gameDue = scheduler.AcquireDue(ClockDomain::Game, 10.0);
    REQUIRE(gameDue.size() == 1);
    for (auto& task : gameDue) {
        task.body();
    }

    REQUIRE(gameRuns == 1);
    REQUIRE(realRuns == 1);
    REQUIRE(activeRuns == 1);
    REQUIRE(scheduler.PendingCount() == 0);
    REQUIRE(realToken.domain == ClockDomain::Real);
    REQUIRE(activeToken.domain == ClockDomain::Active);
}

TEST_CASE("scheduler session replacement invalidates every pending task") {
    Scheduler scheduler;
    std::size_t runs = 0;

    const std::uint64_t before = scheduler.Session();
    REQUIRE(before == 1);
    std::ignore = scheduler.Schedule(ClockDomain::Game, 5.0, [&] { ++runs; });
    std::ignore = scheduler.Schedule(ClockDomain::Real, 5.0, [&] { ++runs; });
    std::ignore = scheduler.Schedule(ClockDomain::Active, 5.0, [&] { ++runs; });

    const std::uint64_t session = scheduler.NewSession();
    REQUIRE(session == before + 1);
    REQUIRE(scheduler.Session() == session);

    for (const auto domain : {ClockDomain::Game, ClockDomain::Real, ClockDomain::Active}) {
        REQUIRE(scheduler.AcquireDue(domain, 10.0).empty());
        REQUIRE(scheduler.PendingCount(domain) == 0);
    }
    REQUIRE(runs == 0);

    // Tasks scheduled after the replacement are live and cancellable.
    std::ignore = scheduler.Schedule(ClockDomain::Game, 5.0, [&] { ++runs; });
    const auto fresh = scheduler.Schedule(ClockDomain::Game, 5.0, [&] { ++runs; });
    REQUIRE(scheduler.Cancel(fresh));
}

TEST_CASE("scheduler recycles dead heap entries instead of growing forever") {
    Scheduler scheduler;

    for (int round = 0; round < 512; ++round) {
        const auto token = scheduler.Schedule(ClockDomain::Real, static_cast<double>(round), {});
        REQUIRE(scheduler.Cancel(token));
        std::ignore = scheduler.AcquireDue(ClockDomain::Real, static_cast<double>(round) + 0.5);
    }

    // Dead entries surfaced and were recycled; the heap stays bounded.
    REQUIRE(scheduler.HeapSize(ClockDomain::Real) == 0);
    REQUIRE(scheduler.DiscardedCount(ClockDomain::Real) == 512);
    REQUIRE(scheduler.PendingCount() == 0);

    // The recycled structure still schedules and fires correctly.
    std::size_t runs = 0;
    std::ignore = scheduler.Schedule(ClockDomain::Real, 1.0, [&] { ++runs; });
    auto tailDue = scheduler.AcquireDue(ClockDomain::Real, 1.0);
    REQUIRE(tailDue.size() == 1);
    for (auto& task : tailDue) {
        task.body();
    }
    REQUIRE(runs == 1);
}

TEST_CASE("scheduler due set matches reference across randomized interleavings") {
    // Seed 1170 is the project default; extra seeds widen the coverage.
    constexpr std::uint32_t kSeeds[] = {1170, 1171, 2, 3, 7, 19, 4242, 12345};
    constexpr std::size_t kDomains = 3;

    for (const std::uint32_t seed : kSeeds) {
        Scheduler scheduler;
        refsched::ReferenceScheduler reference;
        std::mt19937 rng{seed};

        // Production slots and reference records live in independent index
        // spaces; both map onto the same body id so firing order can be
        // compared after every advance.
        std::vector<TimerToken> prodTokens;
        std::vector<std::pair<std::size_t, std::uint64_t>> refTokens; // (record, gen)
        std::vector<unsigned> refBody;                                // ref record -> body id
        unsigned nextBodyId = 0;
        // Production slots are recycled, so the body id must travel inside
        // the task body itself: each fired append is the executed order.
        const auto produced = std::make_shared<std::vector<unsigned>>();
        double clock[kDomains] = {0.0, 0.0, 0.0}; // days, seconds, seconds

        const int opCount = 240;
        for (int op = 0; op < opCount; ++op) {
            const unsigned kind = static_cast<unsigned>(rng() % 100);
            if (kind < 55) {
                // Schedule on a small quantized deadline grid so ties are
                // common and the stable-order comparison stays meaningful.
                const std::size_t domainIndex = rng() % kDomains;
                const auto domain = static_cast<ClockDomain>(domainIndex);
                const double deadline = clock[domainIndex] + static_cast<double>(rng() % 9);

                const unsigned bodyId = ++nextBodyId;
                prodTokens.push_back(scheduler.Schedule(domain, deadline,
                                                        [produced, bodyId] { produced->push_back(bodyId); }));
                const auto [recordIndex, generation] = reference.Schedule(domainIndex, deadline);
                refTokens.emplace_back(recordIndex, generation);

                refBody.resize(recordIndex + 1, 0);
                refBody[recordIndex] = bodyId;
            } else if (kind < 80 && !prodTokens.empty()) {
                // Cancel a random token, including dead ones: outcomes must
                // match the reference model.
                const std::size_t pick = rng() % prodTokens.size();
                const bool expected = reference.Cancel(refTokens[pick].first,
                                                       refTokens[pick].second);
                const bool actual = scheduler.Cancel(prodTokens[pick]);
                REQUIRE(actual == expected);
            } else if (kind < 88) {
                // Session replacement must be mutually consistent: the
                // scheduler bumps its generation, the reference model
                // invalidates the same accumulated record set.
                scheduler.NewSession();
                reference.NewSession();
            } else {
                // Advance one domain; the extracted order must equal the
                // reference order exactly.
                const std::size_t domainIndex = rng() % kDomains;
                const auto domain = static_cast<ClockDomain>(domainIndex);
                clock[domainIndex] += static_cast<double>(rng() % 4);

                const auto refDue = reference.AcquireDue(domainIndex, clock[domainIndex]);
                const auto due = scheduler.AcquireDue(domain, clock[domainIndex]);
                REQUIRE(due.size() == refDue.size());

                std::vector<unsigned> expectedOrder;
                expectedOrder.reserve(refDue.size());
                for (const auto recordIndex : refDue) {
                    expectedOrder.push_back(refBody[recordIndex]);
                }
                const std::size_t before = produced->size();
                for (const auto& task : due) {
                    task.body();
                }
                const std::size_t fired = produced->size() - before;
                REQUIRE(fired == refDue.size());
                const std::vector<unsigned> actualOrder(produced->begin() + static_cast<std::ptrdiff_t>(before),
                                                        produced->end());
                REQUIRE(actualOrder == expectedOrder);
            }

            // Periodic bookkeeping check: the two
            // models must agree on live work after *every* operation, not
            // only at the final drain.  A silent slot leak or a cancelled
            // task still counted as pending would surface here long before
            // the end of the run.
            for (std::size_t domainIndex = 0; domainIndex < kDomains; ++domainIndex) {
                REQUIRE(scheduler.PendingCount(static_cast<ClockDomain>(domainIndex))
                        == reference.PendingCount(domainIndex));
            }
        }

        // Final drain on every domain with a far-future clock.
        for (std::size_t domainIndex = 0; domainIndex < kDomains; ++domainIndex) {
            const auto domain = static_cast<ClockDomain>(domainIndex);
            clock[domainIndex] += 100.0;

            const auto refDue = reference.AcquireDue(domainIndex, clock[domainIndex]);
            const auto due = scheduler.AcquireDue(domain, clock[domainIndex]);
            REQUIRE(due.size() == refDue.size());

            std::vector<unsigned> expectedOrder;
            expectedOrder.reserve(refDue.size());
            for (const auto recordIndex : refDue) {
                expectedOrder.push_back(refBody[recordIndex]);
            }
            const std::size_t before = produced->size();
            for (const auto& task : due) {
                task.body();
            }
            REQUIRE(produced->size() - before == refDue.size());
            const std::vector<unsigned> actualOrder(produced->begin() + static_cast<std::ptrdiff_t>(before),
                                                    produced->end());
            REQUIRE(actualOrder == expectedOrder);
        }

        REQUIRE(scheduler.PendingCount() == 0);
    }
}

TEST_CASE("scheduler rejects non-finite deadlines and unknown domains") {
    Scheduler scheduler;

    // NaN / infinity deadlines never arm work.
    const auto nanToken = scheduler.Schedule(ClockDomain::Real, std::numeric_limits<double>::quiet_NaN(),
                                             [] {});
    CHECK(nanToken.id.value == 0);
    CHECK_FALSE(scheduler.Cancel(nanToken));
    CHECK_FALSE(scheduler.Cancel(scheduler.Schedule(ClockDomain::Real,
                                                    std::numeric_limits<double>::infinity(), [] {})));

    // Unknown domain ids are caller bugs: report empty/no-op, never index
    // out of the three-domain array.
    const auto bogus = static_cast<ClockDomain>(7);
    CHECK(scheduler.Schedule(bogus, 1.0, [] {}).id.value == 0);
    CHECK(scheduler.AcquireDue(bogus, 10.0).empty());
    CHECK(scheduler.DueCount(bogus, 10.0) == 0);
    CHECK(scheduler.PendingCount(bogus) == 0);
    CHECK(scheduler.HeapSize(bogus) == 0);

    // Non-finite "now" must not drain the heap.
    std::size_t runs = 0;
    const auto token = scheduler.Schedule(ClockDomain::Game, 1.0, [&] { ++runs; });
    REQUIRE(token.id.value != 0);
    CHECK(scheduler.AcquireDue(ClockDomain::Game, std::numeric_limits<double>::quiet_NaN()).empty());
    CHECK(scheduler.DueCount(ClockDomain::Game, std::numeric_limits<double>::infinity()) == 0);
    CHECK(scheduler.PendingCount(ClockDomain::Game) == 1);
    CHECK(runs == 0);
}

TEST_CASE("scheduler session change and slot reuse keep tokens isolated") {
    Scheduler scheduler;
    std::vector<unsigned> fired;

    const auto first = scheduler.Schedule(ClockDomain::Game, 1.0, [&] { fired.push_back(1); });
    REQUIRE(first.id.value != 0);

    // New session invalidates every token; the recycled slot must not
    // resurrect the old token but must serve the next schedule.
    const auto session = scheduler.NewSession();
    CHECK(session > 1);
    CHECK_FALSE(scheduler.Cancel(first));
    CHECK(scheduler.PendingCount(ClockDomain::Game) == 0);

    const auto second = scheduler.Schedule(ClockDomain::Game, 0.5, [&] { fired.push_back(2); });
    REQUIRE(second.id.value != 0);
    CHECK(second.id.value == first.id.value);       // LIFO slot reuse
    CHECK(second.generation != first.generation);   // never the same generation
    CHECK_FALSE(scheduler.Cancel(first));           // stale token stays dead

    auto due = scheduler.AcquireDue(ClockDomain::Game, 1.0);
    REQUIRE(due.size() == 1);
    due.front().body();
    REQUIRE(fired == std::vector<unsigned>{2});

    // Cancel-then-reuse: a token cancelled before its deadline dies for
    // good even when its slot is immediately handed to a new task.
    const auto third = scheduler.Schedule(ClockDomain::Game, 2.0, [&] { fired.push_back(3); });
    REQUIRE(scheduler.Cancel(third));
    const auto fourth = scheduler.Schedule(ClockDomain::Game, 2.0, [&] { fired.push_back(4); });
    CHECK(fourth.id.value == third.id.value);
    CHECK_FALSE(scheduler.Cancel(third));
    due = scheduler.AcquireDue(ClockDomain::Game, 3.0);
    REQUIRE(due.size() == 1);
    due.front().body();
    REQUIRE(fired == (std::vector<unsigned>{2, 4}));
}

TEST_CASE("scheduler reclaims cancelled far-future entries", "[fix05]") {
    Scheduler scheduler;
    for (int i = 0; i < 1000; ++i) {
        REQUIRE(scheduler.Cancel(scheduler.Schedule(ClockDomain::Real, 1000, {})));
    }
    CHECK(scheduler.PendingCount() == 0);
    CHECK(scheduler.HeapSize(ClockDomain::Real) <= 16);
    scheduler.NewSession();
    CHECK(scheduler.HeapSize(ClockDomain::Real) == 0);
    CHECK(scheduler.DiscardedCount(ClockDomain::Real) == 1000);
}

TEST_CASE("scheduler HasDue discards dead roots and preserves live ordering", "[fix05]") {
    Scheduler scheduler;
    const auto dead = scheduler.Schedule(ClockDomain::Real, 1, {});
    const auto live = scheduler.Schedule(ClockDomain::Real, 2, {});
    REQUIRE(scheduler.Cancel(dead));
    CHECK_FALSE(scheduler.HasDue(ClockDomain::Real, 1));
    CHECK(scheduler.HeapSize(ClockDomain::Real) == 1);
    CHECK(scheduler.HasDue(ClockDomain::Real, 2));
    const auto due = scheduler.AcquireDue(ClockDomain::Real, 2);
    REQUIRE(due.size() == 1);
    CHECK(due.front().token == live);
    CHECK_FALSE(scheduler.HasDue(ClockDomain::Real, 2));
}

TEST_CASE("scheduler capture destruction can query the scheduler", "[fix05]") {
    Scheduler scheduler;
    int released = 0;
    auto capture = std::shared_ptr<int>(new int(0), [&](int* value) {
        CHECK(scheduler.PendingCount() == 0);
        ++released;
        delete value;
    });
    const auto token = scheduler.Schedule(ClockDomain::Real, 1000, [capture] {});
    capture.reset();
    SECTION("cancel") { REQUIRE(scheduler.Cancel(token)); }
    SECTION("new session") { scheduler.NewSession(); }
    CHECK(released == 1);
}
