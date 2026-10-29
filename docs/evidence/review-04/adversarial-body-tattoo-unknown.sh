#!/usr/bin/env bash
set -euo pipefail

repo='/e/source/repos/SLSF Reloaded NG'
review="$repo/build/diagnostics/review-04"
clang++ -std=c++23 -Wall -Wextra -Werror -I "$repo/include" \
  "$review/adversarial-body-tattoo-unknown.cpp" \
  "$repo/src/core/gameplay/Visibility.cpp" \
  -o "$review/adversarial-body-tattoo-unknown.exe"
"$review/adversarial-body-tattoo-unknown.exe" | tee "$review/adversarial-body-tattoo-unknown.log"
