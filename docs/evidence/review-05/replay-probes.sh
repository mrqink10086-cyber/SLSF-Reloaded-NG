#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
OUT="$ROOT/build/diagnostics/review-05/archive-replay"
mkdir -p "$OUT/release" "$OUT/instrumented"
export PATH="/c/Program Files/LLVM/bin:/c/Program Files/LLVM/lib/clang/20/lib/windows:$PATH"
SOURCES=(
  src/core/location/LocationIndex.cpp src/core/location/RegionRegistry.cpp
  src/core/numeric/NumericRowOps.cpp src/core/reputation/DiffusionGraph.cpp
  src/core/reputation/DiffusionKernel.cpp src/core/reputation/DiffusionKernelAvx2.cpp
  src/core/reputation/GainRules.cpp src/core/reputation/ProviderRegistry.cpp
  src/core/reputation/ReputationState.cpp src/core/reputation/Settlement.cpp
  src/core/time/Scheduler.cpp src/runtime/ClockService.cpp src/runtime/Dispatcher.cpp
)
release=()
instrumented=()
for source in "${SOURCES[@]}"; do
  name="$(basename "${source%.cpp}")"
  extra=()
  [[ "$name" != DiffusionKernelAvx2 ]] || extra=(-mavx2)
  clang++ -std=c++23 -O2 -DNDEBUG -I "$ROOT/include" "${extra[@]}" \
    -c "$ROOT/$source" -o "$OUT/release/$name.obj"
  release+=("$OUT/release/$name.obj")
  [[ "$name" != NumericRowOps ]] || extra=(-mavx2)
  clang++ -std=c++23 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I "$ROOT/include" "${extra[@]}" -c "$ROOT/$source" -o "$OUT/instrumented/$name.obj"
  instrumented+=("$OUT/instrumented/$name.obj")
done
for probe in algorithm-probe adversarial-probe; do
  clang++ -std=c++23 -O2 -DNDEBUG -I "$ROOT/include" "$SCRIPT_DIR/$probe.cpp" \
    "${release[@]}" -o "$OUT/$probe.exe"
done
clang++ -std=c++23 -O2 -DNDEBUG -I "$ROOT/include" \
  "$SCRIPT_DIR/adversarial-scheduler-reentry.cpp" "$OUT/release/Scheduler.obj" \
  -o "$OUT/adversarial-scheduler-reentry.exe"
clang++ -std=c++23 -O2 -DNDEBUG -I "$ROOT/include" \
  "$SCRIPT_DIR/adversarial-clock-reentry.cpp" "$OUT/release/ClockService.obj" \
  -o "$OUT/adversarial-clock-reentry.exe"
clang++ -std=c++23 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  -I "$ROOT/include" "$SCRIPT_DIR/memory-session-batch.cpp" "${instrumented[@]}" \
  -o "$OUT/memory-session-batch.exe"
clang++ --version | head -n 1
python3 - "$OUT" <<'PY'
from pathlib import Path
import subprocess
import sys

out = Path(sys.argv[1])
for name in ('algorithm-probe', 'adversarial-probe', 'memory-session-batch'):
    result = subprocess.run([str(out / (name + '.exe'))], capture_output=True,
                            text=True, timeout=20)
    print(f'=== {name}: exit={result.returncode} ===', flush=True)
    print(result.stdout, end='', flush=True)
    print(result.stderr, end='', flush=True)
    assert result.returncode == 0, name
    assert 'AddressSanitizer:' not in result.stderr, name
    assert 'runtime error:' not in result.stderr, name
    if name == 'algorithm-probe':
        assert 'set10_then_set20_final=10' in result.stdout
        assert 'second_old_timer=1' in result.stdout
        assert 'final=18' in result.stdout and 'final=20' in result.stdout
        assert 'heap_at_999=1000 heap_at_1000=0' in result.stdout
    if name == 'adversarial-probe':
        for expected in (
            'stale_callback_in_new_session=1',
            'stale_dirty_callback sink_calls=1 pending_dirty=0 active_timers=1',
            'cancel_far_future count=1000 live=0 heap=1000 discarded=0',
            'reordered_published_state status=ok value=22',
            'event_exception_next_round later_event=1 pending=0',
        ):
            assert expected in result.stdout, expected
    if name == 'memory-session-batch':
        assert 'stale_callback_ran_in_new_session=1' in result.stdout
for name, marker in (
    ('adversarial-scheduler-reentry', 'capture_destructor'),
    ('adversarial-clock-reentry', 'wall_supplier'),
):
    try:
        result = subprocess.run([str(out / (name + '.exe'))],
                                capture_output=True, text=True, timeout=3)
    except subprocess.TimeoutExpired as error:
        text = error.stderr or b''
        if isinstance(text, bytes):
            text = text.decode(errors='replace')
        print(f'=== {name}: bounded timeout=3s ===')
        print(text, end='')
        assert marker + '=enter' in text, text
    else:
        print(f'=== {name}: exit={result.returncode} ===')
        print(result.stdout, end='')
        print(result.stderr, end='')
        assert result.returncode != 0, result.returncode
        assert marker + '=enter' in result.stderr, result.stderr
        assert marker + '=returned' not in result.stderr, result.stderr
print('ARCHIVE_REPLAY=PASS')
PY
