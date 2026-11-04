Scriptname SLSF_Reloaded_FameManager extends Quest

GlobalVariable Property GroupGlobal Auto
Quest Property LocationManager Auto
GlobalVariable Property SLSF_Reloaded_CustomLocationCount Auto
GlobalVariable Property NastyGlobal Auto
Quest Property VisibilityManager Auto
String[] Property FameType Auto
GlobalVariable Property WomenGlobal Auto
GlobalVariable Property MasochistGlobal Auto
GlobalVariable Property BeastGlobal Auto
Bool[] Property CustomLocationCanDecay Auto
GlobalVariable Property OralGlobal Auto
GlobalVariable Property OrcGlobal Auto
GlobalVariable Property IsVisiblyBound Auto
Quest Property Logger Auto
GlobalVariable Property BoundGlobal Auto
GlobalVariable Property SlutGlobal Auto
GlobalVariable Property IsLightlyBound Auto
ReferenceAlias Property playerScript Auto
GlobalVariable Property SadistGlobal Auto
Int[] Property DefaultLocationDecayPauseTimer Auto
GlobalVariable Property PregGlobal Auto
GlobalVariable Property WhoreGlobal Auto
Quest Property config Auto
GlobalVariable Property AirheadGlobal Auto
Quest Property LegacyOverwrite Auto
GlobalVariable Property ExhibGlobal Auto
GlobalVariable Property CumDumpGlobal Auto
Int[] Property CustomLocationDecayPauseTimer Auto
Quest Property mods Auto
GlobalVariable Property AnalGlobal Auto
Bool[] Property DefaultLocationCanDecay Auto
Quest Property DATA Auto
GlobalVariable Property CheatGlobal Auto
GlobalVariable Property DomGlobal Auto
GlobalVariable Property KhajiitGlobal Auto
GlobalVariable Property TattooGlobal Auto
GlobalVariable Property ArgonianGlobal Auto
GlobalVariable Property MenGlobal Auto
GlobalVariable Property GentleGlobal Auto
Form Property SexLab Auto
GlobalVariable Property CuckGlobal Auto
Keyword Property SLSF_Reloaded_VaginalPiercing Auto
Quest Property ExternalListener Auto
ObjectReference Property PlayerRef Auto
GlobalVariable Property IsHeavilyBound Auto
Keyword Property SLSF_Reloaded_NipplePiercing Auto
GlobalVariable Property SubGlobal Auto

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
