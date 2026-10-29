#!/usr/bin/env bash
set -u
root="$(cd "$(dirname "$0")/../../.." && pwd)"
cd "$root"
binary="build/diagnostics/review-04/algorithm-performance-probe.exe"
source="build/diagnostics/review-04/algorithm-performance-probe.cpp"
echo "COMMAND: clang++ -std=c++23 -O2 -DNDEBUG -I include $source src/core/gameplay/Witness.cpp -o $binary"
if clang++ -std=c++23 -O2 -DNDEBUG -I include "$source" src/core/gameplay/Witness.cpp -o "$binary"; then
    echo "BUILD_EXIT_CODE=0"
else
    result=$?
    echo "BUILD_EXIT_CODE=$result"
    exit "$result"
fi
echo "COMMAND: $binary"
if "$binary"; then
    result=0
else
    result=$?
fi
echo "EXIT_CODE=$result"
exit "$result"
