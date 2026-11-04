Scriptname SLSF_Reloaded_VisibilityManager extends Quest

GlobalVariable Property VaginalCumGlobal Auto
Keyword Property SLSF_Reloaded_DoesNotHideIdentity Auto
ObjectReference Property PlayerRef Auto
Quest Property config Auto
GlobalVariable Property IsBelted Auto
Quest Property mods Auto
Quest Property DynamicAnonymityScript Auto
GlobalVariable Property IsCollared Auto
Keyword Property SLSF_Reloaded_DoesNotCoverBack Auto
Keyword Property SLSF_Reloaded_CoversFeet Auto
GlobalVariable Property AnalCumCountGlobal Auto
GlobalVariable Property VaginalCumCountGlobal Auto
GlobalVariable Property TotalCumCountGlobal Auto
Form Property SexLab Auto
Keyword Property SLSF_Reloaded_DoesNotCoverHands Auto
GlobalVariable Property IsVisiblyBound Auto
Keyword Property SLSF_Reloaded_DoesNotCoverFeet Auto
GlobalVariable Property OralCumGlobal Auto
GlobalVariable Property OralCumCountGlobal Auto
Keyword Property SLSF_Reloaded_HidesIdentity Auto
Quest Property Logger Auto
GlobalVariable Property IsLightlyBound Auto
Keyword Property SLSF_Reloaded_CoversBack Auto
GlobalVariable Property IsHeavilyBound Auto
GlobalVariable Property AnalCumGlobal Auto
Keyword Property SLSF_Reloaded_CoversHands Auto

Function Startup()
EndFunction

Function UpdateTattooSlots()
EndFunction

Function FullUpdate()
EndFunction

Bool Function IsPlayerAnonymous() Native

Function CheckAppliedTattoos()
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
