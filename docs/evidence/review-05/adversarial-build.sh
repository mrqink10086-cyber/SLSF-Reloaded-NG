#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
BUILD_DIR="$SCRIPT_DIR/adversarial-build"
mkdir -p "$BUILD_DIR"

SOURCES=(
    src/core/location/LocationIndex.cpp
    src/core/location/RegionRegistry.cpp
    src/core/numeric/NumericRowOps.cpp
    src/core/reputation/DiffusionGraph.cpp
    src/core/reputation/DiffusionKernel.cpp
    src/core/reputation/DiffusionKernelAvx2.cpp
    src/core/reputation/GainRules.cpp
    src/core/reputation/ProviderRegistry.cpp
    src/core/reputation/ReputationState.cpp
    src/core/reputation/Settlement.cpp
    src/core/time/Scheduler.cpp
    src/runtime/ClockService.cpp
    src/runtime/Dispatcher.cpp
)

OBJECTS=()
for source in "${SOURCES[@]}"; do
    object="$BUILD_DIR/$(basename "${source%.cpp}").obj"
    extra=()
    if [[ "$source" == "src/core/reputation/DiffusionKernelAvx2.cpp" ]]; then
        extra=(-mavx2)
    fi
    clang++ -std=c++23 -O2 -DNDEBUG -I "$ROOT/include" "${extra[@]}" \
        -c "$ROOT/$source" -o "$object"
    OBJECTS+=("$object")
done

clang++ -std=c++23 -O2 -DNDEBUG -I "$ROOT/include" \
    "$SCRIPT_DIR/adversarial-probe.cpp" "${OBJECTS[@]}" \
    -o "$BUILD_DIR/adversarial-probe.exe"
clang++ -std=c++23 -O2 -DNDEBUG -I "$ROOT/include" \
    "$SCRIPT_DIR/adversarial-scheduler-reentry.cpp" \
    "$BUILD_DIR/Scheduler.obj" -o "$BUILD_DIR/adversarial-scheduler-reentry.exe"
clang++ -std=c++23 -O2 -DNDEBUG -I "$ROOT/include" \
    "$SCRIPT_DIR/adversarial-clock-reentry.cpp" \
    "$BUILD_DIR/ClockService.obj" -o "$BUILD_DIR/adversarial-clock-reentry.exe"

python3 - "$BUILD_DIR/adversarial-probe.exe" \
    "$BUILD_DIR/adversarial-scheduler-reentry.exe" \
    "$BUILD_DIR/adversarial-clock-reentry.exe" <<'PY'
import subprocess
import sys

for executable in sys.argv[1:]:
    print(f"=== {executable} ===", flush=True)
    try:
        result = subprocess.run([executable], check=False, capture_output=True,
                                text=True, timeout=3)
    except subprocess.TimeoutExpired as error:
        print("result=timeout (bounded to 3s)")
        if error.stdout:
            print(error.stdout.decode(errors="replace") if isinstance(error.stdout, bytes)
                  else error.stdout)
        continue
    print(f"exit_code={result.returncode}")
    print(result.stdout, end="")
    if result.stderr:
        print("stderr:")
        print(result.stderr, end="")
PY
