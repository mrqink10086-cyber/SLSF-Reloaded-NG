# Fix 05 comment review

Compared the named files with tree `4c72b3da398994ae3e1c0d4567545ee7c1713696`. Review scope was limited to comments added, edited, or removed in those files.

## Deleted

Deleted 3 comments:

- `include/slsf/core/time/Scheduler.h`: the heap and cancellation implementation overview.
- `include/slsf/core/time/Scheduler.h`: the `discarded` field's compaction-window note.
- `tests/probe/DispatcherOfflineProbe.cpp`: the `OfflineWake` test-double explanation.

The fix-05 changes had also removed comments from `src/core/time/Scheduler.cpp`, `src/runtime/Dispatcher.cpp`, and `src/runtime/ClockService.cpp`. I reviewed those deletions and recommend no restorations: they described implementation details, or their claims no longer hold across the new source-generation handling.

## Kept

- `include/slsf/core/time/Scheduler.h`, `Cancel`: keep-list item 3. The public API comment records that task captures are destroyed after the scheduler lock is released.
- `include/slsf/core/time/Scheduler.h`, `HasDue`: keep-list item 3. Removing dead heap roots is a public method side effect not conveyed by its signature.
- `include/slsf/core/time/Scheduler.h`, `NewSession`: keep-list item 3. The comment records body-release timing and removal of heap entries.
- `include/slsf/core/time/Scheduler.h`, private `NewSession(retired)` overload: keep-list item 5. Capture destruction must also happen after a caller releases its outer transaction lock.
- `include/slsf/runtime/Dispatcher.h`, `Execute`: keep-list item 3. The comment documents settlement order, failure behavior, and publication. I revised it to avoid implying every rejected/no-op command publishes.

## Proposed encoding

For the item 5 constraint, extend the offline probe with a far-future timer whose last capture is a sentinel. Have the sentinel destructor call `Dispatcher::Attached()` while `Detach()` retires the timer. The probe's watchdog then fails if capture destruction happens while the dispatcher mutex is held. This encoding is proposed only; no code change was made.

Suppressions: none added or edited. TODOs deleted without an issue: none. Functional code changes: none; comment-only edits are listed above.
