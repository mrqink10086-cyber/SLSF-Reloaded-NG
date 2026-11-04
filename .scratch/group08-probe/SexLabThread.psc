ScriptName SexLabThread Extends Quest
{ API Script to directly interact with individual SexLab Threads }

;-- Properties --------------------------------------
Int Property CTYPE_ANY = -1 AutoReadOnly
Int Property CTYPE_Anal = 2 AutoReadOnly
Int Property CTYPE_AnimObjFace = 12 AutoReadOnly
Int Property CTYPE_Deepthroat = 5 AutoReadOnly
Int Property CTYPE_Facial = 11 AutoReadOnly
Int Property CTYPE_FootJob = 8 AutoReadOnly
Int Property CTYPE_Grinding = 4 AutoReadOnly
Int Property CTYPE_HandJob = 9 AutoReadOnly
Int Property CTYPE_Kissing = 10 AutoReadOnly
Int Property CTYPE_LickingShaft = 7 AutoReadOnly
Int Property CTYPE_Oral = 3 AutoReadOnly
Int Property CTYPE_Skullfuck = 6 AutoReadOnly
Int Property CTYPE_SuckingToes = 13 AutoReadOnly
Int Property CTYPE_Vaginal = 1 AutoReadOnly
Int Property PATHING_DISABLE = -1 AutoReadOnly
Int Property PATHING_ENABLE = 0 AutoReadOnly
Int Property PATHING_FORCE = 1 AutoReadOnly
Int Property STATUS_ENDING = 4 AutoReadOnly
Int Property STATUS_IDLE = 1 AutoReadOnly
Int Property STATUS_INSCENE = 3 AutoReadOnly
Int Property STATUS_SETUP = 2 AutoReadOnly
Int Property STATUS_UNDEF = 0 AutoReadOnly

;-- Functions ---------------------------------------

Function AddContext(String asContext)
  ; Empty function
EndFunction

Function AdjustEnjoyment(Actor ActorRef, Int AdjustBy)
  ; Empty function
EndFunction

Function BranchTo(Int aiNextBranch)
  ; Empty function
EndFunction

Actor[] Function CanBeImpregnated(Actor akActor, Bool abAllowFutaImpregnation, Bool abFutaCanPregnate, Bool abCreatureCanPregnate)
  ; Empty function
EndFunction

Function DisableOrgasm(Actor ActorRef, Bool OrgasmDisabled)
  ; Empty function
EndFunction

Function ForceOrgasm(Actor ActorRef)
  ; Empty function
EndFunction

String Function GetActiveScene()
  ; Empty function
EndFunction

String Function GetActiveStage()
  ; Empty function
EndFunction

String Function GetActorExpression(Actor akActor)
  ; Empty function
EndFunction

Int Function GetActorSex(Actor akActor)
  ; Empty function
EndFunction

String Function GetActorVoice(Actor akActor)
  ; Empty function
EndFunction

Int Function GetEnjoyment(Actor ActorRef)
  ; Empty function
EndFunction

String Function GetInteractionString(Actor akPosition)
  ; Empty function
EndFunction

Int[] Function GetInteractionTypes(Actor akPosition, Actor akPartner)
  ; Empty function
EndFunction

Int Function GetNthPositionSex(Int n)
  ; Empty function
EndFunction

Actor Function GetPartnerByType(Actor akPosition, Int aiType)
  ; Empty function
EndFunction

Actor Function GetPartnerByTypeRev(Actor akPartner, Int aiType)
  ; Empty function
EndFunction

Actor[] Function GetPartnersByType(Actor akPosition, Int aiType)
  ; Empty function
EndFunction

Actor[] Function GetPartnersByTypeRev(Actor akPartner, Int aiType)
  ; Empty function
EndFunction

String[] Function GetPlayingScenes()
  ; Empty function
EndFunction

Int Function GetPositionIdx(Actor akActor)
  ; Empty function
EndFunction

Int[] Function GetPositionSexes()
  ; Empty function
EndFunction

Actor[] Function GetPositions()
  ; Empty function
EndFunction

String[] Function GetStageHistory()
  ; Empty function
EndFunction

Int Function GetStageHistoryLength()
  ; Empty function
EndFunction

; Skipped compiler generated GetState

Int Function GetStatus()
  ; Empty function
EndFunction

Form Function GetStrapon(Actor ActorRef)
  ; Empty function
EndFunction

Bool Function GetSubmissive(Actor akActor)
  ; Empty function
EndFunction

Actor[] Function GetSubmissives()
  ; Empty function
EndFunction

Int Function GetThreadID()
  ; Empty function
EndFunction

Float Function GetTime()
  ; Empty function
EndFunction

Float Function GetTimeTotal()
  ; Empty function
EndFunction

Float Function GetVelocity(Actor akPosition, Actor akPartner, Int aiType)
  ; Empty function
EndFunction

; Skipped compiler generated GotoState

Bool Function HasActor(Actor akActor)
  ; Empty function
EndFunction

Bool Function HasContext(String asTag)
  ; Empty function
EndFunction

Bool Function HasInteractionType(Int aiType, Actor akPosition, Actor akPartner)
  ; Empty function
EndFunction

Bool Function HasPlayer()
  ; Empty function
EndFunction

Bool Function HasSceneTag(String asTag)
  ; Empty function
EndFunction

Bool Function HasStageTag(String asTag)
  ; Empty function
EndFunction

Bool Function HasTag(String asTag)
  ; Empty function
EndFunction

Bool Function IsConsent()
  ; Empty function
EndFunction

Bool Function IsInteractionRegistered()
  ; Empty function
EndFunction

Bool Function IsOrgasmAllowed(Actor ActorRef)
  ; Empty function
EndFunction

Bool Function IsRedressAllowed(Actor akActor)
  ; Empty function
EndFunction

Bool Function IsUndressAnimationAllowed(Actor akActor)
  ; Empty function
EndFunction

Bool Function IsUsingStrapon(Actor ActorRef)
  ; Empty function
EndFunction

Function ModEnjoymentMult(Actor ActorRef, Float afSet, Bool bAdjust)
  ; Empty function
EndFunction

Function RemoveContext(String asContext)
  ; Empty function
EndFunction

Function ResetCustomStrip(Actor akActor)
  ; Empty function
EndFunction

Bool Function ResetScene(String asScene)
  ; Empty function
EndFunction

Function SetActorExpression(Actor akActor, String asExpression)
  ; Empty function
EndFunction

Function SetActorVoice(Actor akActor, String asVoice, Bool abForceSilent)
  ; Empty function
EndFunction

Function SetConsent(Bool abIsConsent)
  ; Empty function
EndFunction

Function SetCustomStrip(Actor akActor, Int aiSlots, Bool abWeapon, Bool abApplyNow)
  ; Empty function
EndFunction

Function SetEnjoyment(Actor ActorRef, Int aiSet)
  ; Empty function
EndFunction

Function SetIsRedressAllowed(Actor akActor, Bool abAllowed)
  ; Empty function
EndFunction

Function SetIsSubmissive(Actor akActor, Bool abIsSubmissive)
  ; Empty function
EndFunction

Function SetIsUndressAnimationAllowed(Actor akActor, Bool abAllowed)
  ; Empty function
EndFunction

Function SetPathingFlag(Actor akActor, Int aiPathingFlag)
  ; Empty function
EndFunction

Function SetStrapon(Actor ActorRef, Form ToStrapon)
  ; Empty function
EndFunction

Function SkipTo(String asNextStage)
  ; Empty function
EndFunction

Function StopAnimation()
  ; Empty function
EndFunction

Bool Function IsSceneVaginal()
  Return Self.HasSceneTag("Vaginal") ; #DEBUG_LINE_NO:87
EndFunction

Bool Function IsSceneAnal()
  Return Self.HasSceneTag("Anal") ; #DEBUG_LINE_NO:90
EndFunction

Bool Function IsSceneOral()
  Return Self.HasSceneTag("Oral") ; #DEBUG_LINE_NO:93
EndFunction
