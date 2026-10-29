#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR=$(cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(git -C "$SCRIPT_DIR" rev-parse --show-toplevel)
OUT="$ROOT/build/diagnostics/fix-05"
BASE_DIR="$OUT/performance-baseline"
mkdir -p "$BASE_DIR"
git -C "$ROOT" archive 4c72b3da398994ae3e1c0d4567545ee7c1713696 include src | tar -x -C "$BASE_DIR"
SOURCES=(src/core/location/LocationIndex.cpp src/core/location/RegionRegistry.cpp src/core/numeric/NumericRowOps.cpp src/core/reputation/{DiffusionGraph,DiffusionKernel,DiffusionKernelAvx2,GainRules,ProviderRegistry,ReputationState,Settlement}.cpp src/core/time/Scheduler.cpp src/runtime/{ClockService,Dispatcher}.cpp)
for variant in baseline current; do
    source_root="$ROOT"
    define=()
    if [[ "$variant" == baseline ]]; then source_root="$BASE_DIR"; define=(-DFIX05_BASELINE); fi
    object_dir="$OUT/performance-$variant-objects"
    mkdir -p "$object_dir"
    objects=()
    for source in "${SOURCES[@]}"; do
        object="$object_dir/$(basename "${source%.cpp}").obj"
        flags=()
        if [[ "$source" == *DiffusionKernelAvx2.cpp ]]; then flags=(-mavx2); fi
        clang++ -std=c++23 -O2 -DNDEBUG -I "$source_root/include" "${flags[@]}" -c "$source_root/$source" -o "$object"
        objects+=("$object")
    done
    clang++ -std=c++23 -O2 -DNDEBUG -I "$source_root/include" "${define[@]}" -c "$SCRIPT_DIR/performance-probe.cpp" -o "$object_dir/probe.obj"
    clang++ "${objects[@]}" "$object_dir/probe.obj" -o "$OUT/performance-$variant.exe"
    "$OUT/performance-$variant.exe" > "$OUT/performance-$variant.log"
    printf '%s\n' "$variant"
    cat "$OUT/performance-$variant.log"
done
diff <(grep '^checksum=' "$OUT/performance-baseline.log") <(grep '^checksum=' "$OUT/performance-current.log")
printf 'PERFORMANCE_CHECKSUM=PASS\n'
