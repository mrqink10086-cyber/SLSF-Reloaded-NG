# 03 diff comment review

Deleted comments: 0 additional comment blocks were deleted during this review. The implementation diff had already removed the local padding-slot comment and replaced the stale claim that both backends share one scalar implementation.

## Kept comments

1. `include/slsf/core/Commands.h:76-78` — prepared snapshot reuse and invalidation. Keep-list item 3: documents the public cache contract that account/region bindings, graph, custom-region keys, and configuration changes require rebuilding, while numeric writes do not.
2. `include/slsf/core/Commands.h:79-80` — shared mutable workspace. Keep-list item 5: calls sharing scratch must be serialized; the current shared pointer does not encode that constraint.
3. `include/slsf/core/Commands.h:97-99` — `Advance` time boundaries, per-segment decay/propagation, and success/rejection effects. Keep-list item 3: these are observable API semantics. The boundary wording now reflects the implementation's `max(command time, prior boundary)` rule.
4. `include/slsf/core/reputation/DiffusionGraph.h:40-41` — region projection, owner-internal edge removal, and parallel-edge retention. Keep-list item 3: describes the overload's output semantics.
5. `include/slsf/core/reputation/DiffusionKernel.h:45` — scalar/AVX2 slot parity and padding clearing. Keep-list item 3: documents output behavior across public backends.
6. `include/slsf/core/reputation/GainRules.h:56` — tier-cap index mapping. Keep-list item 3: documents how public array data maps to the legacy tier enum.
7. `include/slsf/core/reputation/Settlement.h:15-17` — prepared snapshot contents, input lifetime, and invalidation. Keep-list item 3: clarifies ownership and refresh conditions; custom-region key changes are included explicitly.
8. `include/slsf/core/reputation/Settlement.h:87` — prepared pulse call order and source-row selection. Keep-list item 5: the required settled-through time is not represented by the function's parameter types.

## Suggested encodings

- For workspace serialization, use an exclusive workspace lease/guard that rejects overlapping use, or give each call independent scratch. This would make the item-5 constraint visible and enforceable.
- For pulse ordering, pass a `SettledThrough` value produced by `Advance` into the prepared pulse API, binding the state to its settled time.
- For prepared snapshot freshness, a dependency generation/version carried by the snapshot and checked at use could detect stale custom-region registrations and other binding/configuration changes.

No implementation changes were made or proposed as required for this comment pass. No scoped suppressions or issue-linked TODO comments were found.

Builds and tests were not run, as requested.
