#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/../../.."
out='build/diagnostics/algorithm-chain-research'
mkdir -p "$out"
clang='/c/Program Files/LLVM/bin/clang++.exe'
common=(-std=c++23 -O2 -DNDEBUG -Iinclude -I.)
compile() {
    local source="$1"
    local name="${source##*/}"
    name="${name%.cpp}"
    shift
    "$clang" "${common[@]}" "$@" -c "$source" -o "$out/chain-$name.obj"
}
compile docs/evidence/algorithm-chain-research/chain-probe.cpp
compile src/core/location/RegionRegistry.cpp
compile src/core/reputation/DiffusionGraph.cpp
compile src/core/reputation/DiffusionKernel.cpp
compile src/core/reputation/DiffusionKernelAvx2.cpp -mavx2
"$clang" -std=c++23 -O2 -DNDEBUG "$out/chain-chain-probe.obj" \
    "$out/chain-RegionRegistry.obj" "$out/chain-DiffusionGraph.obj" \
    "$out/chain-DiffusionKernel.obj" "$out/chain-DiffusionKernelAvx2.obj" \
    -o "$out/chain-probe.exe"
"$out/chain-probe.exe" | tee "$out/chain-probe.log"
