;SLSF_Reloaded_Logger.psc - thin Papyrus facade (group 07, task B06).
;
; Frozen downstream-client declarations.  Every function forwards
; to the native SLSF Reloaded NG module; no business logic lives
; in Papyrus.  Compiled to PEX from these declarations, the frozen
; PEXs are the compatibility oracle against the original plugin.
;
; Original instance bindings (see contracts/legacy-api.json):
;   Log -> global-native
;   EnableLogging -> global-native
;   SetLogLevel -> global-native
;

Public Function Log(String LogString, Int Severity)
Public Function EnableLogging(Bool EnableLogging)
Public Function SetLogLevel(Int LogLevel)
