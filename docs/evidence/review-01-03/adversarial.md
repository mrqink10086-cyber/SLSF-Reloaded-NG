# Adversarial review of 01–03 reports

## Scope and method

Compared BASE `5222a3c31841426bb26c3635a37638abb8f707e5` with SNAPSHOT `7a43e4e4b3a67b3ac5fa2f4d33990ac4e0101e70`. This review covers the actionable and positive claims in `spec.md`, the six 01/02 closure claims, the 01–03 delivery matrix, and the no-current-finding standards/memory report. Production code, existing tests, and project documents were read-only. The parent already ran Mode all (130 core cases plus boundary, differential, fuzz, benchmark, and ASan checks); I did not repeat it.

## Spec report items

### S1 — pulse must follow elapsed-time settlement

**Disposition: narrowed to a caller-ordering contract and B02 integration handoff; not a confirmed core defect.**

The stale-state counterexample is arithmetically valid: with target `R=80`, source `L=40`, edge `0.5`, threshold `20`, `decayPerDay=0`, `relaxationPerDay=1`, and `now=1`, calling the pulse on the old snapshot yields `R=80`; settling first relaxes the target to `79`, after which the pulse leaves `R=79`. The counterexample proves the primitive does not settle time itself. It does not prove a contract violation when called as intended: `include/slsf/core/reputation/Settlement.h:64-76` describes `ApplySpreadPulse` as the legacy one-hop primitive and says it performs no decay or cascade; it has no RNG or `AdvanceInputs` with which to do that work. `.scratch/native-migration/spec.md:49` assigns settle-then-pulse behavior to the compatibility operation, while `.scratch/native-migration/design.md:65-68` assigns settle-before-command ordering to Execute. B02 explicitly owns validation, settlement, modification, and revision publication within the synchronous transaction (`docs/superpowers/plans/2026-10-27-runtime-and-compat-plan.md:19-27`). The existing direct-call test uses zero decay (`tests/core/SettlementTests.cpp:489-509`); the composition test manually calls `Advance` before pulse (`:545-569`).

No production core caller currently invokes the primitive. Therefore there is no demonstrated current call path that skips settlement. Carry the settle-before-pulse requirement into the B02 compatibility wrapper and test it there. The header could state the settled-state precondition more explicitly; that is a contract clarity item, not evidence that this low-level primitive must own Advance.

### S2 — category decay parameters resolved in the slot loop

**Disposition: confirmed A06 performance acceptance miss, P2; arithmetic correctness unaffected.**

`.scratch/native-migration/optimization-plan.md:86-89` and `docs/superpowers/plans/2026-10-27-task-books/03-diffusion-and-settlement.md:18-26` require typed/precompiled category parameters and the shared time factor outside the category loop. The period count is already hoisted (`src/core/reputation/Settlement.cpp:111-115`), but `DecayAccount` still visits active slots and calls `LegacyFameMultiplier` and `RuleDecayRoll` per qualifying slot (`:116-150`). In map-backed, non-global configuration, `src/core/reputation/GainRules.cpp:36-49,154-186` constructs a category key and performs map lookups; the typed multiplier array already exists in `include/slsf/core/Configuration.h:35-42`, but the settlement input remains map-backed (`include/slsf/core/Commands.h:30-37`). This establishes a real mismatch with the optimization acceptance criterion, not a numeric result error. Limit the finding to that input path; B07 owns configuration compilation (`docs/superpowers/plans/2026-10-27-runtime-and-compat-plan.md:63-73`), so typed-config integration is a handoff while A06 performance completion remains open.

### S3 — source below 10 returns an error instead of a no-op

**Disposition: narrowed to an internal Result-status mismatch; external/gameplay defect unverified.**

The implementation difference is real: `.scratch/native-migration/spec.md:46` says no-op; `src/core/reputation/GainRules.cpp:109-112` returns `InvalidArgument`; `src/core/reputation/Settlement.cpp:217-248` propagates that result without writing the target. Tests encode the error (`tests/core/GainRulesTests.cpp:194-200`, `tests/core/SettlementTests.cpp:459-463`). State/output values remain unchanged, matching the no-op effect. The old public API has a void result, and there is no current production caller or adapter showing that this internal status aborts an externally observable operation. Do not claim mandatory success status or a player-visible bug absent a normative Result contract or a current abort path. Record the adapter decision as unverified at integration.

### R1 — failure leaves the input queue unchanged

**Disposition: rejected as a gameplay defect; the task-group report overstates the guarantee.**

`src/core/reputation/Settlement.cpp:449-458` sorts caller-owned `inputs.scheduledChanges` before creating the staged state; a due invalid command can then fail at `:494-498`. State is not committed, but queue ordering remains sorted. This refutes the report's literal “state and queue unchanged” statement (`docs/evidence/task-groups/03-diffusion-and-settlement.md:26`). `include/slsf/core/Commands.h:67-69` promises successful calls consume due entries and retain future entries; it does not promise preservation of original order on failure. Stable sorting preserves equal-time order, and chronological normalization is intended. The accurate claim is that state is staged and failure does not consume due entries; no caller-order requirement was found.

## 01/02 closure claims

All six items in `docs/evidence/2026-10-28-01-02-supplement-review.md:29-36` were checked against current source and regressions. No counterexample was found.

| Item | Disposition and countercheck |
|---|---|
| S1 unresolved region state | **Confirmed closed.** `src/core/reputation/ReputationState.cpp:301-349,352-385,388-494` retains unresolved records by stable key, captures bound and unresolved records, restores returning keys, and commits prepared state. `tests/core/ReputationStateTests.cpp:452-494` roundtrips the full record and bind/return sequence; the independent replay log preserves L=42, R=9, flag, protection, settled time, and restored status. No field-loss path was found. |
| S2 owner registration priority | **Confirmed closed for the reported counterexample.** `src/core/location/RegionRegistry.cpp:277-337` ranks a candidate by `min(priority[declared], priority[canonical owner])`. The nested R3/R2/R1 topology with order `[R2,R3,R1]` returns R1 (RegionId 0) in production and independent reference (`build/diagnostics/review-01-02/fix-owner-priority.log`); current tests also cover empty and partial custom order. |
| S3 empty provider registration | **Confirmed closed.** `src/core/reputation/ProviderRegistry.cpp:119-139` persists an empty contribution as changed. `CategoryMask::FromBits(0)` accepts zero (`include/slsf/core/CategoryMask.h:24-29`); codec encode/decode accepts the zero bitset and restore rebuilds counts (`src/core/persistence/StateCodec.cpp:270-277,312-360`; `src/core/reputation/ReputationState.cpp:450-481`). Regression and 255/256 registration probe cover save/restore and capacity (`tests/core/ReputationStateTests.cpp:496+`, `fix-empty-provider.log`). |
| S4 target-only write | **Confirmed closed, including the benchmark evidence.** `src/core/reputation/ReputationState.cpp:198-241` indexes and recomputes only the target account and applies category-count deltas; it does not scan all regions. The archived evidence source `docs/evidence/review-01-02/algorithm-single-write-probe.cpp:24-31` uses the current `RegionId` target type. I compiled that exact source with current `ReputationState.cpp` and `ProviderRegistry.cpp` and ran 1,000,000 writes at each account count; SHA-256 `cd3d8e4a4710cd156c0613ba339b1fecdbfb6c338c9e223ff521a991fb54b83d`, compile/run exit 0, and measured 0.017276/0.017368/0.017086/0.017130 μs/write for 128/4096/16384/65535 accounts (`build/diagnostics/review-01-03/adversarial-probe.log:1-8`). This independently reproduces the flat-cost result. The separate `build/diagnostics/review-01-02` duplicate is stale and was not the evidence source; an initial compile check of that duplicate was withdrawn. |
| S5 RegionId command target | **Confirmed closed.** `ExplicitCommand::target` is a `RegionId` (`include/slsf/core/Commands.h:23-28`), and `ApplyExplicit` indexes it directly (`src/core/reputation/ReputationState.cpp:198-203`). The no-location-record regression writes a valid region directly (`tests/core/ReputationStateTests.cpp:345-355`); the two ID types are distinct and explicit (`include/slsf/core/Types.h:17-38`). |
| S6 provider CategoryMask | **Confirmed closed.** `ProviderContribution` stores `CategoryMask` (`include/slsf/core/reputation/ProviderRegistry.h:46-60`); registration deduplicates bits and updates active counts by set delta (`src/core/reputation/ProviderRegistry.cpp:27-37,89-96,130-135,159-170`). Tests cover duplicate/overlapping categories, empty registration, and invalidation. No byte-array or per-category traversal regression was found. |

## Delivery matrix

| Claim | Disposition |
|---|---|
| 01 contracts, shared types, and build isolation | **No current gap found.** The contract inventory and build evidence in `docs/evidence/task-groups/01-contracts-and-build.md:58-161` are consistent with the current report. The first `-Mode all` exit 2 at `:296` is explicitly a historical skip before differential/fuzz/bench were integrated; it is not evidence against the parent's later successful all-mode run. |
| 02 state/location/provider/codec | **No current gap found in the audited claims.** Six closures above hold. v2 persists current DTO data and v1 compatibility remains documented; JSON/graph reload and save callbacks are explicitly B05/B07 handoffs (`docs/evidence/task-groups/02-state-location-and-codec.md:89-127`), not unfinished 01/02 core requirements. |
| 03 diffusion and settlement | **Core computation claims supported, with the bounded exceptions above and the separate algorithm audit.** Graph compilation, CSR kernels, frozen one-hop aggregation, and scalar/AVX2 implementation are present and tested. The queue-report wording is inaccurate; S2 is a real performance acceptance miss; S1 is caller-ordering/B02 integration; S3 is an unverified status difference. The project progress table still marks implemented A05/A06 work pending (`.scratch/native-migration/spec.md:91-92`), so that status needs a separate docs-owner update. |

The 03 report's historical 119 cases / 119,204 assertions and later 130 / 119,295 counts are successive snapshots, not a contradiction. Parent-reported current Mode all passed; no full suite was repeated for this audit.

## Standards and memory report

**Disposition: no current memory-safety or explicit standards violation confirmed; conclusion is appropriately bounded.** The probe source tests owned graph keys after temporary inputs are destroyed, exact two-row scalar/AVX2 bounds and zero padding, and every truncated prefix of a valid v2 payload (`build/diagnostics/review-01-03/memory-probe.cpp:17-94`). Its ASan+UBSan run passed with AVX2 exercised (`memory-probe.log:42-47`). Source inspection also supports the ownership and staging conclusions in `standards-memory.md:32-43`; the production core uses owned standard containers and no custom pool/arena path. The report correctly limits the inference: STL annotations are disabled in xmake sanitizer targets, the probe does not exhaustively cover v1 or maximum-count payloads, and there is no aggregate payload-byte ceiling (`standards-memory.md:45-51`). Those are coverage/input-policy limits, not a reproduced memory defect. The Fowler items in `standards-memory.md:13-26` are explicitly possible smells rather than findings.

## Algorithm report dispositions

The parallel counter-audit in `algorithm-counter.md` completed and independently reviewed each algorithm claim/probe; no experiments are duplicated here.

| Item | Disposition |
|---|---|
| A1 scheduled command timestamp | **Confirmed mismatch, P2.** The probe isolates chronology with deterministic decay/RNG and shows the new Set value is retroactively decayed: at `at=0.5`, `now=0.5`, queued result is 9 vs boundary-settled 10; at `now=1`, 8 vs 9. Higher-level spec/design and A06 require event/business-boundary settlement; current header/test encode a competing batch interpretation but do not approve that change. Lower P1 to P2 because the core is not runtime-wired and the local contract conflicts. Late events behind the consumed anchor still need an explicit policy. See `algorithm-counter.md:13-19`. |
| A2 CSR compile on no-op | **Confirmed requirement-level performance miss, P2.** `Advance` compiles before no-op return; design and optimization plan put CSR preparation in binding/restore. Timings support cost, though 512/1,536 is a stress case and not a product latency target. See `algorithm-counter.md:21-25`. |
| A3 custom-region membership | **Confirmed O(N×C) source complexity, P3 optimization opportunity.** The reported 2.12× whole-Advance stress ratio includes copying custom-region records; no selected requirement calls for a membership cache or establishes the 512-entry support/latency target. Do not report as a confirmed performance defect. See `algorithm-counter.md:27-33`. |
| Negative Set progression | **Rejected as a current defect.** Immediate same-time Set preserves -42; later settlement moves R to its nonnegative transmission target. The selected contract does not clearly require negative R to persist after settlement. |
| `maxFame=200` manual spread | **Unverified domain boundary.** Local target is capped at 150, but an existing remote 200 still determines public F. Support for `maxFame>150` is not established, so do not elevate without that contract. |
| NaN/+∞ `ApplyGainFame` time | **Unverified input-domain boundary.** `Advance` rejects non-finite time, but the selected contract does not define rejection behavior for this helper. |
| Pulse precondition | **Same narrowed integration handoff as S1.** It is a low-level frozen one-hop primitive, not an elapsed-time transaction; no current production caller shows misuse. |
| Stable-key rebind | **Rejected as a current defect.** Current `Advance` and pulse compile against current stable keys; a caller reusing an externally held stale compiled graph is outside the demonstrated path. |
| Scalar/AVX2 special values | **No defect found.** Directed bitwise comparisons on NaN, infinities, signed zero, and dirty padding passed on AVX2-capable hardware; this is targeted coverage, not exhaustive proof. |
