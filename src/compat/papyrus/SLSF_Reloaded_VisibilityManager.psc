;SLSF_Reloaded_VisibilityManager.psc - thin Papyrus facade (group 07, task B06).
;
; Frozen downstream-client declarations.  Every function forwards
; to the native SLSF Reloaded NG module; no business logic lives
; in Papyrus.  Compiled to PEX from these declarations, the frozen
; PEXs are the compatibility oracle against the original plugin.
;
; Original instance bindings (see contracts/legacy-api.json):
;   Startup -> quest SLSF_Reloaded_Managers (SLSF Reloaded.esp, local 0x80A)
;   UpdateTattooSlots -> quest SLSF_Reloaded_Managers (SLSF Reloaded.esp, local 0x80A)
;   FullUpdate -> quest SLSF_Reloaded_Managers (SLSF Reloaded.esp, local 0x80A)
;   IsPlayerAnonymous -> quest SLSF_Reloaded_Managers (SLSF Reloaded.esp, local 0x80A)
;   CheckAppliedTattoos -> quest SLSF_Reloaded_Managers (SLSF Reloaded.esp, local 0x80A)
;   CountAppliedTattoos -> quest SLSF_Reloaded_Managers (SLSF Reloaded.esp, local 0x80A)
;   CountVisibleTattoos -> quest SLSF_Reloaded_Managers (SLSF Reloaded.esp, local 0x80A)
;   CountBodyTattoos -> quest SLSF_Reloaded_Managers (SLSF Reloaded.esp, local 0x80A)
;   CountHandTattoos -> quest SLSF_Reloaded_Managers (SLSF Reloaded.esp, local 0x80A)
;   CountFaceTattoos -> quest SLSF_Reloaded_Managers (SLSF Reloaded.esp, local 0x80A)
;   CountFootTattoos -> quest SLSF_Reloaded_Managers (SLSF Reloaded.esp, local 0x80A)
;   IsBodyTattooVisible -> quest SLSF_Reloaded_Managers (SLSF Reloaded.esp, local 0x80A)
;   IsFaceTattooVisible -> quest SLSF_Reloaded_Managers (SLSF Reloaded.esp, local 0x80A)
;   IsHandTattooVisible -> quest SLSF_Reloaded_Managers (SLSF Reloaded.esp, local 0x80A)
;   IsFootTattooVisible -> quest SLSF_Reloaded_Managers (SLSF Reloaded.esp, local 0x80A)
;   IsAssCumVisible -> quest SLSF_Reloaded_Managers (SLSF Reloaded.esp, local 0x80A)
;   CountVisibleAssCum -> quest SLSF_Reloaded_Managers (SLSF Reloaded.esp, local 0x80A)
;   IsVaginalCumVisible -> quest SLSF_Reloaded_Managers (SLSF Reloaded.esp, local 0x80A)
;   CountVisibleVaginalCum -> quest SLSF_Reloaded_Managers (SLSF Reloaded.esp, local 0x80A)
;   IsOralCumVisible -> quest SLSF_Reloaded_Managers (SLSF Reloaded.esp, local 0x80A)
;   CountVisibleOralCum -> quest SLSF_Reloaded_Managers (SLSF Reloaded.esp, local 0x80A)
;   CheckBondage -> quest SLSF_Reloaded_Managers (SLSF Reloaded.esp, local 0x80A)
;

Function Startup()
Function UpdateTattooSlots()
Function FullUpdate()
Public Bool Function IsPlayerAnonymous()
Function CheckAppliedTattoos()
Public Int Function CountAppliedTattoos(String TattooArea)
Public Int Function CountVisibleTattoos()
Public Int Function CountBodyTattoos()
Public Int Function CountHandTattoos()
Public Int Function CountFaceTattoos()
Public Int Function CountFootTattoos()
Public Bool Function IsBodyTattooVisible(Int SlotNumber)
Public Bool Function IsFaceTattooVisible(Int SlotNumber)
Public Bool Function IsHandTattooVisible(Int SlotNumber)
Public Bool Function IsFootTattooVisible(Int SlotNumber)
Public Bool Function IsAssCumVisible()
Public Int Function CountVisibleAssCum()
Public Bool Function IsVaginalCumVisible()
Public Int Function CountVisibleVaginalCum()
Public Bool Function IsOralCumVisible()
Public Int Function CountVisibleOralCum()
Public Function CheckBondage()
