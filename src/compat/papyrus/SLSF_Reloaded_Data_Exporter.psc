;SLSF_Reloaded_Data_Exporter.psc - thin Papyrus facade (group 07, task B06).
;
; Frozen downstream-client declarations.  Every function forwards
; to the native SLSF Reloaded NG module; no business logic lives
; in Papyrus.  Compiled to PEX from these declarations, the frozen
; PEXs are the compatibility oracle against the original plugin.
;
; Original instance bindings (see contracts/legacy-api.json):
;   RunExport -> quest SLSF_Reloaded_Managers (SLSF Reloaded.esp, local 0x80A)
;   ExportData -> quest SLSF_Reloaded_Managers (SLSF Reloaded.esp, local 0x80A)
;

Public Function RunExport(String FileName)
Public Function ExportData(String FileName)
