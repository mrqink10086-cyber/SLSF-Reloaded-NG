#!/usr/bin/env bash
set -euo pipefail
cd 'E:/source/repos/SLSF Reloaded NG'
OUT="$PWD/build/diagnostics/review-04"
exec > "$OUT/existing-checks.log" 2>&1
printf 'COMMAND: powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/run-native-tests.ps1 -Mode all -Seed 1170 -Runs 100000 -Jobs 20\n'
set +e
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/run-native-tests.ps1 -Mode all -Seed 1170 -Runs 100000 -Jobs 20
result=$?
set -e
printf 'EXIT_CODE=%s\n' "$result"
test "$result" -eq 0
printf 'COMMAND: xmake build -y SLSFEngineProbe SLSFReloadedNG\n'
xmake build -y SLSFEngineProbe SLSFReloadedNG
printf 'EXIT_CODE=0\nCOMMAND: build/windows/x64/releasedbg/SLSFEngineProbe.exe\n'
build/windows/x64/releasedbg/SLSFEngineProbe.exe > "$OUT/target-probe.log" 2>&1
cat "$OUT/target-probe.log"
printf 'EXIT_CODE=0\n'
