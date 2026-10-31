Scriptname SLSF_Reloaded_PPlusProbe Hidden

String Function Tags(String[] values, String scope) Global
    String result = ""
    Int i = 0
    While i < values.Length
        result += "tag\t" + scope + "\t" + SLSF_Reloaded_ProbeNative.Escape(values[i]) + "\n"
        i += 1
    EndWhile
    Return result
EndFunction

String Function CollectScene(Int threadId, String session, String indexGeneration, String sceneGeneration) Global
    String prefix = "SLSFPP1\nversion\t1\nepoch\t" + session + "\t" + indexGeneration + "\t" + sceneGeneration + "\nactive\tfalse\nstage\t0\t0\n"
    If Game.GetModByName("SexLab.esm") == 255
        Return prefix + "availability\tMissing\n"
    EndIf
    Int[] version = SexLabUtil.GetVersionPack()
    If version.Length < 2
        Return prefix + "availability\tIncompatible\n"
    EndIf
    If version[0] != 2 || version[1] < 18
        Return prefix + "availability\tIncompatible\n"
    EndIf
    SexLabFramework api = SexLabUtil.GetAPI()
    If !api || threadId < 0
        Return prefix + "availability\tUnknown\n"
    EndIf
    SexLabThread thread = api.GetThread(threadId)
    If !thread || thread.GetThreadID() != threadId || thread.GetStatus() != 3
        Return prefix + "availability\tUnknown\n"
    EndIf
    String sceneId = thread.GetActiveScene()
    String stage = thread.GetActiveStage()
    String[] stages = SexlabRegistry.GetAllStages(sceneId)
    Int stageIndex = stages.Find(stage)
    sslThreadModel model = thread as sslThreadModel
    If sceneId == "" || stage == "" || stageIndex < 0 || !model
        Return prefix + "availability\tUnknown\n"
    EndIf
    Bool physics = thread.IsInteractionRegistered()
    String result = "SLSFPP1\nversion\t1\navailability\tAvailable\nprovider\tSexLabPPlus\t" + SexLabUtil.GetStringVer() + "\t1\nepoch\t" + session + "\t" + indexGeneration + "\t" + sceneGeneration + "\nactive\ttrue\nstage\t" + stageIndex + "\t" + stages.Length + "\nscene\t" + SLSF_Reloaded_ProbeNative.Escape(sceneId) + "\t" + SLSF_Reloaded_ProbeNative.Escape(stage) + "\t" + model.IsLeadIn() + "\n"
    result += Tags(SexlabRegistry.GetSceneTags(sceneId), "scene")
    result += Tags(SexlabRegistry.GetSceneAnnotations(sceneId), "scene")
    result += Tags(SexlabRegistry.GetStageTags(sceneId, stage), "stage")
    Actor[] actors = thread.GetPositions()
    If actors.Length == 0 || actors.Length > 64
        Return prefix + "availability\tUnknown\n"
    EndIf
    Int i = 0
    While i < actors.Length
        Actor participant = actors[i]
        If !participant || !thread.HasActor(participant)
            Return prefix + "availability\tUnknown\n"
        EndIf
        String identity = SLSF_Reloaded_ProbeNative.Identity(participant)
        If identity == ""
            Return prefix + "availability\tUnknown\n"
        EndIf
        Int flags = 0
        If participant == Game.GetPlayer()
            flags += 128
        EndIf
        Int roleUnknown = 80
        Int sex = thread.GetActorSex(participant)
        String sexCode = sex As String
        If sex == 1 || sex == 4
            flags += 1
        ElseIf sex < 0 || sex > 4
            roleUnknown += 1
            sexCode = "unknown"
        EndIf
        If thread.IsUsingStrapon(participant)
            flags += 2
        EndIf
        If thread.GetSubmissive(participant)
            flags += 36
        EndIf
        If api.IsAggressor(threadId, participant)
            flags += 8
        EndIf
        Int mask = 0
        Int unknownMask = 0xFFFFFFF
        Int bit = 0
        If physics
            unknownMask = 0x200101
            While bit < 28
                Int collision = CollisionType(bit)
                If collision > 0
                    Bool hit
                    If ReverseDirection(bit)
                        hit = thread.HasInteractionType(collision, None, participant)
                    Else
                        hit = thread.HasInteractionType(collision, participant, None)
                    EndIf
                    If hit
                        mask = Math.LogicalOr(mask, Math.LeftShift(1, bit))
                    EndIf
                EndIf
                bit += 1
            EndWhile
        EndIf
        result += "actor\t" + identity + "\t" + SLSF_Reloaded_ProbeNative.Escape(participant.GetDisplayName()) + "\t" + SLSF_Reloaded_ProbeNative.ActorRace(participant) + "\t" + SLSF_Reloaded_ProbeNative.Hex(flags) + "\t" + SLSF_Reloaded_ProbeNative.Hex(mask) + "\t" + SLSF_Reloaded_ProbeNative.Hex(unknownMask) + "\t" + SLSF_Reloaded_ProbeNative.Hex(roleUnknown) + "\t" + sexCode + "\n"
        i += 1
    EndWhile
    If thread.GetThreadID() != threadId || thread.GetStatus() != 3 || thread.GetActiveScene() != sceneId || thread.GetActiveStage() != stage || thread.IsInteractionRegistered() != physics
        Return prefix + "availability\tUnknown\n"
    EndIf
    Return result
EndFunction

Int Function CollisionType(Int bit) Global
    If bit == 1 || bit == 2
        Return 12
    ElseIf bit == 3 || bit == 10
        Return 13
    ElseIf bit == 4 || bit == 18
        Return 4
    ElseIf bit == 5 || bit == 25
        Return 6
    ElseIf bit == 6 || bit == 19
        Return 9
    ElseIf bit == 7 || bit == 20
        Return 8
    ElseIf bit == 9
        Return 10
    ElseIf bit == 11 || bit == 17
        Return 11
    ElseIf bit == 12 || bit == 23
        Return 3
    ElseIf bit == 13 || bit == 22
        Return 7
    ElseIf bit == 14 || bit == 24
        Return 5
    ElseIf bit == 15 || bit == 26
        Return 1
    ElseIf bit == 16 || bit == 27
        Return 2
    EndIf
    Return 0
EndFunction

Bool Function ReverseDirection(Int bit) Global
    Return bit == 1 || bit == 3 || bit == 17 || bit == 18 || bit == 19 || bit == 20 || bit == 22 || bit == 23 || bit == 24 || bit == 25 || bit == 26 || bit == 27
EndFunction
