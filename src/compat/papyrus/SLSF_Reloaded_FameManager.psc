ScriptName SLSF_Reloaded_FameManager Extends Quest

Bool Property AirheadFameCleared Auto hidden
GlobalVariable Property AirheadGlobal Auto
GlobalVariable Property AnalGlobal Auto
GlobalVariable Property ArgonianGlobal Auto
GlobalVariable Property BeastGlobal Auto
GlobalVariable Property BoundGlobal Auto
GlobalVariable Property CheatGlobal Auto
slsf_reloaded_mcm Property Config Auto
Bool Property CuckFameCleared Auto hidden
GlobalVariable Property CuckGlobal Auto
Bool Property CumDumpFameCleared Auto hidden
GlobalVariable Property CumDumpGlobal Auto
Bool[] Property CustomLocationCanDecay Auto
Int[] Property CustomLocationDecayPauseTimer Auto
slsf_reloaded_datamanager Property Data Auto
Int Property DecayCountdown Auto hidden
Bool[] Property DefaultLocationCanDecay Auto
Int[] Property DefaultLocationDecayPauseTimer Auto
GlobalVariable Property DomGlobal Auto
GlobalVariable Property ExhibGlobal Auto
Bool Property ExternalAirheadFlag Auto hidden
Bool Property ExternalAnalFlag Auto hidden
Bool Property ExternalBestialityFlag Auto hidden
Bool Property ExternalBoundFlag Auto hidden
Bool Property ExternalCuckFlag Auto hidden
Bool Property ExternalCumDumpFlag Auto hidden
Bool Property ExternalDominantFlag Auto hidden
Bool Property ExternalExhibitionistFlag Auto hidden
Bool Property ExternalGentleFlag Auto hidden
Bool Property ExternalGroupFlag Auto hidden
Bool Property ExternalLikesArgonianFlag Auto hidden
Bool Property ExternalLikesKhajiitFlag Auto hidden
Bool Property ExternalLikesMenFlag Auto hidden
Bool Property ExternalLikesOrcFlag Auto hidden
Bool Property ExternalLikesWomenFlag Auto hidden
slsf_reloaded_modeventlistener Property ExternalListener Auto
Bool Property ExternalMasochistFlag Auto hidden
Bool Property ExternalNastyFlag Auto hidden
Bool Property ExternalOralFlag Auto hidden
Bool Property ExternalPregnantFlag Auto hidden
Bool Property ExternalSadistFlag Auto hidden
Bool Property ExternalSlutFlag Auto hidden
Bool Property ExternalSubmissiveFlag Auto hidden
Bool Property ExternalTattooFlag Auto hidden
Bool Property ExternalUnfaithfulFlag Auto hidden
Bool Property ExternalWhoreFlag Auto hidden
Bool Property FameGainNotifyCooldown Auto hidden
String[] Property FameType Auto
Int Property GainIterationMultiplier Auto hidden
GlobalVariable Property GentleGlobal Auto
GlobalVariable Property GroupGlobal Auto
GlobalVariable Property IsHeavilyBound Auto
GlobalVariable Property IsLightlyBound Auto
GlobalVariable Property IsVisiblyBound Auto
GlobalVariable Property KhajiitGlobal Auto
slsf_reloaded_legacyoverwrite Property LegacyOverwrite Auto
slsf_reloaded_locationmanager Property LocationManager Auto
slsf_reloaded_logger Property Logger Auto
GlobalVariable Property MasochistGlobal Auto
GlobalVariable Property MenGlobal Auto
slsf_reloaded_modintegration Property Mods Auto
GlobalVariable Property NastyGlobal Auto
GlobalVariable Property OralGlobal Auto
GlobalVariable Property OrcGlobal Auto
Actor Property PlayerRef Auto
slsf_reloaded_playerscript Property PlayerScript Auto
GlobalVariable Property PregGlobal Auto
Bool Property SLSFCFameTypesCleared Auto hidden
GlobalVariable Property SLSF_Reloaded_CustomLocationCount Auto
Keyword Property SLSF_Reloaded_NipplePiercing Auto
Keyword Property SLSF_Reloaded_VaginalPiercing Auto
GlobalVariable Property SadistGlobal Auto
sexlabframework Property Sexlab Auto
GlobalVariable Property SlutGlobal Auto
Int Property SpreadCountdown Auto hidden
GlobalVariable Property SubGlobal Auto
GlobalVariable Property TattooGlobal Auto
Bool Property UnfaithfulFameCleared Auto hidden
slsf_reloaded_visibilitymanager Property VisibilityManager Auto
GlobalVariable Property WhoreGlobal Auto
GlobalVariable Property WomenGlobal Auto

Function Startup()
EndFunction

Function UpdateFame(Int UpdateMultiplier = 1) Native

Function WipeCuckFame() Native

Function WipeUnfaithfulFame() Native

Function WipeAirheadFame() Native

Function WipeCumDumpFame() Native

Function CheckExternalFlags() Native

Bool Function CanGainWhoreFame()
    return false
EndFunction

Bool Function CanGainSlutFame(String FameLocation)
    return false
EndFunction

Bool Function CanGainExhibitionistFame(String FameLocation)
    return false
EndFunction

Bool Function CanGainOralFame(String FameLocation)
    return false
EndFunction

Bool Function CanGainAnalFame(String FameLocation)
    return false
EndFunction

Bool Function CanGainNastyFame(String FameLocation)
    return false
EndFunction

Bool Function CanGainPregnantFame(String FameLocation)
    return false
EndFunction

Bool Function CanGainBoundFame(String FameLocation)
    return false
EndFunction

Bool Function CanGainTattooFame(String FameLocation)
    return false
EndFunction

Bool Function CanGainCumDumpFame()
    return false
EndFunction

Bool Function CheckTattooExtraFame(String ExtraFame)
    return false
EndFunction

Function FameGainRoll(String FameLocation, Bool CalledExternally = false, Bool FameFromFriend = false, Bool FameFromLover = false) Native

Function GainFame(String Category, String LocationName, Bool IsForeplay) Native

Function DecayFame(Int DecayMultiplier = 1) Native

Function SpreadFameRoll() Native

Function SpreadFame(String SpreadFromLocation) Native

Function FameGainNotification()
EndFunction

Function FameDecayNotification()
EndFunction

Int Function CalculateFameSpread(Float FromFame, Float TargetFame) Native

Function FameSpreadNotification()
EndFunction

Function ClearFame(String LocationToClear) Native

Function ClearAllFame() Native

Function UpdateGlobals() Native
