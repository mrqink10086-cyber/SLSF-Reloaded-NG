#!/usr/bin/env bash
set -u

cd '/e/source/repos/SLSF Reloaded NG'
compiler='C:/Program Files/LLVM/bin/clang++.exe'
source='docs/evidence/review-01-02/algorithm-single-write-probe.cpp'
log='build/diagnostics/review-01-03/adversarial-probe.log'
binary='build/diagnostics/review-01-03/adversarial-single-write.exe'

printf 'source=%s\n' "$source" >"$log"
sha256sum "$source" >>"$log"
"$compiler" -std=c++23 -O2 -DNDEBUG -Iinclude -I. "$source" \
    src/core/reputation/ReputationState.cpp \
    src/core/reputation/ProviderRegistry.cpp \
    -o "$binary" >>"$log" 2>&1
compile_result=$?
printf 'compile_exit=%s\n' "$compile_result" >>"$log"
if [[ $compile_result -ne 0 ]]; then
    exit "$compile_result"
fi

"$binary" 1000000 >>"$log" 2>&1
run_result=$?
printf 'run_exit=%s\n' "$run_result" >>"$log"
exit "$run_result"
