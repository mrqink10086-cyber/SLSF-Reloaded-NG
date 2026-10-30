;SLSF_Reloaded_DynamicAnonymity.psc - thin Papyrus facade (group 07, task B06).
;
; Frozen downstream-client declarations.  Every function forwards
; to the native SLSF Reloaded NG module; no business logic lives
; in Papyrus.  Compiled to PEX from these declarations, the frozen
; PEXs are the compatibility oracle against the original plugin.
;
; Original instance bindings (see contracts/legacy-api.json):
;   CompareLocations -> quest SLSF_Reloaded_Managers (SLSF Reloaded.esp, local 0x80A)
;   SetLastExitTime -> quest SLSF_Reloaded_Managers (SLSF Reloaded.esp, local 0x80A)
;   SetLastEnterTime -> quest SLSF_Reloaded_Managers (SLSF Reloaded.esp, local 0x80A)
;   CheckRecognitionTimeIncrease -> quest SLSF_Reloaded_Managers (SLSF Reloaded.esp, local 0x80A)
;   CheckRecognitionTimeDecay -> quest SLSF_Reloaded_Managers (SLSF Reloaded.esp, local 0x80A)
;   UpdateCurrentLocationAnonymity -> quest SLSF_Reloaded_Managers (SLSF Reloaded.esp, local 0x80A)
;   GetAnonymity -> quest SLSF_Reloaded_Managers (SLSF Reloaded.esp, local 0x80A)
;   CheckAnonymityStatus -> quest SLSF_Reloaded_Managers (SLSF Reloaded.esp, local 0x80A)
;   ThaneStatus -> quest SLSF_Reloaded_Managers (SLSF Reloaded.esp, local 0x80A)
;

Public Function CompareLocations(Location OldLocation, Location NewLocation)
Public Function SetLastExitTime(String FameLocation, Float GameTime)
Public Function SetLastEnterTime(String FameLocation, Float GameTime)
Public Function CheckRecognitionTimeIncrease(String FameLocation)
Public Function CheckRecognitionTimeDecay(String FameLocation)
Function UpdateCurrentLocationAnonymity(Int LocationIndex, Float GameTime, Bool IsDefaultLocation)
Public Function GetAnonymity(Location LocationRef)
Function CheckAnonymityStatus(Int LocationIndex, Bool IsDefaultLocation)
Public Bool Function ThaneStatus(String FameLocation)
