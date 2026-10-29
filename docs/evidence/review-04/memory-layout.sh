#!/usr/bin/env bash
set -euo pipefail
repo='E:/source/repos/SLSF Reloaded NG'
cd "$repo"
if command -v clang++ >/dev/null 2>&1; then
  compiler=clang++
elif command -v g++ >/dev/null 2>&1; then
  compiler=g++
else
  printf '%s\n' 'No standalone C++ compiler found on PATH.' >&2
  exit 127
fi
printf 'compiler='
"$compiler" --version | head -n 1
"$compiler" -std=c++23 -Iinclude build/diagnostics/review-04/memory-layout.cpp -o build/diagnostics/review-04/memory-layout.exe
build/diagnostics/review-04/memory-layout.exe
