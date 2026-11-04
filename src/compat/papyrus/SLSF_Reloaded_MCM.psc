ScriptName SLSF_Reloaded_MCM Extends SKI_ConfigBase

Bool Property AllowBestialityWhenRaped = False Auto hidden
Bool Property AllowCollarBoundFame = False Auto hidden
Bool Property AllowForeplayFame = True Auto hidden
Bool Property AllowLegacyOverwrite = False Auto hidden
Bool Property AllowLikeFameWhenRaped = False Auto hidden
Bool Property AllowSLSCursedCollarBoundFame = False Auto hidden
Bool Property AnonymityEnabled = True Auto hidden
Bool Property ArmUninstall = False Auto hidden
Int Property BodyTattooIndex = 0 Auto hidden
Int Property BodyTattooLimit = 6 Auto hidden
Int Property BodyTattooSlots = 1 Auto hidden
Bool Property ClearAllFameConfirm = False Auto hidden
Bool Property ClearAllFameTrigger = False Auto hidden
slsf_reloaded_commentmanager Property Comments Auto
Bool Property ConfirmUninstall = False Auto hidden
Float Property CumDumpFHUReq = 2.0 Auto hidden
Int[] Property CustomLocationSpreadChance Auto
slsf_reloaded_datamanager Property Data Auto
slsf_reloaded_data_exporter Property DataExporter Auto
slsf_reloaded_data_importer Property DataImporter Auto
Int Property DecayTimeNeeded = 24 Auto hidden
Int[] Property DefaultLocationSpreadChance Auto
Bool Property DisableNakedCommentsWhilePW = False Auto hidden
Bool Property DominantDefault = False Auto hidden
Bool Property DynamicAnonymity = False Auto hidden
Int Property DynamicAnonymityFameCutoff = 50 Auto hidden
Bool Property ExportData = False Auto hidden
String Property ExportName = "" Auto hidden
Int Property FaceTattooIndex = 0 Auto hidden
Int Property FaceTattooLimit = 3 Auto hidden
Int Property FaceTattooSlots = 1 Auto hidden
Int Property FailedSpreadIncrease = 10 Auto hidden
Float[] Property FameCategoryMultiplier Auto
Int Property FameChanceByEnemy = 100 Auto hidden
Int Property FameChanceByFriend = 50 Auto hidden
Int Property FameChanceByLover = 25 Auto hidden
Int Property FameChanceByNeutral = 75 Auto hidden
Float Property FameChangeMultiplier = 1.0 Auto hidden
Bool[] Property FameForbiddenByFriend Auto
Bool[] Property FameForbiddenByLover Auto
slsf_reloaded_famemanager Property FameManager Auto
Bool Property FameSpreadRestrictions = False Auto hidden
Int Property FootTattooIndex = 0 Auto hidden
Int Property FootTattooLimit = 3 Auto hidden
Int Property FootTattooSlots = 1 Auto hidden
Int Property FriendBetrayChance = 50 Auto hidden
Int Property FriendHighFameThreshold = 0 Auto hidden
Bool Property FriendsBetrayFromHighFame = True Auto hidden
Int Property HandTattooIndex = 0 Auto hidden
Int Property HandTattooLimit = 3 Auto hidden
Int Property HandTattooSlots = 1 Auto hidden
Bool[] Property HasFameAtCustomLocation Auto
Bool[] Property HasFameAtDefaultLocation Auto
Bool Property ImportData = False Auto hidden
GlobalVariable Property IsVisiblyBound Auto
Int Property LocationDetailsIndex = 0 Auto hidden
String Property LocationDetailsSelected = "" Auto hidden
slsf_reloaded_locationmanager Property LocationManager Auto
Bool Property Logging = False Auto hidden
Int Property LoggingLevel = 1 Auto hidden
Int Property LoverBetrayChance = 25 Auto hidden
Int Property LoverHighFameThreshold = 50 Auto hidden
Bool Property LoversBetrayFromHighFame = True Auto hidden
Int Property MaxHighFameDecay = 2 Auto hidden
Int Property MaxHighFameGain = 4 Auto hidden
Int Property MaxLowFameDecay = 4 Auto hidden
Int Property MaxLowFameGain = 8 Auto hidden
Int Property MaxMedFameDecay = 3 Auto hidden
Int Property MaxMedFameGain = 6 Auto hidden
Int Property MaxVHighFameDecay = 1 Auto hidden
Int Property MaxVHighFameGain = 2 Auto hidden
Int Property MaxVLowFameDecay = 5 Auto hidden
Int Property MaxVLowFameGain = 10 Auto hidden
Int Property MaximumSpreadCategories = 5 Auto hidden
Int Property MaximumSpreadPercentage = 30 Auto hidden
Int Property MinimumFameToSpread = 30 Auto hidden
Float Property MinimumNPCLOSDistance = 160.0 Auto hidden
slsf_reloaded_modintegration Property Mods Auto
Bool Property NPCNeedsLOS = True Auto hidden
Int Property NightEnd = 6 Auto hidden
Int Property NightStart = 22 Auto hidden
Bool Property NotifyFameDecay = False Auto hidden
Bool Property NotifyFameIncrease = False Auto hidden
Bool Property NotifyFameSpread = False Auto hidden
Int[] Property OptionID Auto hidden
Bool Property PlayerIsAnonymous = False Auto hidden
slsf_reloaded_playerscript Property PlayerScript Auto
Bool Property ReduceFameAtNight = True Auto hidden
Bool Property RegisterLocationConfirm = False Auto hidden
Bool Property RegisterLocationTrigger = False Auto hidden
GlobalVariable Property SLSF_Reloaded_CommentFrequency Auto
GlobalVariable Property SLSF_Reloaded_CustomLocationCount Auto
Int Property SexFameChanceByFriend = 50 Auto hidden
Int Property SexFameChanceByLover = 25 Auto hidden
Int Property SpreadTimeNeeded = 48 Auto hidden
Bool Property SubmissiveDefault = False Auto hidden
Int Property SuccessfulSpreadReduction = 10 Auto hidden
Bool Property TattooLimitChanged Auto hidden
Bool Property TattooLimitUnlocked = False Auto hidden
String Property TattooStatusSelect Auto hidden
Bool Property ThanesCantBeAnonymous = True Auto hidden
slsf_reloaded_uninstall Property Uninstaller Auto
Bool Property UnregisterLocationConfirm = False Auto hidden
Int Property UnregisterLocationIndex = 0 Auto hidden
String Property UnregisterLocationSelection = "" Auto hidden
Bool Property UnregisterLocationTrigger = False Auto hidden
Bool Property UseGlobalFameMultiplier = True Auto hidden
Bool Property UserIsModAuthor = False Auto hidden
Bool Property VictimsAreMasochist = False Auto hidden
slsf_reloaded_visibilitymanager Property VisibilityManager Auto
GlobalVariable Property WICommentChanceNaked Auto

Event OnInit()
    OnGameReload()
EndEvent

Event OnGameReload()
    UnregisterForModEvent("SKICP_configManagerReady")
    UnregisterForModEvent("SKICP_configManagerReset")
EndEvent

Event OnConfigManagerReady(String EventName, String Argument, Float Number, Form Sender)
EndEvent
