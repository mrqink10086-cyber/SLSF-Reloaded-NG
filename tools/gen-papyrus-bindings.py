#!/usr/bin/env python3

import json
import os
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CONTRACT = os.path.join(REPO, "contracts", "legacy-api.json")
RECORDS = os.path.join(REPO, "contracts", "legacy-records.json")
EVENTS = os.path.join(REPO, "contracts", "legacy-events.json")
INC_OUT = os.path.join(REPO, "src", "compat", "papyrus_function_table.inc")
VM_INC_OUT = os.path.join(REPO, "src", "compat", "papyrus_vm_table.inc")
PSC_DIR = os.path.join(REPO, "src", "compat", "papyrus")

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
SCRIPT_OBJECT_FALLBACK = "Script"

BINDINGS = {
    "global-native": "BindingKind::GlobalNative",
    "global": "BindingKind::Global",
    "quest-script": "BindingKind::Quest",
    "reference-alias": "BindingKind::ReferenceAlias",
}

VM_CPP_TYPES = {
    "None": "void",
    "Bool": "bool",
    "Int": "std::int32_t",
    "Float": "float",
    "String": "std::string",
    "Form": "RE::TESForm*",
    "Location": "RE::BGSLocation*",
    "Actor": "RE::Actor*",
    "Quest": "RE::TESQuest*",
    "ReferenceAlias": "RE::BGSBaseAlias*",
    "String[]": "std::vector<std::string>",
    "Form[]": "std::vector<RE::TESForm*>",
    "Actor[]": "RE::BSScript::reference_array<RE::Actor*>",
    "Script": "RE::TESForm*",
    "Unknown": "RE::TESForm*",
}

RECORD_SIGNATURE_TYPES = {
    "GLOB": "GlobalVariable",
    "KYWD": "Keyword",
    "SPEL": "Spell",
    "MGEF": "MagicEffect",
    "QUST": "Quest",
    "SCPT": "Form",
}

EVENT_HANDLER_BRIDGE = "SLSFR_EventSink"
BRIDGE_SCRIPT_BASE = "Form"


def cpp_string(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def map_type(psc_type):
    return TYPE_MAP.get(psc_type, SCRIPT_OBJECT_FALLBACK)


def psc_literal(psc_type, raw):
    if raw is None:
        return None
    text = str(raw).strip()
    if psc_type == "String":
        scaled = cpp_string(text)
        if scaled[1:-1] == text and (text.startswith('"') is False):
            return '"%s"' % text.replace('"', '\\"')
        return scaled
    if psc_type == "Bool":
        return "true" if text.lower() == "true" else "false"
    if psc_type == "Int":
        try:
            return str(int(float(text)))
        except ValueError:
            return "0"
    if psc_type == "Float":
        try:
            value = float(text)
        except ValueError:
            return "0.0"
        if value == int(value):
            return "%.1f" % value
        return repr(value)
    if psc_type in ("None", "Form", "Location", "Actor", "Quest", "ReferenceAlias"):
        return "None"
    return "None"


def script_type_of(inst):
    kind = inst.get("kind", "")
    return {"quest-script": "Quest", "reference-alias": "ReferenceAlias"}.get(kind)


def base_script_for_script_name(name):
    if name in ("SLSF_Reloaded_PlayerScript", "SLSF_Reloaded_SexAnimationAnalyzer"):
        return "ReferenceAlias"
    return "Quest"


def signature_records(records):
    resolved = {}
    for form_id, record in records.items():
        resolved[form_id] = record
    return resolved


def property_type_for(record, value):
    if not isinstance(value, dict):
        return None
    alias = value.get("alias")
    form_id = value.get("formId")
    if alias is not None and int(alias) >= 0:
        return "ReferenceAlias"
    if form_id == "0x00000014":
        return "ObjectReference"
    record = record.get(form_id)
    if record is None:
        return "Form"
    t = record["signature"]
    if t in RECORD_SIGNATURE_TYPES:
        return RECORD_SIGNATURE_TYPES[t]
    return "Form"


def body_for(fn):
    ret = fn["returnType"]
    if ret == "None":
        return ""
    if ret == "Bool":
        return "return false"
    if ret == "Int":
        return "return 0"
    if ret == "Float":
        return "return 0.0"
    if ret == "String":
        return "return None"
    return "return None"


def psc_params(fn):
    parts = []
    for p in fn.get("parameters", []):
        text = "%s %s" % (p["type"], p["name"])
        default = p.get("default")
        if default is not None:
            literal = psc_literal(p["type"], default)
            if literal is not None:
                text += " = " + literal
        parts.append(text)
    return ", ".join(parts)


def event_psc_params(params):
    parts = []
    for p in params:
        text = "%s %s" % (p["type"], p["name"])
        default = p.get("default")
        if default is not None:
            literal = psc_literal(p["type"], default)
            if literal is not None:
                text += " = " + literal
        parts.append(text)
    return ", ".join(parts)


def main():
    with open(CONTRACT, "r", encoding="utf-8") as f:
        doc = json.load(f)
    with open(RECORDS, "r", encoding="utf-8") as f:
        records = {r["formId"]: r for r in json.load(f)["records"]}
    with open(EVENTS, "r", encoding="utf-8") as f:
        events = json.load(f)

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

    lines = []
    lines.append("// GENERATED FILE - do not edit.")
    lines.append("// Source: contracts/legacy-api.json (frozen, group 01).")
    lines.append("// Regenerate with: python tools/gen-papyrus-bindings.py")
    lines.append("")
    lines.append("namespace slsf::compat::detail {")
    lines.append("")
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
                "{%s, LegacyValueType::%s, %s, %s, %d, %s, %s, %s}" % (
                    cpp_string(p["name"]),
                    map_type(p["type"]),
                    "true" if has_default else "false",
                    "true" if d_bool else "false",
                    d_int,
                    repr(d_float) if d_float != int(d_float) else f"{int(d_float)}.0",
                    cpp_string(d_str),
                    cpp_string(p["type"]),
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

    def cpp_type(psc_type):
        return VM_CPP_TYPES.get(psc_type, "RE::TESForm*")

    vm_lines = []
    vm_lines.append("// GENERATED FILE - do not edit.")
    vm_lines.append("// Source: contracts/legacy-api.json (frozen, group 01).")
    vm_lines.append("// Regenerate with: python tools/gen-papyrus-bindings.py")
    vm_lines.append("")
    vm_lines.append(
        "static const detail::VmSignatureTableEntry kVmSignatures[] = {")
    for fn in functions:
        inst = fn.get("instance") or {}
        kind = inst.get("kind", "")
        is_static = kind in ("global", "global-native") or fn["isGlobal"]
        params = [{"name": p["name"], "type": cpp_type(p["type"])}
                  for p in fn.get("parameters", [])]
        vm_lines.append("    {")
        vm_lines.append("        %s," % cpp_string(fn["id"]))
        vm_lines.append("        %s, %s," % (
            "true" if is_static else "false", cpp_type(fn["returnType"])))
        vm_lines.append("        {%s}," % ", ".join(
            "{%s, %s}" % (cpp_string(p["name"]), p["type"]) for p in params))
        vm_lines.append("    },")
    vm_lines.append("};")
    vm_lines.append("")
    with open(VM_INC_OUT, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(vm_lines))
    print("wrote %s (%d signatures)" % (VM_INC_OUT, len(functions)))

    os.makedirs(PSC_DIR, exist_ok=True)
    by_script = {}
    for fn in functions:
        by_script.setdefault(fn["script"], []).append(fn)

    for script, fns in sorted(by_script.items()):
        out = []
        base = base_script_for_script_name(script)
        out.append("Scriptname %s extends %s" % (script, base))
        out.append("")
        vmad_props = vmad_properties_for(records, script)
        for ptype, pname in vmad_props:
            out.append("%s Property %s Auto" % (ptype, pname))
        if vmad_props:
            out.append("")
        for fn in fns:
            params = psc_params(fn)
            ret = "" if fn["returnType"] == "None" else fn["returnType"] + " "
            mods = []
            if fn["isGlobal"]:
                mods.append("Global")
            if fn["id"] in PAPYRUS_BODY_GLOBALS:
                out.append("%sFunction %s(%s) Global" % (
                    ret, fn["name"], params))
                out.append(PAPYRUS_BODY_GLOBALS[fn["id"]])
                out.append("EndFunction")
                out.append("")
                continue
            if script == "SLSF_Reloaded_ModEventListener" and fn["name"] == "RegisterExternalEvents":
                continue
            if fn["isPublic"]:
                mods.append("Native")
                out.append("%sFunction %s(%s) %s" % (
                    ret, fn["name"], params, " ".join(mods)))
            else:
                body = body_for(fn)
                if body:
                    out.append("%sFunction %s(%s)" % (
                        ret, fn["name"], params))
                    out.append("    " + body)
                    out.append("EndFunction")
                else:
                    out.append("%sFunction %s(%s)" % (
                        ret, fn["name"], params))
                    out.append("EndFunction")
            out.append("")
        if script == "SLSF_Reloaded_ModEventListener":
            events_lines = ModEventListenerExtras(events, PAPYRUS_BODY_GLOBALS)
            out.extend(events_lines)
        path = os.path.join(PSC_DIR, script + ".psc")
        with open(path, "w", encoding="utf-8", newline="\n") as f:
            f.write("\n".join(out))
    print("wrote %d thin PSC files to %s" % (len(by_script), PSC_DIR))

    sink = []
    sink.append("Scriptname %s extends %s Hidden" % (EVENT_HANDLER_BRIDGE, BRIDGE_SCRIPT_BASE))
    sink.append("")
    for event in events["inputEvents"]:
        params = event_psc_params(event["parameters"])
        sink.append("Function %s(%s) Global Native" % (event["handler"], params))
    sink.append("")
    path = os.path.join(PSC_DIR, EVENT_HANDLER_BRIDGE + ".psc")
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(sink))
    print("wrote %s (%d handlers)" % (path, len(events["inputEvents"])))

    return 0


PAPYRUS_BODY_GLOBALS = {
    "SLSF_Reloaded_ModIntegration.GetFHUData":
        '    return Game.GetFormFromFile(0xD63, "sr_FillherUp.esp") as sr_InflateQuest',
    "SLSF_Reloaded_ModIntegration.GetPWData":
        '    return Game.GetFormFromFile(0xD63, "Public Whore.esp") as PW_MainLoopScript',
    "SLSF_Reloaded_ModIntegration.GetLegacyConfig":
        '    return Game.GetFormFromFile(0x00016AB9, "SexLab - Sexual Fame [SLSF].esm") as SLSF_Configuration',
}


def vmad_properties_for(records, script):
    with open(os.path.join(REPO, "contracts", "legacy-property-types.json"), encoding="utf-8") as f:
        declared = json.load(f).get(script, {})
    matched = None
    for _, record in records.items():
        if record["signature"] != "QUST":
            continue
        vmad = record.get("vmad")
        if not vmad or not isinstance(vmad, dict):
            continue
        for entry in vmad.get("scripts", []):
            if entry.get("name") == script and entry.get("properties"):
                matched = entry
                break
        if matched:
            break
    if matched is None or not matched.get("properties"):
        return []
    out = []
    for prop in matched["properties"]:
        name = prop.get("name", "")
        if not name:
            continue
        value = prop.get("value")
        ptype = "Form"
        if isinstance(value, list):
            has_string = [v for v in value if isinstance(v, str)]
            has_float = [v for v in value if isinstance(v, float)]
            has_bool = [v for v in value if isinstance(v, bool)]
            if has_string:
                ptype = "String[]"
            elif has_float:
                ptype = "Float[]"
            elif has_bool:
                ptype = "Bool[]"
            else:
                ptype = "Int[]"
        elif value is None:
            ptype = "Int"
        elif isinstance(value, int):
            ptype = "Int"
        elif isinstance(value, bool):
            ptype = "Bool"
        elif isinstance(value, float):
            ptype = "Float"
        elif isinstance(value, str):
            ptype = "String"
        elif isinstance(value, dict):
            found = property_type_for(records, value)
            if found:
                ptype = found
        out.append((declared.get(name, ptype), name))
    return out


def ModEventListenerExtras(events, bodies):
    lines = []
    lines.append("")
    lines.append("Event OnInit()")
    lines.append("    RegisterExternalEvents()")
    lines.append("EndEvent")
    lines.append("")
    lines.append("Function RegisterExternalEvents()")
    for event in events["inputEvents"]:
        lines.append('    RegisterForModEvent("%s", "%s")' % (
            event["event"], event["handler"]))
    lines.append("EndFunction")
    lines.append("")
    for event in events["inputEvents"]:
        params = event_psc_params(event["parameters"])
        lines.append("Event %s(%s)" % (event["handler"], params))
        call_args = ", ".join((p["name"] for p in event["parameters"]))
        lines.append("    %s.%s(%s)" % (EVENT_HANDLER_BRIDGE, event["handler"], call_args))
        lines.append("EndEvent")
    return lines


if __name__ == "__main__":
    sys.exit(main())
