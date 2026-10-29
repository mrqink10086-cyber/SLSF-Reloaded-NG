# 05 B01/B02 — Standards + memory/heap/stack/thread audit

## Scope and evidence

Read-only review of pinned tree `4c72b3da398994ae3e1c0d4567545ee7c1713696` against base `5222a3c31841426bb26c3635a37638abb8f707e5`; `commits.txt` is empty and the working HEAD remains the base. Sources were extracted with `git archive` into `build/diagnostics/review-05/memory-src`. The audit covers Scheduler, ClockService, Dispatcher, ReputationState query/publication code, the WakeSource interface, their headers/tests, and the target Xmake inputs. It does not use the delivered report as evidence. There is no first-party CONTRIBUTING or CODING_STANDARDS file; vendor conventions do not govern this code.

## Standards summary (under 400 words)

The project’s binding rules are in `.scratch/native-migration/spec.md`, `design.md`, `optimization-plan.md`, `docs/superpowers/plans/2026-10-27-task-books/05-scheduler-and-dispatcher.md`, and `docs/superpowers/plans/2026-10-27-runtime-and-compat-plan.md`.

Three source-level deviations are supported. `Dispatcher::Attach` holds `mutex_` while allocating a workspace and calling `PrepareSettlement`; this conflicts with `design.md:101-103`, which assigns preparation outside the transaction lock. `Step` extracts an entire due batch, then invokes each timer body without rechecking its session (`Dispatcher.cpp:535-550`), so a callback can detach and attach a new session while later old-session bodies remain runnable; the B01 acceptance rule requires session replacement to invalidate work and the executor to validate epochs (`05-scheduler-and-dispatcher.md:22-25`, `runtime-and-compat-plan.md:11-17`). A wait callback has its own weak-entry guard, but generic `Schedule` bodies and dirty callbacks do not. `Query` resolves `RegionId` with `std::find` over `regionIds` even though `MakePublishedState` writes `RegionId(i)` at index `i`; this conflicts with numeric single-item lookup in `design.md:91-98` and `optimization-plan.md:57-72`.

Conditional lock-reentry risk: `Scheduler::Cancel` and `NewSession` destroy arbitrary `TaskBody` captures while holding the scheduler mutex. This can deadlock only when a capture’s destructor re-enters that scheduler; Dispatcher lifecycle and wait paths can additionally hold the dispatcher mutex across those calls. Audited built-in wait and dirty captures do not themselves re-enter. `ClockService` also invokes its injected `WallNowFn` under its mutex; the public contract does not state that the supplier must not re-enter ClockService.

No unambiguous violations were found among the 12 requested Fowler smells. They are heuristic prompts, not hard standards; the checklist below records each independently. Existing project standards take priority over style heuristics.

## Findings

### S1 — An extracted timer batch can cross a session boundary

`Scheduler::AcquireDue` moves every currently due body out of its slot table and retires the slots before returning (`src/core/time/Scheduler.cpp:126-171`). `Dispatcher::Step` then invokes the returned bodies in sequence with no session/epoch stamp or validity check (`src/runtime/Dispatcher.cpp:535-550`). If the first body calls `Detach()` and `Attach()` reentrantly, `NewSession()` cannot revoke the already-extracted later bodies. This is same-thread reentry; it does not rely on racing Flow destruction. A stale dirty body can also target a same-`RegionId` request newly added after reattach (`RequestDirtyRefresh` captures `[this, target]` at `Dispatcher.cpp:460-475`; `FireDirty` only checks target presence at `482-495`). Generic scheduled bodies can run arbitrary old-session work.

The existing regression test proves the wait-specific case is guarded: `DispatcherTests.cpp:738-780` makes flow A detach during a timer batch and flow B’s weak `WaitEntry` callback does not resume it. That does not cover generic `Schedule` or the stale-dirty/same-target case. This finding is established by the extraction/dispatch path and confirmed by root's replay of `memory-build/session_batch.exe` via `replay-instrumented-batch.sh`: it exited 0 and printed `first_ran=1 stale_callback_ran_in_new_session=1`, with no ASan/UBSan diagnostic. The initial 30-second attempt lacked the ASan runtime directory on PATH and did not provide a valid result. Replay log: `build/diagnostics/review-05/instrumented-batch-replay.log`.

### S2 — Arbitrary task captures can be destroyed while the scheduler lock is held

`RetireSlotLocked` executes `released = nullptr` under the caller-held mutex (`src/core/time/Scheduler.cpp:39-49`); `Cancel` calls it under that mutex (`:104-123`). `NewSession` repeats the same destruction at `:221-243`. A `std::function` capture can own an RAII object whose destructor calls `PendingCount`, `Cancel`, or another method on the same scheduler; that call blocks forever on the non-recursive mutex. The risk is conditional on a re-entering capture destructor, not on an ordinary callback body, and no built-in wait/dirty capture inspected here has that behavior. Dispatcher `RetireWait`, `Attach`, and `Detach` acquire `Dispatcher::mutex_` before scheduler calls (`src/runtime/Dispatcher.cpp:158-185,230-260,263-299`), so a re-entering destructor can also extend the lock-order hazard to dispatcher state.

### S3 — Attach performs preparation while holding the transaction mutex

`Dispatcher::Attach` acquires `mutex_` at `src/runtime/Dispatcher.cpp:230-232`, then allocates `SettlementWorkspace` and calls `PrepareSettlement` before releasing it (`:239-249`). `PrepareSettlement` prepares settlement inputs and can allocate/copy graph-derived data. The approved rule says graph preparation and external/file work happen outside the short transaction lock, with only validated candidate commit inside (`.scratch/native-migration/design.md:101-103`; `.scratch/native-migration/configuration-design.md:44-48`). This is a lock-duration/architecture deviation on the Attach path, even though normal `Execute` keeps ApplyExplicit and publish in one lock as required (`Dispatcher.cpp:330-346`).

### S4 — Published single-item query performs a linear region scan

`MakePublishedState` fills `regionIds` with `RegionId(i)` (`src/core/reputation/ReputationState.cpp:497-518`). `Query` first maps location to region, then linearly searches `regionIds` for that same ID (`:525-538`). Since a valid region’s value is the row index, this makes an otherwise numeric single-item query O(region count). The project design says ordinary single-item queries use numeric addressing (`.scratch/native-migration/design.md:91-98`) and the optimization plan calls for direct numeric lookup (`.scratch/native-migration/optimization-plan.md:57-72`).

### S5 — Event queue drains with repeated front erasure

`Dispatcher::Step` moves `events_.front()` and calls `events_.erase(events_.begin())` for every event (`src/runtime/Dispatcher.cpp:552-578`). Each erase shifts the remaining `EngineEvent` elements, so draining N queued events is O(N²) element movement. This is a performance concern, not a correctness or memory-safety defect demonstrated here. Event bodies are invoked outside the lock; unlike timer bodies, event-body exceptions are not caught and stop the current `Step` after that event was removed.

### S6 — Injected wall-clock supplier runs under ClockService mutex

`ClockService::Tick` and `SetMenuOpen` call `wallNow_()` while holding `mutex_` (`src/runtime/ClockService.cpp:30-49,65-89`). `SetWallSource` accepts an arbitrary `std::function<double()>` (`include/slsf/runtime/ClockService.h:32-60`), but its contract only requires monotonic seconds and does not prohibit reentry. A supplier that calls back into `Tick`, `Reset`, or `SetWallSource` deadlocks. The production default `steady_clock` supplier and the inspected fake suppliers are non-reentrant; this is an API-contract gap, not evidence that the default path deadlocks.

## Ownership, memory, heap, stack, and thread contract

- Each clock domain owns `vector<HeapEntry>`, `vector<Slot>` and `vector<size_t> freeSlots`; a heap entry stores deadline, sequence, slot and generation (`include/slsf/core/time/Scheduler.h:124-138`). Task closures live in `std::function` slot bodies. Cancelled heap entries are lazy and compaction rebuilds them; slot vectors retain their high-water capacity and recycled slots are reused (`Scheduler.cpp:50-65,168-170,221-243`). No pool, arena, `pmr` resource or custom coroutine allocator was found in the audited `include/slsf`, `src/core`, or `src/runtime` tree.
- `SpawnFlow` heap-allocates the callable with `make_unique<std::function<Flow()>>`; the promise owns it until frame destruction because the coroutine frame stores the lambda `this` pointer (`src/runtime/Dispatcher.cpp:102-118`, `include/slsf/runtime/Dispatcher.h:116-139`). Each wait entry is `make_shared`, its timer closure captures a `weak_ptr`, and cancellation/detach marks it dead before resuming outside the lock (`Dispatcher.cpp:124-196,263-299`). Flow destroys its frame once; an abort-ignoring flow can synchronously hang, as documented in `Dispatcher.h:29-33`. Unhandled coroutine exceptions call `std::abort()` (`:135-139`).
- `AcquireDue` returns all due tasks in one vector, temporarily retaining the whole due cohort and its closures; burst dispatch can therefore create a transient allocation spike. Published snapshots are copied into a new `shared_ptr<const PublishedState>` under the dispatcher mutex (`Dispatcher.cpp:607-611`); readers retaining older atomic snapshots intentionally retain their memory.
- The documented model is one executor thread for `Step`/`WakeTick`/`AdvanceTime`; a `Flow` must not be started, awaited, or destroyed concurrently with callbacks for that flow (`include/slsf/runtime/Dispatcher.h:20-27`). This audit does not classify contract-forbidden concurrent Flow destruction as a defect. Event/timer user bodies and wait-frame resumes run outside dispatcher/scheduler locks; the noted exceptions are task-capture destruction and the injected clock supplier.
- No stack watermark or worst-case call-depth measurement was performed. `sizeof(Flow)` or a coroutine-frame size would not measure peak native stack use. Coroutine frame and callable storage are heap-owned as described above.

## Fowler smell checklist (heuristic only)

| Smell | Result |
|---|---|
| Mysterious Name | None: Scheduler/Dispatcher/ClockService names and methods expose their responsibilities. |
| Duplicated Code | No repeated implementation with a clear shared shape found; Attach/Detach have distinct lifecycle work. |
| Feature Envy | None established; wait and timer state is owned by Dispatcher/Scheduler. |
| Data Clumps | No recurring parameter group shown to justify a new type. |
| Primitive Obsession | Clock-domain times and epoch/ID values use domain types; no clear extraction warranted in scope. |
| Repeated Switches | ClockDomain dispatch has a single small switch in `DomainNowLocked`; no repeated mapping found. |
| Shotgun Surgery | Scheduler, clock service and dispatcher are intentionally separate task owners; no one logical change was shown to require scattered edits. |
| Divergent Change | Dispatcher is broad, but this is the specified single executor owning timers, waits, events, dirty merge and sync commands. |
| Speculative Generality | `IWakeSource` is a planned adapter seam (`include/slsf/interfaces/WakeSource.h:9-16`); not unused generality by itself. |
| Message Chains | No problematic caller navigation chain found in audited paths. |
| Middle Man | No delegation-only layer identified. |
| Refused Bequest | No inheritance hierarchy in scope. |

## Independent build / probe status

Clang 20.1.0 (`x86_64-pc-windows-msvc`) compiled all 20 pinned `src/core` and `src/runtime` production translation units with C++23, `-fsanitize=address,undefined`, and `-fno-omit-frame-pointer`; AVX2 was applied only to `NumericRowOps.cpp` and `DiffusionKernelAvx2.cpp`. The instrumented objects linked with the independent session-batch harness. Root replayed it with the ASan runtime directory on PATH; `memory-build/session_batch.exe` exited 0 and printed `first_ran=1 stale_callback_ran_in_new_session=1`, confirming the stale callback ran after reattachment, with no ASan/UBSan diagnostic. The initial attempt lacked the runtime directory on PATH; see `build/diagnostics/review-05/instrumented-batch-replay.log`. The bounded reentrant-capture executable was not reached. No TSan, LSan, stack-peak measurement, or 1.6.1170 WakeSource runtime probe was performed here. Parent reports the shared baseline’s 210 ASan cases / 128,222 assertions passed; this audit did not rerun that shared test suite or Xmake.
