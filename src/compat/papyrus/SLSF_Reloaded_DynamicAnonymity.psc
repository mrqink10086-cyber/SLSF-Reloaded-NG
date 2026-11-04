ScriptName SLSF_Reloaded_DynamicAnonymity Extends Quest

slsf_reloaded_mcm Property Config Auto
Float[] Property CustomLocationLastEnterTime Auto
Float[] Property CustomLocationLastExitTime Auto
Bool[] Property CustomLocationLocalAnonymityFlag Auto
Bool[] Property CustomLocationLocalAnonymityStartedAsTrue Auto
Float[] Property CustomLocationRecognitionTime Auto
Float[] Property CustomLocationVisitTime Auto
Quest Property DLC2StoneCleanse Auto
slsf_reloaded_datamanager Property DataManager Auto
Float[] Property DefaultLocationLastEnterTime Auto
Float[] Property DefaultLocationLastExitTime Auto
Bool[] Property DefaultLocationLocalAnonymityFlag Auto
Bool[] Property DefaultLocationLocalAnonymityStartedAsTrue Auto
Float[] Property DefaultLocationRecognitionTime Auto
Float[] Property DefaultLocationVisitTime Auto
Bool Property IsAnonymous Auto
slsf_reloaded_locationmanager Property LocationManager Auto
slsf_reloaded_logger Property Logger Auto
Faction Property OrcFriends Auto
slsf_reloaded_playerscript Property PlayerScript Auto
GlobalVariable Property SLSF_Reloaded_CustomLocationCount Auto
favorjarlsmakefriendsscript Property ThaneScript Auto
slsf_reloaded_visibilitymanager Property VisibilityManager Auto

Function CompareLocations(Location OldLocation, Location NewLocation) Native

Function SetLastExitTime(String FameLocation, Float GameTime) Native

Function SetLastEnterTime(String FameLocation, Float GameTime) Native

Function CheckRecognitionTimeIncrease(String FameLocation) Native

Function CheckRecognitionTimeDecay(String FameLocation) Native

Function UpdateCurrentLocationAnonymity(Int LocationIndex, Float GameTime, Bool IsDefaultLocation)
EndFunction

Function GetAnonymity(Location LocationRef) Native

Function CheckAnonymityStatus(Int LocationIndex, Bool IsDefaultLocation)
EndFunction

Bool Function ThaneStatus(String FameLocation) Native
