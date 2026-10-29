# 04 Review: standards, memory, and thread safety

Scope: the seven assigned implementation files (`src/engine/{GameData,ActorQueries}.cpp` and `src/core/gameplay/{Anonymity,Visibility,Witness,AnimationClassifier,Comments}.cpp`), their headers, and the linked 04 group design/evidence. Read-only review; no xmake or project tests run. Apart from this report, the only added files are the independent `memory-layout.cpp`, `memory-layout.sh`, and their standalone output `memory-layout.exe`.

## Standards

No violation of a documented first-party C++ coding rule was found. The repository has no first-party `CONTRIBUTING.md`, `CODING_STANDARDS.md`, `CLAUDE.md`, or `.clang-format`; the vendored CommonLib guidance is outside this review. The applicable documented requirements are to keep engine facts behind owned DTOs and legal-context seams (`.scratch/native-migration/design.md:113-127`; `include/slsf/engine/EngineFacts.h:24-33`), serialize mutable engine/cache work through the main thread or owning executor (`.scratch/native-migration/spec.md:62-65`; `include/slsf/engine/GameData.h:22-23`), and keep external queries outside state locks (`.scratch/native-migration/design.md:101-103`). The reviewed code follows those boundaries: `ToNpcScanTargetFacts` copies fields (`src/engine/ActorQueries.cpp:123-138`), core gameplay uses value DTOs, and the cache is explicitly caller-serialized.

Fowler smell baseline review (smells only; none is a documented-rule violation):

| Smell | Assessment |
|---|---|
| Mysterious Name | None identified. |
| Duplicated Code | Possible low-priority smell: the `TattooSubcategory` switches for branch A and branch B repeat several cases (`src/core/gameplay/Visibility.cpp:238-332`). Their equations intentionally differ for `None`, `Back`, and `Thigh`, so this is not a correctness finding. If later edits repeatedly drift, extract a helper with explicit branch policy and retain those distinctions. |
| Feature Envy | None identified; the rules are the intended owners of their DTO calculations. |
| Data Clumps | None identified; the grouped facts are documented input snapshots. |
| Primitive Obsession | None identified; `Fact`, `StableFormKey`, enums, and `CategoryMask` express the domain. |
| Repeated Switches | None identified for a repeated decision over the same type. |
| Shotgun Surgery | None identified in this bounded task group. |
| Divergent Change | None identified in the reviewed modules. |
| Speculative Generality | None identified. |
| Message Chains | None identified. |
| Middle Man | None identified; query and DTO seams validate/copy data. |
| Refused Bequest | Not applicable; these classes do not use inheritance. |

## Memory and ownership

- `GameDataService` borrows `IEngineBackend&` (`include/slsf/engine/GameData.h:62`); `ActorQueriesService` borrows backend and game-data references (`include/slsf/engine/ActorQueries.h:33-34`); `NpcScanner`, `CandidateSelector`, and `FameGainRoller` borrow `IRandom&` (`include/slsf/core/gameplay/Witness.h:113-120,138-146,181-189`). The referenced objects must outlive the services/rules. No concrete production composition is in this scope, so that lifetime could not be verified at the integration site; the 04 evidence says lifecycle wiring/backend implementation is pending (`docs/evidence/task-groups/04-gameplay-and-engine-facts.md:209-210,225-230`). This is an integration boundary, not an observed dangling reference.
- Snapshots and gameplay facts own their `std::string`/`std::vector` data; no engine object pointer is stored in the DTOs. `ActorQueriesService::QueryCachedUnguarded` copies the cache value before returning (`src/engine/ActorQueries.cpp:114-121`). `GameDataService::Find` explicitly documents invalidation (`include/slsf/engine/GameData.h:49-51`); no unsafe retained use appears in the reviewed sources. `Plugins()` returns the member vector by const reference (`include/slsf/engine/GameData.h:29`); normal vector element references/iterators must not be retained across `RefreshPlugins` or `Clear` (`src/engine/GameData.cpp:72,276-280`). No such retained use was found in the scoped call sites.
- Cache iteration is safe against its own mutations: dirty keys are copied before refresh (`src/engine/GameData.cpp:156-165`), target keys are deduplicated and copied before processing (`:168-194`), and the refresh loop mutates cache/dirty state without iterating those containers (`:196-258`). `QueryAll` may leave earlier successful cache writes when a later query fails; this partial side effect is documented (`src/engine/ActorQueries.cpp:58-75`; `docs/evidence/task-groups/04-gameplay-and-engine-facts.md:91,211`).
- No custom pool, arena, raw owning allocation, or explicit stack recursion appears in these files. Variable-sized collections use STL containers and therefore allocate on the heap. `NpcScanner` retains copied actor keys and prior entries only while resuming a paused batch, and clears them after completion or `Reset` (`src/core/gameplay/Witness.cpp:24-30,45-58,139-147,180-187`). Small fixed `std::vector<Tri>` temporaries are also created by `AnyUnhideable`, `AcceptedLockables`, and initializer-list calls to `TriAny`/`TriAll` (`Visibility.h:61`; `AnimationClassifier.cpp:305-312`; `Visibility.cpp:618-620`). This is bounded allocation churn per evaluation, not a correctness or exhaustion finding in the observed call path.
- Stack/object-size probe command: `& 'C:/Program Files/Git/bin/bash.exe' --noprofile --norc '/e/source/repos/SLSF Reloaded NG/build/diagnostics/review-04/memory-layout.sh'` (Clang 20.1.0, C++23, exit 0). Measured sizes in bytes: `ActorSnapshot=2144`, `EquipmentSnapshot=1952`, `EquippedItemSnapshot=32`, `GameDataService=64`, `VisibilityFacts=808`, `AnimationFacts=152`, `NpcScanTargetFacts=72`, `NpcScanEntry=88`, `NpcScanner=128`, `FameGainRollFacts=288`, `CommentFacts=24`. In particular, `ActorSnapshot` embeds 61 vector objects in its equipment snapshot (`include/slsf/engine/EngineFacts.h:38-45,55-80`), but its measured ~2.1 KiB fixed stack footprint is not excessive; payload strings/vector elements remain heap-owned.

## Threading, context, and failure boundaries

- Engine query entry points reject contexts without both `mainThread` and `inGame` before calling the backend (`include/slsf/engine/EngineFacts.h:24-33`; `src/engine/ActorQueries.cpp:50-92`; `src/engine/GameData.cpp:64-68,156-194`). `GameDataService` has no internal mutex and states its single-owner-thread contract (`include/slsf/engine/GameData.h:22-23`); this matches the one-executor design (`.scratch/native-migration/spec.md:62-65`). No reachable concurrent caller or race was found within the supplied implementation.
- Core rule classes hold no global mutable state. `NpcScanner` is stateful across `Run` calls; rule methods that use injected `IRandom&` mutate that RNG. Those objects are not internally synchronized and require the same serialized owner as the calling executor. This matches the task design's single executor and fixed/random-source contract (`.scratch/native-migration/design.md:119-127`).
- No lock is held across backend calls because these services use no lock; this is consistent with the requirement to obtain external facts outside state locks (`.scratch/native-migration/spec.md:62`; `.scratch/native-migration/design.md:101-103`). The real backend is not present, so synchronous callback pumping, reentry into a service, or a context transition during a backend query cannot be confirmed. The code has no explicit reentrancy guard; no such path is evidenced here, so this remains a runtime-integration check rather than a race finding.

## Conclusion

No demonstrated memory-safety, stack-exhaustion, or thread-safety defect was found in the reviewed source boundary. This conclusion covers ownership and invalidation visible in the 04 DTO/cache/rule implementations, container mutation during refresh, fixed object sizes, and the declared single-thread context contract. It does not validate the absent live backend, production service construction/destruction order, engine callback reentrancy, or runtime-only relationship/detection/LOS behavior.
