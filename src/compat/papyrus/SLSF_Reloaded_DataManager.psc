Scriptname SLSF_Reloaded_DataManager extends Quest

Bool[] Property WhiterunSpreadFlags Auto
Quest Property LocationManager Auto
Bool[] Property FalkreathSpreadFlags Auto
Int[] Property CustomLocation6Fame Auto
Int[] Property CustomLocation5Fame Auto
Int[] Property NarzulburFame Auto
Bool[] Property WinterholdSpreadFlags Auto
Bool[] Property MarkarthSpreadFlags Auto
Bool[] Property CustomLocation14SpreadFlags Auto
Int[] Property IvarsteadFame Auto
Bool[] Property CustomLocation15SpreadFlags Auto
Bool[] Property CustomLocation3SpreadFlags Auto
Int[] Property DawnstarFame Auto
Int[] Property RavenRockFame Auto
Int[] Property MorthalFame Auto
Quest Property Logger Auto
Int[] Property CustomLocation13Fame Auto
Bool[] Property RiftenSpreadFlags Auto
Int[] Property CustomLocation16Fame Auto
Bool[] Property CustomLocation21SpreadFlags Auto
Int[] Property CustomLocation1Fame Auto
Bool[] Property NarzulburSpreadFlags Auto
Bool[] Property CustomLocation1SpreadFlags Auto
Bool[] Property CustomLocation16SpreadFlags Auto
Int[] Property CustomLocation14Fame Auto
Bool[] Property LargashburSpreadFlags Auto
Int[] Property CustomLocation9Fame Auto
Int[] Property DragonBridgeFame Auto
Int[] Property CustomLocation19Fame Auto
Int[] Property CustomLocation10Fame Auto
Int[] Property WhiterunFame Auto
Bool[] Property DragonBridgeSpreadFlags Auto
Int[] Property CustomLocation4Fame Auto
Bool[] Property CustomLocation10SpreadFlags Auto
Int[] Property MorKhazgurFame Auto
Int[] Property LargashburFame Auto
Bool[] Property CustomLocation2SpreadFlags Auto
Bool[] Property CustomLocation20SpreadFlags Auto
Int[] Property SolitudeFame Auto
Quest Property FameManager Auto
Int[] Property SkaalVillageFame Auto
Int[] Property CustomLocation8Fame Auto
Bool[] Property DefaultLocationHasSpreadableFame Auto
Int[] Property ShorsStoneFame Auto
Int[] Property CustomLocation20Fame Auto
Bool[] Property CustomLocation9SpreadFlags Auto
Bool[] Property WindhelmSpreadFlags Auto
Int[] Property CustomLocation12Fame Auto
Bool[] Property CustomLocationDynamicAnonymityFlag Auto
Bool[] Property ExternalFlags Auto
Bool[] Property CustomLocation8SpreadFlags Auto
Bool[] Property CustomLocation4SpreadFlags Auto
Int[] Property FalkreathFame Auto
Int[] Property RiverwoodFame Auto
Bool[] Property CustomLocation13SpreadFlags Auto
Bool[] Property SkaalVillageSpreadFlags Auto
Bool[] Property CustomLocation19SpreadFlags Auto
Bool[] Property RoriksteadSpreadFlags Auto
Int[] Property CustomLocation11Fame Auto
Int[] Property KarthwastenFame Auto
Bool[] Property RiverwoodSpreadFlags Auto
Bool[] Property CustomLocation18SpreadFlags Auto
Bool[] Property CustomLocation6SpreadFlags Auto
Bool[] Property CustomLocation12SpreadFlags Auto
Bool[] Property DushnikhYalSpreadFlags Auto
Bool[] Property CustomLocation5SpreadFlags Auto
Bool[] Property SolitudeSpreadFlags Auto
Bool[] Property CustomLocation11SpreadFlags Auto
Bool[] Property MorthalSpreadFlags Auto
Int[] Property CustomLocation18Fame Auto
Int[] Property WinterholdFame Auto
Int[] Property CustomLocation2Fame Auto
Bool[] Property MorKhazgurSpreadFlags Auto
Int[] Property CustomLocation3Fame Auto
Int[] Property RoriksteadFame Auto
Int[] Property WindhelmFame Auto
Bool[] Property IvarsteadSpreadFlags Auto
Bool[] Property DawnstarSpreadFlags Auto
Int[] Property CustomLocation15Fame Auto
Bool[] Property ShorsStoneSpreadFlags Auto
Int[] Property CustomLocation7Fame Auto
Quest Property config Auto
Int[] Property MarkarthFame Auto
Int[] Property CustomLocation21Fame Auto
Bool[] Property KarthwastenSpreadFlags Auto
Bool[] Property CustomLocation17SpreadFlags Auto
Bool[] Property CustomLocationHasSpreadableFame Auto
Bool[] Property RavenRockSpreadFlags Auto
Bool[] Property DefaultLocationDynamicAnonymityFlag Auto
Bool[] Property CustomLocation7SpreadFlags Auto
Int[] Property CustomLocation17Fame Auto
Int[] Property RiftenFame Auto
Int[] Property DushnikhYalFame Auto

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
