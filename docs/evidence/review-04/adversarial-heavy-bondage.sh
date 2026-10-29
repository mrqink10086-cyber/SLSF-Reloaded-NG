#!/usr/bin/env bash
set -euo pipefail

repo='/e/source/repos/SLSF Reloaded NG'
review="$repo/build/diagnostics/review-04"
clang++ -std=c++23 -Wall -Wextra -Werror -I "$repo/include" \
  "$review/adversarial-heavy-bondage.cpp" \
  "$repo/src/core/gameplay/Visibility.cpp" \
  -o "$review/adversarial-heavy-bondage.exe"
"$review/adversarial-heavy-bondage.exe" | tee "$review/adversarial-heavy-bondage.log"
