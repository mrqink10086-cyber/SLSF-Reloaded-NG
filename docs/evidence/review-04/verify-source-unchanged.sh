#!/usr/bin/env bash
set -euo pipefail
cd 'E:/source/repos/SLSF Reloaded NG'
OUT="$PWD/build/diagnostics/review-04"
export GIT_INDEX_FILE="$OUT/final-source.index"
SNAPSHOT=2b3912e81d62b4a26dbe43b20b92e18f5ce522cf
git read-tree "$SNAPSHOT"
git add -- include src tests xmake.lua contracts AGENTS.md GLOSSARY.md .scratch/native-migration docs/agents docs/superpowers docs/evidence/task-groups/04-gameplay-and-engine-facts.md 2> "$OUT/final-source-git.log"
CURRENT=$(git write-tree)
if [[ "$CURRENT" != "$SNAPSHOT" ]]; then
    git diff --stat "$SNAPSHOT" "$CURRENT"
    echo 'SOURCE_UNCHANGED=FAIL'
    exit 1
fi
printf 'SOURCE_UNCHANGED=PASS\nSNAPSHOT=%s\nHEAD=%s\n' "$CURRENT" "$(git rev-parse HEAD)"
