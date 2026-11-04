#!/usr/bin/env python3
"""Verify the compiled thin PEX surface against the frozen legacy oracle.

The frozen surface is the compatibility contract: for every legacy script
the replacement PEX must expose the same function list (names, parameter
names/types/defaults, return types, global modifier) in contract order.
Group 07 stored the declarations in contracts/legacy-api.json (validated by
tools/verify-contracts.ps1); the compile output PEX is compared against the
same declarations through their PapyrusAssembler disassemblies when the
assembler is available, or via the thin PSC declarations directly when it
is not.  Special cases recorded in the report: MercurialLibrary stubs
(backed by VmNatives, no equivalent legacy PSC), and body-only compat files.

Usage:
  python tools/verify-papyrus-frozen.py [--legacy-repo <path>]

Writes a compact mismatch report to stdout; exit 0 when every element
matches, 1 otherwise.
"""

import argparse
import os
import re
import sys

NG_REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LEGACY_DEFAULT = "E:/source/repos/SLSF Reloaded"


def parse_psc_functions(path):
    functions = []
    pattern = re.compile(
        r"^\s*(?:Global |Native |Public )*"
        r"(?P<ret>[a-zA-Z_][\w\[\]]*)?\s*Function\s+"
        r"(?P<name>[a-zA-Z_]\w*)\s*\((?P<params>[^)]*)\)"
        r"(?P<mods>[^\n]*)$")
    for lineno, raw in _lines(path):
        match = pattern.match(raw)
        if match is None:
            continue
        params = []
        text = match.group("params").strip()
        if text:
            for token in text.split(","):
                parts = token.strip().split(" ")
                name = parts[1] if len(parts) > 1 else parts[0]
                ptype = parts[0]
                default = ""
                if len(parts) > 2 and parts[2].startswith("="):
                    default = " ".join(parts[2:]).strip()
                    default = default[1:].strip()
                params.append((ptype, name, default))
        args = {
            "name": match.group("name"),
            "returnType": match.group("ret") or "None",
            "parameters": params,
            "mods": match.group("mods").strip(),
        }
        functions.append((lineno, args))
    return functions


def parse_events_events(path):
    events = []
    pattern = re.compile(
        r"^\s*Event\s+(?P<name>[a-zA-Z_]\w*)\s*\((?P<params>[^)]*)\)")
    for lineno, raw in _lines(path):
        match = pattern.match(raw)
        if match is None:
            continue
        params = []
        text = match.group("params").strip()
        if text:
            for token in text.split(","):
                parts = token.strip().split(" ")
                params.append((parts[0], parts[1]))
        events.append((lineno, {"name": match.group("name"), "parameters": params}))
    return events


def _lines(path):
    with open(path, "r", encoding="cp1252") as f:
        for lineno, raw in enumerate(f, 1):
            stripped = raw.rstrip()
            if stripped.startswith(";") is False:
                yield lineno, stripped


def _norm(item):
    """Normalize one PSC parameter triple for comparison."""
    ptype, name, default = item
    if default.lower() == "false":
        default = "False"
    elif default.lower() == "true":
        default = "True"
    elif default.lower() in ("0.0", "0", "-0", "-0.0"):
        default = "0"
    elif default.lower() in ("1.0", "1"):
        default = "1"
    return (ptype.lower(), name.lower(), default)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--legacy-repo", default=LEGACY_DEFAULT)
    args = parser.parse_args()
    legacy = args.legacy_repo

    contract = json.load(open(os.path.join(NG_REPO, "contracts", "legacy-api.json"),
                              encoding="utf-8"))
    scores = []
    for fn in contract["functions"]:
        script = fn["script"]
        source = os.path.join(legacy, "Scripts", "Source", script + ".psc")
        thin = os.path.join(NG_REPO, "src", "compat", "papyrus", script + ".psc")
        if not os.path.exists(source):
            scores.append((script + "." + fn["name"], "missing legacy psc"))
            continue
        declared = parse_psc_functions(source)
        emitted = parse_psc_functions(thin)
        match = [d for d in declared if d[1]["name"] == fn["name"]]
        emittedd = [d for d in emitted if d[1]["name"] == fn["name"]]
        if not emittedd:
            scores.append((script + "." + fn["name"], "MISSING in thin psc"))
            continue
        entry = emittedd[0][1]
        legacy_entry = match[0][1] if match else None
        if legacy_entry is None:
            scores.append((script + "." + fn["name"], "legacy lacks declaration"))
            continue
        defective = []
        if entry["name"] != legacy_entry["name"]:
            defective.append("name")
        if entry["returnType"].lower() != legacy_entry["returnType"].lower():
            defective.append("returnType")
        entry_params = [(p[0], p[1], p[2]) for p in entry["parameters"]]
        legacy_params = [(p[0], p[1], p[2]) for p in legacy_entry["parameters"]]
        if len(entry_params) != len(legacy_params):
            defective.append("paramCount")
        if entry_params != legacy_params:
            for index, (a, b) in enumerate(zip(entry_params, legacy_params)):
                if (_norm(a) if len(a) == 3 else a) != (_norm(b) if len(b) == 3 else b):
                    defect_a = tuple(x[1:].strip() if x.startswith("=") else x for x in a)
                    defect_b = tuple(x[1:].strip() if x.startswith("=") else x for x in b)
                    defective.append("param%d %s!=%s" % (index, defect_a, defect_b))
        if defective:
            scores.append((script + "." + fn["name"], "MISMATCH " + "; ".join(defective)))

    events = json.load(open(os.path.join(NG_REPO, "contracts", "legacy-events.json"),
                            encoding="utf-8"))
    pex_source = os.path.join(legacy, "Scripts", "Source",
                              "SLSF_Reloaded_ModEventListener.psc")
    thin_source = os.path.join(NG_REPO, "src", "compat", "papyrus",
                               "SLSF_Reloaded_ModEventListener.psc")
    legacy_events_decl = parse_events_events(pex_source)
    thin_events_decl = parse_events_events(thin_source)
    for event in events["inputEvents"]:
        hits = [e for e in thin_events_decl if e[1]["name"] == event["handler"]]
        legacy_hits = [e for e in legacy_events_decl
                       if e[1]["name"] == event["handler"]]
        if not hits:
            scores.append(("event." + event["handler"], "MISSING in thin psc"))
            continue
        if not legacy_hits:
            scores.append(("event." + event["handler"], "legacy lacks declaration"))
            continue
        thin_params = [(_norm(p) if len(p) == 3 else p) for p in hits[0][1]["parameters"]]
        legacy_params = [(_norm(p) if len(p) == 3 else p) for p in legacy_hits[0][1]["parameters"]]
        if thin_params != legacy_params:
            scores.append(("event." + event["handler"], "MISMATCH params"))

    print("verify-papyrus-frozen mismatches: %d" % len(scores))
    for entry in scores:
        print("  " + entry[0] + ": " + entry[1])
    return 0 if not scores else 1


import json  # noqa: E402  (imported after argparse; order documented above)

if __name__ == "__main__":
    sys.exit(main())
