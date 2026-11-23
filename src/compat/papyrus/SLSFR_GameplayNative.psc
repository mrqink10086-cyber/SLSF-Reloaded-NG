Scriptname SLSFR_GameplayNative Hidden

Function Refresh(String Source) Global Native
Function ObserveNpc(Actor Target) Global Native
Function Equipped(Form Item) Global Native
Function SceneEvent(String Topic, Int ThreadID, String ObservedStart) Global Native
Bool Function OutputCurrent(String Token) Global Native
Bool Function ReplyCurrent(String Token) Global Native

Bool Function PublishStorage(String Token, String StorageKey, Int[] Values) Global
    If !OutputCurrent(Token)
        Return False
    EndIf
    If StorageUtil.IntListCopy(None, StorageKey, Values)
        Return True
    EndIf
    If !OutputCurrent(Token)
        Return False
    EndIf
    StorageUtil.IntListClear(None, StorageKey)
    Int i = 0
    While i < Values.Length
        If !OutputCurrent(Token) || StorageUtil.IntListAdd(None, StorageKey, Values[i]) != i
            Return False
        EndIf
        i += 1
    EndWhile
    Return StorageUtil.IntListCount(None, StorageKey) == Values.Length
EndFunction

Int Function OpenReply(String Token, String EventName) Global
    If ReplyCurrent(Token)
        Return ModEvent.Create(EventName)
    EndIf
    Return 0
EndFunction

Bool Function SendReply(String Token, Int Handle) Global
    If ReplyCurrent(Token)
        Return ModEvent.Send(Handle)
    EndIf
    ModEvent.Release(Handle)
    Return False
EndFunction

Bool Function ReturnRequestedLocation(String Token, String LocationName) Global
    Int handle = OpenReply(Token, "SLSF_Reloaded_ReturnRequestedLocation")
    If handle == 0
        Return False
    EndIf
    ModEvent.PushString(handle, LocationName)
    Return SendReply(Token, handle)
EndFunction

Bool Function ReturnRecognitionTime(String Token, Float RecognitionTime) Global
    Int handle = OpenReply(Token, "SLSF_Reloaded_ReturnRecognitionTime")
    If handle == 0
        Return False
    EndIf
    ModEvent.PushFloat(handle, RecognitionTime)
    Return SendReply(Token, handle)
EndFunction

Bool Function ReturnRequestedFame(String Token, String LocationName, String Category, Int RequestedFame) Global
    Int handle = OpenReply(Token, "SLSF_Reloaded_ReturnRequestedFame")
    If handle == 0
        Return False
    EndIf
    ModEvent.PushString(handle, LocationName)
    ModEvent.PushString(handle, Category)
    ModEvent.PushInt(handle, RequestedFame)
    Return SendReply(Token, handle)
EndFunction

Bool Function ReturnRequestedCum(String Token, Bool Visible) Global
    Int handle = OpenReply(Token, "SLSF_Reloaded_ReturnRequestedCum")
    If handle == 0
        Return False
    EndIf
    ModEvent.PushBool(handle, Visible)
    Return SendReply(Token, handle)
EndFunction

Bool Function ReturnFlagState(String Token, String FlagName, String FlagExists, Bool RequestedFlagState) Global
    Int handle = OpenReply(Token, "SLSF_Reloaded_ReturnFlagState")
    If handle == 0
        Return False
    EndIf
    ModEvent.PushString(handle, FlagName)
    ModEvent.PushString(handle, FlagExists)
    ModEvent.PushBool(handle, RequestedFlagState)
    Return SendReply(Token, handle)
EndFunction

Bool Function ReturnModFlagState(String Token, String ModName, Bool IsModRegistered, String FlagName, String FlagExists, Bool RequestedModFlagState) Global
    Int handle = OpenReply(Token, "SLSF_Reloaded_ReturnModFlagState")
    If handle == 0
        Return False
    EndIf
    ModEvent.PushString(handle, ModName)
    ModEvent.PushBool(handle, IsModRegistered)
    ModEvent.PushString(handle, FlagName)
    ModEvent.PushString(handle, FlagExists)
    ModEvent.PushBool(handle, RequestedModFlagState)
    Return SendReply(Token, handle)
EndFunction

Bool Function ReturnModRegisteredState(String Token, String ModName, Bool IsModRegistered) Global
    Int handle = OpenReply(Token, "SLSF_Reloaded_ReturnModRegisteredState")
    If handle == 0
        Return False
    EndIf
    ModEvent.PushString(handle, ModName)
    ModEvent.PushBool(handle, IsModRegistered)
    Return SendReply(Token, handle)
EndFunction
