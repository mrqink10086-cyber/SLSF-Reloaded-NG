#!/usr/bin/env bash
set -euo pipefail
archive="$(cd "$(dirname "$0")" && pwd)"
repo="$(cd "$archive/../../.." && pwd)"
out="$repo/build/diagnostics/review-04/archive-replay"
mkdir -p "$out"
cd "$repo"
compiler="${CXX:-clang++}"
"$compiler" --version

build_run() {
    local name="$1"
    shift
    local binary="$out/$name.exe"
    local command=("$compiler" -std=c++23 -I "$repo/include" "$archive/$name.cpp" "$@" -o "$binary")
    printf '\nPROBE=%s\nCOMMAND:' "$name"
    printf ' %q' "${command[@]}"
    printf '\n'
    "${command[@]}"
    printf 'BUILD_EXIT_CODE=0\n'
    "$binary"
    printf 'EXIT_CODE=0\n'
}

build_run memory-layout | tee "$out/memory-layout.log"
build_run algorithm-gameplay-probe -O0 -g "$repo/src/core/gameplay/Visibility.cpp" | tee "$out/algorithm-gameplay-probe.log"
build_run algorithm-performance-probe -O2 -DNDEBUG "$repo/src/core/gameplay/Witness.cpp" | tee "$out/algorithm-performance-probe.log"
build_run adversarial-heavy-bondage -Wall -Wextra -Werror "$repo/src/core/gameplay/Visibility.cpp" | tee "$out/adversarial-heavy-bondage.log"
build_run adversarial-body-tattoo-unknown -Wall -Wextra -Werror "$repo/src/core/gameplay/Visibility.cpp" | tee "$out/adversarial-body-tattoo-unknown.log"
printf '\nARCHIVE_REPLAY=PASS\n'
