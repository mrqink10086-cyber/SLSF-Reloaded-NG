# Scheduler, ClockService, and Dispatcher algorithm audit

## Scope and method

Audited the production implementations and public interfaces for Scheduler, ClockService, and Dispatcher against `.scratch/native-migration/spec.md`, `.scratch/native-migration/design.md`, `.scratch/native-migration/optimization-plan.md`, and `docs/superpowers/plans/2026-10-27-task-books/05-scheduler-and-dispatcher.md`. The inspected production blobs matched target tree `4c72b3da398994ae3e1c0d4567545ee7c1713696` for `Scheduler.h`, `Scheduler.cpp`, `ClockService.h/.cpp`, `Dispatcher.h/.cpp`, `ReputationState.cpp`, and `WakeSource.h`.

Built and ran the standalone probe in `algorithm-probe.sh` against the production translation units listed in that script. It does not invoke xmake or the shared test target. The suite was not run here; the parent agent is running it.

## Confirmed behavior and semantic defects

### Equal-time scheduled changes reverse enqueue order

`Dispatcher::ScheduleChange` uses `std::lower_bound` with a strict `< timestamp` comparator and inserts at the returned position (`src/runtime/Dispatcher.cpp:361-376`). For equal timestamps that position is before all earlier equal-time entries. `Advance` then `stable_sort`s by timestamp, preserving that already-reversed order, and applies the commands sequentially (`src/core/reputation/Settlement.cpp:618-623,664-681`).

Production probe: schedule `Set(10)` then `Set(20)` for the same region, category, and day; `AdvanceTime(day)` publishes `10`. FIFO enqueue semantics require `20`. This conflicts with the stable same-deadline requirement in `.scratch/native-migration/optimization-plan.md:91,103` and `docs/superpowers/plans/2026-10-27-task-books/05-scheduler-and-dispatcher.md:22`.

Equivalent correction: insert after existing equal timestamps with `upper_bound`, or order by `(at, enqueueSequence)`. `upper_bound` is the smaller change and retains the sorted-vector implementation; its insertion is still O(n).

### Execute can apply a write before pending game time is settled

`Dispatcher::Execute` sets `RuleContext.clock` from `lastAdvance_` (or zero), calls `ApplyExplicit`, and publishes without advancing pending time (`src/runtime/Dispatcher.cpp:330-347`). `ApplyExplicit` explicitly ignores the context and a `Set` writes `account.local` without updating `settledAt` (`src/core/reputation/ReputationState.cpp:162-165,207-215`). The design requires Execute to process pending time before committing the command (`.scratch/native-migration/design.md:63-66,79,95-103`).

Production probe using the public wake path: `WakeTick(day 1)` with no due change or game timer, then `Execute(Set 20)`, then `AdvanceTime(day 1)` yields `18`; settling day 1 before Execute yields `20`. With `LegacyDefaultParameters().decayPerDay == 2.0` (`src/core/reputation/Settlement.cpp:329-331`), the later advance charges two half-day periods to a value whose anchor remains day 0. This is a confirmed public Dispatcher contract gap. `IWakeSource` is only an abstract callback interface in `include/slsf/interfaces/WakeSource.h:9-16`; no target runtime implementation is present here, so the deployed adapter's call ordering cannot be established from this tree.

Equivalent correction: make the synchronous transaction settle the current recorded game time before applying the explicit command, then apply/publish under the same state transaction. Ensure a rejected advance leaves the command unapplied. A dispatcher-level current-time seam is preferable to letting individual commands repair anchors.

### Detach during a due batch does not suppress later callbacks from that batch

`Step` extracts the whole due batch before invoking its bodies (`src/runtime/Dispatcher.cpp:532-550`). `Detach` advances the scheduler session (`src/runtime/Dispatcher.cpp:263-295`), but each extracted task was already retired from the slot table by `AcquireDue` and remains in the local vector. Therefore the next body still runs. The probe schedules two same-domain zero-delay generic timers; the first calls `Detach`, and the second increments its counter. Output: `attached=0 first=1 second_old_timer=1`.

This contradicts the lifecycle intent that session replacement invalidates corresponding tasks (`docs/superpowers/plans/2026-10-27-task-books/05-scheduler-and-dispatcher.md:23`) and the `Detach` comment that no timer can call back in (`src/runtime/Dispatcher.cpp:275`). The existing flow wait path has its own weak-entry/session handling; this result concerns ordinary public `Dispatcher::Schedule` bodies. If due-batch snapshot semantics are intended to override session invalidation, that contract needs to say so explicitly.

Equivalent correction if Detach must invalidate the rest of an acquired batch: capture the scheduler session generation for the batch and recheck it before each callback, discarding old-session tasks after a callback advances the session. Do not add a thread pool; callback order stays synchronous and stable.

### [P2] Far-future cancellation does not trigger heap compaction

`Cancel` retires the slot but does not increment either discard counter (`src/core/time/Scheduler.cpp:104-124`). `AcquireDue` increments `discarded` only when a dead heap entry reaches the root and is popped (`src/core/time/Scheduler.cpp:134-150`); `NewSession` compacts only when that counter is already at least 16 (`src/core/time/Scheduler.cpp:221-242`). The optimization plan requires handling container recovery under repeated cancellation (`.scratch/native-migration/optimization-plan.md:103`).

Production probe: schedule and cancel 1,000 timers at deadline 1000. Immediately after cancellation and after `NewSession`, `PendingCount=0`, `HeapSize=1000`, and `DiscardedCount=0`. `AcquireDue(999)` leaves all 1,000 heap entries. They are only removed by `AcquireDue(1000)`, which reports 1,000 discarded entries. Repeating far-future cancellations can therefore retain tombstones until their deadlines, even though slots and task bodies are recycled.

Equivalent correction: track pending tombstones separately from the cumulative physical-discard statistic. Increment it when cancellation/session replacement invalidates a heap entry, decrement it when lazy popping removes that entry, and reset it after compaction. Compact when the pending-dead share/threshold justifies the scan. This preserves lazy O(log n) cancellation below the threshold and bounds long-lived stale heap storage.

## Measured local costs and equivalent alternatives

Environment: Clang 20.1.0, target `x86_64-pc-windows-msvc`, CPU `13th Gen Intel(R) Core(TM) i5-13600KF`; `-std=c++23 -O2 -DNDEBUG`, with `-mavx2` only for `DiffusionKernelAvx2.cpp`. Random event payloads use seed 1170. Each timing is the median of seven samples; event checksum outputs match across all queue variants.

The event numbers are a local same-payload container-drain microbenchmark, not end-to-end `Dispatcher::Step` timings. They isolate repeated front erasure from O(1) removal and do not include Dispatcher locks, clock work, epoch validation, or callback posting.

| Events drained | `vector.erase(begin())` | vector head cursor + final clear | `deque.pop_front()` | Checksum |
|---:|---:|---:|---:|---|
| 100 | 12.1 µs | 0.5 µs | 0.5 µs | all `1169043493954920809` |
| 1,000 | 1.4447 ms | 5.4 µs | 4.6 µs | all `17845786339273512148` |
| 5,000 | 28.6748 ms | 25.3 µs | 23.7 µs | all `9927661155201209343` |

The current event loop moves and erases `events_.begin()` once per item (`src/runtime/Dispatcher.cpp:552-579`), shifting the remaining vector on every iteration: O(E²) element moves per drained batch. The alternatives preserve this probe's FIFO checksum; the deque is the simpler direct replacement for front removal. Any production change must preserve all of these behaviors: (1) post order equals consume order; (2) `Post` called by a callback is visible later in the same `Step`; (3) `Detach` clears queued events; (4) if a callback throws, that event is already removed but later events remain queued; and (5) validate each event's epoch immediately before its callback. A head cursor also needs to report only unconsumed events from `PendingEvents` and reset on Detach/Attach. Swapping the batch into a local vector would change same-round posting and exception behavior.

`Step` uses `scheduler_.DueCount(Game, now) > 0` as an existence test (`src/runtime/Dispatcher.cpp:518-530`), while exact `DueCount` scans every heap entry and checks its deadline and slot generation (`src/core/time/Scheduler.cpp:174-191`). On heaps of live future timers, measured exact-query medians were:

| Heap entries | Production `DueCount`, per call | Local root-key existence model, per call |
|---:|---:|---:|
| 128 | 52.2 ns | 11.8 ns |
| 1,024 | 750.6 ns | 11.8 ns |
| 8,192 | 2.5866 µs | 11.8 ns |

Each sample made 500 calls; all measured checksums were zero and the pre-timing equivalence check passed. The local model uses the same min-heap ordering and a mutex, but only stores `(deadline, sequence)` keys and has no slot/generation validation. It is a lower-bound comparator for the normal no-due/no-tombstone path, not a production implementation benchmark or a drop-in replacement. A production `HasDue` must handle slot/session validity and stale due roots. It can check the root and return in O(1) when that root is live and in the future; when the root is due but stale, it must pop/account stale roots until it finds a live due entry or reaches a future/empty heap. Keep exact-count `DueCount` for callers that need its public count semantics; make `Step` use the boolean seam. Pruning stale roots must preserve the current no-advance behavior when no live timer is due, since an unnecessary `AdvanceTime` can publish a revision.

Other observed costs that were not benchmarked: scheduled-change insertion has O(n) vector movement (`Dispatcher.cpp:369-376`); each `AdvanceTime` moves the pending vector into inputs, `Advance` copies it into `workspace.orderedChanges` and stable-sorts it, then erases due entries (`Dispatcher.cpp:402-419`, `Settlement.cpp:618-623,695-697`). These are candidates only if real queue sizes show they matter; the available probe establishes no workload scale for them. `PendingCount` also scans slots (`Scheduler.cpp:193-219`) but is not used in the per-step due check.

## Clock and module notes

No ClockService cadence/pause defect was confirmed by source inspection. `RecordGameTime` ignores non-finite or rewinding samples (`src/runtime/ClockService.cpp:18-28`); `SetMenuOpen` attributes the pre-transition interval to the old menu state (`:30-49`); `Tick` accumulates Real through menus and Active only outside them (`:65-89`); `Reset` clears the clocks but preserves engine-owned menu state (`:106-116`). The six existing tests cover anchoring, menu pause, game rewind, transitions, and non-finite wall samples (`tests/core/ClockServiceTests.cpp:37-146`); they were not run by this probe.

In codebase-design terms, Scheduler is the module that owns heap/tombstone invariants. A separate boolean `HasDue` interface gives `Dispatcher::Step` the shallow existence operation it needs without weakening `DueCount`'s exact-count interface. Dispatcher already owns the executor-round seam; replacing its event container keeps FIFO callback policy local to the Implementation. WakeSource remains an adapter-facing interface without an implementation here, so deployed cadence still needs a target-runtime probe as required by the task book.

## Replay

From the repository root, run:

```powershell
& 'C:/Program Files/Git/bin/bash.exe' --noprofile --norc build/diagnostics/review-05/algorithm-probe.sh
```

The script compiles the actual core and runtime translation units into a temporary directory, links `algorithm-probe.cpp`, runs the semantic probes and microbenchmarks, and removes the temporary objects/executable on exit. It prints compiler, target, CPU, flags, seed, scales, medians, and checksums.
