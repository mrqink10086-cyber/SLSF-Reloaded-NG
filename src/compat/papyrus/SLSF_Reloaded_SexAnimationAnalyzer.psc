;SLSF_Reloaded_SexAnimationAnalyzer.psc - thin Papyrus facade (group 07, task B06).
;
; Frozen downstream-client declarations.  Every function forwards
; to the native SLSF Reloaded NG module; no business logic lives
; in Papyrus.  Compiled to PEX from these declarations, the frozen
; PEXs are the compatibility oracle against the original plugin.
;
; Original instance bindings (see contracts/legacy-api.json):
;   AnimationAnalyze -> alias PlayerAlias on quest SLSF_Reloaded_Main (SLSF Reloaded.esp, local 0x808)
;   AnimationSubAnalyze -> alias PlayerAlias on quest SLSF_Reloaded_Main (SLSF Reloaded.esp, local 0x808)
;

Public Function AnimationAnalyze(Int threadID)
Public Function AnimationSubAnalyze(Actor[] Actors, sslThreadController PlayerController, Int PlayerThread, Bool Foreplay, String PlayerLocation, Bool Friend, Bool Lover)
