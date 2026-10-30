;SLSF_Reloaded_PlayerScript.psc - thin Papyrus facade (group 07, task B06).
;
; Frozen downstream-client declarations.  Every function forwards
; to the native SLSF Reloaded NG module; no business logic lives
; in Papyrus.  Compiled to PEX from these declarations, the frozen
; PEXs are the compatibility oracle against the original plugin.
;
; Original instance bindings (see contracts/legacy-api.json):
;   Startup -> alias PlayerAlias on quest SLSF_Reloaded_Main (SLSF Reloaded.esp, local 0x808)
;   RunNPCDetect -> alias PlayerAlias on quest SLSF_Reloaded_Main (SLSF Reloaded.esp, local 0x808)
;

Function Startup()
Public Function RunNPCDetect()
