;SLSF_Reloaded_CommentManager.psc - thin Papyrus facade (group 07, task B06).
;
; Frozen downstream-client declarations.  Every function forwards
; to the native SLSF Reloaded NG module; no business logic lives
; in Papyrus.  Compiled to PEX from these declarations, the frozen
; PEXs are the compatibility oracle against the original plugin.
;
; Original instance bindings (see contracts/legacy-api.json):
;   NakedCommentPublicWhoreCheck -> quest SLSF_Reloaded_Managers (SLSF Reloaded.esp, local 0x80A)
;

Function NakedCommentPublicWhoreCheck()
