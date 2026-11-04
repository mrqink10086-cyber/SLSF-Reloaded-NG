Scriptname SLSF_Reloaded_ModEventListener extends Quest

Quest Property VisibilityManager Auto
GlobalVariable Property SLSF_Reloaded_CustomLocationCount Auto
Quest Property LocationManager Auto
ReferenceAlias Property playerScript Auto
Quest Property FameManager Auto
Quest Property Anonymity Auto
Quest Property DATA Auto
Quest Property Logger Auto


Event OnInit()
    RegisterExternalEvents()
EndEvent

Function RegisterExternalEvents()
    RegisterForModEvent("SLSF_Reloaded_SendFameGainRoll", "OnExternalFameGainRoll")
    RegisterForModEvent("SLSF_Reloaded_SendFameGain", "OnExternalFameGain")
    RegisterForModEvent("SLSF_Reloaded_SendManualFameGain", "OnExternalManualFameGain")
    RegisterForModEvent("SLSF_Reloaded_SendManualFameGainAllInLocation", "OnExternalManualFameGainAllInLocation")
    RegisterForModEvent("SLSF_Reloaded_SendManualFameGainAll", "OnExternalManualFameGainAll")
    RegisterForModEvent("SLSF_Reloaded_SendFameDecay", "OnExternalFameDecay")
    RegisterForModEvent("SLSF_Reloaded_SendManualFameDecay", "OnExternalManualFameDecay")
    RegisterForModEvent("SLSF_Reloaded_SendManualFameDecayAllInLocation", "OnExternalManualFameDecayAllInLocation")
    RegisterForModEvent("SLSF_Reloaded_SendManualFameDecayAll", "OnExternalManualFameDecayAll")
    RegisterForModEvent("SLSF_Reloaded_SendFameSpreadRoll", "OnExternalFameSpreadRoll")
    RegisterForModEvent("SLSF_Reloaded_SendFameSpread", "OnExternalFameSpread")
    RegisterForModEvent("SLSF_Reloaded_SendFameSpreadByName", "OnExternalFameSpreadByName")
    RegisterForModEvent("SLSF_Reloaded_SendManualFameSpread", "OnExternalManualFameSpread")
    RegisterForModEvent("SLSF_Reloaded_SendLocationRegister", "OnExternalLocationRegister")
    RegisterForModEvent("SLSF_Reloaded_SendLocationUnregister", "OnExternalLocationUnregister")
    RegisterForModEvent("SLSF_Reloaded_RegisterMod", "OnExternalModRegister")
    RegisterForModEvent("SLSF_Reloaded_UnregisterMod", "OnExternalModUnregister")
    RegisterForModEvent("SLSF_Reloaded_SetSlutFlag", "OnExternalSlutFlag")
    RegisterForModEvent("SLSF_Reloaded_SetWhoreFlag", "OnExternalWhoreFlag")
    RegisterForModEvent("SLSF_Reloaded_SetExhibitionistFlag", "OnExternalExhibitionistFlag")
    RegisterForModEvent("SLSF_Reloaded_SetOralFlag", "OnExternalOralFlag")
    RegisterForModEvent("SLSF_Reloaded_SetAnalFlag", "OnExternalAnalFlag")
    RegisterForModEvent("SLSF_Reloaded_SetNastyFlag", "OnExternalNastyFlag")
    RegisterForModEvent("SLSF_Reloaded_SetPregnantFlag", "OnExternalPregnantFlag")
    RegisterForModEvent("SLSF_Reloaded_SetDominantFlag", "OnExternalDominantFlag")
    RegisterForModEvent("SLSF_Reloaded_SetSubmissiveFlag", "OnExternalSubmissiveFlag")
    RegisterForModEvent("SLSF_Reloaded_SetSadistFlag", "OnExternalSadistFlag")
    RegisterForModEvent("SLSF_Reloaded_SetMasochistFlag", "OnExternalMasochistFlag")
    RegisterForModEvent("SLSF_Reloaded_SetGentleFlag", "OnExternalGentleFlag")
    RegisterForModEvent("SLSF_Reloaded_SetLikesMenFlag", "OnExternalLikesMenFlag")
    RegisterForModEvent("SLSF_Reloaded_SetLikesWomenFlag", "OnExternalLikesWomenFlag")
    RegisterForModEvent("SLSF_Reloaded_SetLikesOrcFlag", "OnExternalLikesOrcFlag")
    RegisterForModEvent("SLSF_Reloaded_SetLikesKhajiitFlag", "OnExternalLikesKhajiitFlag")
    RegisterForModEvent("SLSF_Reloaded_SetLikesArgonianFlag", "OnExternalLikesArgonianFlag")
    RegisterForModEvent("SLSF_Reloaded_SetBestialityFlag", "OnExternalBestialityFlag")
    RegisterForModEvent("SLSF_Reloaded_SetGroupFlag", "OnExternalGroupFlag")
    RegisterForModEvent("SLSF_Reloaded_SetBoundFlag", "OnExternalBoundFlag")
    RegisterForModEvent("SLSF_Reloaded_SetTattooFlag", "OnExternalTattooFlag")
    RegisterForModEvent("SLSF_Reloaded_SetCumDumpFlag", "OnExternalCumDumpFlag")
    RegisterForModEvent("SLSF_Reloaded_SetUnfaithfulFlag", "OnExternalUnfaithfulFlag")
    RegisterForModEvent("SLSF_Reloaded_SetCuckFlag", "OnExternalCuckFlag")
    RegisterForModEvent("SLSF_Reloaded_SetAirheadFlag", "OnExternalAirheadFlag")
    RegisterForModEvent("SLSF_Reloaded_SetWhoreEventFlag", "OnExternalWhoreEventFlag")
    RegisterForModEvent("SLSF_Reloaded_SetSexWorkerFlag", "OnExternalSexWorkerFlag")
    RegisterForModEvent("SLSF_Reloaded_EnableFameType", "OnEnableFameType")
    RegisterForModEvent("SLSF_Reloaded_RequestLocation", "OnRequestLocation")
    RegisterForModEvent("SLSF_Reloaded_RequestRecognitionTime", "OnRequestRecognitionTime")
    RegisterForModEvent("SLSF_Reloaded_RequestFame", "OnRequestFame")
    RegisterForModEvent("SLSF_Reloaded_RequestCumVisibility", "OnRequestCumVisibility")
    RegisterForModEvent("SLSF_Reloaded_RequestFlagState", "OnRequestFlagState")
    RegisterForModEvent("SLSF_Reloaded_RequestModFlagState", "OnRequestModFlagState")
    RegisterForModEvent("SLSF_Reloaded_RequestModRegisterState", "OnRequestModRegisterState")
EndFunction

Event OnExternalFameGainRoll(String EventLocation)
    SLSFR_EventSink.OnExternalFameGainRoll(EventLocation)
EndEvent
Event OnExternalFameGain(String Category, String EventLocation, Int MinFame, Int MaxFame)
    SLSFR_EventSink.OnExternalFameGain(Category, EventLocation, MinFame, MaxFame)
EndEvent
Event OnExternalManualFameGain(String Category, String EventLocation, Int MinIncrease, Int MaxIncrease)
    SLSFR_EventSink.OnExternalManualFameGain(Category, EventLocation, MinIncrease, MaxIncrease)
EndEvent
Event OnExternalManualFameGainAllInLocation(String EventLocation, Int MinIncrease, Int MaxIncrease)
    SLSFR_EventSink.OnExternalManualFameGainAllInLocation(EventLocation, MinIncrease, MaxIncrease)
EndEvent
Event OnExternalManualFameGainAll(Int MinIncrease, Int MaxIncrease)
    SLSFR_EventSink.OnExternalManualFameGainAll(MinIncrease, MaxIncrease)
EndEvent
Event OnExternalFameDecay()
    SLSFR_EventSink.OnExternalFameDecay()
EndEvent
Event OnExternalManualFameDecay(String EventLocation, String Category, Int MinDecay, Int MaxDecay)
    SLSFR_EventSink.OnExternalManualFameDecay(EventLocation, Category, MinDecay, MaxDecay)
EndEvent
Event OnExternalManualFameDecayAllInLocation(String EventLocation, Int MinDecay, Int MaxDecay)
    SLSFR_EventSink.OnExternalManualFameDecayAllInLocation(EventLocation, MinDecay, MaxDecay)
EndEvent
Event OnExternalManualFameDecayAll(Int MinDecay, Int MaxDecay)
    SLSFR_EventSink.OnExternalManualFameDecayAll(MinDecay, MaxDecay)
EndEvent
Event OnExternalFameSpreadRoll()
    SLSFR_EventSink.OnExternalFameSpreadRoll()
EndEvent
Event OnExternalFameSpread(Location SpreadFromLocation)
    SLSFR_EventSink.OnExternalFameSpread(SpreadFromLocation)
EndEvent
Event OnExternalFameSpreadByName(String SpreadFromLocation)
    SLSFR_EventSink.OnExternalFameSpreadByName(SpreadFromLocation)
EndEvent
Event OnExternalManualFameSpread(String SpreadFromLocation, String SpreadToLocation, String Category, Int PercentToSpread)
    SLSFR_EventSink.OnExternalManualFameSpread(SpreadFromLocation, SpreadToLocation, Category, PercentToSpread)
EndEvent
Event OnExternalLocationRegister(Location LocationToRegister)
    SLSFR_EventSink.OnExternalLocationRegister(LocationToRegister)
EndEvent
Event OnExternalLocationUnregister(Location LocationToUnregister)
    SLSFR_EventSink.OnExternalLocationUnregister(LocationToUnregister)
EndEvent
Event OnExternalModRegister(String ModName = "-EMPTY-")
    SLSFR_EventSink.OnExternalModRegister(ModName)
EndEvent
Event OnExternalModUnregister(String ModName = "-EMPTY-")
    SLSFR_EventSink.OnExternalModUnregister(ModName)
EndEvent
Event OnExternalSlutFlag(String ModName, Bool Flag)
    SLSFR_EventSink.OnExternalSlutFlag(ModName, Flag)
EndEvent
Event OnExternalWhoreFlag(String ModName, Bool Flag)
    SLSFR_EventSink.OnExternalWhoreFlag(ModName, Flag)
EndEvent
Event OnExternalExhibitionistFlag(String ModName, Bool Flag)
    SLSFR_EventSink.OnExternalExhibitionistFlag(ModName, Flag)
EndEvent
Event OnExternalOralFlag(String ModName, Bool Flag)
    SLSFR_EventSink.OnExternalOralFlag(ModName, Flag)
EndEvent
Event OnExternalAnalFlag(String ModName, Bool Flag)
    SLSFR_EventSink.OnExternalAnalFlag(ModName, Flag)
EndEvent
Event OnExternalNastyFlag(String ModName, Bool Flag)
    SLSFR_EventSink.OnExternalNastyFlag(ModName, Flag)
EndEvent
Event OnExternalPregnantFlag(String ModName, Bool Flag)
    SLSFR_EventSink.OnExternalPregnantFlag(ModName, Flag)
EndEvent
Event OnExternalDominantFlag(String ModName, Bool Flag)
    SLSFR_EventSink.OnExternalDominantFlag(ModName, Flag)
EndEvent
Event OnExternalSubmissiveFlag(String ModName, Bool Flag)
    SLSFR_EventSink.OnExternalSubmissiveFlag(ModName, Flag)
EndEvent
Event OnExternalSadistFlag(String ModName, Bool Flag)
    SLSFR_EventSink.OnExternalSadistFlag(ModName, Flag)
EndEvent
Event OnExternalMasochistFlag(String ModName, Bool Flag)
    SLSFR_EventSink.OnExternalMasochistFlag(ModName, Flag)
EndEvent
Event OnExternalGentleFlag(String ModName, Bool Flag)
    SLSFR_EventSink.OnExternalGentleFlag(ModName, Flag)
EndEvent
Event OnExternalLikesMenFlag(String ModName, Bool Flag)
    SLSFR_EventSink.OnExternalLikesMenFlag(ModName, Flag)
EndEvent
Event OnExternalLikesWomenFlag(String ModName, Bool Flag)
    SLSFR_EventSink.OnExternalLikesWomenFlag(ModName, Flag)
EndEvent
Event OnExternalLikesOrcFlag(String ModName, Bool Flag)
    SLSFR_EventSink.OnExternalLikesOrcFlag(ModName, Flag)
EndEvent
Event OnExternalLikesKhajiitFlag(String ModName, Bool Flag)
    SLSFR_EventSink.OnExternalLikesKhajiitFlag(ModName, Flag)
EndEvent
Event OnExternalLikesArgonianFlag(String ModName, Bool Flag)
    SLSFR_EventSink.OnExternalLikesArgonianFlag(ModName, Flag)
EndEvent
Event OnExternalBestialityFlag(String ModName, Bool Flag)
    SLSFR_EventSink.OnExternalBestialityFlag(ModName, Flag)
EndEvent
Event OnExternalGroupFlag(String ModName, Bool Flag)
    SLSFR_EventSink.OnExternalGroupFlag(ModName, Flag)
EndEvent
Event OnExternalBoundFlag(String ModName, Bool Flag)
    SLSFR_EventSink.OnExternalBoundFlag(ModName, Flag)
EndEvent
Event OnExternalTattooFlag(String ModName, Bool Flag)
    SLSFR_EventSink.OnExternalTattooFlag(ModName, Flag)
EndEvent
Event OnExternalCumDumpFlag(String ModName, Bool Flag)
    SLSFR_EventSink.OnExternalCumDumpFlag(ModName, Flag)
EndEvent
Event OnExternalUnfaithfulFlag(String ModName, Bool Flag)
    SLSFR_EventSink.OnExternalUnfaithfulFlag(ModName, Flag)
EndEvent
Event OnExternalCuckFlag(String ModName, Bool Flag)
    SLSFR_EventSink.OnExternalCuckFlag(ModName, Flag)
EndEvent
Event OnExternalAirheadFlag(String ModName, Bool Flag)
    SLSFR_EventSink.OnExternalAirheadFlag(ModName, Flag)
EndEvent
Event OnExternalWhoreEventFlag(String ModName, Bool Flag)
    SLSFR_EventSink.OnExternalWhoreEventFlag(ModName, Flag)
EndEvent
Event OnExternalSexWorkerFlag(String ModName, Bool Flag)
    SLSFR_EventSink.OnExternalSexWorkerFlag(ModName, Flag)
EndEvent
Event OnEnableFameType(String FameType, String ModName, Bool Flag)
    SLSFR_EventSink.OnEnableFameType(FameType, ModName, Flag)
EndEvent
Event OnRequestLocation(Bool Strict)
    SLSFR_EventSink.OnRequestLocation(Strict)
EndEvent
Event OnRequestRecognitionTime(String FameLocation)
    SLSFR_EventSink.OnRequestRecognitionTime(FameLocation)
EndEvent
Event OnRequestFame(String LocationName, String Category)
    SLSFR_EventSink.OnRequestFame(LocationName, Category)
EndEvent
Event OnRequestCumVisibility(String CumLocation)
    SLSFR_EventSink.OnRequestCumVisibility(CumLocation)
EndEvent
Event OnRequestFlagState(String FlagName)
    SLSFR_EventSink.OnRequestFlagState(FlagName)
EndEvent
Event OnRequestModFlagState(String ModName, String FlagName)
    SLSFR_EventSink.OnRequestModFlagState(ModName, FlagName)
EndEvent
Event OnRequestModRegisterState(String ModName)
    SLSFR_EventSink.OnRequestModRegisterState(ModName)
EndEvent