Scriptname SLSF_Reloaded_DynamicAnonymity extends Quest

Form Property DLC2StoneCleanse Auto
Float[] Property DefaultLocationRecognitionTime Auto
Quest Property LocationManager Auto
Quest Property DataManager Auto
GlobalVariable Property SLSF_Reloaded_CustomLocationCount Auto
Quest Property config Auto
Form Property ThaneScript Auto
Quest Property VisibilityManager Auto
Form Property OrcFriends Auto
Float[] Property CustomLocationLastExitTime Auto
Float[] Property DefaultLocationVisitTime Auto
Float[] Property DefaultLocationLastExitTime Auto
Bool[] Property CustomLocationLocalAnonymityStartedAsTrue Auto
Bool[] Property CustomLocationLocalAnonymityFlag Auto
Bool[] Property DefaultLocationLocalAnonymityStartedAsTrue Auto
Float[] Property CustomLocationVisitTime Auto
Float[] Property CustomLocationRecognitionTime Auto
Quest Property Logger Auto
Float[] Property CustomLocationLastEnterTime Auto
Float[] Property DefaultLocationLastEnterTime Auto
ReferenceAlias Property playerScript Auto
Int Property IsAnonymous Auto
Bool[] Property DefaultLocationLocalAnonymityFlag Auto

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
