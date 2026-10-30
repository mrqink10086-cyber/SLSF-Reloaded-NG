;SLSF_Reloaded_LocationManager.psc - thin Papyrus facade (group 07, task B06).
;
; Frozen downstream-client declarations.  Every function forwards
; to the native SLSF Reloaded NG module; no business logic lives
; in Papyrus.  Compiled to PEX from these declarations, the frozen
; PEXs are the compatibility oracle against the original plugin.
;
; Original instance bindings (see contracts/legacy-api.json):
;   IsLocationValid -> quest SLSF_Reloaded_Managers (SLSF Reloaded.esp, local 0x80A)
;   LocationCanBeRegistered -> quest SLSF_Reloaded_Managers (SLSF Reloaded.esp, local 0x80A)
;   UpdateCustomLocationCount -> quest SLSF_Reloaded_Managers (SLSF Reloaded.esp, local 0x80A)
;   FetchLocationName -> quest SLSF_Reloaded_Managers (SLSF Reloaded.esp, local 0x80A)
;   CurrentLocationName -> quest SLSF_Reloaded_Managers (SLSF Reloaded.esp, local 0x80A)
;   GetLocalizedFameLocation -> quest SLSF_Reloaded_Managers (SLSF Reloaded.esp, local 0x80A)
;   GetFameLocation -> quest SLSF_Reloaded_Managers (SLSF Reloaded.esp, local 0x80A)
;   IsLocationExcluded -> quest SLSF_Reloaded_Managers (SLSF Reloaded.esp, local 0x80A)
;   RegisterCustomLocation -> quest SLSF_Reloaded_Managers (SLSF Reloaded.esp, local 0x80A)
;   AddToLocationList -> quest SLSF_Reloaded_Managers (SLSF Reloaded.esp, local 0x80A)
;   RemoveFromLocationList -> quest SLSF_Reloaded_Managers (SLSF Reloaded.esp, local 0x80A)
;   UnregisterCustomLocation -> quest SLSF_Reloaded_Managers (SLSF Reloaded.esp, local 0x80A)
;   RegisterCustomLocationExternal -> quest SLSF_Reloaded_Managers (SLSF Reloaded.esp, local 0x80A)
;   UnregisterCustomLocationExternal -> quest SLSF_Reloaded_Managers (SLSF Reloaded.esp, local 0x80A)
;

Public Bool Function IsLocationValid(String CheckedLocation)
Public Bool Function LocationCanBeRegistered(String LocationToRegister, Bool ExternalRegister)
Public Function UpdateCustomLocationCount()
Public String Function FetchLocationName(Location LocationRef)
Public String Function CurrentLocationName()
Public String Function GetLocalizedFameLocation(Location LocationRef)
Public String Function GetFameLocation(Location LocationRef)
Public Bool Function IsLocationExcluded(Location LocationRef)
Public Function RegisterCustomLocation()
Public Function AddToLocationList(Location LocationRef)
Public Function RemoveFromLocationList(String FameLocation)
Public Function UnregisterCustomLocation(Int LocationIndexToUnregister)
Public Function RegisterCustomLocationExternal(String LocationToRegister, Location LocationRefToRegister)
Public Function UnregisterCustomLocationExternal(String LocationToUnregister)
