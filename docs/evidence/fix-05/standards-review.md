# Fix 05 — Standards review

Reviewed the fixed diff from `4c72b3da398994ae3e1c0d4567545ee7c1713696` to `963ba4608f52c9dba83745b17ab75036e16ac57c`, including the Scheduler-only increment over `465683f661aa00156305fc9ead1c241d64b34051`.

## Documented standards

No unconditional documented standards breach found. The design requires preparation and external queries outside the transaction lock (`E:/source/repos/SLSF Reloaded NG/.scratch/native-migration/design.md:103`); the task book calls for one executor, stable due order, epoch checks, and one-time resource release (`E:/source/repos/SLSF Reloaded NG/docs/superpowers/plans/2026-10-27-task-books/05-scheduler-and-dispatcher.md:17-24`). The reviewed normal paths prepare `Attach` before commit and invoke clock suppliers, timer bodies, dirty sinks, and event bodies outside their owning locks (`E:/source/repos/SLSF Reloaded NG/src/runtime/Dispatcher.cpp:234-275,519-535,541-634`; `E:/source/repos/SLSF Reloaded NG/src/runtime/ClockService.cpp:30-80`). The offline wake probe follows the controllable-adapter testing approach; actual runtime behavior remains a separate probe (`E:/source/repos/SLSF Reloaded NG/docs/superpowers/plans/2026-10-27-task-books/00-native-testing.md:3,43`).

**Previously reported conditional lock-contract risk: closed.** `NewSession(retired)` reserves for the existing contents plus all scheduler slots before changing the session or retiring bodies (`E:/source/repos/SLSF Reloaded NG/src/core/time/Scheduler.cpp:249-260`). Thus `retired.push_back` cannot grow its vector during retirement. `RetireSlotLocked` adds the free-list entry before moving the body (`E:/source/repos/SLSF Reloaded NG/src/core/time/Scheduler.cpp:39-46`), so a free-list allocation failure occurs before the capture is moved into a temporary. These ordering changes close the reported allocation-failure path in which a capture destructor could run under the scheduler and outer dispatcher locks. This is code-path review, not runtime observation; no broader OOM recovery mechanism is required by this finding.

## Fowler heuristic

**Previously reported possible Duplicated Code: closed.** `DueCount` now calls `IsLiveLocked`, the same liveness predicate used by heap operations (`E:/source/repos/SLSF Reloaded NG/src/core/time/Scheduler.cpp:49-53,198-213`); the inline predicate drift identified in the earlier diff is gone. This was advisory, not a project-rule breach.

No other clear Fowler-baseline smell or hard standards violation was identified in this focused incremental review. No build or tests were run.
