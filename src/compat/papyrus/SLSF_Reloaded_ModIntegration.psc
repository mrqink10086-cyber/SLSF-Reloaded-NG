Scriptname SLSF_Reloaded_ModIntegration extends Quest

Quest Property LegacyOverwrite Auto
GlobalVariable Property SLSF_Reloaded_CommentFrequency Auto
Quest Property config Auto

Function CheckInstalledMods()
EndFunction

sr_InflateQuest Function GetFHUData() Global
    return Game.GetFormFromFile(0xD63, "sr_FillherUp.esp") as sr_InflateQuest
EndFunction

PW_MainLoopScript Function GetPWData() Global
    return Game.GetFormFromFile(0xD63, "Public Whore.esp") as PW_MainLoopScript
EndFunction

SLSF_Configuration Function GetLegacyConfig() Global
    return Game.GetFormFromFile(0x00016AB9, "SexLab - Sexual Fame [SLSF].esm") as SLSF_Configuration
EndFunction

Function OverwriteLegacyConfig()
EndFunction

Bool Function IsFMPregnant(Actor actorRef) Native

Bool Function IsECPregnant(Actor actorRef) Native

Bool Function IsESPregnant(Actor actorRef) Native

Bool Function IsHentaiPregnant(Actor actorRef) Native

Bool Function IsPublicWhore() Native

Float Function GetFHUInflation(Actor actorRef) Native
