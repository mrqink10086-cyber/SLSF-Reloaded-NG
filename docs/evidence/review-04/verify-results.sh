#!/usr/bin/env bash
set -euo pipefail
cd 'E:/source/repos/SLSF Reloaded NG'
OUT=build/diagnostics/review-04
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/verify-contracts.ps1 -NegativeFixtures > "$OUT/contracts.log" 2>&1
python tools/verify-task-books.py > "$OUT/task-books.log" 2>&1
python - <<'PY' > "$OUT/junit-summary.txt"
from pathlib import Path
import xml.etree.ElementTree as ET
for name in ('core', 'boundary', 'core-asan', 'boundary-asan', 'differential'):
    root = ET.parse(Path('build/test-results') / f'{name}.xml').getroot()
    cases = root.findall('.//testcase')
    keys = [(c.get('classname'), c.get('name')) for c in cases]
    suites = root.findall('.//testsuite')
    if root.tag == 'testsuite':
        suites.insert(0, root)
    assertions = sum(int(s.get('tests', 0)) for s in suites)
    failures = sum(int(s.get('failures', 0)) for s in suites)
    errors = sum(int(s.get('errors', 0)) for s in suites)
    assert len(keys) == len(set(keys)), (name, 'duplicate case')
    assert failures == errors == 0, (name, failures, errors)
    print(f'{name}: xml_instances={len(cases)}, assertions={assertions}, failures={failures}, errors={errors}, unique=yes')
PY
cat "$OUT/junit-summary.txt"
cat "$OUT/task-books.log"
tail -n 3 "$OUT/contracts.log"
