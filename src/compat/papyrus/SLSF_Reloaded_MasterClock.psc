ScriptName SLSF_Reloaded_MasterClock Extends Quest

slsf_reloaded_mcm Property Config Auto
Int Property DaySync Auto hidden
Int Property DecayTicker Auto hidden
slsf_reloaded_famemanager Property FameManager Auto
Int Property HalfHourSync Auto hidden
Int Property HourSync Auto hidden
Float Property LastTimeChecked Auto hidden
slsf_reloaded_legacyoverwrite Property LegacyOverwrite Auto
slsf_reloaded_logger Property Logger Auto
GlobalVariable Property SLSF_Reloaded_Skooma Auto
Int Property SpreadTicker Auto hidden

Function Startup()
EndFunction

Function HalfHourChecks(Int Multiplier)
EndFunction

Function DecayHandler(Int Multiplier)
EndFunction

Function SpreadHandler(Int Multiplier)
EndFunction

Function PeriodicFameUpdate(Int Multiplier)
EndFunction
