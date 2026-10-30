;SLSF_Reloaded_ModIntegration.psc - thin Papyrus facade (group 07, task B06).
;
; Frozen downstream-client declarations.  Every function forwards
; to the native SLSF Reloaded NG module; no business logic lives
; in Papyrus.  Compiled to PEX from these declarations, the frozen
; PEXs are the compatibility oracle against the original plugin.
;
; Original instance bindings (see contracts/legacy-api.json):
;   CheckInstalledMods -> quest SLSF_Reloaded_Main (SLSF Reloaded.esp, local 0x808)
;   GetFHUData -> global
;   GetPWData -> global
;   GetLegacyConfig -> global
;   OverwriteLegacyConfig -> quest SLSF_Reloaded_Main (SLSF Reloaded.esp, local 0x808)
;   IsFMPregnant -> quest SLSF_Reloaded_Main (SLSF Reloaded.esp, local 0x808)
;   IsECPregnant -> quest SLSF_Reloaded_Main (SLSF Reloaded.esp, local 0x808)
;   IsESPregnant -> quest SLSF_Reloaded_Main (SLSF Reloaded.esp, local 0x808)
;   IsHentaiPregnant -> quest SLSF_Reloaded_Main (SLSF Reloaded.esp, local 0x808)
;   IsPublicWhore -> quest SLSF_Reloaded_Main (SLSF Reloaded.esp, local 0x808)
;   GetFHUInflation -> quest SLSF_Reloaded_Main (SLSF Reloaded.esp, local 0x808)
;

Function CheckInstalledMods()
Public sr_InflateQuest Function GetFHUData()
Public PW_MainLoopScript Function GetPWData()
Public SLSF_Configuration Function GetLegacyConfig()
Function OverwriteLegacyConfig()
Public Bool Function IsFMPregnant(Actor actorRef)
Public Bool Function IsECPregnant(Actor actorRef)
Public Bool Function IsESPregnant(Actor actorRef)
Public Bool Function IsHentaiPregnant(Actor actorRef)
Public Bool Function IsPublicWhore()
Public Float Function GetFHUInflation(Actor actorRef)
