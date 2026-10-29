#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
TEMP_ROOT="${TMPDIR:-/tmp}"
TEMP_DIR="$(mktemp -d "$TEMP_ROOT/advance-probe.XXXXXX")"
BEFORE="$TEMP_DIR/before-src"
mkdir -p "$BEFORE" "$ROOT/build/diagnostics/implement-03/replay"
git -C "$ROOT" archive 7a43e4e4b3a67b3ac5fa2f4d33990ac4e0101e70 include/slsf/core src/core | tar -x -C "$BEFORE"
LOG="$ROOT/build/diagnostics/implement-03/replay/advance-probe.log"
: > "$LOG"

log_run() {
    "$@" 2>&1 | tee -a "$LOG"
}

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
)

compile_probe() {
    local source_root="$1"
    local current="$2"
    local output="$3"
    local object_dir="$TEMP_DIR/obj-$current"
    mkdir -p "$object_dir"
    local objects=()

    log_run clang++ --version
    for source in "${SOURCES[@]}"; do
        local name="$(basename "${source%.cpp}")"
        local object="$object_dir/$name.o"
        local extra=()
        if [[ "$source" == "src/core/reputation/DiffusionKernelAvx2.cpp" ]]; then
            extra=(-mavx2)
        fi
        log_run clang++ -std=c++23 -O2 -DNDEBUG -I "$source_root/include" \
            "${extra[@]}" -c "$source_root/$source" -o "$object"
        objects+=("$object")
    done

    local probe_object="$object_dir/advance-probe.o"
    log_run clang++ -std=c++23 -O2 -DNDEBUG -DPROBE_CURRENT="$current" \
        -I "$source_root/include" -c "$SCRIPT_DIR/advance-probe.cpp" -o "$probe_object"
    objects+=("$probe_object")
    log_run clang++ -O2 "${objects[@]}" -o "$output"
}

printf 'Benchmark source: %s\n' "$SCRIPT_DIR/advance-probe.cpp" | tee -a "$LOG"
printf 'Baseline source tree: %s (archived from 7a43e4e4b3a67b3ac5fa2f4d33990ac4e0101e70)\n' "$BEFORE" | tee -a "$LOG"
printf 'Current source tree: %s\n' "$ROOT" | tee -a "$LOG"
printf 'Compiler flags: Clang -std=c++23 -O2 -DNDEBUG; AVX2 TU uses -mavx2\n' | tee -a "$LOG"

compile_probe "$BEFORE" 0 "$TEMP_DIR/advance-before.exe"
compile_probe "$ROOT" 1 "$TEMP_DIR/advance-current.exe"

printf '\n=== baseline raw ===\n' | tee -a "$LOG"
log_run "$TEMP_DIR/advance-before.exe" --raw
printf '\n=== current raw cache ===\n' | tee -a "$LOG"
log_run "$TEMP_DIR/advance-current.exe" --raw
printf '\n=== current explicit prepared ===\n' | tee -a "$LOG"
log_run "$TEMP_DIR/advance-current.exe" --prepared

awk '$1 == "RESULT" && /build=current/ && /mode=raw/ {
    for (i = 2; i <= NF; ++i) { split($i, field, "="); if (field[1] == "case") case_name=field[2]; if (field[1] == "work") work=field[2]; if (field[1] == "checksum") checksum=field[2] }
    print case_name "\t" work "\t" checksum
}' "$LOG" > "$TEMP_DIR/current-raw.tsv"
awk '$1 == "RESULT" && /build=current/ && /mode=prepared/ {
    for (i = 2; i <= NF; ++i) { split($i, field, "="); if (field[1] == "case") case_name=field[2]; if (field[1] == "work") work=field[2]; if (field[1] == "checksum") checksum=field[2] }
    print case_name "\t" work "\t" checksum
}' "$LOG" > "$TEMP_DIR/current-prepared.tsv"

awk '$1 == "RESULT" && /build=before/ {
    for (i = 2; i <= NF; ++i) { split($i, field, "="); if (field[1] == "case") case_name=field[2]; if (field[1] == "work") work=field[2]; if (field[1] == "checksum") checksum=field[2] }
    print case_name "\t" work "\t" checksum
}' "$LOG" > "$TEMP_DIR/before.tsv"

if diff -u "$TEMP_DIR/before.tsv" "$TEMP_DIR/current-raw.tsv" | tee -a "$LOG" &&
   diff -u "$TEMP_DIR/current-raw.tsv" "$TEMP_DIR/current-prepared.tsv" | tee -a "$LOG"; then
    printf '\nCHECKSUM_COMPARE=PASS (before == current raw == current prepared)\n' | tee -a "$LOG"
else
    printf '\nCHECKSUM_COMPARE=FAIL\n' | tee -a "$LOG"
    exit 1
fi

printf 'Probe binaries and intermediate objects are outside the repository at %s\n' "$TEMP_DIR" | tee -a "$LOG"
