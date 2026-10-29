# Issue tracker: Local Markdown

Specs and issues live under `.scratch/<feature-slug>/`.
- Spec: `spec.md`.
- Tickets: `issues/<NN>-<slug>.md`, numbered from `01`, one file per ticket.
- Triage state: a `Status:` line using `triage-labels.md`.
- Dependencies: a `Blocked by: NN, NN` line.
- Discussion: append under `## Comments`.

"Publish to the issue tracker" means create or update these local files.
"Fetch a ticket" means read its referenced file.

For wayfinding, use `map.md` and one child file per ticket under `issues/`.
Record `Type: research|prototype|grilling|task` and `Status: claimed|resolved`.
Claim before work; resolve with an `## Answer` and a decision pointer in the map.
A ticket is unblocked when all referenced blockers are resolved.
Choose the first open, unblocked, unclaimed ticket by number.
