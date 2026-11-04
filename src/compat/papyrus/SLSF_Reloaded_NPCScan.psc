ScriptName SLSF_Reloaded_NPCScan Extends ActiveMagicEffect

slsf_reloaded_mcm Property Config Auto
String Property CurrentLocation Auto hidden
slsf_reloaded_famemanager Property FameManager Auto
slsf_reloaded_locationmanager Property LocationManager Auto
slsf_reloaded_logger Property Logger Auto
Actor Property PlayerRef Auto
GlobalVariable Property SLSF_Reloaded_NPCScanSucess Auto
sexlabframework Property Sexlab Auto

Event OnEffectStart(Actor akTarget, Actor akCaster)
    SLSFR_GameplayNative.ObserveNpc(akTarget)
EndEvent
