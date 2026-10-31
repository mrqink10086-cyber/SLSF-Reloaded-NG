Scriptname SLSFGroup08PPlusProbe

bool Function Probe(SexLabThread threadRef, Actor actorRef)
    bool[] flags = threadRef.GetCurrentInteractionFlags(actorRef)
    return flags.Length == 28 && threadRef.HasTag("Oral") && threadRef.HasSceneTag("Oral") && threadRef.HasStageTag("LeadIn")
EndFunction
