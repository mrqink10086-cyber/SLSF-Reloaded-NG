# Counter-audit of the 01–03 algorithm findings

Scope: independent review of A1–A3 in `algorithms.md` against the fixed source snapshot and the selected native-migration requirements. Production files, existing tests, and project documentation were read-only. Existing public/cost probe sources and logs were inspected; no shared build artifacts were rebuilt.

## Verdict

| Item | Counter-audit result | Priority |
|---|---|---|
| A1 — queued timestamp versus settlement boundary | The numeric mismatch is real and contradicts the approved event-time requirement. Current header comments and a test encode the implementation’s batch ordering, but they do not supersede the project spec or A06 acceptance criteria. Narrow from P1 to P2 because this core API is not yet runtime-wired and its local contract is internally contradictory. | P2 |
| A2 — graph compilation on no-op Advance | Confirmed requirement-level performance miss: design says graph recompilation belongs in preparation, while every call compiles before the no-op check. The reported timings support the cost, but the larger sizes are stress inputs rather than a product latency target. | P2 |
| A3 — custom-region membership scan | O(N×C) lookup is confirmed. The reported 2.12× is a whole-Advance stress comparison that includes copying custom-region records; no selected requirement requires a membership cache. Keep as a conditional optimization opportunity, not a confirmed P2 defect. | P3 / measure when relevant |

## A1 — real mismatch, not an unqualified P1

The reported counterexample reproduces: with fixed-minimum randomness, `Set(10)` scheduled at 0.5 returns fame 9 when advanced to 0.5 and fame 8 when advanced to 1.0; processing the boundary first yields 10 and 9. Because the RNG returns the same lower bound for every draw, this difference comes from applying pre-event elapsed decay to the new value, not from a changed random sequence.

There is a real authority conflict. `include/slsf/core/Commands.h:88-93` describes the current batch as “due scheduled changes (at <= now) … local decay over elapsed time,” and `tests/core/SettlementTests.cpp:218-248` tests time-sorted due commands with zero decay. Those establish the current implementation-local behavior. They do not constitute an approved change to the higher-level contract: `.scratch/native-migration/spec.md:48,52` requires settlement over actual time and business boundaries; `.scratch/native-migration/design.md:65` says to settle pending time before submitting a command; the A06 plan requires splitting necessary protection/business boundaries and processing elapsed time by those boundaries (`docs/superpowers/plans/2026-10-27-foundation-and-core-plan.md:65,69`); the A06 taskbook repeats boundary settlement (`docs/superpowers/plans/2026-10-27-task-books/03-diffusion-and-settlement.md:24`). The 03 evidence report at `docs/evidence/task-groups/03-diffusion-and-settlement.md:24` records the implemented batch order; it is delivery evidence, not an approved requirement override.

So the report is right about both the code path and the mismatch. It overstates certainty when it calls the behavior an unqualified P1: the API comment and its test encode a competing batch contract, and the core has no runtime caller yet. Keep the issue at P2 and resolve the contract before integration. For commands whose timestamps lie inside the unconsumed interval, the intended implementation should settle old state to each distinct command time, apply same-time commands in stable order, then settle to `now` and commit once. Late commands whose timestamps are already behind the consumed anchor need a separately stated policy.

## A2 — explicit design miss, modest default cost

`src/core/reputation/Settlement.cpp:441-447` calls `CompileDiffusionGraph` before the no-op return at `src/core/reputation/Settlement.cpp:474-476`. The independent cost probe measures the actual production compiler and no-op `Advance`: 3,961.99/3,955.91 ns at 21 nodes/78 edges, 33,894.46/33,948.06 ns at 128/384, and 163,706.00/164,815.10 ns at 512/1,536. A valid compiled-CSR check is much cheaper (35.50, 143.86, 528.20 ns), though this check alone does not replace compilation in today’s `Advance` API.

The performance claim is therefore supported. It is also a selected-design mismatch, not just an optional cache idea: `design.md:103` puts graph recompilation in the preparation stage, and `optimization-plan.md:76` says CSR is derived state rebuilt during binding/restore. The current `AdvanceInputs` takes only `const DiffusionGraph*`, so it cannot reuse a prepared graph. Recompiling when graph content or stable-key binding changes, then passing that compiled graph into settlement, is the direction consistent with those requirements. The default no-op cost is about 4 μs; the 512/1,536 case is useful scaling evidence, not evidence that the shipped topology incurs 165 μs or misses a frame budget.

## A3 — confirmed scan, unproven product impact

`src/core/reputation/Settlement.cpp:410-417` searches `customRegions` from the beginning for a matching account key, and `:518-525` calls it inside the account loop. This is O(N×C) in the worst case (about C/2 comparisons per account when all accounts match entries spread through the vector).

The probe’s 512-account comparison (306,387.50 ns with no custom regions; 648,792.50 ns with 512; 2.12×) is a useful full-entry stress observation, not an isolated lookup measurement. `Advance` stages/copies `State`; `CustomRegionState` contains a `std::string` and `StableRegionKey`, so the all-custom case also adds O(C) copying and associated string work. Other per-account settlement work is in both cases. The ratio cannot be assigned to membership scans alone.

The selected docs require continuous aligned numeric rows and name particular derived masks/caches, but do not require a custom-region membership bitmap/index. The 21-region default topology is the documented baseline; 512 accounts/custom records are a stress scale with no stated support target or latency threshold. Retain the source-level complexity observation, but lower it to an optimization opportunity. Reconsider only if supported custom-region counts and an isolated or controlled benchmark show that this metadata scan is a material part of real settlement cost; a contiguous linear scan can be a reasonable choice for cold, small metadata.

## Disposition of adjacent observations

| Observation | Independent disposition |
|---|---|
| Negative Set | Same-time Set preserves -42 in L/R/F. On later settlement the transmitted target is clamped nonnegative and R moves to 0, so F becomes 0. The explicit Set contract covers the immediate full-Int32 write; the selected contract does not clearly require a negative remote value to persist after a settlement pass. Do not elevate this boundary observation to a new finding. |
| `maxFame=200` manual spread | The spread rule returns/writes local 150, while an already-existing remote value of 200 keeps public F at 200 (`F=max(L,R)`). The selected manual-spread rule caps its target at 150, but support for `maxFame>150` is not established by the selected configuration contract. Keep conditional on that configuration being supported. |
| NaN/+∞ time in `ApplyGainFame` | The gain helper accepts these values while `Advance` rejects non-finite time. No selected requirement explicitly defines non-finite `ApplyGainFame` inputs; treat as an invalid-domain observation unless the public boundary promises rejection. |
| `ApplySpreadPulse` settlement precondition | The helper’s header says it performs one hop “without decay or cascade,” and its signature has no RNG. The spec requires the public Spread/SpreadRoll flow to settle actual time before the pulse. A caller can compose `Advance` then pulse, but the helper’s own precondition is not explicit and no production caller currently demonstrates that composition. Keep this as an integration/API-precondition gap for the wiring review, not a proven defect in this standalone one-hop kernel. |
| Stable-key rebind | State rows rebind by stable identity and the probe compiles the graph against the new `accountKeys`; source/target indices match the rebound rows. Reusing an externally held stale compiled graph would be caller misuse, not shown by the current `Advance` path. |
| Scalar/AVX2 special values | The probe performs bitwise comparisons across NaN, infinities, signed zero, and dirty padding on the available AVX2 machine; it passed. This is useful directed evidence, not an exhaustive proof over all inputs. |

## Evidence reviewed

- `build/diagnostics/review-01-03/algorithms.md`, `algorithm-public-probes.cpp`, `algorithm-cost-probes.cpp`, `algorithm-probes.log`, and `algorithm-build-run.sh`.
- `src/core/reputation/Settlement.cpp`, `src/core/reputation/DiffusionKernel.cpp`, `include/slsf/core/Commands.h`, `include/slsf/core/reputation/Settlement.h`, and `include/slsf/core/reputation/DiffusionGraph.h`.
- `.scratch/native-migration/spec.md`, `design.md`, `optimization-plan.md`, A06 plan/taskbook, and the 03 task-group evidence report.
