ScriptName SLSF_Reloaded_DataManager Extends Quest

Bool[] Property AirheadFlags Auto hidden
Bool[] Property AnalFlags Auto hidden
Bool[] Property BestialityFlags Auto hidden
Bool[] Property BoundFlags Auto hidden
slsf_reloaded_mcm Property Config Auto
Bool[] Property CuckFlags Auto hidden
Bool[] Property CumDumpFlags Auto hidden
Int[] Property CustomLocation10Fame Auto
Bool[] Property CustomLocation10SpreadFlags Auto
Int[] Property CustomLocation11Fame Auto
Bool[] Property CustomLocation11SpreadFlags Auto
Int[] Property CustomLocation12Fame Auto
Bool[] Property CustomLocation12SpreadFlags Auto
Int[] Property CustomLocation13Fame Auto
Bool[] Property CustomLocation13SpreadFlags Auto
Int[] Property CustomLocation14Fame Auto
Bool[] Property CustomLocation14SpreadFlags Auto
Int[] Property CustomLocation15Fame Auto
Bool[] Property CustomLocation15SpreadFlags Auto
Int[] Property CustomLocation16Fame Auto
Bool[] Property CustomLocation16SpreadFlags Auto
Int[] Property CustomLocation17Fame Auto
Bool[] Property CustomLocation17SpreadFlags Auto
Int[] Property CustomLocation18Fame Auto
Bool[] Property CustomLocation18SpreadFlags Auto
Int[] Property CustomLocation19Fame Auto
Bool[] Property CustomLocation19SpreadFlags Auto
Int[] Property CustomLocation1Fame Auto
Bool[] Property CustomLocation1SpreadFlags Auto
Int[] Property CustomLocation20Fame Auto
Bool[] Property CustomLocation20SpreadFlags Auto
Int[] Property CustomLocation21Fame Auto
Bool[] Property CustomLocation21SpreadFlags Auto
Int[] Property CustomLocation2Fame Auto
Bool[] Property CustomLocation2SpreadFlags Auto
Int[] Property CustomLocation3Fame Auto
Bool[] Property CustomLocation3SpreadFlags Auto
Int[] Property CustomLocation4Fame Auto
Bool[] Property CustomLocation4SpreadFlags Auto
Int[] Property CustomLocation5Fame Auto
Bool[] Property CustomLocation5SpreadFlags Auto
Int[] Property CustomLocation6Fame Auto
Bool[] Property CustomLocation6SpreadFlags Auto
Int[] Property CustomLocation7Fame Auto
Bool[] Property CustomLocation7SpreadFlags Auto
Int[] Property CustomLocation8Fame Auto
Bool[] Property CustomLocation8SpreadFlags Auto
Int[] Property CustomLocation9Fame Auto
Bool[] Property CustomLocation9SpreadFlags Auto
Bool[] Property CustomLocationDynamicAnonymityFlag Auto
Bool[] Property CustomLocationHasSpreadableFame Auto
Int[] Property DawnstarFame Auto
Bool[] Property DawnstarSpreadFlags Auto
Bool[] Property DefaultLocationDynamicAnonymityFlag Auto
Bool[] Property DefaultLocationHasSpreadableFame Auto
Bool[] Property DisplayFameFlags Auto hidden
Bool[] Property DominantFlags Auto hidden
Int[] Property DragonBridgeFame Auto
Bool[] Property DragonBridgeSpreadFlags Auto
Int[] Property DushnikhYalFame Auto
Bool[] Property DushnikhYalSpreadFlags Auto
Bool[] Property EnableAirheadFlags Auto hidden
Bool[] Property EnableCuckFlags Auto hidden
Bool[] Property EnableCumDumpFlags Auto hidden
Bool[] Property EnableUnfaithfulFlags Auto hidden
Bool[] Property ExhibitionistFlags Auto hidden
Bool[] Property ExternalFlags Auto
String[] Property ExternalMods Auto hidden
Int[] Property FalkreathFame Auto
Bool[] Property FalkreathSpreadFlags Auto
slsf_reloaded_famemanager Property FameManager Auto
Bool[] Property GentleFlags Auto hidden
Bool[] Property GroupFlags Auto hidden
Int[] Property IvarsteadFame Auto
Bool[] Property IvarsteadSpreadFlags Auto
Int[] Property KarthwastenFame Auto
Bool[] Property KarthwastenSpreadFlags Auto
Int[] Property LargashburFame Auto
Bool[] Property LargashburSpreadFlags Auto
Bool[] Property LikesArgonianFlags Auto hidden
Bool[] Property LikesKhajiitFlags Auto hidden
Bool[] Property LikesMenFlags Auto hidden
Bool[] Property LikesOrcFlags Auto hidden
Bool[] Property LikesWomenFlags Auto hidden
slsf_reloaded_locationmanager Property LocationManager Auto
slsf_reloaded_logger Property Logger Auto
Int[] Property MarkarthFame Auto
Bool[] Property MarkarthSpreadFlags Auto
Bool[] Property MasochistFlags Auto hidden
Int[] Property MorKhazgurFame Auto
Bool[] Property MorKhazgurSpreadFlags Auto
Int[] Property MorthalFame Auto
Bool[] Property MorthalSpreadFlags Auto
Int[] Property NarzulburFame Auto
Bool[] Property NarzulburSpreadFlags Auto
Bool[] Property NastyFlags Auto hidden
Bool[] Property OralFlags Auto hidden
Bool[] Property PregnantFlags Auto hidden
Int[] Property RavenRockFame Auto
Bool[] Property RavenRockSpreadFlags Auto
Int[] Property RiftenFame Auto
Bool[] Property RiftenSpreadFlags Auto
Int[] Property RiverwoodFame Auto
Bool[] Property RiverwoodSpreadFlags Auto
Int[] Property RoriksteadFame Auto
Bool[] Property RoriksteadSpreadFlags Auto
Bool[] Property SadistFlags Auto hidden
Bool[] Property SexWorkerFlags Auto hidden
Int[] Property ShorsStoneFame Auto
Bool[] Property ShorsStoneSpreadFlags Auto
Int[] Property SkaalVillageFame Auto
Bool[] Property SkaalVillageSpreadFlags Auto
Bool[] Property SlutFlags Auto hidden
Int[] Property SolitudeFame Auto
Bool[] Property SolitudeSpreadFlags Auto
Bool[] Property SubmissiveFlags Auto hidden
Bool[] Property TattooFlags Auto hidden
Bool[] Property UnfaithfulFlags Auto hidden
Int[] Property WhiterunFame Auto
Bool[] Property WhiterunSpreadFlags Auto
Bool[] Property WhoreFlags Auto hidden
Int[] Property WindhelmFame Auto
Bool[] Property WindhelmSpreadFlags Auto
Int[] Property WinterholdFame Auto
Bool[] Property WinterholdSpreadFlags Auto

Function FameCheck()
EndFunction

Function SetDefaults()
EndFunction

Function CheckSpreadableFame()
EndFunction

Int Function GetFameValue(String LocationName, String FameCategory) Native

Function SetFameValue(String LocationName, String FameCategory, Int FameValue) Native

Function DecayDynamicAnonymityCheck(Int LocationIndex, Bool IsDefaultLocation)
EndFunction

Bool Function GetExternalFlags(String FlagName) Native

Function RegisterExternalMod(String ModName) Native

Int Function CountExternalMods() Native

Function CleanExternalModList() Native

Function UnregisterExternalMod(String ModName) Native

Bool Function GetModFlagState(String ModName, String FlagName) Native

String Function DoesFlagExist(String FlagName) Native

Bool Function IsModRegistered(String ModName) Native

Function SetExternalFlags(String ModName, String FlagName, Bool FlagValue) Native

Bool Function CheckSexWorker() Native

Function CheckFlags()
EndFunction

Function FameOverviewCheck()
EndFunction
