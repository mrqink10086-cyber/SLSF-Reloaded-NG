ScriptName SLSF_Reloaded_VisibilityManager Extends Quest

GlobalVariable Property AnalCumCountGlobal Auto
GlobalVariable Property AnalCumGlobal Auto
Bool[] Property BodyTattooApplied Auto hidden
Bool[] Property BodyTattooExcluded Auto hidden
String[] Property BodyTattooExtraFameType Auto hidden
String[] Property BodyTattooSubcategory Auto hidden
Bool[] Property BodyTattooVisible Auto hidden
slsf_reloaded_mcm Property Config Auto
Bool Property CountingBodyTats Auto hidden
Bool Property DD_BeltVisible Auto hidden
Bool Property DD_BraVisible Auto hidden
Bool Property DD_CollarVisible Auto hidden
Bool Property DD_HarnessVisible Auto hidden
slsf_reloaded_dynamicanonymity Property DynamicAnonymityScript Auto
Bool[] Property FaceTattooApplied Auto hidden
Bool[] Property FaceTattooExcluded Auto hidden
String[] Property FaceTattooExtraFameType Auto hidden
Bool[] Property FaceTattooVisible Auto hidden
Bool[] Property FootTattooApplied Auto hidden
Bool[] Property FootTattooExcluded Auto hidden
String[] Property FootTattooExtraFameType Auto hidden
Bool[] Property FootTattooVisible Auto hidden
Bool[] Property HandTattooApplied Auto hidden
Bool[] Property HandTattooExcluded Auto hidden
String[] Property HandTattooExtraFameType Auto hidden
Bool[] Property HandTattooVisible Auto hidden
GlobalVariable Property IsBelted Auto
GlobalVariable Property IsCollared Auto
GlobalVariable Property IsHeavilyBound Auto
GlobalVariable Property IsLightlyBound Auto
GlobalVariable Property IsVisiblyBound Auto
Bool Property IsWearingUnhideable Auto hidden
slsf_reloaded_logger Property Logger Auto
slsf_reloaded_modintegration Property Mods Auto
GlobalVariable Property OralCumCountGlobal Auto
GlobalVariable Property OralCumGlobal Auto
Actor Property PlayerRef Auto
Keyword Property SLSF_Reloaded_CoversBack Auto
Keyword Property SLSF_Reloaded_CoversFeet Auto
Keyword Property SLSF_Reloaded_CoversHands Auto
Keyword Property SLSF_Reloaded_DoesNotCoverBack Auto
Keyword Property SLSF_Reloaded_DoesNotCoverFeet Auto
Keyword Property SLSF_Reloaded_DoesNotCoverHands Auto
Keyword Property SLSF_Reloaded_DoesNotHideIdentity Auto
Keyword Property SLSF_Reloaded_HidesIdentity Auto
sexlabframework Property Sexlab Auto
GlobalVariable Property TotalCumCountGlobal Auto
Bool Property UpdateRunning Auto hidden
GlobalVariable Property VaginalCumCountGlobal Auto
GlobalVariable Property VaginalCumGlobal Auto
Int Property VisibleBodyTats Auto hidden

Function Startup()
    SLSFR_GameplayNative.Refresh("FullUpdate")
EndFunction

Function UpdateTattooSlots()
    SLSFR_GameplayNative.Refresh("FullUpdate")
EndFunction

Function FullUpdate()
    SLSFR_GameplayNative.Refresh("FullUpdate")
EndFunction

Bool Function IsPlayerAnonymous() Native

Function CheckAppliedTattoos()
    SLSFR_GameplayNative.Refresh("FullUpdate")
EndFunction

Int Function CountAppliedTattoos(String TattooArea) Native

Int Function CountVisibleTattoos() Native

Int Function CountBodyTattoos() Native

Int Function CountHandTattoos() Native

Int Function CountFaceTattoos() Native

Int Function CountFootTattoos() Native

Bool Function IsBodyTattooVisible(Int SlotNumber) Native

Bool Function IsFaceTattooVisible(Int SlotNumber) Native

Bool Function IsHandTattooVisible(Int SlotNumber) Native

Bool Function IsFootTattooVisible(Int SlotNumber) Native

Bool Function IsAssCumVisible() Native

Int Function CountVisibleAssCum() Native

Bool Function IsVaginalCumVisible() Native

Int Function CountVisibleVaginalCum() Native

Bool Function IsOralCumVisible() Native

Int Function CountVisibleOralCum() Native

Function CheckBondage() Native
