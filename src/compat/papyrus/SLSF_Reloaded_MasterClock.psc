;SLSF_Reloaded_MasterClock.psc - thin Papyrus facade (group 07, task B06).
;
; Frozen downstream-client declarations.  Every function forwards
; to the native SLSF Reloaded NG module; no business logic lives
; in Papyrus.  Compiled to PEX from these declarations, the frozen
; PEXs are the compatibility oracle against the original plugin.
;
; Original instance bindings (see contracts/legacy-api.json):
;   Startup -> quest SLSF_Reloaded_MasterClock (SLSF Reloaded.esp, local 0x839)
;   HalfHourChecks -> quest SLSF_Reloaded_MasterClock (SLSF Reloaded.esp, local 0x839)
;   DecayHandler -> quest SLSF_Reloaded_MasterClock (SLSF Reloaded.esp, local 0x839)
;   SpreadHandler -> quest SLSF_Reloaded_MasterClock (SLSF Reloaded.esp, local 0x839)
;   PeriodicFameUpdate -> quest SLSF_Reloaded_MasterClock (SLSF Reloaded.esp, local 0x839)
;

Function Startup()
Function HalfHourChecks(Int Multiplier)
Function DecayHandler(Int Multiplier)
Function SpreadHandler(Int Multiplier)
Function PeriodicFameUpdate(Int Multiplier)
