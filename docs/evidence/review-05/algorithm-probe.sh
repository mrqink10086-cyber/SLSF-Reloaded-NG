#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
TEMP_DIR="$(mktemp -d "${TMPDIR:-/tmp}/algorithm-probe.XXXXXX")"
trap 'rm -rf "$TEMP_DIR"' EXIT

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
    object="$TEMP_DIR/$(basename "${source%.cpp}").obj"
    extra=()
    if [[ "$source" == "src/core/reputation/DiffusionKernelAvx2.cpp" ]]; then
        extra=(-mavx2)
    fi
    clang++ -std=c++23 -O2 -DNDEBUG -I "$ROOT/include" "${extra[@]}" \
        -c "$ROOT/$source" -o "$object"
    OBJECTS+=("$object")
done

PROBE_OBJ="$TEMP_DIR/algorithm-probe.obj"
clang++ -std=c++23 -O2 -DNDEBUG -I "$ROOT/include" \
    -c "$SCRIPT_DIR/algorithm-probe.cpp" -o "$PROBE_OBJ"
OBJECTS+=("$PROBE_OBJ")
clang++ -O2 "${OBJECTS[@]}" -o "$TEMP_DIR/algorithm-probe.exe"

printf 'Compiler: '
clang++ --version | head -n 1
printf 'Target: '
clang++ -dumpmachine
printf 'CPU: '
powershell.exe -NoProfile -Command '(Get-CimInstance Win32_Processor | Select-Object -First 1 -ExpandProperty Name)'
printf 'Flags: -std=c++23 -O2 -DNDEBUG; DiffusionKernelAvx2.cpp also -mavx2\n'
printf 'Seed: 1170; repetitions: 7; due-count calls per sample: 500\n'
"$TEMP_DIR/algorithm-probe.exe"
