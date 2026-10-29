# Production `Advance` comparison

## Build and run

The probe compiled and ran directly with Clang 20.1.0 targeting `x86_64-pc-windows-msvc`. Both source trees used `-std=c++23 -O2 -DNDEBUG`; `DiffusionKernelAvx2.cpp` used `-mavx2`. The same probe source directly calls production `slsf::Advance` for every timed operation. No xmake command was run.

- Baseline: Git tree `7a43e4e4b3a67b3ac5fa2f4d33990ac4e0101e70`, extracted into a temporary directory by the archived script.
- Current source: working tree at HEAD `5222a3c31841426bb26c3635a37638abb8f707e5`, including its local changes at probe time. Final reviewed source tree: `28b4160cbba5afc84ce869d3f990648510db74fa`; subsequent test/comment edits did not change production calculations.
- Reproduce from PowerShell: `& 'C:\Program Files\Git\bin\bash.exe' --noprofile --norc 'E:/source/repos/SLSF Reloaded NG/docs/evidence/implementation-03/advance-probe.sh'`. The script requires the baseline Git object in this repository and writes fresh output to `build/diagnostics/implement-03/replay/advance-probe.log`.
- Full compiler and run output: [advance-probe.log](advance-probe.log).

Each measurement is the median of seven rounds, in microseconds per `Advance`. Every round starts from the same deterministic state and makes one untimed warm-up call before its measured loop. `raw` baseline recompiles the graph in each measured `Advance`; current `raw` measures the cached workspace path after its warm-up. Current `prepared` measures `Advance` after `PrepareSettlement` and workspace creation outside the timer; its one-time preparation median is shown separately. Raw warm-up medians are also recorded in the log.

## Results

| Graph and workload | Edges | Calls / round | Baseline raw | Current raw cache | Raw speedup | Current prepared | Prepare cost | Matching checksum |
|---|---:|---:|---:|---:|---:|---:|---|
| Default 21×78, no-op at day 1 | 78 | 4,000 | 3.865 | 0.472 | 8.2× | 0.087 | 8.300 | `d750c52cb153695b` |
| Default 21×78, active small steps | 78 | 1,000 | 7.242 | 2.324 | 3.1× | 1.831 | 12.200 | `a3f8a63f723e844c` |
| Uniform 128×384, no-op at day 1 | 384 | 1,000 | 32.742 | 2.252 | 14.5× | 0.448 | 63.300 | `0746ecec7f99d560` |
| Uniform 128×384, active small steps | 384 | 250 | 109.662 | 13.140 | 8.3× | 11.002 | 66.600 | `1e53eb108770bb21` |
| Uniform 512×1536, no-op at day 1 | 1,536 | 200 | 167.021 | 8.937 | 18.7× | 1.741 | 292.300 | `76261de2a7a66fbe` |
| Uniform 512×1536, active small steps | 1,536 | 50 | 253.172 | 54.722 | 4.6× | 49.092 | 326.100 | `a7f5e301821926cc` |
| 512 custom rows, no-op at day 1 | 1,536 | 100 | 167.420 | 10.000 | 16.7× | 1.709 | 360.700 | `7c3f35bec13fca72` |
| 512 custom rows, active small steps | 1,536 | 20 | 564.355 | 78.710 | 7.2× | 46.415 | 358.800 | `d2ccdf034b79d773` |

All table values are µs except call counts, edges, speedup, and checksums. The checksum column is identical for baseline raw, current raw cache, and current prepared in all eight graph/workload pairs. The script reports `CHECKSUM_COMPARE=PASS`. Checksums cover local, remote and fame values; row clocks, flags and protection; account masks, derived masks and counts; revision/result clocks; custom rows; scheduled-change count; and RNG calls.

## Input and interpretation

- The default case calls `BuildLegacyDefaultGraph` with 21 stable keys and retains its 78 directed edges. Uniform cases use exactly three outgoing edges per account at retention `0.6`.
- Each case runs a no-op workload repeatedly at day `1.0` and an active workload advancing by `0.00001` days per call. Initial local and remote values are deterministic and in `[1, 150]`; parameters cap fame at `150`. The fixed RNG returns its lower bound. The scheduled-change queue is empty, and these short intervals cause zero RNG calls.
- The measured timer covers the full production `Advance`, including graph compilation in the baseline, current cache checks, and production diffusion work. It excludes state construction and post-run checksums. Explicit prepared medians exclude the reported `PrepareSettlement` cost, so their total value depends on how many advances reuse the prepared snapshot. These are native-call measurements, not game frame-rate estimates.
- The host was an Intel Core i5-13600KF. P/E core affinity and clock frequency were not fixed; the repeated calls use warm data. Active intervals exercise propagation and remote relaxation without triggering a decay draw; behavioral tests cover decay and scheduled commands separately.
