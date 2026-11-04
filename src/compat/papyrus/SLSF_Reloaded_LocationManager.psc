ScriptName SLSF_Reloaded_LocationManager Extends Quest

Location Property CurrentLocation Auto hidden
String[] Property CustomLocation Auto
Location[] Property CustomLocationRef Auto
Bool Property CustomLocationsFull Auto hidden
slsf_reloaded_datamanager Property DataManager Auto
String[] Property DefaultLocation Auto
slsf_reloaded_dynamicanonymity Property DynamicAnonymityScript Auto
String[] Property EastmarchLocationList Auto
Keyword[] Property ExcludedLocations Auto
String[] Property FalkreathLocationList Auto
slsf_reloaded_famemanager Property FameManager Auto
String[] Property HaafingarLocationList Auto
String[] Property HjaalmarchLocationList Auto
slsf_reloaded_logger Property Logger Auto
Location[] Property MajorLocations Auto
Location[] Property MinorLocations Auto
Keyword[] Property OverrideExcludedLocation Auto
String[] Property PaleLocationList Auto
String[] Property ReachLocationList Auto
String[] Property RiftLocationList Auto
GlobalVariable Property SLSF_Reloaded_CustomLocationCount Auto
Location Property SolstheimLocation Auto
String[] Property SolstheimLocationList Auto
String[] Property UndefinedLocationList Auto
String[] Property WhiterunLocationList Auto
String[] Property WinterholdLocationList Auto

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
