ScriptName SLSF_Reloaded_ModIntegration Extends Quest

Faction Property AND_Ass Auto hidden
Faction Property AND_Bottomless Auto hidden
Faction Property AND_Bra Auto hidden
Faction Property AND_Chest Auto hidden
Faction Property AND_Genitals Auto hidden
Faction Property AND_Nude Auto hidden
Faction Property AND_Topless Auto hidden
Faction Property AND_Underwear Auto hidden
Faction Property ChaurusBreeder Auto hidden
slsf_reloaded_mcm Property Config Auto
Keyword Property DD_AnalPlug Auto hidden
Keyword Property DD_ArmCuffs Auto hidden
Keyword Property DD_ArmCuffsFront Auto hidden
Keyword Property DD_Armbinder Auto hidden
Keyword Property DD_ArmbinderElbow Auto hidden
Keyword Property DD_Belt Auto hidden
Keyword Property DD_Blindfold Auto hidden
Keyword Property DD_Boots Auto hidden
Keyword Property DD_Bra Auto hidden
Keyword Property DD_Collar Auto hidden
Keyword Property DD_Corset Auto hidden
Keyword Property DD_Gag Auto hidden
Keyword Property DD_GagPanel Auto hidden
Keyword Property DD_Gloves Auto hidden
Keyword Property DD_Harness Auto hidden
Keyword Property DD_HeavyBondage Auto hidden
Keyword Property DD_Hood Auto hidden
Keyword Property DD_LegCuffs Auto hidden
Keyword Property DD_Lockable Auto hidden
Keyword Property DD_NipplePiercing Auto hidden
Keyword Property DD_Suit Auto hidden
Keyword Property DD_VaginalPiercing Auto hidden
Keyword Property DD_VaginalPlug Auto hidden
sr_inflatequest Property FHU Auto hidden
Faction Property FertilityFaction Auto hidden
Faction Property HentaiPregFaction Auto hidden
Bool Property IsANDInstalled Auto hidden
Bool Property IsBimbosInstalled Auto hidden
Bool Property IsDDInstalled Auto hidden
Bool Property IsECInstalled Auto hidden
Bool Property IsESInstalled Auto hidden
Bool Property IsFHUInstalled Auto hidden
Bool Property IsFMInstalled Auto hidden
Bool Property IsFameCommentsInstalled Auto hidden
Bool Property IsHentaiPregInstalled Auto hidden
Bool Property IsLegacySLSFInstalled Auto hidden
Bool Property IsPWInstalled Auto hidden
Bool Property IsSLACSInstalled Auto hidden
Bool Property IsSLSInstalled Auto hidden
Bool Property IsSexlabApproachInstalled Auto hidden
Bool Property IsSlaveTatsInstalled Auto hidden
slsf_configuration Property LegacyConfig Auto hidden
slsf_reloaded_legacyoverwrite Property LegacyOverwrite Auto
pw_trackerscript Property PW_Tracker Auto hidden
slacs_main Property SLACS Auto hidden
GlobalVariable Property SLSF_CurrentFamePCLocation_Anal Auto hidden
GlobalVariable Property SLSF_CurrentFamePCLocation_Argonian Auto hidden
GlobalVariable Property SLSF_CurrentFamePCLocation_Beast Auto hidden
GlobalVariable Property SLSF_CurrentFamePCLocation_Dom_Mas Auto hidden
GlobalVariable Property SLSF_CurrentFamePCLocation_Exh_Exp Auto hidden
GlobalVariable Property SLSF_CurrentFamePCLocation_GentleL Auto hidden
GlobalVariable Property SLSF_CurrentFamePCLocation_Group Auto hidden
GlobalVariable Property SLSF_CurrentFamePCLocation_Khajiit Auto hidden
GlobalVariable Property SLSF_CurrentFamePCLocation_LikeMan Auto hidden
GlobalVariable Property SLSF_CurrentFamePCLocation_LikeWoman Auto hidden
GlobalVariable Property SLSF_CurrentFamePCLocation_Masoc Auto hidden
GlobalVariable Property SLSF_CurrentFamePCLocation_Nasty Auto hidden
GlobalVariable Property SLSF_CurrentFamePCLocation_Oral Auto hidden
GlobalVariable Property SLSF_CurrentFamePCLocation_Orc Auto hidden
GlobalVariable Property SLSF_CurrentFamePCLocation_Pregna Auto hidden
GlobalVariable Property SLSF_CurrentFamePCLocation_Sadic Auto hidden
GlobalVariable Property SLSF_CurrentFamePCLocation_SkoomaUse Auto hidden
GlobalVariable Property SLSF_CurrentFamePCLocation_Slut Auto hidden
GlobalVariable Property SLSF_CurrentFamePCLocation_Sub_Slave Auto hidden
GlobalVariable Property SLSF_CurrentFamePCLocation_Whore Auto hidden
GlobalVariable Property SLSF_Reloaded_CommentFrequency Auto
Keyword Property SLS_BikiniArmor Auto hidden
MagicEffect Property SLS_CollarCurse Auto hidden
Faction Property SpiderBreeder Auto hidden

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

Float Function GetFHUInflation(Actor actorRef)
    sr_InflateQuest source = GetFHUData()
    if source && actorRef
        return source.GetInflation(actorRef)
    endif
    return 0.0
EndFunction
