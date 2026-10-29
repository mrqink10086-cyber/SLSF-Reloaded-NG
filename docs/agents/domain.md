# Domain docs

Use one root `GLOSSARY.md` and `docs/adr/`.
Before exploration, read the glossary and relevant ADRs when present.
Proceed silently when absent; create them lazily through domain modeling.
Use glossary terminology and surface conflicts with existing ADRs.

For native migration, start with `.scratch/native-migration/spec.md`.
It owns requirements, approved choices, the roadmap and acceptance criteria.
For module interfaces, state ownership or runtime wiring, read
`.scratch/native-migration/design.md` before changing the implementation.
For selected layouts, indexes, category masks, time queues and SIMD work, use
`.scratch/native-migration/optimization-plan.md`; it assigns the work and checks
to the existing task groups.
For settings ownership, JSON reloads or menu events, read
`.scratch/native-migration/configuration-design.md`; configuration is shared
across saves, while reputation and registrations belong to each save.
`docs/superpowers/plans/` contains subordinate task instructions;
`docs/superpowers/specs/` explains the location, diffusion and lifecycle structures.
Read the relevant `docs/research/` audit for source facts and `docs/evidence/`
for verification. Update the project spec when an approved requirement changes,
and link implementation reports when a task is completed.
