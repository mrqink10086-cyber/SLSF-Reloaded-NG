;SLSF_Reloaded_DataManager.psc - thin Papyrus facade (group 07, task B06).
;
; Frozen downstream-client declarations.  Every function forwards
; to the native SLSF Reloaded NG module; no business logic lives
; in Papyrus.  Compiled to PEX from these declarations, the frozen
; PEXs are the compatibility oracle against the original plugin.
;
; Original instance bindings (see contracts/legacy-api.json):
;   FameCheck -> quest SLSF_Reloaded_Managers (SLSF Reloaded.esp, local 0x80A)
;   SetDefaults -> quest SLSF_Reloaded_Managers (SLSF Reloaded.esp, local 0x80A)
;   CheckSpreadableFame -> quest SLSF_Reloaded_Managers (SLSF Reloaded.esp, local 0x80A)
;   GetFameValue -> quest SLSF_Reloaded_Managers (SLSF Reloaded.esp, local 0x80A)
;   SetFameValue -> quest SLSF_Reloaded_Managers (SLSF Reloaded.esp, local 0x80A)
;   DecayDynamicAnonymityCheck -> quest SLSF_Reloaded_Managers (SLSF Reloaded.esp, local 0x80A)
;   GetExternalFlags -> quest SLSF_Reloaded_Managers (SLSF Reloaded.esp, local 0x80A)
;   RegisterExternalMod -> quest SLSF_Reloaded_Managers (SLSF Reloaded.esp, local 0x80A)
;   CountExternalMods -> quest SLSF_Reloaded_Managers (SLSF Reloaded.esp, local 0x80A)
;   CleanExternalModList -> quest SLSF_Reloaded_Managers (SLSF Reloaded.esp, local 0x80A)
;   UnregisterExternalMod -> quest SLSF_Reloaded_Managers (SLSF Reloaded.esp, local 0x80A)
;   GetModFlagState -> quest SLSF_Reloaded_Managers (SLSF Reloaded.esp, local 0x80A)
;   DoesFlagExist -> quest SLSF_Reloaded_Managers (SLSF Reloaded.esp, local 0x80A)
;   IsModRegistered -> quest SLSF_Reloaded_Managers (SLSF Reloaded.esp, local 0x80A)
;   SetExternalFlags -> quest SLSF_Reloaded_Managers (SLSF Reloaded.esp, local 0x80A)
;   CheckSexWorker -> quest SLSF_Reloaded_Managers (SLSF Reloaded.esp, local 0x80A)
;   CheckFlags -> quest SLSF_Reloaded_Managers (SLSF Reloaded.esp, local 0x80A)
;   FameOverviewCheck -> quest SLSF_Reloaded_Managers (SLSF Reloaded.esp, local 0x80A)
;

Function FameCheck()
Function SetDefaults()
Function CheckSpreadableFame()
Public Int Function GetFameValue(String LocationName, String FameCategory)
Public Function SetFameValue(String LocationName, String FameCategory, Int FameValue)
Function DecayDynamicAnonymityCheck(Int LocationIndex, Bool IsDefaultLocation)
Public Bool Function GetExternalFlags(String FlagName)
Public Function RegisterExternalMod(String ModName)
Public Int Function CountExternalMods()
Public Function CleanExternalModList()
Public Function UnregisterExternalMod(String ModName)
Public Bool Function GetModFlagState(String ModName, String FlagName)
Public String Function DoesFlagExist(String FlagName)
Public Bool Function IsModRegistered(String ModName)
Public Function SetExternalFlags(String ModName, String FlagName, Bool FlagValue)
Public Bool Function CheckSexWorker()
Function CheckFlags()
Function FameOverviewCheck()
