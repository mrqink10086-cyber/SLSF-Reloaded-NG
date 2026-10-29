#!/usr/bin/env bash
set -euo pipefail
set -x

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
review_dir="$repo_root/build/diagnostics/review-01-03"
object_dir="$review_dir/memory-probe-objects"
compiler='C:/Program Files/LLVM/bin/clang++.exe'
export PATH="$PATH:/c/Program Files/LLVM/lib/clang/20/lib/windows"
mkdir -p "$object_dir"
cd "$repo_root"

common=(-std=c++23 -I include -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer)
sources=(
    build/diagnostics/review-01-03/memory-probe.cpp
    src/core/location/LocationIndex.cpp
    src/core/location/RegionRegistry.cpp
    src/core/reputation/DiffusionGraph.cpp
    src/core/reputation/DiffusionKernel.cpp
    src/core/persistence/StateCodec.cpp
)

"$compiler" --version
for source in "${sources[@]}"; do
    object="$object_dir/$(basename "${source%.cpp}").o"
    "$compiler" "${common[@]}" -c "$source" -o "$object"
done
"$compiler" "${common[@]}" -mavx2 -c src/core/reputation/DiffusionKernelAvx2.cpp \
    -o "$object_dir/DiffusionKernelAvx2.o"
"$compiler" "${common[@]}" "$object_dir"/*.o -o "$review_dir/memory-probe.exe"
"$review_dir/memory-probe.exe"
