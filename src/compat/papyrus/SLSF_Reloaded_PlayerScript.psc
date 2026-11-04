ScriptName SLSF_Reloaded_PlayerScript Extends ReferenceAlias

slsf_reloaded_sexanimationanalyzer Property AnimationAnalyzer Auto
slsf_reloaded_mcm Property Config Auto
slsf_reloaded_datamanager Property Data Auto
slsf_reloaded_dynamicanonymity Property DynamicAnonymityScript Auto
slsf_reloaded_famemanager Property FameManager Auto
slsf_reloaded_legacyoverwrite Property LegacyOverwrite Auto
slsf_reloaded_modeventlistener Property Listener Auto
slsf_reloaded_locationmanager Property LocationManager Auto
slsf_reloaded_logger Property Logger Auto
slsf_reloaded_modintegration Property Mods Auto
Spell Property NPCScanSpell Auto
Actor Property PlayerRef Auto
GlobalVariable Property SLSF_AllowComment Auto
GlobalVariable Property SLSF_Reloaded_NPCScanSucess Auto
GlobalVariable Property SLSF_Reloaded_Skooma Auto
slsf_reloaded_timemanager Property TimeManager Auto
slsf_reloaded_visibilitymanager Property VisibilityManager Auto
Bool Property WaitingOnVisibilityManager = False Auto hidden

Function Startup()
    RegisterForModEvent("HookAnimationStart", "OnAnimationStart")
    RegisterForModEvent("HookLeadInEnd", "OnLeadInEnd")
    RegisterForModEvent("HookAnimationEnd", "OnAnimationEnd")
    RegisterForModEvent("HookStageStart", "OnStageStart")
    RegisterForModEvent("HookStageChange", "OnStageChange")
EndFunction

Event OnInit()
    Startup()
EndEvent

Event OnObjectEquipped(Form Item, ObjectReference Reference)
    SLSFR_GameplayNative.Equipped(Item)
    SLSFR_GameplayNative.Refresh("FullUpdate")
EndEvent

Event OnObjectUnequipped(Form Item, ObjectReference Reference)
    SLSFR_GameplayNative.Refresh("FullUpdate")
EndEvent

Event OnPlayerLoadGame()
    Startup()
EndEvent

Event OnAnimationStart(Int ThreadID, Bool HasPlayer)
    If HasPlayer
        SLSFR_GameplayNative.SceneEvent("HookAnimationStart", ThreadID)
    EndIf
EndEvent

Event OnLeadInEnd(Int ThreadID, Bool HasPlayer)
    If HasPlayer
        SLSFR_GameplayNative.SceneEvent("HookLeadInEnd", ThreadID)
    EndIf
EndEvent

Event OnAnimationEnd(Int ThreadID, Bool HasPlayer)
    If HasPlayer
        SLSFR_GameplayNative.SceneEvent("HookAnimationEnd", ThreadID)
    EndIf
EndEvent

Event OnStageStart(Int ThreadID, Bool HasPlayer)
    If HasPlayer
        SLSFR_GameplayNative.SceneEvent("HookStageStart", ThreadID)
    EndIf
EndEvent

Event OnStageChange(Int ThreadID, Bool HasPlayer)
    If HasPlayer
        SLSFR_GameplayNative.SceneEvent("HookStageChange", ThreadID)
    EndIf
EndEvent

Function RunNPCDetect() Native
