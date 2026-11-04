Scriptname SLSF_Reloaded_LocationManager extends Quest

Int[] Property OverrideExcludedLocation Auto
String[] Property WhiterunLocationList Auto
Quest Property DataManager Auto
GlobalVariable Property SLSF_Reloaded_CustomLocationCount Auto
Int[] Property MinorLocations Auto
Int[] Property ExcludedLocations Auto
String[] Property CustomLocation Auto
String[] Property RiftLocationList Auto
String[] Property WinterholdLocationList Auto
Quest Property DynamicAnonymityScript Auto
String[] Property EastmarchLocationList Auto
String[] Property HjaalmarchLocationList Auto
String[] Property PaleLocationList Auto
Form Property SolstheimLocation Auto
Int[] Property MajorLocations Auto
String[] Property HaafingarLocationList Auto
String[] Property SolstheimLocationList Auto
Int[] Property CustomLocationRef Auto
String[] Property FalkreathLocationList Auto
String[] Property DefaultLocation Auto
Quest Property Logger Auto
String[] Property UndefinedLocationList Auto
String[] Property ReachLocationList Auto
Quest Property FameManager Auto

Bool Function IsLocationValid(String CheckedLocation) Native

Bool Function LocationCanBeRegistered(String LocationToRegister, Bool ExternalRegister = false) Native

Function UpdateCustomLocationCount() Native

String Function FetchLocationName(Location LocationRef) Native

String Function CurrentLocationName() Native

String Function GetLocalizedFameLocation(Location LocationRef) Native

String Function GetFameLocation(Location LocationRef) Native

Bool Function IsLocationExcluded(Location LocationRef) Native

Function RegisterCustomLocation() Native

Function AddToLocationList(Location LocationRef) Native

Function RemoveFromLocationList(String FameLocation) Native

Function UnregisterCustomLocation(Int LocationIndexToUnregister) Native

Function RegisterCustomLocationExternal(String LocationToRegister, Location LocationRefToRegister) Native

Function UnregisterCustomLocationExternal(String LocationToUnregister) Native
