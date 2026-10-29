# 01–03 Standards and memory review

Date: 2026-10-28

## Scope and method

Compared the fixed trees `5222a3c31841426bb26c3635a37638abb8f707e5` and `7a43e4e4b3a67b3ac5fa2f4d33990ac4e0101e70` with the requested path filters. That comparison contains 59 files, 10,348 insertions, and 7 deletions. The worktree is not the fixed diff: the target files are partly untracked and other files are modified. Reviewed production code, tests, and the requested migration specifications/task books; treated those repository documents as normative and the Fowler list as heuristic. No root coding-standard document was present.

## Standards summary

No explicit repository-rule or spec violation was substantiated. The following are only possible design smells; each is a judgment for maintainers, not a defect finding.

| Fowler baseline | Assessment and evidence |
|---|---|
| Mysterious Name | None found. Public operations describe their work, e.g. `BuildDiffusionGraph` and `CompileDiffusionGraph` (`include/slsf/core/reputation/DiffusionGraph.h:30-38`). |
| Duplicated Code | Possible smell: mask-count delta logic appears in `src/core/reputation/ReputationState.cpp:126-138` and `src/core/reputation/Settlement.cpp:45-72`; the removal paths differ on zero-count handling, so consolidation would need to preserve the intended invariant. |
| Feature Envy | None found: provider contribution and count behavior is implemented on `ProviderState` (`src/core/reputation/ProviderRegistry.cpp:65-96`). |
| Data Clumps | None found. Repeated-looking keys remain distinct `StableFormKey` and `StableRegionKey` concepts (`include/slsf/core/Types.h:155-169`); no repeated field bundle clearly lacks a type. |
| Primitive Obsession | Possible smell: gain APIs accept `std::uint32_t categoryIndex` although the core model has `FameCategory` (`include/slsf/core/reputation/GainRules.h:43,78-81`; `include/slsf/core/Types.h:173-200`). Legacy index compatibility may justify this boundary. |
| Repeated Switches | Possible smell: the five-tier lookup is switched separately for gain and decay (`src/core/reputation/GainRules.cpp:24-31,163-169`). The target keys and defaults differ, so a shared mapping must retain that distinction. |
| Shotgun Surgery | No concrete change in this diff was found that requires scattered edits for one responsibility. Category identity/order is substantially centralized (`include/slsf/core/Types.h:173-231`). |
| Divergent Change | Possible smell: `src/core/reputation/Settlement.cpp:169-285,288-407,419-590` groups gain/manual edits, pulses, and scheduled settlement. They share state invariants and transaction flow, so the current grouping is defensible. |
| Speculative Generality | None found. Backend selection has active scalar/AVX2 dispatch (`src/core/reputation/DiffusionKernel.cpp:167-179`). |
| Message Chains | No problematic long navigation chain found; region lookup compares owned snapshot entries directly (`src/core/location/RegionRegistry.cpp:376-399`). |
| Middle Man | No pass-through layer found; graph compilation resolves keys and builds CSR (`src/core/reputation/DiffusionGraph.cpp:105-191`). |
| Refused Bequest | No suspect inheritance found. `IRandom` exposes one required operation that rule code uses (`include/slsf/core/Types.h:469-473`). |

## Memory safety

**Findings: none confirmed in the reviewed 01–03 tree.** No OOB, use-after-free, dangling borrowed view, integer wrap, or partially committed state was reproduced or found by inspection. The 01–03 review does not assess later B02/B05/B07 integration.

### Ownership, borrows, and reallocation

- `LocationIndexSnapshot`, `RegionSnapshot`, `DiffusionGraph`, scene/public snapshots, and persistent state own their vectors and strings (`include/slsf/core/Snapshots.h:17-76,83-120`; `include/slsf/core/persistence/PersistentState.h:21-99`). `SceneSnapshot` explicitly stores no span or RE handle (`include/slsf/core/Snapshots.h:83-97`). `ConfigurationSnapshot` and `PublishedStatePtr` use `shared_ptr<const ...>`; the inventory found no pool, arena, `pmr`, custom allocator, raw `new`, or `free` in production core. Thus there is no self-managed resource reset/release path or associated destruction-order hazard in this scope.
- Borrowed spans are synchronous inputs: location building (`src/core/location/LocationIndex.cpp:23-28`), graph compilation (`src/core/reputation/DiffusionGraph.cpp:105-106`), pulse sources (`src/core/reputation/Settlement.cpp:288-293`), and codec decode (`src/core/persistence/StateCodec.cpp:27-30,383-384`). `Reader` retains its byte span only while `DecodeState` runs; `String` copies into an owned `std::string` (`src/core/persistence/StateCodec.cpp:93-107`). `LegacyDefaultNode::nodeKey` is borrowed, but the lookup map is local to `BuildLegacyDefaultGraph`; its pointer/view entries are used within that call and returned edges copy stable keys (`src/core/reputation/DiffusionGraph.cpp:203-215,241-260`). The probe destroyed the input bindings before reading the returned graph and passed.
- `BindStateToRegions` keeps map pointers into `state.unresolvedAccounts` only while constructing replacement vectors, then commits the prepared vectors (`src/core/reputation/ReputationState.cpp:301-347`). `UpdateProvider` takes a contribution pointer after `push_back` and does not use it after erasing its element (`src/core/reputation/ProviderRegistry.cpp:119-138,147-175`). No pointer/reference from these paths is stored across a call or rebind.
- `Advance` validates its pointer inputs before dereferencing them and uses the graph, parameters, configuration, and RNG synchronously (`src/core/reputation/Settlement.cpp:419-447,484-590`). On work that can fail, it operates on a staged `State` and moves the completed result into place at the end (`src/core/reputation/Settlement.cpp:478-483,585-590`). `BindStateToRegions` and `RestoreState` similarly prepare owned replacements before committing (`src/core/reputation/ReputationState.cpp:339-349,423-494`). Allocation failures can propagate as exceptions, but the inspected builders decode into local owned values and state commits occur after construction/validation; no leaked or dangling intermediate state was identified.

### Bounds, alignment, arithmetic, and stack

- `NumericRow` is 32-byte aligned and exactly 256 bytes (32 doubles), with compile-time size/alignment/trivial-copy assertions (`include/slsf/core/NumericRow.h:13-55`). The seven AVX2 loads cover slots 0–27; all are unaligned loads within that row. The stores cover the same 28 slots and the kernel explicitly zeroes padding 25–31 (`src/core/reputation/DiffusionKernelAvx2.cpp:20-51`). Scalar and AVX2 dispatch validates the compiled graph and row-span lengths first (`src/core/reputation/DiffusionKernel.cpp:121-179`). Xmake confines AVX2 flags to the numeric and AVX2 kernel translation units (`xmake.lua:17-28`).
- The main size conversions have explicit bounds: location count before 32-bit IDs and entry/exit clocks (`src/core/location/LocationIndex.cpp:23-25,76-80`); API regions before 16-bit indices (`src/core/location/RegionRegistry.cpp:148-151,202-229`); account and operand counts before 32-bit CSR indices (`src/core/reputation/DiffusionGraph.cpp:105-124,164-189`); serialized counts and string lengths before allocation/read (`include/slsf/core/persistence/StateCodec.h:13-18`; `src/core/persistence/StateCodec.cpp:420-449,93-107`). `Reader::Need` checks remaining bytes before each read and offset advances (`src/core/persistence/StateCodec.cpp:35-105,124-131`). I found no reachable overflow at these validated bounds.
- Location and ownership traversals use explicit vectors/stacks rather than recursion (`src/core/location/LocationIndex.cpp:76-113`; `src/core/location/RegionRegistry.cpp:243-275`). No large fixed stack allocation or unbounded recursion was found in scope.

### Adversarial probe and limits

Built the independent `memory-probe.cpp` with Clang 20 ASan+UBSan; only `DiffusionKernelAvx2.cpp` received `-mavx2`. The probe passed (exit 0, no sanitizer output; `memory-probe.log`). It checks graph keys after destruction of dynamic string-view bindings, scalar/AVX2 parity with the final row as source in an exact two-row buffer, zeroed output padding, and rejection of every truncated prefix of a valid v2 codec payload. AVX2 was supported on this host, so the SIMD path ran.

The direct probe did not define `_DISABLE_VECTOR_ANNOTATION` or `_DISABLE_STRING_ANNOTATION`. The repository ASan targets do define both (`xmake.lua:64,78`), which reduces STL container-overflow annotation coverage in those targets; this is a test-coverage limitation, not evidence of a defect. The shared xmake build was not run for this audit.

Decode limits bound individual counts and strings but do not impose one total payload-byte ceiling (`include/slsf/core/persistence/StateCodec.h:13-18`, `src/core/persistence/StateCodec.cpp:420-449`). Very large inputs can therefore request correspondingly large owning allocations. The byte provenance/caller size policy is outside this 01–03 review, so I did not elevate proportional allocation volume to a memory-safety finding or assign severity. The probe covers a valid small v2 payload and all its truncations; it does not exhaustively fuzz the v1 codec or maximum-count payloads.
