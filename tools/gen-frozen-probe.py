#!/usr/bin/env python3
"""Generate the frozen-client probe PSC used by the group 11 game run.

The probe is compiled against the LEGACY repository's Papyrus sources (the
declarations the frozen client surface was published with) plus the vanilla
source set, so its bytecode represents the un-recompiled calling convention
that the replacement provider must satisfy.  At runtime the VM resolves each
call onto the replacement PEXs shipped by the NG package; the probe sends the
52 frozen input events through ModEvent, calls the public 86 through quest
instance casts, and reports results through SLSF_Reloaded_Logger.Log (a
frozen native surface whose replacement lives in the NG DLL).

The probe cannot register the 7 reply events from a static context, so the
frozen reply check is done by the plugin's own reply sender (recorded in the
plugin log) plus an offline contract replay.

Output: .scratch/tools/SLSFR_FrozenProbe.psc
"""

import json
import os
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CONTRACT = os.path.join(REPO, "contracts", "legacy-api.json")
RECORDS = os.path.join(REPO, "contracts", "legacy-records.json")
EVENTS = os.path.join(REPO, "contracts", "legacy-events.json")
OUT = os.path.join(REPO, ".scratch", "tools", "SLSFR_FrozenProbe.psc")

SCRIPT_QUEST = {}
QUEST_FORM = {}
for record in json.load(open(RECORDS, encoding="utf-8"))["records"]:
    if record["signature"] == "QUST":
        QUEST_FORM[record["editorId"]] = record["formId"]

QUEST_OF = {
    "SLSF_Reloaded_CommentManager": "SLSF_Reloaded_Managers",
    "SLSF_Reloaded_DataManager": "SLSF_Reloaded_Managers",
    "SLSF_Reloaded_Data_Exporter": "SLSF_Reloaded_Managers",
    "SLSF_Reloaded_Data_Importer": "SLSF_Reloaded_Managers",
    "SLSF_Reloaded_DynamicAnonymity": "SLSF_Reloaded_Managers",
    "SLSF_Reloaded_FameManager": "SLSF_Reloaded_Main",
    "SLSF_Reloaded_LegacyOverwrite": "SLSF_Reloaded_Managers",
    "SLSF_Reloaded_LocationManager": "SLSF_Reloaded_Managers",
    "SLSF_Reloaded_Logger": "SLSF_Reloaded_Main",
    "SLSF_Reloaded_MasterClock": "SLSF_Reloaded_MasterClock",
    "SLSF_Reloaded_ModIntegration": "SLSF_Reloaded_Main",
    "SLSF_Reloaded_ModEventListener": "SLSF_Reloaded_Main",
    "SLSF_Reloaded_PlayerScript": "SLSF_Reloaded_Main",
    "SLSF_Reloaded_SexAnimationAnalyzer": "SLSF_Reloaded_Main",
    "SLSF_Reloaded_Uninstall": "SLSF_Reloaded_Managers",
    "SLSF_Reloaded_VisibilityManager": "SLSF_Reloaded_Managers",
}

# The quest-dependent safe subset: functions with trivial names the frozen
# runner can call right after load; the skip list is intentional and traced
# in the group 11 report.
SKIP_IDS = {
    "SLSF_Reloaded_ModIntegration.GetFHUData",
    "SLSF_Reloaded_ModIntegration.GetPWData",
    "SLSF_Reloaded_ModIntegration.GetLegacyConfig",  # needs external esps
    "SLSF_Reloaded_Data_Exporter.RunExport",
    "SLSF_Reloaded_Data_Exporter.ExportData",
    "SLSF_Reloaded_Data_Importer.RunImport",
    "SLSF_Reloaded_Data_Importer.ImportData",
    "SLSF_Reloaded_Uninstall.RunUninstall",
    "SLSF_Reloaded_PlayerScript.RunNPCDetect",
    "SLSF_Reloaded_SexAnimationAnalyzer.AnimationAnalyze",
    "SLSF_Reloaded_SexAnimationAnalyzer.AnimationSubAnalyze",
    "SLSF_Reloaded_FameManager.SpreadFameRoll",
}


def psc_default(p):
    unused = p
    return None



def typed_literal(psc_type, raw):
    if raw is None:
        return {"String": '""', "Int": "0", "Bool": "True", "Float": "0.0"}.get(psc_type, "None")
    if psc_type == "String":
        return '"%s"' % str(raw).replace('"', '\\"')
    if psc_type == "Int":
        return str(int(float(raw)))
    if psc_type == "Float":
        value = float(raw)
        return "%.1f" % value if value == int(value) else repr(value)
    if psc_type == "Bool":
        return "True" if str(raw).lower() == "true" else "False"
    return "None"

def literal(value):
    if value is None:
        return "None"
    if isinstance(value, bool):
        return "True" if value else "False"
    if isinstance(value, (int, float)):
        return str(value)
    if isinstance(value, str):
        return '"%s"' % value.replace('"', '\\"') if value else '""'
    raise ValueError("unknown literal type: " + repr(value))


def main():
    doc = json.load(open(CONTRACT, encoding="utf-8"))
    events = json.load(open(EVENTS, encoding="utf-8"))
    lines = []
    lines.append("ScriptName SLSFR_FrozenProbe extends Quest Hidden Hidden")
    lines.append("")
    lines.append("Function RunAll() Global")
    aliasScripts = {"SLSF_Reloaded_PlayerScript": "PlayerAlias",
                    "SLSF_Reloaded_SexAnimationAnalyzer": "PlayerAlias"}
    for script in sorted(QUEST_OF):
        quest = QUEST_OF[script]
        local = QUEST_FORM.get(quest, "0x0")
        tag = script.replace("_", "")
        lines.append('    Quest questRef_%s = Game.GetFormFromFile(%s, "SLSF Reloaded.esp") as Quest'
                     % (tag, local))
        if script in aliasScripts:
            lines.append('    Alias aliasRef_%s = questRef_%s.GetAliasByName("%s")' % (
                tag, tag, aliasScripts[script]))
            lines.append('    If aliasRef_%s == None' % tag)
            lines.append('        Debug.Trace("SLSFR FrozenProbe: alias instance missing for %s")' % script)
            lines.append('        return')
            lines.append('    EndIf')
            lines.append('    %s probe_%s = aliasRef_%s as %s' % (
                script, tag, tag, script))
        else:
            lines.append('    If questRef_%s == None' % tag)
            lines.append('        Debug.Trace("SLSFR FrozenProbe: quest instance missing for %s")' % script)
            lines.append('        return')
            lines.append('    EndIf')
            lines.append('    %s probe_%s = questRef_%s as %s' % (
                script, tag, tag, script))
    lines.append("")
    # -- the 52 input events ------------------------------------------------
    for index, event in enumerate(events["inputEvents"], 1):
        if index > 52:
            break
        args = ", ".join(typed_literal(p["type"], p.get("default"))
                         for p in event["parameters"])
        lines.append("    Int handle%d = ModEvent.Create(\"%s\")" % (index, event["event"]))
        for param_idx, p in enumerate(event["parameters"], 1):
            value = typed_literal(p["type"], p.get("default"))
            if p["type"] == "String":
                lines.append('    ModEvent.PushString(handle%d, %s)' % (index, value))
            elif p["type"] == "Int":
                lines.append('    ModEvent.PushInt(handle%d, %s)' % (index, value))
            elif p["type"] == "Float":
                lines.append('    ModEvent.PushFloat(handle%d, %s)' % (index, value))
            elif p["type"] == "Bool":
                lines.append('    ModEvent.PushBool(handle%d, %s)' % (index, value))
            else:
                lines.append('    ModEvent.PushForm(handle%d, Game.GetFormFromFile(0x0000109C, "Skyrim.esm"))' % index)
        lines.append("    ModEvent.Send(handle%d)" % index)
        lines.append('    SLSF_Reloaded_Logger.Log("SLSFR FrozenProbe: event %s sent", 0)' % event["event"])
    lines.append("")
    # -- the public surface --------------------------------------------------
    fn_counter = 0
    for fn in doc["functions"]:
        if not fn["isPublic"]:
            continue
        fn_id = fn["id"]
        if fn_id in SKIP_IDS:
            lines.append('    Debug.Trace("SLSFR FrozenProbe: skip %s (runtime-unsafe)")' % fn_id)
            continue
        args = []
        for p in fn["parameters"]:
            default = p.get("default")
            fallback = {"String": "", "Int": 0, "Bool": True, "Float": 0.0}.get(p["type"])
            value = typed_literal(p["type"], default)
            args.append(value)
        script_probe = fn["script"] if fn.get("isGlobal") else ("probe_" + fn["script"].replace("_", ""))
        call = "%s.%s(%s)" % (script_probe, fn["name"], ", ".join(args))
        ret = fn["returnType"]
        if ret in ("Int", "Bool", "Float", "String"):
            fn_counter += 1
            value = "    %s out_%d = %s" % (ret, fn_counter, call)
            if ret == "Int":
                value = "    Int out_%d = %s" % (fn_counter, call)
            elif ret == "Bool":
                value = "    Bool out_%d = %s" % (fn_counter, call)
            elif ret == "Float":
                value = "    Float out_%d = %s" % (fn_counter, call)
            else:
                value = "    String out_%d = %s" % (fn_counter, call)
            lines.append(value)
            shown = "float(" + "main" + str(fn_counter) + ")"
            shown = "out_%d" % fn_counter
            if ret == "Int":
                shown = "out_%d as String" % fn_counter
            elif ret == "Bool":
                shown = "out_%d as String" % fn_counter
            elif ret == "Float":
                shown = "out_%d as String" % fn_counter
            else:
                shown = "out_%d" % fn_counter
            lines.append('    SLSF_Reloaded_Logger.Log("SLSFR FrozenProbe: %s -> " + %s, 0)' % (fn_id, shown))
        else:
            lines.append("    %s" % call)
            lines.append('    SLSF_Reloaded_Logger.Log("SLSFR FrozenProbe: %s -> (sent)", 0)' % fn_id)
    lines.append("")
    lines.append('    SLSF_Reloaded_Logger.Log("SLSFR FrozenProbe: run all done functions=%d", 0)' % fn_counter)
    lines.append('    Debug.Trace("SLSFR FrozenProbe: run all done")')
    lines.append("EndFunction")
    lines.append("")

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines))
    print("wrote %s (%d lines)" % (OUT, len(lines)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
