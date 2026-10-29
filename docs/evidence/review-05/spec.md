# Group 05 B01/B02 — Spec review

Fixed comparison: `5222a3c31841426bb26c3635a37638abb8f707e5` → `4c72b3da398994ae3e1c0d4567545ee7c1713696`; commit list empty, `HEAD` remains the base. Read-only review of the pinned diff; no shared tests or xmake runs.

## (a) Missing or partial

- **B01 live WakeSource evidence is partial.** The plan requires a 1.6.1170 game probe to record real wake entry/context and menu, idle, and load-screen timing (`docs/superpowers/plans/2026-10-27-runtime-and-compat-plan.md:15-17`). The changed probe only prints static API signatures and `unverified:menu-open-wake,load-screen-wake,no-input-idle-wake` (`tests/probe/TargetProbe.cpp:134-152`); it does not establish runtime behavior. Treat this as an unmet acceptance item, not proof of a player-facing failure.
- **Execute skips settlement before applying a synchronous command.** B02 requires “校验、结算、修改及 revision 发布” in the transaction (`docs/superpowers/plans/2026-10-27-runtime-and-compat-plan.md:21`), and Execute is to sample the current clock and process business before publish (`.scratch/native-migration/design.md:97`). `Dispatcher::Execute` uses `lastAdvance_.value_or(GameTime{})` as context and calls only `ApplyExplicit` (`src/runtime/Dispatcher.cpp:338-345`). Trigger: advance wall/game time without a due timer/change, Execute a Set, then settle later; elapsed decay is applied after the Set rather than before it.

## (b) Scope creep

- None found in the reviewed B01/B02 diff.

## (c) Present but wrong

- **Equal-time scheduled changes reverse enqueue order.** The contract says “同一时间保持入队顺序” (`.scratch/native-migration/optimization-plan.md:91`). `lower_bound` inserts a new equal-time item before existing items (`src/runtime/Dispatcher.cpp:371-375`), and the later `stable_sort` preserves that reversed order (`src/core/reputation/Settlement.cpp:620-623`). Trigger: enqueue Set(10), then Set(20), at the same game time and advance past both; the final value is 10.
- **Graph preparation runs under the dispatcher mutex.** The design says graph preparation is outside the transaction lock (`.scratch/native-migration/design.md:103`). `Dispatcher::Attach` holds `mutex_` across `PrepareSettlement` (`src/runtime/Dispatcher.cpp:230-249`), which compiles the graph/rules and custom-region map (`src/core/reputation/Settlement.cpp:205-246`). A large graph therefore holds up concurrent dispatcher operations during preparation.

Epoch validation uses the composite session/index/scene `Epoch` and exact equality at consumption (`include/slsf/core/Types.h:86-93`, `src/runtime/Dispatcher.cpp:565-571`). Published snapshots pair revision, epoch, configuration revision, mappings, values, and flags (`include/slsf/core/Snapshots.h:108-118`). B03/B06 hookup absence was excluded from these findings.
