ScriptName zadNativeFunctions hidden

;-- Functions ---------------------------------------

Bool Function ApplyExpression(Actor akActor, Float[] aaExpression, Int aiStrength, Bool abOpenMouth, Int aiPriority) Global Native

Function CTrace(String msg) Global Native

Function ExecuteConsoleCmd(String asCmd) Global Native

Form Function FindMatchingDevice(Actor akActor, Keyword kwd) Global Native

Bool Function FormHasKeywordString(Form theForm, String kwd) Global Native

Armor Function GetDeviceByName(String asName) Global Native

Armor[] Function GetDevices(Actor akActor, Int aiMode, Bool abWorn) Global Native

String[] Function GetEditingMods(Armor akInvDevice) Global Native

Float[] Function GetExpression(Actor akActor) Global Native

Armor Function GetInventoryDevice(Armor akRendDevice) Global Native

Bool Function GetManipulated(Actor akActor, Armor akInvDevice) Global Native

Bool Function GetPropertyBool(Armor akInvDevice, String asPropertyName, Bool akDefValue, Int aiMode) Global Native

Bool[] Function GetPropertyBoolArray(Armor akInvDevice, String asPropertyName, Int aiMode) Global Native

Float Function GetPropertyFloat(Armor akInvDevice, String asPropertyName, Float akDefValue, Int aiMode) Global Native

Float[] Function GetPropertyFloatArray(Armor akInvDevice, String asPropertyName, Int aiMode) Global Native

Form Function GetPropertyForm(Armor akInvDevice, String asPropertyName, Form akDefValue, Int aiMode) Global Native

Form[] Function GetPropertyFormArray(Armor akInvDevice, String asPropertyName, Int aiMode) Global Native

Int Function GetPropertyInt(Armor akInvDevice, String asPropertyName, Int akDefValue, Int aiMode) Global Native

Int[] Function GetPropertyIntArray(Armor akInvDevice, String asPropertyName, Int aiMode) Global Native

String Function GetPropertyString(Armor akInvDevice, String asPropertyName, String akDefValue, Int aiMode) Global Native

String[] Function GetPropertyStringArray(Armor akInvDevice, String asPropertyName, Int aiMode) Global Native

Armor Function GetRenderDevice(Armor akInvDevice) Global Native

; Skipped compiler generated GetState

Armor Function GetWornDevice(Actor akActor, Keyword akKeyword, Bool fuzzy) Global Native

; Skipped compiler generated GotoState

Function HideWeapons(Actor akActor) Global Native

Bool Function IsActorStripped(Actor akActor) Global Native

Bool Function PluginInstalled(String asName) Global Native

Bool Function Print(String msg) Global Native

Bool Function RegisterDefaultGagType(Faction[] aakFactions, Int[] aaiDefaults) Global Native

Bool Function RegisterGagType(Keyword akKeyword, Faction[] aakFactions, Int[] aaiDefaults) Global Native

Function ResetExpression(Actor akActor, Int aiPriority) Global Native

Function ResetGagExpression(Actor akActor) Global Native

Function SetActorStripped(Actor akActor, Bool abStripped, Int aiArmorFilter, Int aiDeviceFilter) Global Native

Function SetManipulated(Actor akActor, Armor akInvDevice, Bool abManip) Global Native

Function ShowWeapons(Actor akActor) Global Native

Function SyncSetting(Int[] aaFilter, Int aiSetting) Global Native

Function UpdateGagExpression(Actor akActor) Global Native

Event onBeginState()
{ Event received when this state is switched to }
  ; Empty function
EndEvent

Event onEndState()
{ Event received when this state is switched away from }
  ; Empty function
EndEvent
