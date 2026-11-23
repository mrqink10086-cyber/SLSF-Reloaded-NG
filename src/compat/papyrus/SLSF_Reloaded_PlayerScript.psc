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
    If Listener
        Listener.RegisterExternalEvents()
    EndIf
EndEvent

Function ForwardSceneEvent(String eventName, Int ThreadID, Bool HasPlayer, Bool Ending = False)
    If !HasPlayer
        Return
    EndIf
    String observed = SLSF_Reloaded_PPlusProbe.ObserveStart(ThreadID, Ending)
    If observed != ""
        SLSFR_GameplayNative.SceneEvent(eventName, ThreadID, observed)
    EndIf
EndFunction

Event OnAnimationStart(Int ThreadID, Bool HasPlayer)
    ForwardSceneEvent("HookAnimationStart", ThreadID, HasPlayer)
EndEvent

Event OnLeadInEnd(Int ThreadID, Bool HasPlayer)
    ForwardSceneEvent("HookLeadInEnd", ThreadID, HasPlayer)
EndEvent

Event OnAnimationEnd(Int ThreadID, Bool HasPlayer)
    ForwardSceneEvent("HookAnimationEnd", ThreadID, HasPlayer, True)
EndEvent

Event OnStageStart(Int ThreadID, Bool HasPlayer)
    ForwardSceneEvent("HookStageStart", ThreadID, HasPlayer)
EndEvent

Event OnStageChange(Int ThreadID, Bool HasPlayer)
    ForwardSceneEvent("HookStageChange", ThreadID, HasPlayer)
EndEvent

Function RunNPCDetect() Native
