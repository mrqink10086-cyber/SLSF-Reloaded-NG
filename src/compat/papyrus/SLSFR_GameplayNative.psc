Scriptname SLSFR_GameplayNative Hidden

Function Refresh(String Source) Global Native
Function ObserveNpc(Actor Target) Global Native
Function Equipped(Form Item) Global Native
Function SceneEvent(String Topic, Int ThreadID) Global Native
Bool Function OutputCurrent(String Token) Global Native

Function PublishStorage(String Token, String StorageKey, Int[] Values) Global
    If OutputCurrent(Token)
        StorageUtil.IntListCopy(None, StorageKey, Values)
    EndIf
EndFunction
