#!/usr/bin/env bash
set -euo pipefail
set -x
cd '/e/source/repos/SLSF Reloaded NG'

out='build/diagnostics/review-01-03'
obj="$out/algorithm-obj"
mkdir -p "$obj"
clang='/c/Program Files/LLVM/bin/clang++.exe'
common=(-std=c++23 -O2 -DNDEBUG -Iinclude -I.)

compile() {
    local source="$1"
    local name="${source##*/}"
    name="${name%.cpp}"
    shift
    "$clang" "${common[@]}" "$@" -c "$source" -o "$obj/algorithm-$name.obj"
}

compile build/diagnostics/review-01-03/algorithm-public-probes.cpp
compile build/diagnostics/review-01-03/algorithm-cost-probes.cpp
compile src/core/location/RegionRegistry.cpp
compile src/core/reputation/ProviderRegistry.cpp
compile src/core/reputation/ReputationState.cpp
compile src/core/reputation/DiffusionGraph.cpp
compile src/core/reputation/DiffusionKernel.cpp
compile src/core/reputation/GainRules.cpp
compile src/core/reputation/Settlement.cpp
compile src/core/reputation/DiffusionKernelAvx2.cpp -mavx2

core_objects=(
    "$obj/algorithm-RegionRegistry.obj"
    "$obj/algorithm-ProviderRegistry.obj"
    "$obj/algorithm-ReputationState.obj"
    "$obj/algorithm-DiffusionGraph.obj"
    "$obj/algorithm-DiffusionKernel.obj"
    "$obj/algorithm-GainRules.obj"
    "$obj/algorithm-Settlement.obj"
    "$obj/algorithm-DiffusionKernelAvx2.obj"
)

"$clang" -std=c++23 -O2 -DNDEBUG \
    "$obj/algorithm-algorithm-public-probes.obj" "${core_objects[@]}" \
    -o "$out/algorithm-public-probes.exe"
"$out/algorithm-public-probes.exe"

"$clang" -std=c++23 -O2 -DNDEBUG \
    "$obj/algorithm-algorithm-cost-probes.obj" "${core_objects[@]}" \
    -o "$out/algorithm-cost-probes.exe"
"$out/algorithm-cost-probes.exe"
