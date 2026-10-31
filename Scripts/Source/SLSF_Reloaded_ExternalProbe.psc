Scriptname SLSF_Reloaded_ExternalProbe Hidden

String Function Presence(String plugin) Global
    If Game.GetModByName(plugin) == 255
        Return "Missing"
    EndIf
    Return "Available"
EndFunction

String Function Collect(String payload) Global
    String epoch = SLSF_Reloaded_ProbeNative.RequestEpoch(payload)
    String prefix = "SLSFEX1\nversion\t1\n" + epoch
    If epoch == ""
        Return ""
    EndIf
    Actor[] actors = SLSF_Reloaded_ProbeNative.RequestActors(payload)
    If actors.Length == 0 || actors.Length > 64
        Return prefix + "availability\tUnknown\n"
    EndIf
    String pwState = Presence("Public Whore.esp")
    PW_TrackerScript pw
    Bool pwActive = False
    If pwState == "Available"
        pw = Game.GetFormFromFile(0x16C84, "Public Whore.esp") as PW_TrackerScript
        If pw
            pwActive = pw.GetCurrentStatus() == 2
        Else
            pwState = "Unknown"
        EndIf
    EndIf
    String fhuState = Presence("sr_FillHerUp.esp")
    sr_InflateQuest fhu
    If fhuState == "Available"
        fhu = Game.GetFormFromFile(0xD63, "sr_FillHerUp.esp") as sr_InflateQuest
        If !fhu
            fhuState = "Unknown"
        EndIf
    EndIf
    String fmState = Presence("Fertility Mode 3 Fixes and Updates.esp")
    Faction fm = Game.GetFormFromFile(0x862, "Fertility Mode 3 Fixes and Updates.esp") as Faction
    If fmState == "Available" && !fm
        fmState = "Unknown"
    EndIf
    String hpState = Presence("HentaiPregnancy.esm")
    Faction hp = Game.GetFormFromFile(0x12085, "HentaiPregnancy.esm") as Faction
    If hpState == "Available" && !hp
        hpState = "Unknown"
    EndIf
    String ecState = Presence("EstrusChaurus.esp")
    Faction ec = Game.GetFormFromFile(0x160A9, "EstrusChaurus.esp") as Faction
    If ecState == "Available" && !ec
        ecState = "Unknown"
    EndIf
    String esState = Presence("EstrusSpider.esp")
    Faction es = Game.GetFormFromFile(0x4E258, "EstrusSpider.esp") as Faction
    If esState == "Available" && !es
        esState = "Unknown"
    EndIf
    String rows = ""
    Bool playerPresent = False
    Int i = 0
    While i < actors.Length
        Actor participant = actors[i]
        String identity = SLSF_Reloaded_ProbeNative.Identity(participant)
        If !participant || identity == "" || !participant.GetActorBase()
            Return prefix + "availability\tUnknown\n"
        EndIf
        playerPresent = playerPresent || participant == Game.GetPlayer()
        Bool female = participant.GetActorBase().GetSex() == 1
        Int fmRank = -2
        Int hpRank = -2
        String fmRaw = "unknown"
        String hpRaw = "unknown"
        If fmState == "Available"
            fmRank = participant.GetFactionRank(fm)
            fmRaw = fmRank as String
        EndIf
        If hpState == "Available"
            hpRank = participant.GetFactionRank(hp)
            hpRaw = hpRank as String
        EndIf
        Bool ecMember = False
        Bool esMember = False
        If ecState == "Available"
            ecMember = participant.IsInFaction(ec)
        EndIf
        If esState == "Available"
            esMember = participant.IsInFaction(es)
        EndIf
        Bool inflationKnown = fhuState == "Available"
        Float inflation = 0.0
        If inflationKnown
            inflation = fhu.GetInflation(participant)
        EndIf
        rows += "actor\t" + identity + "\t" + female + "\t" + (fmRank > 30) + "\t" + (hpRank > 1) + "\t" + ecMember + "\t" + esMember + "\t" + inflationKnown + "\t" + inflation + "\t" + fmRaw + "\t" + hpRaw + "\n"
        i += 1
    EndWhile
    String result = prefix + "availability\tAvailable\nprovider\tSLSF_Reloaded_ExternalProbe\t1\t1\nplayer\t" + playerPresent + "\npublicwhore\t" + pwActive + "\n"
    result += "mod\tPublicWhore\t" + pwState + "\t\nmod\tFillHerUp\t" + fhuState + "\t\nmod\tFertilityMode\t" + fmState + "\t\nmod\tEstrusChaurus\t" + ecState + "\t\nmod\tEstrusSpider\t" + esState + "\t\nmod\tHentaiPregnancy\t" + hpState + "\t\n"
    result += "mod\tFameComments\t" + Presence("SLSFFameComments.esp") + "\t\nmod\tBimbos\t" + Presence("CustomComments.esp") + "\t\n"
    Return result + rows
EndFunction
