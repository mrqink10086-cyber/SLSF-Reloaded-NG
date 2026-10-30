#!/usr/bin/env python3
"""Generate the frozen Papyrus binding table and thin PSC facades.

Reads the frozen legacy API contract (contracts/legacy-api.json, group 01)
and emits:

  1. src/compat/papyrus_function_table.inc
       A C++ aggregate table with all 124 legacy function declarations
       (86 public + 38 internal), their parameters, instance bindings and
       invalid-input results.  Included by src/compat/PapyrusBindings.cpp.

  2. src/compat/papyrus/<ScriptName>.psc
       One thin PSC per original script: the original public/non-public
       function declarations (names, PSC types, order), forwarding to the
       native module.  This is the frozen downstream-client surface: the
       PEXs compiled from these declarations are the compatibility oracle.

The JSON is the single source of truth; re-run this script only when the
frozen contract changes (it is validated by tools/verify-contracts.ps1).
"""

import json
import os
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CONTRACT = os.path.join(REPO, "contracts", "legacy-api.json")
RECORDS = os.path.join(REPO, "contracts", "legacy-records.json")
INC_OUT = os.path.join(REPO, "src", "compat", "papyrus_function_table.inc")
PSC_DIR = os.path.join(REPO, "src", "compat", "papyrus")

# PSC type name -> slsf::LegacyValueType
TYPE_MAP = {
    "None": "None",
    "Bool": "Bool",
    "Int": "Int",
    "Float": "Float",
    "String": "String",
    "Form": "Form",
    "ObjectReference": "Form",
    "Location": "Form",
    "Actor": "Actor",
    "Quest": "Quest",
    "ReferenceAlias": "ReferenceAlias",
    "String[]": "StringArray",
    "Form[]": "FormArray",
    "Actor[]": "ActorArray",
}
# Everything else is a script-object type in PSC.
SCRIPT_OBJECT_FALLBACK = "Script"

BINDINGS = {
    "global-native": "BindingKind::GlobalNative",
    "global": "BindingKind::Global",
    "quest-script": "BindingKind::Quest",
    "reference-alias": "BindingKind::ReferenceAlias",
}


def cpp_string(s: str) -> str:
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def map_type(psc_type: str) -> str:
    return TYPE_MAP.get(psc_type, SCRIPT_OBJECT_FALLBACK)


def main() -> int:
    with open(CONTRACT, "r", encoding="utf-8") as f:
        doc = json.load(f)
    with open(RECORDS, "r", encoding="utf-8") as f:
        records = {r["formId"]: r for r in json.load(f)["records"]}

    def quest_key(inst):
        raw_id = inst.get("formId") or inst.get("questFormId")
        if raw_id is None:
            return "", 0
        record = records[raw_id]
        raw = int(raw_id, 16)
        if record["signature"] != "QUST" or raw >> 24 != 0x06:
            raise ValueError("expected frozen full-plugin quest ID: " + raw_id)
        return record["plugin"], raw & 0x00FFFFFF

    functions = doc["functions"]
    if len(functions) != doc["functionCount"]:
        print("function count mismatch", file=sys.stderr)
        return 1

    # ---- 1. C++ table -----------------------------------------------------
    lines = []
    lines.append("// GENERATED FILE - do not edit.")
    lines.append("// Source: contracts/legacy-api.json (frozen, group 01).")
    lines.append("// Regenerate with: python tools/gen-papyrus-bindings.py")
    lines.append("")
    lines.append("namespace slsf::compat::detail {")
    lines.append("")
    # Emit each function as a full aggregate with an inline parameter list.
    lines.append("static const PapyrusFunction kTable[] = {")
    for fn in functions:
        inst = fn.get("instance") or {}
        binding = BINDINGS.get(inst.get("kind", ""), "BindingKind::Global")
        quest_plugin, quest_local = quest_key(inst)
        quest_editor = inst.get("editorId") or inst.get("questEditorId") or ""
        alias_index = inst.get("aliasIndex", -1)
        alias_name = inst.get("aliasName", "")
        invalid = fn.get("invalidInputResult", "")
        params = []
        for p in fn.get("parameters", []):
            raw = p.get("default")
            has_default = raw is not None
            d_bool = str(raw).strip().lower() == "true" if has_default else False
            try:
                d_int = int(raw) if has_default else 0
            except (TypeError, ValueError):
                d_int = 0
            try:
                d_float = float(raw) if has_default else 0.0
            except (TypeError, ValueError):
                d_float = 0.0
            d_str = str(raw) if has_default else ""
            params.append(
                "{%s, LegacyValueType::%s, %s, %s, %d, %s, %s}" % (
                    cpp_string(p["name"]),
                    map_type(p["type"]),
                    "true" if has_default else "false",
                    "true" if d_bool else "false",
                    d_int,
                    repr(d_float) if d_float != int(d_float) else f"{int(d_float)}.0",
                    cpp_string(d_str),
                )
            )
        param_init = "{%s}" % ", ".join(params) if params else "{}"
        lines.append("    {")
        lines.append("        %s, %s, %s," % (
            cpp_string(fn["id"]), cpp_string(fn["script"]), cpp_string(fn["name"])))
        lines.append("        %s, LegacyValueType::%s," % (
            cpp_string(fn["returnType"]), map_type(fn["returnType"])))
        lines.append("        %s," % param_init)
        lines.append("        %s, %s, %s, %s, %s," % (
            "true" if fn["isGlobal"] else "false",
            "true" if fn["isNative"] else "false",
            "true" if fn.get("isLatent") else "false",
            "true" if fn["isPublic"] else "false",
            cpp_string(fn["classification"])))
        lines.append("        %s," % cpp_string(invalid))
        lines.append("        %s, {%s, 0x%Xu}, %s," % (
            binding, cpp_string(quest_plugin), quest_local, cpp_string(quest_editor)))
        lines.append("        %d, %s}," % (alias_index, cpp_string(alias_name)))
    lines.append("};")
    lines.append("")
    lines.append("static_assert(std::size(kTable) == 124,")
    lines.append("              \"legacy function table must hold all 124 declarations\");")
    lines.append("")
    lines.append("} // namespace slsf::compat::detail")
    lines.append("")

    os.makedirs(os.path.dirname(INC_OUT), exist_ok=True)
    with open(INC_OUT, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines))
    print("wrote %s (%d functions)" % (INC_OUT, len(functions)))

    # ---- 2. Thin PSC facades ---------------------------------------------
    os.makedirs(PSC_DIR, exist_ok=True)
    by_script = {}
    for fn in functions:
        by_script.setdefault(fn["script"], []).append(fn)

    for script, fns in sorted(by_script.items()):
        out = []
        out.append(";%s.psc - thin Papyrus facade (group 07, task B06)." % script)
        out.append(";")
        out.append("; Frozen downstream-client declarations.  Every function forwards")
        out.append("; to the native SLSF Reloaded NG module; no business logic lives")
        out.append("; in Papyrus.  Compiled to PEX from these declarations, the frozen")
        out.append("; PEXs are the compatibility oracle against the original plugin.")
        out.append(";")
        out.append("; Original instance bindings (see contracts/legacy-api.json):")
        for fn in fns:
            inst = fn.get("instance") or {}
            kind = inst.get("kind", "")
            plugin, local_id = quest_key(inst)
            if kind == "quest-script":
                out.append(";   %s -> quest %s (%s, local 0x%X)" % (
                    fn["name"], inst.get("editorId"), plugin, local_id))
            elif kind == "reference-alias":
                out.append(";   %s -> alias %s on quest %s (%s, local 0x%X)" % (
                    fn["name"], inst.get("aliasName"), inst.get("questEditorId"),
                    plugin, local_id))
            else:
                out.append(";   %s -> %s" % (fn["name"], kind or "global"))
        out.append(";")
        out.append("")
        # Group functions; keep contract order (it is the registration order).
        for fn in fns:
            ret = fn["returnType"]
            ret_str = "" if ret == "None" else ret + " "
            params = []
            for p in fn.get("parameters", []):
                params.append("%s %s" % (p["type"], p["name"]))
            params_str = ", ".join(params)
            pub = "Public " if fn["isPublic"] else ""
            out.append("%s%sFunction %s(%s)" % (pub, ret_str, fn["name"], params_str))
        out.append("")
        path = os.path.join(PSC_DIR, script + ".psc")
        with open(path, "w", encoding="utf-8", newline="\n") as f:
            f.write("\n".join(out))
    print("wrote %d thin PSC files to %s" % (len(by_script), PSC_DIR))
    return 0


if __name__ == "__main__":
    sys.exit(main())
