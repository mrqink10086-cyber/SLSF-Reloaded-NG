ScriptName zadLibs Extends Quest
{ DDI framework conventions:
The vision of Devious Devices is providing a foundation for a mulitude of bondage-themed mods users can install and enjoy at the same time. Due to the unique characteristics of a large family of mods providing content having a strong potential to affect each other, the following conventions for the use of the DD framework API are in effect, to ensure maximum compatibility and fair play between DD mods. DD content mods are expected to respect these conventions. Linking to the DD framework and use of the DD API is considered consent to these conventions. The DD team reserves the right to take necessary steps to ensure the integrity of these framework conventions.

1. DD content mods shall not overwrite standard framework features with their own. Other mods will rely on framework resources working the way they are coded/defined in the framework. If you want to create custom devices or behavior, clone the items or features in question and change them locally, in your own mod. In particular, do not make ANY changes to the base scripts.
2. Do not re-pack/re-distribute any framework resources with your own mod. All framework assets are subject to changes and having outdated framework resources packed in DD content mods is just creating a support nightmare for everyone involved. You CAN re-use code or portions thereof, but give the scripts new names, in your own namespace.
3. The zad_ and zadx_ namespaces are reserved for the framework. Please use your own namespace for your custom assets to avoid confusion and accidental incompatibilities.
4. Standard keys and standard items can be created/dropped/crafted/sold by any content mod at their own discretion. DD mods should not rely on standard keys to be difficult to obtain etc.
5. Items not marked with either zad_BlockGeneric OR zad_QuestItem are considered generic and can be removed by any DD mod at any time for any reason, including helpful blacksmiths, mercyful NPCs etc.
6. Items marked with zad_BlockGeneric should NOT be removed by third party mods via trivial means of escape (e.g. blacksmith dialogues) or without compelling reason. They can be removed -when necessary-, to be replaced with quest items (and these only!), or temporarily, if required to run a scene etc. In the latter case, the removing mod is expected to equip back the item after completing the scene.
7. Items tagged with zad_QuestItem mark essential items that are not to be removed for ANY reason by any content mod other than the mod that equipped them and the framework itself. Modders are asked to use this tag sparingly and only when warranted, as this might prevent other mods' quests or scenes from getting started. Live and let live!
8. DD content mods are strongly encouraged to provide a local means to terminate their quests and remove their items upon user request, -particularly- if they make use of zad_QuestItem tagged items. The framework will send the DDI_Quest_SigTerm mod event if the user triggers the built-in DDI debug function. It is encouraged to register for this event and terminate/clean up all local DD quests when it's received. }

;-- Variables ---------------------------------------
GlobalVariable _GameDaysPassed

;-- Properties --------------------------------------
Keyword Property ActorTypeAnimal Auto
Keyword Property ActorTypeCreature Auto
Keyword Property ActorTypeDwarven Auto
Keyword Property ActorTypeNPC Auto
slautilscr Property Aroused Auto
Bool Property BoundAnimsAvailable = True Auto
zadboundcombatscript Property BoundCombat Auto
zadconfig Property Config Auto
Idle Property DD_FT_Bleedout_Armbinder Auto
Idle Property DD_FT_Bleedout_BBYoke Auto
Idle Property DD_FT_Bleedout_Elbowbinder Auto
Idle Property DD_FT_Bleedout_Frontcuffs Auto
Idle Property DD_FT_Bleedout_Yoke Auto
Idle Property DD_FT_CollarMe Auto
Idle Property DD_FT_Egyptian Auto
Idle Property DD_FT_Inspection Auto
Idle Property DD_FT_Surrender Auto
Idle Property DD_FT_Surrender_Hobbled Auto
Idle Property DD_FT_Unconcious Auto
Bool Property DeviceMutex Auto
zaddevicesunderneathscript Property DevicesUnderneath Auto
GlobalVariable Property DialogueArmbinderDisable Auto
GlobalVariable Property DialogueGagDisable Auto
Sound Property EdgedSound Auto
Race Property ElderRace Auto
Bool Property EnableVRSupport = False Auto
zadeventslots Property EventSlots Auto
zadexpressionlibs Property ExpLibs Auto
Sound Property GaggedSound Auto
GlobalVariable Property GameDaysPassed
  GlobalVariable Function Get()
    If !_GameDaysPassed ; #DEBUG_LINE_NO:287
      _GameDaysPassed = Game.GetForm(57) as GlobalVariable ; #DEBUG_LINE_NO:288
    EndIf
    Return _GameDaysPassed ; #DEBUG_LINE_NO:290
  EndFunction
EndProperty
Bool Property GlobalEventFlag Auto
Float Property LastInflationAdjustmentAnal = 0.0 Auto
Float Property LastInflationAdjustmentVaginal = 0.0 Auto
MiscObject Property Lockpick Auto
Perk Property LustgemCrafting Auto
Race Property ManakinRace Auto
Sound Property MoanSound Auto
Sound Property OrgasmSound Auto
Perk Property PiercedClit Auto
Spell Property PiercedClitSpell Auto
Perk Property PiercedNipples Auto
Spell Property PiercedNipplesSpell Auto
Actor Property PlayerRef Auto
Bool Property RepopulateMutex Auto
sexlabframework Property SexLab Auto
Faction Property SexLabAnimatingFaction Auto
Spell Property ShockEffect Auto
Sound Property ShortMoanSound Auto
Sound Property ShortPantSound Auto
SoulGem Property SoulgemEmpty Auto
SoulGem Property SoulgemFilled Auto
MiscObject Property SoulgemStand Auto
Float Property SpellCastVibrateCooldown Auto
Bool Property Terminate Auto
Int Property TweenMenuKey Auto
Sound Property VibrateStandardSound Auto
Sound Property VibrateStrongSound Auto
Sound Property VibrateVeryStrongSound Auto
Sound Property VibrateVeryWeakSound Auto
Sound Property VibrateWeakSound Auto
Armor Property armbinder Auto
Armor Property armbinderRendered Auto
Armor Property beltIron Auto
Armor Property beltIronRendered Auto
Armor Property beltPadded Auto
Armor Property beltPaddedOpen Auto
Armor Property beltPaddedOpenRendered Auto
Armor Property beltPaddedRendered Auto
Armor Property blindfold Auto
Armor Property blindfoldRendered Auto
Armor Property braPadded Auto
Armor Property braPaddedRendered Auto
Key Property chastityKey Auto
Armor Property collarPosture Auto
Armor Property collarPostureLeather Auto
Armor Property collarPostureLeatherRendered Auto
Armor Property collarPostureRendered Auto
Armor Property collarRestrictive Auto
Armor Property collarRestrictiveRendered Auto
Armor Property corset Auto
Armor Property corsetRendered Auto
Armor Property cuffsLeatherArms Auto
Armor Property cuffsLeatherArmsRendered Auto
Armor Property cuffsLeatherCollar Auto
Armor Property cuffsLeatherCollarRendered Auto
Armor Property cuffsLeatherLegs Auto
Armor Property cuffsLeatherLegsRendered Auto
Armor Property cuffsPaddedArms Auto
Armor Property cuffsPaddedArmsRendered Auto
Armor Property cuffsPaddedCollar Auto
Armor Property cuffsPaddedCollarRendered Auto
Armor Property cuffsPaddedComplete Auto
Armor Property cuffsPaddedCompleteRendered Auto
Armor Property cuffsPaddedLegs Auto
Armor Property cuffsPaddedLegsRendered Auto
Faction Property currentfollowerfaction Auto
Armor Property deviceRemovalToken Auto
Faction Property dunPrisonerFaction Auto
Armor Property gagBall Auto
Armor Property gagBallRendered Auto
Armor Property gagPanel Auto
Armor Property gagPanelRendered Auto
Armor Property gagRing Auto
Armor Property gagRingRendered Auto
Armor Property gagStrapBall Auto
Armor Property gagStrapBallRendered Auto
Armor Property gagStrapRing Auto
Armor Property gagStrapRingRendered Auto
Armor Property glovesRestrictive Auto
Armor Property glovesRestrictiveRendered Auto
Armor Property harnessBody Auto
Armor Property harnessBodyRendered Auto
Armor Property harnessCollar Auto
Armor Property harnessCollarRendered Auto
Keyword Property isBeastRace Auto
Faction Property isGuardFaction Auto
Float Property lastRepopulateTime Auto
Key Property piercingKey Auto
Armor Property piercingNSoul Auto
Armor Property piercingNSoulRendered Auto
Armor Property piercingVSoul Auto
Armor Property piercingVSoulRendered Auto
Armor Property plugChargeableRenderedVag Auto
Armor Property plugChargeableVag Auto
Armor Property plugInflatable Auto
Armor Property plugInflatableAn Auto
Armor Property plugInflatableAnRendered Auto
Armor Property plugInflatableRendered Auto
Armor Property plugInflatableVag Auto
Armor Property plugInflatableVagRendered Auto
Armor Property plugIron Auto
Armor Property plugIronAn Auto
Armor Property plugIronAnRendered Auto
Armor Property plugIronRendered Auto
Armor Property plugIronVag Auto
Armor Property plugIronVagRendered Auto
Armor Property plugPrimitive Auto
Armor Property plugPrimitiveAn Auto
Armor Property plugPrimitiveAnRendered Auto
Armor Property plugPrimitiveRendered Auto
Armor Property plugPrimitiveVag Auto
Armor Property plugPrimitiveVagRendered Auto
Armor Property plugSoulgem Auto
Armor Property plugSoulgemAn Auto
Armor Property plugSoulgemAnRendered Auto
Armor Property plugSoulgemRendered Auto
Armor Property plugSoulgemVag Auto
Armor Property plugSoulgemVagRendered Auto
Armor Property plugTrainingRenderedVag Auto
Armor Property plugTrainingVag Auto
Keyword Property questItemRemovalAuthorizationToken Auto
Key Property restraintsKey Auto
Quest Property vrikActionsAPI Auto
Armor Property yoke Auto
Armor Property yokeRendered Auto
Faction Property zadAnimatingFaction Auto
GlobalVariable Property zadDeviceEscapeSuccessCount Auto
FormList Property zadDeviceTypes Auto
Faction Property zadDisableDialogueFaction Auto
Outfit Property zadEmptyOutfit Auto
Faction Property zadGagPanelFaction Auto
GlobalVariable Property zadInflatablePlugStateAnal Auto
GlobalVariable Property zadInflatablePlugStateVaginal Auto
zadnpcquestscript Property zadNPCQuest Auto
Quest Property zadNPCSlots Auto
FormList Property zadStandardKeywords Auto
FormList Property zadTemporaryDeviceStorage Auto
Faction Property zadVibratorFaction Auto
FormList Property zad_AlwaysSilent Auto
Keyword Property zad_BlockGeneric Auto
Keyword Property zad_BlockGenericEvents Auto
Keyword Property zad_BoundCombatDisableKick Auto
Keyword Property zad_BraNoBlockPiercings Auto
Message Property zad_DD_EscapeCutFailureMSG Auto
Message Property zad_DD_EscapeCutMSG Auto
Message Property zad_DD_EscapeCutSuccessMSG Auto
Message Property zad_DD_EscapeDeviceMSG Auto
Message Property zad_DD_EscapeLockPickFailureMSG Auto
Message Property zad_DD_EscapeLockPickMSG Auto
Message Property zad_DD_EscapeLockPickSuccessMSG Auto
Message Property zad_DD_EscapeStruggleFailureMSG Auto
Message Property zad_DD_EscapeStruggleMSG Auto
Message Property zad_DD_EscapeStruggleSuccessMSG Auto
Message Property zad_DD_KeyBreakJamMSG Auto
Message Property zad_DD_KeyBreakMSG Auto
Message Property zad_DD_OnEquipDeviceMSG Auto
Message Property zad_DD_OnLeaveItNotWornMSG Auto
Message Property zad_DD_OnLeaveItWornMSG Auto
Message Property zad_DD_OnNoKeyMSG Auto
Message Property zad_DD_OnNotEnoughKeysMSG Auto
Message Property zad_DD_OnPutOnDevice Auto
Message Property zad_DD_RepairLockFailureMSG Auto
Message Property zad_DD_RepairLockMSG Auto
Message Property zad_DD_RepairLockNotJammedMSG Auto
Message Property zad_DD_RepairLockSuccessMSG Auto
FormList Property zad_DD_StandardCuttingToolsList Auto
Message Property zad_DD_UnlockFailJammedMSG Auto
Message Property zad_DeviceEscapeMsg Auto
Keyword Property zad_DeviousAnkleShackles Auto
Keyword Property zad_DeviousArmCuffs Auto
Keyword Property zad_DeviousArmbinder Auto
Keyword Property zad_DeviousArmbinderElbow Auto
Keyword Property zad_DeviousBelt Auto
Keyword Property zad_DeviousBlindfold Auto
Keyword Property zad_DeviousBondageMittens Auto
Keyword Property zad_DeviousBoots Auto
Keyword Property zad_DeviousBra Auto
Keyword Property zad_DeviousClamps Auto
Keyword Property zad_DeviousCollar Auto
Keyword Property zad_DeviousCorset Auto
Keyword Property zad_DeviousCuffsFront Auto
Keyword Property zad_DeviousElbowTie Auto
Keyword Property zad_DeviousGag Auto
Keyword Property zad_DeviousGagLarge Auto
Keyword Property zad_DeviousGagPanel Auto
Keyword Property zad_DeviousGloves Auto
Keyword Property zad_DeviousHarness Auto
Keyword Property zad_DeviousHeavyBondage Auto
Keyword Property zad_DeviousHobbleSkirt Auto
Keyword Property zad_DeviousHobbleSkirtRelaxed Auto
Keyword Property zad_DeviousHood Auto
Keyword Property zad_DeviousLegCuffs Auto
Keyword Property zad_DeviousPetSuit Auto
Keyword Property zad_DeviousPiercingsNipple Auto
Keyword Property zad_DeviousPiercingsVaginal Auto
Keyword Property zad_DeviousPlug Auto
Keyword Property zad_DeviousPlugAnal Auto
Keyword Property zad_DeviousPlugVaginal Auto
Keyword Property zad_DeviousPonyGear Auto
Keyword Property zad_DeviousStraitJacket Auto
Keyword Property zad_DeviousSuit Auto
Keyword Property zad_DeviousYoke Auto
Keyword Property zad_DeviousYokeBB Auto
Keyword Property zad_EffectChaosPlug Auto
Keyword Property zad_EffectCompressBelly Auto
Keyword Property zad_EffectCompressBreasts Auto
Keyword Property zad_EffectEdgeOnly Auto
Keyword Property zad_EffectEdgeRandom Auto
Keyword Property zad_EffectElectroStim Auto
Keyword Property zad_EffectForcedWalk Auto
Keyword Property zad_EffectHealthDraining Auto
Keyword Property zad_EffectLively Auto
Keyword Property zad_EffectManaDraining Auto
Keyword Property zad_EffectPossessed Auto
Keyword Property zad_EffectRemote Auto
Keyword Property zad_EffectShockOnFullArousal Auto
Keyword Property zad_EffectShocking Auto
Keyword Property zad_EffectStaminaDraining Auto
Keyword Property zad_EffectTrainOnSpellCast Auto
Keyword Property zad_EffectVeryLively Auto
Keyword Property zad_EffectVibrateOnSpellCast Auto
Keyword Property zad_EffectVibrating Auto
Keyword Property zad_EffectVibratingRandom Auto
Keyword Property zad_EffectVibratingStrong Auto
Keyword Property zad_EffectVibratingVeryStrong Auto
Keyword Property zad_EffectVibratingVeryWeak Auto
Keyword Property zad_EffectVibratingWeak Auto
Keyword Property zad_EffectsLinked Auto
Keyword Property zad_ExposedBreasts Auto
Keyword Property zad_GagCustomExpression Auto
Keyword Property zad_GagNoOpenMouth Auto
Keyword Property zad_HasPumps Auto
Keyword Property zad_InventoryDevice Auto
Keyword Property zad_Lockable Auto
Keyword Property zad_NoCompressBelly Auto
Keyword Property zad_NoCompressBreasts Auto
Keyword Property zad_NonUniqueKey Auto
Keyword Property zad_PermitAnal Auto
Keyword Property zad_PermitOral Auto
Keyword Property zad_PermitVaginal Auto
Message Property zad_PlugsConfirmMSG Auto
Message Property zad_PlugsDeflatePumpsFail Auto
Keyword Property zad_QuestItem Auto
Armor Property zad_armBinderHisec_Inventory Auto
Armor Property zad_armBinderHisec_Rendered Auto
MiscObject Property zad_gagPanelPlug Auto
Keyword Property zad_kw_InflatablePlugAnal Auto
Keyword Property zad_kw_InflatablePlugVaginal Auto
Package Property zad_pk_collarme Auto
Package Property zad_pk_egyptian Auto
Package Property zad_pk_inspection Auto
Package Property zad_pk_surrender Auto
Package Property zad_pk_surrender_hobbled Auto
Spell Property zad_splMagickaPenalty Auto

;-- Functions ---------------------------------------

; Skipped compiler generated GetState

; Skipped compiler generated GotoState

Bool Function LockDevice(Actor akActor, Armor deviceInventory, Bool force)
  Self.Log("LockDevice called for " + akActor.GetLeveledActorBase().GetName() + ": " + deviceInventory.GetName() + ")", 0) ; #DEBUG_LINE_NO:389
  If force ; #DEBUG_LINE_NO:390
    Keyword kw = Self.GetDeviceKeyword(deviceInventory) ; #DEBUG_LINE_NO:391
    If kw ; #DEBUG_LINE_NO:392
      Return Self.SwapDevices(akActor, deviceInventory, kw, False, True) ; #DEBUG_LINE_NO:393
    EndIf
  EndIf
  If akActor.GetItemCount(deviceInventory as Form) <= 0 ; #DEBUG_LINE_NO:396
    akActor.AddItem(deviceInventory as Form, 1, True) ; #DEBUG_LINE_NO:397
  EndIf
  akActor.EquipItemEx(deviceInventory as Form, 0, False, True) ; #DEBUG_LINE_NO:399
  Return True ; #DEBUG_LINE_NO:400
EndFunction

Bool Function UnlockDevice(Actor akActor, Armor deviceInventory, Armor deviceRendered, Keyword zad_DeviousDevice, Bool destroyDevice, Bool genericonly)
  Self.Log("UnlockDevice called for " + akActor.GetLeveledActorBase().GetName() + ": " + deviceInventory.GetName() + ")", 0) ; #DEBUG_LINE_NO:412
  If genericonly && deviceInventory.HasKeyWord(zad_BlockGeneric) || deviceInventory.HasKeyWord(zad_QuestItem) ; #DEBUG_LINE_NO:413
    Self.Log("UnlockDevice aborted because " + deviceInventory.GetName() + " is not a generic item.", 0) ; #DEBUG_LINE_NO:414
    Return False ; #DEBUG_LINE_NO:415
  EndIf
  If akActor.IsEquipped(deviceInventory as Form) ; #DEBUG_LINE_NO:417
    storageutil.SetIntValue(akActor as Form, "zad_RemovalToken" + deviceInventory as String, 1) ; #DEBUG_LINE_NO:418
    akActor.UnequipItemEx(deviceInventory as Form, 0, False) ; #DEBUG_LINE_NO:419
  Else
    Keyword kw
    Armor rDevice
    If !zad_DeviousDevice ; #DEBUG_LINE_NO:427
      kw = Self.GetDeviceKeyword(deviceInventory) ; #DEBUG_LINE_NO:428
    Else
      kw = zad_DeviousDevice ; #DEBUG_LINE_NO:431
    EndIf
    If !deviceRendered ; #DEBUG_LINE_NO:433
      rDevice = Self.GetWornRenderedDeviceByKeyword(akActor, kw) ; #DEBUG_LINE_NO:434
    Else
      rDevice = deviceRendered ; #DEBUG_LINE_NO:436
    EndIf
    akActor.RemoveItem(rDevice as Form, 1, True, None) ; #DEBUG_LINE_NO:438
    If kw == zad_DeviousHeavyBondage || kw == zad_DeviousPonyGear || kw == zad_DeviousHobbleSkirt ; #DEBUG_LINE_NO:440
      Self.Log("Removing Bound Effects", 0) ; #DEBUG_LINE_NO:441
      BoundCombat.EvaluateAA(akActor) ; #DEBUG_LINE_NO:442
    EndIf
    If kw == zad_DeviousGag ; #DEBUG_LINE_NO:444
      Self.Log("Removing Gag Effects", 0) ; #DEBUG_LINE_NO:445
      Self.RemoveGagEffect(akActor) ; #DEBUG_LINE_NO:446
      If rDevice.HasKeyWord(zad_DeviousGagPanel) ; #DEBUG_LINE_NO:447
        If akActor.GetFactionRank(zadGagPanelFaction) == 0 ; #DEBUG_LINE_NO:448
          akActor.RemoveItem(zad_gagPanelPlug as Form, 1, False, None) ; #DEBUG_LINE_NO:449
        EndIf
        akActor.SetFactionRank(zadGagPanelFaction, 0) ; #DEBUG_LINE_NO:451
        akActor.RemoveFromFaction(zadGagPanelFaction) ; #DEBUG_LINE_NO:452
      EndIf
    EndIf
    Self.UnsetStoredDevice(akActor, kw) ; #DEBUG_LINE_NO:455
    Self.SendDeviceRemovalEvent(Self.LookupDeviceType(zad_DeviousDevice), akActor) ; #DEBUG_LINE_NO:456
    Self.SendDeviceRemovedEventVerbose(deviceInventory, kw, akActor) ; #DEBUG_LINE_NO:457
    Self.Log("UnlockDevice: " + akActor.GetLeveledActorBase().GetName() + "'s " + deviceInventory.GetName() + " has been removed.", 0) ; #DEBUG_LINE_NO:458
  EndIf
  If destroyDevice ; #DEBUG_LINE_NO:460
    akActor.RemoveItem(deviceInventory as Form, 1, True, None) ; #DEBUG_LINE_NO:461
  EndIf
  Return True ; #DEBUG_LINE_NO:463
EndFunction

Bool Function SwapDevices(Actor akActor, Armor deviceInventory, Keyword zad_DeviousDevice, Bool destroyDevice, Bool genericonly)
  Keyword kw
  Self.Log("SwapDevices called for " + akActor.GetLeveledActorBase().GetName() + ": " + deviceInventory.GetName() + ")", 0) ; #DEBUG_LINE_NO:470
  If !zad_DeviousDevice ; #DEBUG_LINE_NO:472
    kw = Self.GetDeviceKeyword(deviceInventory) ; #DEBUG_LINE_NO:473
  Else
    kw = zad_DeviousDevice ; #DEBUG_LINE_NO:475
  EndIf
  Armor WornDevice = Self.GetWornRenderedDeviceByKeyword(akActor, kw) ; #DEBUG_LINE_NO:477
  If WornDevice ; #DEBUG_LINE_NO:478
    Armor idevice = Self.GetWornDevice(akActor, kw) ; #DEBUG_LINE_NO:479
    If !Self.UnlockDevice(akActor, idevice, WornDevice, kw, destroyDevice, genericonly) ; #DEBUG_LINE_NO:480
      Self.Log("UnlockDevice() failed. Aborting.", 0) ; #DEBUG_LINE_NO:481
      Return False ; #DEBUG_LINE_NO:482
    EndIf
    Int counter = 5 ; #DEBUG_LINE_NO:484
    While akActor.IsEquipped(WornDevice as Form) && counter > 0 ; #DEBUG_LINE_NO:485
      Self.Log("Removing " + idevice.GetName() + ". Waiting for removal operation to complete.", 0) ; #DEBUG_LINE_NO:486
      counter -= 1 ; #DEBUG_LINE_NO:487
      Utility.Wait(1.0) ; #DEBUG_LINE_NO:488
    EndWhile
    If akActor.IsEquipped(WornDevice as Form) && counter == 0 ; #DEBUG_LINE_NO:490
      Self.Log("Unlock operation timed out and failed. Aborting.", 0) ; #DEBUG_LINE_NO:491
      Return False ; #DEBUG_LINE_NO:492
    EndIf
    Utility.Wait(1.0) ; #DEBUG_LINE_NO:494
    Self.Log("Device removed. Proceeding.", 0) ; #DEBUG_LINE_NO:495
  Else
    Self.Log("No confilicting device worn. Proceeding.", 0) ; #DEBUG_LINE_NO:497
  EndIf
  If akActor.GetItemCount(deviceInventory as Form) <= 0 ; #DEBUG_LINE_NO:499
    akActor.AddItem(deviceInventory as Form, 1, True) ; #DEBUG_LINE_NO:500
  EndIf
  akActor.EquipItemEx(deviceInventory as Form, 0, False, True) ; #DEBUG_LINE_NO:502
  Return True ; #DEBUG_LINE_NO:503
EndFunction

Bool Function UnlockDeviceByKeyword(Actor akActor, Keyword zad_DeviousDevice, Bool destroyDevice)
  Self.Log("UnlockDeviceByKeyword called for " + zad_DeviousDevice as String, 0) ; #DEBUG_LINE_NO:508
  Armor idevice = Self.GetWornDevice(akActor, zad_DeviousDevice) ; #DEBUG_LINE_NO:509
  If Self.UnlockDevice(akActor, idevice, None, zad_DeviousDevice, destroyDevice, True) ; #DEBUG_LINE_NO:510
    Return True ; #DEBUG_LINE_NO:511
  EndIf
  Return False ; #DEBUG_LINE_NO:513
EndFunction

Function RemoveQuestDevice(Actor akActor, Armor deviceInventory, Armor deviceRendered, Keyword zad_DeviousDevice, Keyword RemovalToken, Bool destroyDevice, Bool skipMutex)
  Self.Log("RemoveQuestDevice called for " + deviceInventory.GetName(), 0) ; #DEBUG_LINE_NO:519
  If !akActor.IsEquipped(deviceInventory as Form) && !akActor.IsEquipped(deviceRendered as Form) ; #DEBUG_LINE_NO:520
    Self.Warn(("RemoveQuestDevice called for " + deviceInventory as String) + ", but this device is not currently worn.") ; #DEBUG_LINE_NO:521
    Return  ; #DEBUG_LINE_NO:522
  EndIf
  If !deviceInventory.HasKeyWord(zad_QuestItem) && !deviceRendered.HasKeyWord(zad_QuestItem) ; #DEBUG_LINE_NO:524
    Self.Log("RemoveQuestDevice aborted for " + deviceInventory.GetName() + " because it's not a quest item.", 0) ; #DEBUG_LINE_NO:525
    Return  ; #DEBUG_LINE_NO:526
  EndIf
  If !RemovalToken || zadStandardKeywords.HasForm(RemovalToken as Form) || !(deviceInventory.HasKeyWord(RemovalToken) || deviceRendered.HasKeyWord(RemovalToken)) ; #DEBUG_LINE_NO:528
    Self.Log("RemoveQuestDevice called for " + deviceInventory.GetName() + " with invalid removal token. Aborted.", 0) ; #DEBUG_LINE_NO:529
    Return  ; #DEBUG_LINE_NO:530
  EndIf
  questItemRemovalAuthorizationToken = RemovalToken ; #DEBUG_LINE_NO:532
  storageutil.SetIntValue(akActor as Form, "zad_RemovalToken" + deviceInventory as String, 1) ; #DEBUG_LINE_NO:533
  akActor.UnequipItemEx(deviceInventory as Form, 0, False) ; #DEBUG_LINE_NO:534
  akActor.RemoveItem(deviceRendered as Form, 1, True, None) ; #DEBUG_LINE_NO:535
  If destroyDevice ; #DEBUG_LINE_NO:536
    akActor.RemoveItem(deviceInventory as Form, 1, True, None) ; #DEBUG_LINE_NO:537
  EndIf
EndFunction

Int Function IsWearingDevice(Actor akActor, Armor deviceRendered, Keyword zad_DeviousDevice)
  If akActor == None || deviceRendered == None || zad_DeviousDevice == None ; #DEBUG_LINE_NO:544
    Self.Error("IsWearingDevice received none argument.") ; #DEBUG_LINE_NO:545
    Return -1 ; #DEBUG_LINE_NO:546
  EndIf
  If akActor.WornHasKeyword(zad_DeviousDevice) ; #DEBUG_LINE_NO:548
    If akActor.GetItemCount(deviceRendered as Form) == 0 ; #DEBUG_LINE_NO:549
      Return 2 ; #DEBUG_LINE_NO:550
    EndIf
    Return 1 ; #DEBUG_LINE_NO:552
  EndIf
  Return 0 ; #DEBUG_LINE_NO:554
EndFunction

Bool Function TightenDevice(Actor akActor, Armor deviceInventory)
  If deviceInventory.HasKeyWord(zad_QuestItem) ; #DEBUG_LINE_NO:561
    Self.Log("TightenDevice aborted because " + deviceInventory.GetName() + " is a quest item.", 0) ; #DEBUG_LINE_NO:562
    Return False ; #DEBUG_LINE_NO:563
  EndIf
  If akActor == PlayerRef && akActor.IsEquipped(deviceInventory as Form) ; #DEBUG_LINE_NO:565
    storageutil.SetIntValue(akActor as Form, "zad_RemovalToken" + deviceInventory as String, 1) ; #DEBUG_LINE_NO:566
    storageutil.SetIntValue(akActor as Form, "zad_TightenToken" + deviceInventory as String, 1) ; #DEBUG_LINE_NO:567
    akActor.UnequipItemEx(deviceInventory as Form, 0, False) ; #DEBUG_LINE_NO:568
    Return True ; #DEBUG_LINE_NO:569
  EndIf
  Return False ; #DEBUG_LINE_NO:571
EndFunction

Bool Function UntightenDevice(Actor akActor, Armor deviceInventory)
  If deviceInventory.HasKeyWord(zad_QuestItem) ; #DEBUG_LINE_NO:578
    Self.Log("UntightenDevice aborted because " + deviceInventory.GetName() + " is a quest item.", 0) ; #DEBUG_LINE_NO:579
    Return False ; #DEBUG_LINE_NO:580
  EndIf
  If akActor == PlayerRef && akActor.IsEquipped(deviceInventory as Form) ; #DEBUG_LINE_NO:582
    storageutil.SetIntValue(akActor as Form, "zad_RemovalToken" + deviceInventory as String, 1) ; #DEBUG_LINE_NO:583
    storageutil.SetIntValue(akActor as Form, "zad_UntightenToken" + deviceInventory as String, 1) ; #DEBUG_LINE_NO:584
    akActor.UnequipItemEx(deviceInventory as Form, 0, False) ; #DEBUG_LINE_NO:585
    Return True ; #DEBUG_LINE_NO:586
  EndIf
  Return False ; #DEBUG_LINE_NO:588
EndFunction

Bool Function CanTightenDevice(Actor akActor, Armor akDeviceInventory)
  Return zadnativefunctions.GetPropertyForm(akDeviceInventory, "LinkedDeviceEquipOnTighten", None, 0) as Bool ; #DEBUG_LINE_NO:593
EndFunction

Bool Function CanUntightenDevice(Actor akActor, Armor akDeviceInventory)
  Return zadnativefunctions.GetPropertyForm(akDeviceInventory, "LinkedDeviceEquipOnUntighten", None, 0) as Bool ; #DEBUG_LINE_NO:598
EndFunction

Int Function IsLockJammed(Actor akActor, Keyword zad_DeviousDevice)
  If akActor != PlayerRef || !akActor.WornHasKeyword(zad_DeviousDevice) ; #DEBUG_LINE_NO:603
    Return -1 ; #DEBUG_LINE_NO:604
  EndIf
  If storageutil.GetIntValue(akActor as Form, "zad_Equipped" + Self.LookupDeviceType(zad_DeviousDevice) + "_LockJammedStatus", 0) == 1 ; #DEBUG_LINE_NO:606
    Return 1 ; #DEBUG_LINE_NO:607
  EndIf
  Return 0 ; #DEBUG_LINE_NO:609
EndFunction

Bool Function JamLock(Actor akActor, Keyword zad_DeviousDevice)
  If akActor != PlayerRef || !akActor.WornHasKeyword(zad_DeviousDevice) ; #DEBUG_LINE_NO:614
    Return False ; #DEBUG_LINE_NO:615
  EndIf
  storageutil.SetIntValue(akActor as Form, "zad_Equipped" + Self.LookupDeviceType(zad_DeviousDevice) + "_LockJammedStatus", 1) ; #DEBUG_LINE_NO:617
  Return True ; #DEBUG_LINE_NO:618
EndFunction

Bool Function UnJamLock(Actor akActor, Keyword zad_DeviousDevice)
  If akActor != PlayerRef || !akActor.WornHasKeyword(zad_DeviousDevice) ; #DEBUG_LINE_NO:622
    Return False ; #DEBUG_LINE_NO:623
  EndIf
  storageutil.SetIntValue(akActor as Form, "zad_Equipped" + Self.LookupDeviceType(zad_DeviousDevice) + "_LockJammedStatus", 0) ; #DEBUG_LINE_NO:625
  Return True ; #DEBUG_LINE_NO:626
EndFunction

Int Function GetSlotMaskForDeviceType(Keyword kw)
  If kw == zad_DeviousArmCuffs ; #DEBUG_LINE_NO:631
    Return Armor.GetMaskForSlot(59) ; #DEBUG_LINE_NO:632
  ElseIf kw == zad_DeviousGag ; #DEBUG_LINE_NO:633
    Return Armor.GetMaskForSlot(44) ; #DEBUG_LINE_NO:634
  ElseIf kw == zad_DeviousHarness ; #DEBUG_LINE_NO:635
    Return Armor.GetMaskForSlot(58) ; #DEBUG_LINE_NO:636
  ElseIf kw == zad_DeviousCorset ; #DEBUG_LINE_NO:637
    Return Armor.GetMaskForSlot(58) ; #DEBUG_LINE_NO:638
  ElseIf kw == zad_DeviousCollar ; #DEBUG_LINE_NO:639
    Return Armor.GetMaskForSlot(45) ; #DEBUG_LINE_NO:640
  ElseIf kw == zad_DeviousHeavyBondage ; #DEBUG_LINE_NO:641
    If PlayerRef.WornHasKeyword(zad_DeviousStraitJacket) ; #DEBUG_LINE_NO:642
      Return Armor.GetMaskForSlot(32) ; #DEBUG_LINE_NO:643
    Else
      Return Armor.GetMaskForSlot(46) ; #DEBUG_LINE_NO:645
    EndIf
  ElseIf kw == zad_DeviousPlugAnal ; #DEBUG_LINE_NO:647
    Return Armor.GetMaskForSlot(48) ; #DEBUG_LINE_NO:648
  ElseIf kw == zad_DeviousBelt ; #DEBUG_LINE_NO:649
    Return Armor.GetMaskForSlot(49) ; #DEBUG_LINE_NO:650
  ElseIf kw == zad_DeviousPiercingsVaginal ; #DEBUG_LINE_NO:651
    Return Armor.GetMaskForSlot(50) ; #DEBUG_LINE_NO:652
  ElseIf kw == zad_DeviousPiercingsNipple ; #DEBUG_LINE_NO:653
    Return Armor.GetMaskForSlot(51) ; #DEBUG_LINE_NO:654
  ElseIf kw == zad_DeviousLegCuffs ; #DEBUG_LINE_NO:655
    Return Armor.GetMaskForSlot(53) ; #DEBUG_LINE_NO:656
  ElseIf kw == zad_DeviousBlindfold ; #DEBUG_LINE_NO:657
    Return Armor.GetMaskForSlot(55) ; #DEBUG_LINE_NO:658
  ElseIf kw == zad_DeviousBra ; #DEBUG_LINE_NO:659
    Return Armor.GetMaskForSlot(56) ; #DEBUG_LINE_NO:660
  ElseIf kw == zad_DeviousPlugVaginal ; #DEBUG_LINE_NO:661
    Return Armor.GetMaskForSlot(57) ; #DEBUG_LINE_NO:662
  ElseIf kw == zad_DeviousSuit ; #DEBUG_LINE_NO:663
    Return Armor.GetMaskForSlot(32) ; #DEBUG_LINE_NO:664
  ElseIf kw == zad_DeviousGloves ; #DEBUG_LINE_NO:665
    Return Armor.GetMaskForSlot(33) ; #DEBUG_LINE_NO:666
  ElseIf kw == zad_DeviousHood ; #DEBUG_LINE_NO:667
    Return Armor.GetMaskForSlot(30) ; #DEBUG_LINE_NO:668
  ElseIf kw == zad_DeviousBoots ; #DEBUG_LINE_NO:669
    Return Armor.GetMaskForSlot(37) ; #DEBUG_LINE_NO:670
  EndIf
  Return -1 ; #DEBUG_LINE_NO:672
EndFunction

Armor Function GetWornRenderedDeviceByKeyword(Actor a, Keyword kw)
  Int i = Self.GetSlotMaskForDeviceType(kw) ; #DEBUG_LINE_NO:677
  If i == -1 ; #DEBUG_LINE_NO:678
    Return None ; #DEBUG_LINE_NO:679
  EndIf
  Armor x = a.GetWornForm(i) as Armor ; #DEBUG_LINE_NO:681
  If x as Bool && (x.HasKeyWord(zad_Lockable) || x.HasKeyWord(zad_DeviousPlug)) ; #DEBUG_LINE_NO:682
    Return x ; #DEBUG_LINE_NO:683
  EndIf
  Return None ; #DEBUG_LINE_NO:685
EndFunction

Bool Function IsGenericDevice(Actor a, Keyword kw)
  Armor arm = Self.GetWornRenderedDeviceByKeyword(a, kw) ; #DEBUG_LINE_NO:690
  If (arm as Bool && arm.HasKeyWord(zad_BlockGeneric)) || arm.HasKeyWord(zad_QuestItem) ; #DEBUG_LINE_NO:691
    Return False ; #DEBUG_LINE_NO:692
  EndIf
  Return True ; #DEBUG_LINE_NO:694
EndFunction

Bool Function IsLockManipulated(Actor a, Keyword kw)
  If a != PlayerRef || !a.WornHasKeyword(kw) ; #DEBUG_LINE_NO:699
    Return False ; #DEBUG_LINE_NO:700
  EndIf
  If storageutil.GetIntValue(a as Form, "zad_Equipped" + Self.LookupDeviceType(kw) + "_ManipulatedStatus", 0) == 1 ; #DEBUG_LINE_NO:702
    Return True ; #DEBUG_LINE_NO:703
  EndIf
  Return False ; #DEBUG_LINE_NO:705
EndFunction

Bool Function SetLockManipulated(Actor a, Keyword kw)
  If a != PlayerRef || !a.WornHasKeyword(kw) ; #DEBUG_LINE_NO:710
    Return False ; #DEBUG_LINE_NO:711
  EndIf
  If storageutil.SetIntValue(a as Form, "zad_Equipped" + Self.LookupDeviceType(kw) + "_ManipulatedStatus", 1) ; #DEBUG_LINE_NO:713
    Return True ; #DEBUG_LINE_NO:714
  EndIf
  Return False ; #DEBUG_LINE_NO:716
EndFunction

Bool Function SetLockUnManipulated(Actor a, Keyword kw)
  If a != PlayerRef || !a.WornHasKeyword(kw) ; #DEBUG_LINE_NO:721
    Return False ; #DEBUG_LINE_NO:722
  EndIf
  If storageutil.SetIntValue(a as Form, "zad_Equipped" + Self.LookupDeviceType(kw) + "_ManipulatedStatus", 0) ; #DEBUG_LINE_NO:724
    Return True ; #DEBUG_LINE_NO:725
  EndIf
  Return False ; #DEBUG_LINE_NO:727
EndFunction

sslBaseAnimation[] Function SelectValidDDAnimations(Actor[] actors, Int count, Bool forceaggressive, String includetag, String suppresstag)
  Return Config.beltedAnims.FilterQuest.SelectValidDDAnimations(actors, count, forceaggressive, includetag, suppresstag) ; #DEBUG_LINE_NO:733
EndFunction

Bool Function StartValidDDAnimation(Actor[] SexActors, Bool forceaggressive, String includetag, String suppresstag, Actor victim, Bool allowbed, String hook, Bool nofallbacks)
  Return Config.beltedAnims.FilterQuest.StartValidDDAnimation(SexActors, forceaggressive, includetag, suppresstag, victim, allowbed, hook, nofallbacks) ; #DEBUG_LINE_NO:741
EndFunction

Armor Function GetWornDevice(Actor akActor, Keyword kw)
  Return zadnativefunctions.GetWornDevice(akActor, kw, False) ; #DEBUG_LINE_NO:751
EndFunction

String Function GetDeviceName(Armor device)
  Return zadnativefunctions.GetPropertyString(device, "deviceName", "", 0) ; #DEBUG_LINE_NO:756
EndFunction

Armor Function GetWornDeviceFuzzyMatch(Actor akActor, Keyword kw)
  Return zadnativefunctions.GetWornDevice(akActor, kw, True) ; #DEBUG_LINE_NO:762
EndFunction

Key Function GetDeviceKey(Armor device)
  Return zadnativefunctions.GetPropertyForm(device, "deviceKey", None, 0) as Key ; #DEBUG_LINE_NO:767
EndFunction

Keyword Function GetDeviceKeyword(Armor device)
  Return zadnativefunctions.GetPropertyForm(device, "zad_DeviousDevice", None, 0) as Keyword ; #DEBUG_LINE_NO:772
EndFunction

Armor Function GetRenderedDevice(Armor device)
  Return zadnativefunctions.GetRenderDevice(device) ; #DEBUG_LINE_NO:779
EndFunction

Bool Function isWearingDeviceType(Actor akActor, Keyword kw)
  Return akActor.WornHasKeyword(kw) ; #DEBUG_LINE_NO:784
EndFunction

Function PlayHornyAnimation(Actor akActor)
  If akActor == PlayerRef && PlayerRef.IsInCombat() ; #DEBUG_LINE_NO:789
    Return  ; #DEBUG_LINE_NO:790
  EndIf
  If EnableVRSupport ; #DEBUG_LINE_NO:793
    Self.PlayThirdPersonAnimation(akActor, "DDZazHornyA", 3, True) ; #DEBUG_LINE_NO:794
  Else
    Self.PlayThirdPersonAnimation(akActor, "DDZazHornyA", 19, True) ; #DEBUG_LINE_NO:796
  EndIf
EndFunction

Function Orgasm(Actor akActor)
  Int sID = OrgasmSound.Play(akActor as ObjectReference) ; #DEBUG_LINE_NO:803
  Sound.SetInstanceVolume(sID, Config.VolumeOrgasm) ; #DEBUG_LINE_NO:804
  Aroused.SetActorExposure(akActor, 10) ; #DEBUG_LINE_NO:805
  Aroused.UpdateActorOrgasmDate(akActor) ; #DEBUG_LINE_NO:806
  If !Self.IsAnimating(akActor) ; #DEBUG_LINE_NO:807
    Bool[] cameraState = Self.StartThirdPersonAnimation(akActor, Self.AnimSwitchKeyword(akActor, "Orgasm"), True) ; #DEBUG_LINE_NO:808
    If EnableVRSupport ; #DEBUG_LINE_NO:809
      Utility.Wait(4.0) ; #DEBUG_LINE_NO:810
    Else
      Utility.Wait(20.0) ; #DEBUG_LINE_NO:812
    EndIf
    Self.EndThirdPersonAnimation(akActor, cameraState, True) ; #DEBUG_LINE_NO:814
  EndIf
EndFunction

Function SendInflationEvent(Actor who, Bool PlugisVaginal, Bool WasInflated, Int PlugState)
  Int Handle = modevent.Create("zad_Inflated_Plug") ; #DEBUG_LINE_NO:821
  If Handle ; #DEBUG_LINE_NO:822
    modevent.PushForm(Handle, who as Form) ; #DEBUG_LINE_NO:823
    modevent.PushBool(Handle, PlugisVaginal) ; #DEBUG_LINE_NO:824
    modevent.PushBool(Handle, WasInflated) ; #DEBUG_LINE_NO:825
    modevent.PushInt(Handle, PlugState) ; #DEBUG_LINE_NO:826
    modevent.Send(Handle) ; #DEBUG_LINE_NO:827
  EndIf
EndFunction

Function InflateAnalPlug(Actor akActor, Int amount)
  If !akActor.WornHasKeyword(zad_kw_InflatablePlugAnal) ; #DEBUG_LINE_NO:832
    Return  ; #DEBUG_LINE_NO:834
  EndIf
  Int currentVal = -1 ; #DEBUG_LINE_NO:836
  If akActor == PlayerRef ; #DEBUG_LINE_NO:837
    currentVal = zadInflatablePlugStateAnal.GetValueInt() ; #DEBUG_LINE_NO:838
    If currentVal < 5 ; #DEBUG_LINE_NO:840
      currentVal += amount ; #DEBUG_LINE_NO:841
      If currentVal > 5 ; #DEBUG_LINE_NO:842
        currentVal = 5 ; #DEBUG_LINE_NO:843
      EndIf
      Self.Log("Setting anal plug inflation to " + currentVal as String, 0) ; #DEBUG_LINE_NO:845
      zadInflatablePlugStateAnal.SetValueInt(currentVal) ; #DEBUG_LINE_NO:846
    EndIf
    LastInflationAdjustmentAnal = Self.GameDaysPassed.GetValue() ; #DEBUG_LINE_NO:848
  EndIf
  Self.SendInflationEvent(akActor, False, True, currentVal) ; #DEBUG_LINE_NO:850
  Aroused.UpdateActorExposure(akActor, 15, "") ; #DEBUG_LINE_NO:851
  Utility.Wait(0.100000001) ; #DEBUG_LINE_NO:853
  If Self.IsAnimating(akActor) ; #DEBUG_LINE_NO:855
    Return  ; #DEBUG_LINE_NO:856
  EndIf
  If akActor == PlayerRef && PlayerRef.IsInCombat() ; #DEBUG_LINE_NO:859
    Return  ; #DEBUG_LINE_NO:860
  EndIf
  If Aroused.GetActorExposure(akActor) < 90 || zadInflatablePlugStateAnal.GetValueInt() < 3 ; #DEBUG_LINE_NO:862
    Self.notify("Your plug makes you more horny...", False) ; #DEBUG_LINE_NO:863
    Self.SexlabMoan(akActor, -1, None) ; #DEBUG_LINE_NO:864
    Self.PlayHornyAnimation(akActor) ; #DEBUG_LINE_NO:865
  Else
    Self.PlayHornyAnimation(akActor) ; #DEBUG_LINE_NO:867
    Self.notify("Your plug sends you over the edge...", False) ; #DEBUG_LINE_NO:868
    Self.Orgasm(akActor) ; #DEBUG_LINE_NO:869
  EndIf
EndFunction

Function InflateVaginalPlug(Actor akActor, Int amount)
  If !akActor.WornHasKeyword(zad_kw_InflatablePlugVaginal) ; #DEBUG_LINE_NO:874
    Return  ; #DEBUG_LINE_NO:876
  EndIf
  Int currentVal = -1 ; #DEBUG_LINE_NO:878
  If akActor == PlayerRef ; #DEBUG_LINE_NO:879
    currentVal = zadInflatablePlugStateVaginal.GetValueInt() ; #DEBUG_LINE_NO:880
    If currentVal < 5 ; #DEBUG_LINE_NO:882
      currentVal += amount ; #DEBUG_LINE_NO:883
      If currentVal > 5 ; #DEBUG_LINE_NO:884
        currentVal = 5 ; #DEBUG_LINE_NO:885
      EndIf
      Self.Log("Setting vaginal plug inflation to " + currentVal as String, 0) ; #DEBUG_LINE_NO:887
      zadInflatablePlugStateVaginal.SetValueInt(currentVal) ; #DEBUG_LINE_NO:888
    EndIf
    LastInflationAdjustmentVaginal = Self.GameDaysPassed.GetValue() ; #DEBUG_LINE_NO:890
  EndIf
  Self.SendInflationEvent(akActor, True, True, currentVal) ; #DEBUG_LINE_NO:892
  Aroused.UpdateActorExposure(akActor, 15, "") ; #DEBUG_LINE_NO:893
  Utility.Wait(0.100000001) ; #DEBUG_LINE_NO:895
  If Self.IsAnimating(akActor) ; #DEBUG_LINE_NO:897
    Return  ; #DEBUG_LINE_NO:898
  EndIf
  If akActor == PlayerRef && PlayerRef.IsInCombat() ; #DEBUG_LINE_NO:901
    Return  ; #DEBUG_LINE_NO:902
  EndIf
  If Aroused.GetActorExposure(akActor) < 90 || zadInflatablePlugStateVaginal.GetValueInt() < 3 ; #DEBUG_LINE_NO:904
    Self.notify("Your plug makes you more horny...", False) ; #DEBUG_LINE_NO:905
    Self.SexlabMoan(akActor, -1, None) ; #DEBUG_LINE_NO:906
    Self.PlayHornyAnimation(akActor) ; #DEBUG_LINE_NO:907
  Else
    Self.PlayHornyAnimation(akActor) ; #DEBUG_LINE_NO:909
    Self.notify("Your plug sends you over the edge...", False) ; #DEBUG_LINE_NO:910
    Self.Orgasm(akActor) ; #DEBUG_LINE_NO:911
  EndIf
EndFunction

Function InflateRandomPlug(Actor akActor, Int amount)
  If akActor.WornHasKeyword(zad_kw_InflatablePlugVaginal) && akActor.WornHasKeyword(zad_kw_InflatablePlugAnal) ; #DEBUG_LINE_NO:917
    If Utility.RandomInt(1, 2) == 1 ; #DEBUG_LINE_NO:918
      Self.InflateVaginalPlug(akActor, amount) ; #DEBUG_LINE_NO:919
    Else
      Self.InflateAnalPlug(akActor, amount) ; #DEBUG_LINE_NO:921
    EndIf
    Return  ; #DEBUG_LINE_NO:923
  EndIf
  If akActor.WornHasKeyword(zad_kw_InflatablePlugVaginal) ; #DEBUG_LINE_NO:925
    Self.InflateVaginalPlug(akActor, amount) ; #DEBUG_LINE_NO:926
    Return  ; #DEBUG_LINE_NO:927
  EndIf
  If akActor.WornHasKeyword(zad_kw_InflatablePlugAnal) ; #DEBUG_LINE_NO:929
    Self.InflateAnalPlug(akActor, amount) ; #DEBUG_LINE_NO:930
    Return  ; #DEBUG_LINE_NO:931
  EndIf
EndFunction

Function DeflateVaginalPlug(Actor akActor, Int amount)
  If !akActor.WornHasKeyword(zad_kw_InflatablePlugVaginal) ; #DEBUG_LINE_NO:936
    Return  ; #DEBUG_LINE_NO:938
  EndIf
  Int currentVal = -1 ; #DEBUG_LINE_NO:940
  If akActor == PlayerRef ; #DEBUG_LINE_NO:941
    currentVal = zadInflatablePlugStateVaginal.GetValueInt() ; #DEBUG_LINE_NO:942
    If currentVal > 0 ; #DEBUG_LINE_NO:943
      currentVal -= amount ; #DEBUG_LINE_NO:944
      If currentVal < 0 ; #DEBUG_LINE_NO:945
        currentVal = 0 ; #DEBUG_LINE_NO:946
      EndIf
      Self.Log("Setting vaginal plug inflation to " + currentVal as String, 0) ; #DEBUG_LINE_NO:948
      zadInflatablePlugStateVaginal.SetValueInt(currentVal) ; #DEBUG_LINE_NO:949
    EndIf
    LastInflationAdjustmentVaginal = Self.GameDaysPassed.GetValue() ; #DEBUG_LINE_NO:951
  EndIf
  Self.SendInflationEvent(akActor, True, False, currentVal) ; #DEBUG_LINE_NO:953
EndFunction

Function DeflateAnalPlug(Actor akActor, Int amount)
  If !akActor.WornHasKeyword(zad_kw_InflatablePlugAnal) ; #DEBUG_LINE_NO:957
    Return  ; #DEBUG_LINE_NO:959
  EndIf
  Int currentVal = -1 ; #DEBUG_LINE_NO:961
  If akActor == PlayerRef ; #DEBUG_LINE_NO:962
    currentVal = zadInflatablePlugStateAnal.GetValueInt() ; #DEBUG_LINE_NO:963
    If currentVal > 0 ; #DEBUG_LINE_NO:964
      currentVal -= amount ; #DEBUG_LINE_NO:965
      If currentVal < 0 ; #DEBUG_LINE_NO:966
        currentVal = 0 ; #DEBUG_LINE_NO:967
      EndIf
      Self.Log("Setting anal plug inflation to " + currentVal as String, 0) ; #DEBUG_LINE_NO:969
      zadInflatablePlugStateAnal.SetValueInt(currentVal) ; #DEBUG_LINE_NO:970
    EndIf
    LastInflationAdjustmentAnal = Self.GameDaysPassed.GetValue() ; #DEBUG_LINE_NO:972
  EndIf
  Self.SendInflationEvent(akActor, False, False, currentVal) ; #DEBUG_LINE_NO:974
EndFunction

Bool Function ValidForInteraction(Actor currenttest, Int genderreq, Bool creatureok, Bool animalok, Bool beastreaceok, Bool elderok, Bool guardok)
  Self.Log("Checking actor validity for interaction", 0) ; #DEBUG_LINE_NO:984
  If currenttest.HasKeyWord(ActorTypeNPC) ; #DEBUG_LINE_NO:985
    Self.Log("Processing: " + currenttest.GetLeveledActorBase().GetName() + ". Is NPC.", 0) ; #DEBUG_LINE_NO:986
  ElseIf currenttest.HasKeyWord(ActorTypeCreature) && !currenttest.HasKeyWord(ActorTypeAnimal) ; #DEBUG_LINE_NO:987
    Self.Log("Processing: " + currenttest.GetLeveledActorBase().GetName() + ". Is Creature.", 0) ; #DEBUG_LINE_NO:988
  ElseIf currenttest.HasKeyWord(ActorTypeAnimal) ; #DEBUG_LINE_NO:989
    Self.Log("Processing: " + currenttest.GetLeveledActorBase().GetName() + ". Is Animal.", 0) ; #DEBUG_LINE_NO:990
  Else
    Self.Log("Processing: " + currenttest.GetLeveledActorBase().GetName() + ". WARNING: Type cannot be determined. Rejected!", 0) ; #DEBUG_LINE_NO:992
    Return False ; #DEBUG_LINE_NO:993
  EndIf
  If Self.IsAnimating(currenttest) || currenttest.GetCurrentScene() != None ; #DEBUG_LINE_NO:995
    Self.Log("Rejected: " + currenttest.GetLeveledActorBase().GetName() + ". Reason: Actor is already having sex or is involved in a scene.", 0) ; #DEBUG_LINE_NO:996
    Return False ; #DEBUG_LINE_NO:997
  EndIf
  If currenttest == PlayerRef || currenttest.IsDisabled() || currenttest.IsDead() || currenttest.isChild() || currenttest.IsGhost() || currenttest.GetActorBase().GetRace() == ManakinRace || currenttest.IsInFaction(currentfollowerfaction) || currenttest.IsPlayerTeammate() || currenttest.HasKeyWord(ActorTypeDwarven) || currenttest.IsInFaction(dunPrisonerFaction) ; #DEBUG_LINE_NO:999
    Self.Log("Rejected: " + currenttest.GetLeveledActorBase().GetName() + ". Reason: Actor is player character, follower, disabled, child, dwarven construct, a prisoner, or dead.", 0) ; #DEBUG_LINE_NO:1000
    Return False ; #DEBUG_LINE_NO:1001
  EndIf
  If genderreq > 0 ; #DEBUG_LINE_NO:1004
    If currenttest.HasKeyWord(ActorTypeNPC) && currenttest.GetLeveledActorBase().GetSex() != genderreq ; #DEBUG_LINE_NO:1005
      Self.Log("Rejected: " + currenttest.GetLeveledActorBase().GetName() + ". Reason: Actor is wrong gender.", 0) ; #DEBUG_LINE_NO:1006
      Return False ; #DEBUG_LINE_NO:1007
    EndIf
  EndIf
  If currenttest.HasKeyWord(ActorTypeNPC) && (!elderok && currenttest.GetLeveledActorBase().GetRace() == ElderRace || !beastreaceok && currenttest.HasKeyWord(isBeastRace) || !guardok && currenttest.IsInFaction(isGuardFaction)) ; #DEBUG_LINE_NO:1011
    Self.Log("Rejected: " + currenttest.GetLeveledActorBase().GetName() + ". Reason: Actor is disallowed guard/elder/beast race (MCM settings).", 0) ; #DEBUG_LINE_NO:1012
    Return False ; #DEBUG_LINE_NO:1013
  EndIf
  If currenttest.HasKeyWord(ActorTypeCreature) && !creatureok ; #DEBUG_LINE_NO:1016
    Self.Log("Rejected: " + currenttest.GetLeveledActorBase().GetName() + ". Reason: Actor is humanoid creature.", 0) ; #DEBUG_LINE_NO:1017
    Return False ; #DEBUG_LINE_NO:1018
  EndIf
  If currenttest.HasKeyWord(ActorTypeAnimal) && !animalok ; #DEBUG_LINE_NO:1021
    Self.Log("Rejected: " + currenttest.GetLeveledActorBase().GetName() + ". Reason: Actor is animal.", 0) ; #DEBUG_LINE_NO:1022
    Return False ; #DEBUG_LINE_NO:1023
  EndIf
  If currenttest.GetWorldSpace() != PlayerRef.GetWorldSpace() ; #DEBUG_LINE_NO:1025
    Self.Log("Rejected: " + currenttest.GetLeveledActorBase().GetName() + ". Reason: Actor is in different world space than player.", 0) ; #DEBUG_LINE_NO:1026
    Return False ; #DEBUG_LINE_NO:1027
  EndIf
  Return True ; #DEBUG_LINE_NO:1029
EndFunction

Int Function ArousalThreshold(String index)
  If index == "Content" ; #DEBUG_LINE_NO:1035
    Return 0 ; #DEBUG_LINE_NO:1036
  ElseIf index == "Desire" ; #DEBUG_LINE_NO:1037
    Return 25 ; #DEBUG_LINE_NO:1038
  ElseIf index == "Horny" ; #DEBUG_LINE_NO:1039
    Return 50 ; #DEBUG_LINE_NO:1040
  ElseIf index == "Desperate" ; #DEBUG_LINE_NO:1041
    Return 75 ; #DEBUG_LINE_NO:1042
  Else
    Self.Error("Error: ArousalThreshold passed invalid lookup string (" + index + ")!") ; #DEBUG_LINE_NO:1044
    Return -1 ; #DEBUG_LINE_NO:1045
  EndIf
EndFunction

Function EnableEventProcessing()
  Self.Log("EnableEventProcessing()", 0) ; #DEBUG_LINE_NO:1051
  GlobalEventFlag = True ; #DEBUG_LINE_NO:1052
EndFunction

Function DisableEventProcessing()
  Self.Log("DisableEventProcessing()", 0) ; #DEBUG_LINE_NO:1057
  GlobalEventFlag = False ; #DEBUG_LINE_NO:1058
EndFunction

Bool Function AllowGenericEvents(Actor a, Keyword kw)
  If !a.WornHasKeyword(kw) ; #DEBUG_LINE_NO:1062
    Return False ; #DEBUG_LINE_NO:1063
  EndIf
  Armor arm = Self.GetWornRenderedDeviceByKeyword(a, kw) ; #DEBUG_LINE_NO:1065
  If arm as Bool && arm.HasKeyWord(zad_BlockGenericEvents) ; #DEBUG_LINE_NO:1066
    Return False ; #DEBUG_LINE_NO:1067
  EndIf
  Return True ; #DEBUG_LINE_NO:1069
EndFunction

Bool Function HasBreastsExposed(Actor a)
  Form arm = a.GetWornForm(4) ; #DEBUG_LINE_NO:1073
  If !arm || a.WornHasKeyword(zad_ExposedBreasts) ; #DEBUG_LINE_NO:1074
    Return True ; #DEBUG_LINE_NO:1075
  EndIf
  Return False ; #DEBUG_LINE_NO:1077
EndFunction

Bool[] Function StartThirdPersonAnimation(Actor akActor, String animation, Bool permitRestrictive)
  Self.Log("StartThirdPersonAnimation(" + akActor.GetLeveledActorBase().GetName() + "," + animation + ")", 0) ; #DEBUG_LINE_NO:1084
  Bool[] ret = new Bool[2] ; #DEBUG_LINE_NO:1085
  If Self.IsAnimating(akActor) ; #DEBUG_LINE_NO:1086
    Self.Log("Actor already in animating faction.", 0) ; #DEBUG_LINE_NO:1087
    Return ret ; #DEBUG_LINE_NO:1088
  EndIf
  If !Self.IsValidActor(akActor) || akActor.WornHasKeyword(zad_DeviousArmbinder) && !permitRestrictive ; #DEBUG_LINE_NO:1090
    Self.Log("Actor is not loaded (Or is otherwise invalid). Aborting.", 0) ; #DEBUG_LINE_NO:1091
    Return ret ; #DEBUG_LINE_NO:1092
  EndIf
  If akActor.IsWeaponDrawn() ; #DEBUG_LINE_NO:1094
    akActor.SheatheWeapon() ; #DEBUG_LINE_NO:1095
    Int timeout = 0 ; #DEBUG_LINE_NO:1097
    While akActor.IsWeaponDrawn() && timeout <= 35 ; #DEBUG_LINE_NO:1098
      Utility.Wait(0.100000001) ; #DEBUG_LINE_NO:1099
      timeout += 1 ; #DEBUG_LINE_NO:1100
    EndWhile
    ret[1] = True ; #DEBUG_LINE_NO:1102
  EndIf
  If akActor.IsWeaponDrawn() ; #DEBUG_LINE_NO:1106
    Return ret ; #DEBUG_LINE_NO:1107
  EndIf
  Self.SendDDFunctionEvent("StartThirdPersonAnimation", akActor) ; #DEBUG_LINE_NO:1109
  If akActor == PlayerRef ; #DEBUG_LINE_NO:1111
    Int cameraOld = Game.GetCameraState() ; #DEBUG_LINE_NO:1113
    If cameraOld == 10 || akActor.IsOnMount() ; #DEBUG_LINE_NO:1114
      akActor.Dismount() ; #DEBUG_LINE_NO:1115
      Game.ForceThirdPerson() ; #DEBUG_LINE_NO:1116
      Int timeout = 0 ; #DEBUG_LINE_NO:1117
      While akActor.IsOnMount() && timeout <= 30 ; #DEBUG_LINE_NO:1118
        Utility.Wait(0.100000001) ; #DEBUG_LINE_NO:1119
        timeout += 1 ; #DEBUG_LINE_NO:1120
      EndWhile
    ElseIf cameraOld == 11 ; #DEBUG_LINE_NO:1122
      Self.Warn("Actor is bleeding out. Hmm.") ; #DEBUG_LINE_NO:1123
      Return ret ; #DEBUG_LINE_NO:1124
    ElseIf cameraOld == 12 ; #DEBUG_LINE_NO:1125
      Self.Warn("Actor is dragon? Not sure what happened here.") ; #DEBUG_LINE_NO:1126
      Return ret ; #DEBUG_LINE_NO:1127
    ElseIf cameraOld == 8 || cameraOld == 9 || cameraOld == 7 ; #DEBUG_LINE_NO:1128
      
    Else
      ret[0] = True ; #DEBUG_LINE_NO:1131
      Game.ForceThirdPerson() ; #DEBUG_LINE_NO:1132
    EndIf
    Self.DisableControls() ; #DEBUG_LINE_NO:1138
  Else
    akActor.SetDontMove(True) ; #DEBUG_LINE_NO:1140
  EndIf
  Self.SendDDAnimationEvent(animation, akActor) ; #DEBUG_LINE_NO:1142
  Self.SetAnimating(akActor, True) ; #DEBUG_LINE_NO:1143
  Debug.SendAnimationEvent(akActor as ObjectReference, animation) ; #DEBUG_LINE_NO:1144
  Return ret ; #DEBUG_LINE_NO:1145
EndFunction

Function PlayThirdPersonAnimation(Actor akActor, String animation, Int duration, Bool permitRestrictive)
  Self.Log(("PlayThirdPersonAnimation(" + akActor.GetLeveledActorBase().GetName() + "," + animation + "," + duration as String) + ")", 0) ; #DEBUG_LINE_NO:1150
  If Self.IsAnimating(akActor) ; #DEBUG_LINE_NO:1151
    Self.Log("Actor already in animating faction.", 0) ; #DEBUG_LINE_NO:1152
    Return  ; #DEBUG_LINE_NO:1153
  EndIf
  If SexLab.ValidateActor(akActor) < 0 || akActor.IsSwimming() || akActor.IsOnMount() || akActor.GetCurrentScene() != None || akActor.GetSitState() != 0 ; #DEBUG_LINE_NO:1155
    Self.Warn("Not playing third person animation: Actor is already in a blocking animation.") ; #DEBUG_LINE_NO:1156
    Return  ; #DEBUG_LINE_NO:1157
  EndIf
  Bool[] cameraState = Self.StartThirdPersonAnimation(akActor, animation, permitRestrictive) ; #DEBUG_LINE_NO:1159
  Self.Log(("Playing animation for " + duration as String) + " seconds.", 0) ; #DEBUG_LINE_NO:1160
  Self.SendDDFunctionEvent("PlayThirdPersonAnimation", akActor) ; #DEBUG_LINE_NO:1161
  Utility.Wait(duration as Float) ; #DEBUG_LINE_NO:1162
  Self.EndThirdPersonAnimation(akActor, cameraState, permitRestrictive) ; #DEBUG_LINE_NO:1163
EndFunction

Function EndThirdPersonAnimation(Actor akActor, Bool[] cameraState, Bool permitRestrictive)
  Self.Log(("EndThirdPersonAnimation( " + akActor.GetLeveledActorBase().GetName() + "," + cameraState[0] as String) + ")", 0) ; #DEBUG_LINE_NO:1168
  Self.SetAnimating(akActor, False) ; #DEBUG_LINE_NO:1169
  If !akActor.Is3DLoaded() || akActor.IsDead() || akActor.IsDisabled() ; #DEBUG_LINE_NO:1170
    Self.Log("Actor is not loaded (Or is otherwise invalid). Aborting.", 0) ; #DEBUG_LINE_NO:1171
    Return  ; #DEBUG_LINE_NO:1172
  EndIf
  Debug.SendAnimationEvent(akActor as ObjectReference, "IdleForceDefaultState") ; #DEBUG_LINE_NO:1175
  If akActor == PlayerRef ; #DEBUG_LINE_NO:1176
    Self.UpdateControls() ; #DEBUG_LINE_NO:1177
    If cameraState[0] ; #DEBUG_LINE_NO:1178
      Game.ForceFirstPerson() ; #DEBUG_LINE_NO:1179
    EndIf
  Else
    akActor.SetDontMove(False) ; #DEBUG_LINE_NO:1182
  EndIf
  Self.SendDDFunctionEvent("EndThirdPersonAnimation", akActor) ; #DEBUG_LINE_NO:1184
EndFunction

Float Function GetVersion()
  Return 14.0 ; #DEBUG_LINE_NO:1189
EndFunction

String Function GetVersionString()
  Return "5.2-NG" ; #DEBUG_LINE_NO:1193
EndFunction

Function StoreExposureRate(Actor akActor)
  Float original = storageutil.GetFloatValue(akActor as Form, "zad.StoredExposureRate", 0.0) ; #DEBUG_LINE_NO:1202
  If original <= 0.0 ; #DEBUG_LINE_NO:1203
    Float exposureRate = Aroused.GetActorExposureRate(akActor) ; #DEBUG_LINE_NO:1204
    If exposureRate <= 0.0 ; #DEBUG_LINE_NO:1205
      Self.Warn("Exposure rate was originally 0. Changing to default.") ; #DEBUG_LINE_NO:1206
      exposureRate = Aroused.slaConfig.DefaultExposureRate ; #DEBUG_LINE_NO:1207
    EndIf
    storageutil.SetFloatValue(akActor as Form, "zad.StoredExposureRate", exposureRate) ; #DEBUG_LINE_NO:1209
  Else
    Self.Warn("StoreExposureRate called for actor " + akActor.GetLeveledActorBase().GetName() + ", but actor already has this information stored.") ; #DEBUG_LINE_NO:1211
  EndIf
EndFunction

Float Function GetOriginalRate(Actor akActor)
  Return storageutil.GetFloatValue(akActor as Form, "zad.StoredExposureRate", Aroused.slaConfig.DefaultExposureRate) ; #DEBUG_LINE_NO:1217
EndFunction

Float Function GetModifiedRate(Actor akActor)
  If akActor.WornHasKeyword(zad_DeviousPlug) ; #DEBUG_LINE_NO:1222
    Return Self.GetOriginalRate(akActor) * Self.GetPlugRateMult() ; #DEBUG_LINE_NO:1223
  ElseIf akActor.WornHasKeyword(zad_DeviousBelt) ; #DEBUG_LINE_NO:1224
    Return Self.GetOriginalRate(akActor) * Self.GetBeltRateMult() ; #DEBUG_LINE_NO:1225
  Else
    Self.Warn("GetModifiedRate() returning original rate: Actor is not belted.") ; #DEBUG_LINE_NO:1227
    Return Self.GetOriginalRate(akActor) ; #DEBUG_LINE_NO:1228
  EndIf
EndFunction

Bool Function CheckConfigInit()
  If Config == None ; #DEBUG_LINE_NO:1233
    Self.Error("Config is not initialized. User didn't update properly?") ; #DEBUG_LINE_NO:1234
    Return False ; #DEBUG_LINE_NO:1235
  EndIf
  Return True ; #DEBUG_LINE_NO:1237
EndFunction

Float Function GetPlugRateMult()
  If Self.CheckConfigInit() ; #DEBUG_LINE_NO:1241
    Return Config.PlugRateMult ; #DEBUG_LINE_NO:1242
  Else
    Self.Error("GetPlugRateMult returning -1") ; #DEBUG_LINE_NO:1244
    Return -1.0 ; #DEBUG_LINE_NO:1245
  EndIf
EndFunction

Float Function GetBeltRateMult()
  If Self.CheckConfigInit() ; #DEBUG_LINE_NO:1250
    Return Config.BeltRateMult ; #DEBUG_LINE_NO:1251
  Else
    Self.Error("GetBeltRateMult returning -1") ; #DEBUG_LINE_NO:1253
    Return -1.0 ; #DEBUG_LINE_NO:1254
  EndIf
EndFunction

Function notify(String out, Bool messageBox)
  If messageBox ; #DEBUG_LINE_NO:1259
    Debug.messageBox(out) ; #DEBUG_LINE_NO:1260
  Else
    Debug.Notification(out) ; #DEBUG_LINE_NO:1262
  EndIf
EndFunction

Function NotifyNPC(String out, Bool messageBox)
  If Config.NpcMessages ; #DEBUG_LINE_NO:1268
    Self.notify(out, messageBox) ; #DEBUG_LINE_NO:1269
  Else
    Self.Log("NpcMessages==false: " + out, 0) ; #DEBUG_LINE_NO:1271
  EndIf
EndFunction

Function NotifyPlayer(String out, Bool messageBox)
  If Config.PlayerMessages ; #DEBUG_LINE_NO:1277
    Self.notify(out, messageBox) ; #DEBUG_LINE_NO:1278
  Else
    Self.Log("PlayerMessages==false: " + out, 0) ; #DEBUG_LINE_NO:1280
  EndIf
EndFunction

Function NotifyActor(String out, Actor akActor, Bool messageBox)
  If akActor == PlayerRef ; #DEBUG_LINE_NO:1286
    Self.NotifyPlayer(out, messageBox) ; #DEBUG_LINE_NO:1287
  Else
    Self.NotifyNPC(out, messageBox) ; #DEBUG_LINE_NO:1289
  EndIf
EndFunction

Function Log(String in, Int level)
  If !Config.LogMessages ; #DEBUG_LINE_NO:1295
    Return  ; #DEBUG_LINE_NO:1296
  EndIf
  String out = "[Zad-NG]" ; #DEBUG_LINE_NO:1298
  If level >= 2 ; #DEBUG_LINE_NO:1299
    Debug.Trace("============================================================", 0) ; #DEBUG_LINE_NO:1300
    Debug.Trace(out + " (((ERROR))): " + in, 0) ; #DEBUG_LINE_NO:1301
    Debug.Trace("============================================================", 0) ; #DEBUG_LINE_NO:1302
    Return  ; #DEBUG_LINE_NO:1303
  ElseIf level == 1 ; #DEBUG_LINE_NO:1304
    out = out + " ((WARNING)): " + in ; #DEBUG_LINE_NO:1305
  ElseIf level == 0 ; #DEBUG_LINE_NO:1306
    out = out + ": " + in ; #DEBUG_LINE_NO:1307
  EndIf
  Debug.Trace(out, 0) ; #DEBUG_LINE_NO:1309
EndFunction

Function Warn(String in)
  Self.Log(in, 1) ; #DEBUG_LINE_NO:1314
EndFunction

Function Error(String in)
  Self.Log(in, 2) ; #DEBUG_LINE_NO:1319
  Debug.messageBox("An error has ocurred with your installation of Devious Devices. Please check the log for more information. Error Text: " + in) ; #DEBUG_LINE_NO:1320
EndFunction

Bool Function WearingConflictingDevice(Actor akActor, Armor deviceRendered, Keyword zad_DeviousDevice)
  If akActor == None ; #DEBUG_LINE_NO:1325
    Self.Error("WearingConflictingDevice received none argument.") ; #DEBUG_LINE_NO:1326
  EndIf
  If akActor.GetItemCount(deviceRendered as Form) == 0 && akActor.WornHasKeyword(zad_DeviousDevice) ; #DEBUG_LINE_NO:1328
    Return True ; #DEBUG_LINE_NO:1329
  EndIf
  Return False ; #DEBUG_LINE_NO:1331
EndFunction

Function UpdateExposure(Actor akRef, Float val, Bool skipMultiplier)
  If akRef == None ; #DEBUG_LINE_NO:1337
    Self.Error("UpdateExposure passed none reference.") ; #DEBUG_LINE_NO:1338
    Return  ; #DEBUG_LINE_NO:1339
  EndIf
  Float lastRank = Aroused.GetActorExposure(akRef) as Float ; #DEBUG_LINE_NO:1341
  Self.Log(((("UpdateExposure(" + akRef as String) + "," + val as String) + "," + skipMultiplier as String) + ")", 0) ; #DEBUG_LINE_NO:1342
  If skipMultiplier ; #DEBUG_LINE_NO:1343
    Float newVal = lastRank + val ; #DEBUG_LINE_NO:1346
    If newVal > 100.0 ; #DEBUG_LINE_NO:1347
      newVal = 100.0 ; #DEBUG_LINE_NO:1348
    EndIf
    If Config.GotOSLA ; #DEBUG_LINE_NO:1352
      Aroused.SetActorExposure(akRef, (newVal + 1.0) as Int) ; #DEBUG_LINE_NO:1353
    Else
      storageutil.SetFloatValue(akRef as Form, "SLAroused.ActorExposure", newVal) ; #DEBUG_LINE_NO:1355
      storageutil.SetFloatValue(akRef as Form, "SLAroused.ActorExposureDate", Self.GameDaysPassed.GetValue()) ; #DEBUG_LINE_NO:1356
    EndIf
  Else
    Float rate = Aroused.GetActorExposureRate(akRef) ; #DEBUG_LINE_NO:1360
    Int newRank = (lastRank + val * rate) as Int ; #DEBUG_LINE_NO:1361
    Aroused.SetActorExposure(akRef, newRank) ; #DEBUG_LINE_NO:1362
  EndIf
EndFunction

Function AcquireAndSpinlock()
  If DeviceMutex ; #DEBUG_LINE_NO:1369
    Self.Log("SpinLock() Started.", 0) ; #DEBUG_LINE_NO:1370
    Int timeout = 0 ; #DEBUG_LINE_NO:1371
    While DeviceMutex
      Utility.Wait(0.100000001) ; #DEBUG_LINE_NO:1373
      If timeout >= 150 ; #DEBUG_LINE_NO:1374
        Self.Warn("SpinLock() timed out.") ; #DEBUG_LINE_NO:1375
        DeviceMutex = False ; #DEBUG_LINE_NO:1376
      EndIf
      timeout += 1 ; #DEBUG_LINE_NO:1378
    EndWhile
    Self.Log("SpinLock() Completed. Cycles:" + timeout as String, 0) ; #DEBUG_LINE_NO:1380
  EndIf
  DeviceMutex = True ; #DEBUG_LINE_NO:1382
EndFunction

Function SendDeviceRemovalEvent(String deviceName, Actor akActor)
  Int isPlayer = 0 ; #DEBUG_LINE_NO:1387
  If akActor == PlayerRef ; #DEBUG_LINE_NO:1388
    isPlayer = 1 ; #DEBUG_LINE_NO:1389
  EndIf
  Self.SendDeviceEvent("DeviceRemoved" + deviceName, akActor.GetLeveledActorBase().GetName(), isPlayer) ; #DEBUG_LINE_NO:1391
EndFunction

Function SendDeviceEquippedEvent(String deviceName, Actor akActor)
  Int isPlayer = 0 ; #DEBUG_LINE_NO:1395
  If akActor == PlayerRef ; #DEBUG_LINE_NO:1396
    isPlayer = 1 ; #DEBUG_LINE_NO:1397
  EndIf
  Self.SendDeviceEvent("DeviceEquipped" + deviceName, akActor.GetLeveledActorBase().GetName(), isPlayer) ; #DEBUG_LINE_NO:1399
EndFunction

Function SendDeviceEvent(String eventName, String actorName, Int isPlayer)
  Self.Log(("Sending device event " + eventName + "(" + actorName + ":" + isPlayer as String) + ")", 0) ; #DEBUG_LINE_NO:1403
  Self.SendModEvent(eventName, actorName, isPlayer as Float) ; #DEBUG_LINE_NO:1404
EndFunction

Function SendDeviceEquippedEventVerbose(Armor inventoryDevice, Keyword deviceKeyword, Actor akActor)
  Int Handle = modevent.Create("DDI_DeviceEquipped") ; #DEBUG_LINE_NO:1409
  If Handle ; #DEBUG_LINE_NO:1410
    modevent.PushForm(Handle, inventoryDevice as Form) ; #DEBUG_LINE_NO:1411
    modevent.PushForm(Handle, deviceKeyword as Form) ; #DEBUG_LINE_NO:1412
    modevent.PushForm(Handle, akActor as Form) ; #DEBUG_LINE_NO:1413
    modevent.Send(Handle) ; #DEBUG_LINE_NO:1414
  EndIf
EndFunction

Function SendDeviceRemovedEventVerbose(Armor inventoryDevice, Keyword deviceKeyword, Actor akActor)
  Int Handle = modevent.Create("DDI_DeviceRemoved") ; #DEBUG_LINE_NO:1419
  If Handle ; #DEBUG_LINE_NO:1420
    modevent.PushForm(Handle, inventoryDevice as Form) ; #DEBUG_LINE_NO:1421
    modevent.PushForm(Handle, deviceKeyword as Form) ; #DEBUG_LINE_NO:1422
    modevent.PushForm(Handle, akActor as Form) ; #DEBUG_LINE_NO:1423
    modevent.Send(Handle) ; #DEBUG_LINE_NO:1424
  EndIf
EndFunction

Function SendDeviceKeyBreakEventVerbose(Armor inventoryDevice, Keyword deviceKeyword, Actor akActor)
  Int Handle = modevent.Create("DDI_KeyBreak") ; #DEBUG_LINE_NO:1429
  If Handle ; #DEBUG_LINE_NO:1430
    modevent.PushForm(Handle, inventoryDevice as Form) ; #DEBUG_LINE_NO:1431
    modevent.PushForm(Handle, deviceKeyword as Form) ; #DEBUG_LINE_NO:1432
    modevent.PushForm(Handle, akActor as Form) ; #DEBUG_LINE_NO:1433
    modevent.Send(Handle) ; #DEBUG_LINE_NO:1434
  EndIf
EndFunction

Function SendDeviceJamLockEventVerbose(Armor inventoryDevice, Keyword deviceKeyword, Actor akActor)
  Int Handle = modevent.Create("DDI_JamLock") ; #DEBUG_LINE_NO:1439
  If Handle ; #DEBUG_LINE_NO:1440
    modevent.PushForm(Handle, inventoryDevice as Form) ; #DEBUG_LINE_NO:1441
    modevent.PushForm(Handle, deviceKeyword as Form) ; #DEBUG_LINE_NO:1442
    modevent.PushForm(Handle, akActor as Form) ; #DEBUG_LINE_NO:1443
    modevent.Send(Handle) ; #DEBUG_LINE_NO:1444
  EndIf
EndFunction

Function SendDeviceEscapeEvent(Armor inventoryDevice, Keyword deviceKeyword, Bool successful)
  Int Handle = modevent.Create("DDI_DeviceEscapeAttempt") ; #DEBUG_LINE_NO:1449
  If Handle ; #DEBUG_LINE_NO:1450
    modevent.PushForm(Handle, inventoryDevice as Form) ; #DEBUG_LINE_NO:1451
    modevent.PushForm(Handle, deviceKeyword as Form) ; #DEBUG_LINE_NO:1452
    modevent.PushBool(Handle, successful) ; #DEBUG_LINE_NO:1453
    modevent.Send(Handle) ; #DEBUG_LINE_NO:1454
  EndIf
EndFunction

Function SendQuestSigTerm()
  Int Handle = modevent.Create("DDI_Quest_SigTerm") ; #DEBUG_LINE_NO:1461
  If Handle ; #DEBUG_LINE_NO:1462
    modevent.Send(Handle) ; #DEBUG_LINE_NO:1463
  EndIf
EndFunction

Function SendDDFunctionEvent(String FunctionName, Actor akActor)
  Int Handle = modevent.Create("DDI_FunctionCalled") ; #DEBUG_LINE_NO:1469
  If Handle ; #DEBUG_LINE_NO:1470
    modevent.PushString(Handle, FunctionName) ; #DEBUG_LINE_NO:1471
    modevent.PushForm(Handle, akActor as Form) ; #DEBUG_LINE_NO:1472
    modevent.Send(Handle) ; #DEBUG_LINE_NO:1473
  EndIf
EndFunction

Function SendDDAnimationEvent(String animation, Actor akActor)
  Int Handle = modevent.Create("DDI_AnimationStarted") ; #DEBUG_LINE_NO:1479
  If Handle ; #DEBUG_LINE_NO:1480
    modevent.PushString(Handle, animation) ; #DEBUG_LINE_NO:1481
    modevent.PushForm(Handle, akActor as Form) ; #DEBUG_LINE_NO:1482
    modevent.Send(Handle) ; #DEBUG_LINE_NO:1483
  EndIf
EndFunction

Function DDI_DebugFixDevices()
  Self.Log("DDI_DebugFixDevices called. Attempting to fix broken devices.", 0) ; #DEBUG_LINE_NO:1490
  Armor idevice = None ; #DEBUG_LINE_NO:1491
  Armor rDevice = None ; #DEBUG_LINE_NO:1492
  Form kForm = None ; #DEBUG_LINE_NO:1493
  Int RemovedItems = 0 ; #DEBUG_LINE_NO:1494
  zadTemporaryDeviceStorage.Revert() ; #DEBUG_LINE_NO:1495
  Int i = PlayerRef.GetNumItems() ; #DEBUG_LINE_NO:1496
  Bool clearAll = True ; #DEBUG_LINE_NO:1497
  While i > 0 ; #DEBUG_LINE_NO:1498
    i -= 1 ; #DEBUG_LINE_NO:1499
    kForm = PlayerRef.GetNthForm(i) ; #DEBUG_LINE_NO:1500
    If kForm as Armor ; #DEBUG_LINE_NO:1501
      If kForm.HasKeyWord(zad_InventoryDevice) && PlayerRef.IsEquipped(kForm) ; #DEBUG_LINE_NO:1502
        idevice = kForm as Armor ; #DEBUG_LINE_NO:1503
        If idevice ; #DEBUG_LINE_NO:1504
          ObjectReference tmpORef = PlayerRef.placeAtMe(kForm, 1, False, True) ; #DEBUG_LINE_NO:1505
          zadequipscript tmpZRef = tmpORef as zadequipscript ; #DEBUG_LINE_NO:1506
          If tmpZRef != None && PlayerRef.GetItemCount(tmpZRef.deviceRendered as Form) > 0 ; #DEBUG_LINE_NO:1507
            rDevice = tmpZRef.deviceRendered ; #DEBUG_LINE_NO:1508
            zadTemporaryDeviceStorage.AddForm(rDevice as Form) ; #DEBUG_LINE_NO:1509
            clearAll = False ; #DEBUG_LINE_NO:1510
          EndIf
          tmpORef.delete() ; #DEBUG_LINE_NO:1512
        EndIf
      EndIf
    EndIf
  EndWhile
  i = PlayerRef.GetNumItems() ; #DEBUG_LINE_NO:1517
  While i > 0 ; #DEBUG_LINE_NO:1518
    i -= 1 ; #DEBUG_LINE_NO:1519
    kForm = PlayerRef.GetNthForm(i) ; #DEBUG_LINE_NO:1520
    If kForm as Armor ; #DEBUG_LINE_NO:1521
      If kForm.HasKeyWord(zad_Lockable) && !kForm.HasKeyWord(zad_InventoryDevice) ; #DEBUG_LINE_NO:1522
        Int c = PlayerRef.GetItemCount(kForm) ; #DEBUG_LINE_NO:1523
        If clearAll ; #DEBUG_LINE_NO:1524
          PlayerRef.RemoveItem(kForm, c, True, None) ; #DEBUG_LINE_NO:1525
        Else
          Int j = zadTemporaryDeviceStorage.GetSize() ; #DEBUG_LINE_NO:1527
          Bool removeRendered = True ; #DEBUG_LINE_NO:1528
          While j > 0 ; #DEBUG_LINE_NO:1529
            j -= 1 ; #DEBUG_LINE_NO:1530
            rDevice = zadTemporaryDeviceStorage.GetAt(j) as Armor ; #DEBUG_LINE_NO:1531
            If rDevice as Form == kForm ; #DEBUG_LINE_NO:1532
              removeRendered = False ; #DEBUG_LINE_NO:1533
            EndIf
          EndWhile
          If removeRendered ; #DEBUG_LINE_NO:1536
            PlayerRef.RemoveItem(kForm, c, True, None) ; #DEBUG_LINE_NO:1537
            RemovedItems += c ; #DEBUG_LINE_NO:1538
          ElseIf c > 1 ; #DEBUG_LINE_NO:1539
            c -= 1 ; #DEBUG_LINE_NO:1540
            PlayerRef.RemoveItem(kForm, c, True, None) ; #DEBUG_LINE_NO:1541
            RemovedItems += c ; #DEBUG_LINE_NO:1542
          EndIf
        EndIf
      EndIf
    EndIf
  EndWhile
  Self.notify(("DDI Debug broken item removal completed, " + RemovedItems as String) + " items removed.", False) ; #DEBUG_LINE_NO:1548
  zadTemporaryDeviceStorage.Revert() ; #DEBUG_LINE_NO:1549
EndFunction

Function DDI_DebugTerminate()
  Self.Log("DDI_DebugTerminate called. Removing devices.", 0) ; #DEBUG_LINE_NO:1556
  Self.notify("DDI Debug Terminate signal triggered.\nAttempting to clear all DD items, including quest devices.", True) ; #DEBUG_LINE_NO:1557
  Utility.Wait(1.0) ; #DEBUG_LINE_NO:1558
  Armor idevice = None ; #DEBUG_LINE_NO:1559
  Armor rDevice = None ; #DEBUG_LINE_NO:1560
  Keyword kw = None ; #DEBUG_LINE_NO:1561
  Keyword token = None ; #DEBUG_LINE_NO:1562
  If !PlayerRef.WornHasKeyword(zad_Lockable) ; #DEBUG_LINE_NO:1563
    Self.notify("No DD items found on the player. Aborting.", True) ; #DEBUG_LINE_NO:1565
    Return  ; #DEBUG_LINE_NO:1566
  EndIf
  Int i = PlayerRef.GetNumItems() ; #DEBUG_LINE_NO:1568
  While i > 0 ; #DEBUG_LINE_NO:1569
    i -= 1 ; #DEBUG_LINE_NO:1570
    Form kForm = PlayerRef.GetNthForm(i) ; #DEBUG_LINE_NO:1571
    If kForm as Armor ; #DEBUG_LINE_NO:1572
      If kForm.HasKeyWord(zad_InventoryDevice) ; #DEBUG_LINE_NO:1573
        idevice = kForm as Armor ; #DEBUG_LINE_NO:1574
        If idevice ; #DEBUG_LINE_NO:1575
          rDevice = Self.GetRenderedDevice(idevice) ; #DEBUG_LINE_NO:1576
          kw = Self.GetDeviceKeyword(idevice) ; #DEBUG_LINE_NO:1577
          If (idevice as Bool && rDevice as Bool) && kw as Bool ; #DEBUG_LINE_NO:1578
            If idevice.HasKeyWord(zad_QuestItem) || rDevice.HasKeyWord(zad_QuestItem) ; #DEBUG_LINE_NO:1580
              Int k = rDevice.GetNumKeywords() ; #DEBUG_LINE_NO:1582
              Bool done = False ; #DEBUG_LINE_NO:1583
              While k > 0 && !done ; #DEBUG_LINE_NO:1585
                k -= 1 ; #DEBUG_LINE_NO:1586
                Keyword dKeyword = rDevice.GetNthKeyword(k) ; #DEBUG_LINE_NO:1587
                If dKeyword as Bool && !zadStandardKeywords.HasForm(dKeyword as Form) ; #DEBUG_LINE_NO:1588
                  token = dKeyword ; #DEBUG_LINE_NO:1589
                  done = True ; #DEBUG_LINE_NO:1590
                EndIf
              EndWhile
              If token ; #DEBUG_LINE_NO:1593
                Self.RemoveQuestDevice(PlayerRef, idevice, rDevice, kw, token, True, True) ; #DEBUG_LINE_NO:1594
              EndIf
            Else
              Self.removeDevice(PlayerRef, idevice, rDevice, kw, True, False, True) ; #DEBUG_LINE_NO:1597
            EndIf
            Utility.Wait(1.0) ; #DEBUG_LINE_NO:1599
          EndIf
        EndIf
      EndIf
    EndIf
  EndWhile
  Self.notify("DDI Debug item removal completed. Sending SigTerm to content mods.", True) ; #DEBUG_LINE_NO:1605
  Utility.Wait(1.0) ; #DEBUG_LINE_NO:1606
  Self.SendQuestSigTerm() ; #DEBUG_LINE_NO:1607
EndFunction

Function Masturbate(Actor a, Bool feedback)
  sslBaseAnimation[] Manims
  If a == PlayerRef && feedback ; #DEBUG_LINE_NO:1617
    Self.NotifyPlayer("You cannot resist your urges anymore!", False) ; #DEBUG_LINE_NO:1618
  ElseIf a.GetLeveledActorBase().GetSex() == 1 && feedback ; #DEBUG_LINE_NO:1620
    Self.NotifyPlayer(a.GetLeveledActorBase().GetName() + " can't resist her urges anymore...", False) ; #DEBUG_LINE_NO:1621
  ElseIf a.GetLeveledActorBase().GetSex() == 0 && feedback ; #DEBUG_LINE_NO:1622
    Self.NotifyPlayer(a.GetLeveledActorBase().GetName() + " can't resist his urges anymore...", False) ; #DEBUG_LINE_NO:1623
  EndIf
  If a.WornHasKeyword(zad_DeviousArmbinder) ; #DEBUG_LINE_NO:1626
    Manims = new sslBaseAnimation[1] ; #DEBUG_LINE_NO:1627
    Manims[0] = SexLab.GetAnimationObject("DDArmbinderSolo") ; #DEBUG_LINE_NO:1628
  ElseIf a.WornHasKeyword(zad_DeviousYoke) ; #DEBUG_LINE_NO:1629
    Manims = new sslBaseAnimation[1] ; #DEBUG_LINE_NO:1630
    Manims[0] = SexLab.GetAnimationObject("DDYokeSolo") ; #DEBUG_LINE_NO:1631
  ElseIf a.WornHasKeyword(zad_DeviousArmbinderElbow) ; #DEBUG_LINE_NO:1632
    Manims = new sslBaseAnimation[1] ; #DEBUG_LINE_NO:1633
    Manims[0] = SexLab.GetAnimationObject("DDElbowbinderSolo") ; #DEBUG_LINE_NO:1634
  ElseIf a.WornHasKeyword(zad_DeviousYokeBB) ; #DEBUG_LINE_NO:1635
    Manims = new sslBaseAnimation[1] ; #DEBUG_LINE_NO:1636
    Manims[0] = SexLab.GetAnimationObject("DDBBYokeSolo") ; #DEBUG_LINE_NO:1637
  ElseIf a.WornHasKeyword(zad_DeviousCuffsFront) ; #DEBUG_LINE_NO:1638
    Manims = new sslBaseAnimation[1] ; #DEBUG_LINE_NO:1639
    Manims[0] = SexLab.GetAnimationObject("DDFrontCuffsSolo") ; #DEBUG_LINE_NO:1640
  ElseIf a.WornHasKeyword(zad_DeviousElbowTie) ; #DEBUG_LINE_NO:1641
    Manims = new sslBaseAnimation[1] ; #DEBUG_LINE_NO:1642
    Manims[0] = SexLab.GetAnimationObject("DDElbowTieSolo") ; #DEBUG_LINE_NO:1643
  ElseIf (a.WornHasKeyword(zad_DeviousBelt) || a.WornHasKeyword(zad_DeviousHarness)) && !a.WornHasKeyword(zad_DeviousHeavyBondage) && !a.WornHasKeyword(zad_PermitVaginal) ; #DEBUG_LINE_NO:1646
    Manims = new sslBaseAnimation[1] ; #DEBUG_LINE_NO:1647
    Manims[0] = SexLab.GetAnimationObject("DDBeltedSolo") ; #DEBUG_LINE_NO:1648
  ElseIf a.WornHasKeyword(zad_DeviousHeavyBondage) ; #DEBUG_LINE_NO:1649
    Self.PlayHornyAnimation(a) ; #DEBUG_LINE_NO:1651
    Return  ; #DEBUG_LINE_NO:1652
  ElseIf a.GetLeveledActorBase().GetSex() == 1 ; #DEBUG_LINE_NO:1654
    Manims = SexLab.GetAnimationsByTag(1, "Solo", "F", "", "", True) ; #DEBUG_LINE_NO:1655
  Else
    Manims = SexLab.GetAnimationsByTag(1, "Solo", "M", "", "", True) ; #DEBUG_LINE_NO:1657
  EndIf
  Actor[] tmp = new Actor[1] ; #DEBUG_LINE_NO:1660
  tmp[0] = a ; #DEBUG_LINE_NO:1661
  SexLab.StartSex(tmp, Manims, None, None, True, "") ; #DEBUG_LINE_NO:1662
EndFunction

Function SendOrgasmEvent(Actor akActor, Int aiArousalSet)
  Self.SendModEvent("DeviceActorOrgasm", akActor.GetLeveledActorBase().GetName(), 0.0) ; #DEBUG_LINE_NO:1678
  Int loc_handle = modevent.Create("DeviceActorOrgasmEx") ; #DEBUG_LINE_NO:1681
  If loc_handle ; #DEBUG_LINE_NO:1682
    modevent.PushForm(loc_handle, Self as Form) ; #DEBUG_LINE_NO:1683
    modevent.PushForm(loc_handle, akActor as Form) ; #DEBUG_LINE_NO:1684
    modevent.PushInt(loc_handle, aiArousalSet) ; #DEBUG_LINE_NO:1685
    modevent.Send(loc_handle) ; #DEBUG_LINE_NO:1686
  EndIf
EndFunction

Function ActorOrgasm(Actor akActor, Int setArousalTo, Int vsID)
  Self.SendOrgasmEvent(akActor, setArousalTo) ; #DEBUG_LINE_NO:1697
  If setArousalTo < 0 ; #DEBUG_LINE_NO:1698
    setArousalTo = Utility.RandomInt(0, 75) ; #DEBUG_LINE_NO:1699
  EndIf
  Int sID = OrgasmSound.Play(akActor as ObjectReference) ; #DEBUG_LINE_NO:1701
  Sound.SetInstanceVolume(sID, Config.VolumeOrgasm) ; #DEBUG_LINE_NO:1702
  Aroused.SetActorExposure(akActor, setArousalTo) ; #DEBUG_LINE_NO:1703
  Aroused.UpdateActorOrgasmDate(akActor) ; #DEBUG_LINE_NO:1704
  If !Self.IsAnimating(akActor) ; #DEBUG_LINE_NO:1705
    Bool[] cameraState = Self.StartThirdPersonAnimation(akActor, Self.AnimSwitchKeyword(akActor, "Orgasm"), True) ; #DEBUG_LINE_NO:1706
    Int i = 0 ; #DEBUG_LINE_NO:1707
    While i < 20 ; #DEBUG_LINE_NO:1708
      i += 1 ; #DEBUG_LINE_NO:1709
      If !Self.IsVibrating(akActor) && vsID != -1 ; #DEBUG_LINE_NO:1710
        Sound.StopInstance(vsID) ; #DEBUG_LINE_NO:1711
        vsID = -1 ; #DEBUG_LINE_NO:1712
      EndIf
      Utility.Wait(1.0) ; #DEBUG_LINE_NO:1714
    EndWhile
    Self.EndThirdPersonAnimation(akActor, cameraState, True) ; #DEBUG_LINE_NO:1716
  ElseIf !Self.IsVibrating(akActor) && vsID != -1 ; #DEBUG_LINE_NO:1718
    Sound.StopInstance(vsID) ; #DEBUG_LINE_NO:1719
    vsID = -1 ; #DEBUG_LINE_NO:1720
  EndIf
EndFunction

Function Pant(Actor akActor)
  Int psID = ShortPantSound.Play(akActor as ObjectReference) ; #DEBUG_LINE_NO:1727
  Sound.SetInstanceVolume(psID, 1.0) ; #DEBUG_LINE_NO:1728
EndFunction

Function SexlabMoan(Actor akActor, Int arousal, sslBaseVoice voice)
  If akActor.WornHasKeyword(zad_DeviousGag) ; #DEBUG_LINE_NO:1734
    Int gsID = GaggedSound.Play(akActor as ObjectReference) ; #DEBUG_LINE_NO:1735
    Sound.SetInstanceVolume(gsID, 1.0) ; #DEBUG_LINE_NO:1736
    Return  ; #DEBUG_LINE_NO:1737
  EndIf
  If !voice ; #DEBUG_LINE_NO:1740
    voice = SexLab.PickVoice(akActor) ; #DEBUG_LINE_NO:1741
  EndIf
  If arousal == -1 ; #DEBUG_LINE_NO:1744
    arousal = Aroused.GetActorExposure(akActor) ; #DEBUG_LINE_NO:1745
  EndIf
  voice.Moan(akActor, arousal, False) ; #DEBUG_LINE_NO:1748
EndFunction

Function Moan(Actor akActor, Int arousal, sslBaseVoice voice)
  If akActor.WornHasKeyword(zad_DeviousGag) ; #DEBUG_LINE_NO:1753
    Int gsID = GaggedSound.Play(akActor as ObjectReference) ; #DEBUG_LINE_NO:1754
    Sound.SetInstanceVolume(gsID, 1.0) ; #DEBUG_LINE_NO:1755
  Else
    Int msID = ShortMoanSound.Play(akActor as ObjectReference) ; #DEBUG_LINE_NO:1757
    Sound.SetInstanceVolume(msID, 1.0) ; #DEBUG_LINE_NO:1758
  EndIf
EndFunction

Bool Function PlaySceneAndWait(Scene toPlay, Bool forceStart, Int timeout, Int increment)
  If forceStart ; #DEBUG_LINE_NO:1764
    toPlay.forceStart() ; #DEBUG_LINE_NO:1765
  Else
    toPlay.Start() ; #DEBUG_LINE_NO:1767
  EndIf
  Int i = 0 ; #DEBUG_LINE_NO:1769
  While toPlay.IsPlaying() && i < timeout ; #DEBUG_LINE_NO:1770
    i += increment ; #DEBUG_LINE_NO:1771
    Utility.Wait(increment as Float) ; #DEBUG_LINE_NO:1772
  EndWhile
  If i >= timeout ; #DEBUG_LINE_NO:1774
    If toPlay.IsPlaying() ; #DEBUG_LINE_NO:1775
      toPlay.Stop() ; #DEBUG_LINE_NO:1776
    EndIf
    Self.Log("Scene timed out.", 0) ; #DEBUG_LINE_NO:1778
    Return False ; #DEBUG_LINE_NO:1779
  EndIf
  Return True ; #DEBUG_LINE_NO:1781
EndFunction

Function SendEdgeEvent(Actor akActor)
  Self.SendModEvent("DeviceEdgedActor", akActor.GetLeveledActorBase().GetName(), 0.0) ; #DEBUG_LINE_NO:1797
  Int loc_handle = modevent.Create("DeviceEdgedActorEx") ; #DEBUG_LINE_NO:1800
  If loc_handle ; #DEBUG_LINE_NO:1801
    modevent.PushForm(loc_handle, Self as Form) ; #DEBUG_LINE_NO:1802
    modevent.PushForm(loc_handle, akActor as Form) ; #DEBUG_LINE_NO:1803
    modevent.Send(loc_handle) ; #DEBUG_LINE_NO:1804
  EndIf
EndFunction

Function EdgeActor(Actor akActor)
  Self.SendEdgeEvent(akActor) ; #DEBUG_LINE_NO:1811
  Int sID = EdgedSound.Play(akActor as ObjectReference) ; #DEBUG_LINE_NO:1812
  Sound.SetInstanceVolume(sID, Config.VolumeEdged) ; #DEBUG_LINE_NO:1813
  If EnableVRSupport ; #DEBUG_LINE_NO:1814
    Self.PlayThirdPersonAnimation(akActor, Self.AnimSwitchKeyword(akActor, "Edged"), 3, True) ; #DEBUG_LINE_NO:1815
  Else
    Self.PlayThirdPersonAnimation(akActor, Self.AnimSwitchKeyword(akActor, "Edged"), 19, True) ; #DEBUG_LINE_NO:1817
  EndIf
EndFunction

Float Function GetMoanVolume(Actor akActor, Int exposure)
  If akActor == PlayerRef ; #DEBUG_LINE_NO:1823
    If exposure == -1 ; #DEBUG_LINE_NO:1824
      exposure = Aroused.GetActorExposure(akActor) ; #DEBUG_LINE_NO:1825
    EndIf
    Return 0.25 + ((exposure as Float * 0.005) as Int) as Float ; #DEBUG_LINE_NO:1827
  Else
    Return 0.200000003 ; #DEBUG_LINE_NO:1829
  EndIf
EndFunction

Function ApplyExpression(Actor akActor, sslBaseExpression expression, Int strength, Bool openMouth)
  If !expression ; #DEBUG_LINE_NO:1839
    Self.Log("ApplyExpression(): Expression is none.", 0) ; #DEBUG_LINE_NO:1840
    Return  ; #DEBUG_LINE_NO:1841
  EndIf
  ExpLibs.ApplyExpression(akActor, expression, strength, openMouth, 0) ; #DEBUG_LINE_NO:1844
EndFunction

Function ResetExpression(Actor akActor, sslBaseExpression expression)
  ExpLibs.ResetExpressionRaw(akActor, 0) ; #DEBUG_LINE_NO:1849
EndFunction

Function ApplyExpression_v2(Actor akActor, sslBaseExpression expression, Int iPriority, Int strength, Bool openMouth)
  If !expression ; #DEBUG_LINE_NO:1854
    Self.Log("ApplyExpression(): Expression is none.", 0) ; #DEBUG_LINE_NO:1855
    Return  ; #DEBUG_LINE_NO:1856
  EndIf
  ExpLibs.ApplyExpression(akActor, expression, strength, openMouth, iPriority) ; #DEBUG_LINE_NO:1858
EndFunction

Function ApplyExpressionRaw(Actor akActor, Float[] expression, Int iPriority, Int strength, Bool openMouth)
  ExpLibs.ApplyExpressionRaw(akActor, expression, strength, openMouth, iPriority) ; #DEBUG_LINE_NO:1863
EndFunction

Function ResetExpressionRaw(Actor akActor, Int iPriority)
  ExpLibs.ResetExpressionRaw(akActor, iPriority) ; #DEBUG_LINE_NO:1867
EndFunction

String Function GetVibrationStrength(Int vibStrength)
  If vibStrength == 5 ; #DEBUG_LINE_NO:1875
    Return "extremely powerfully" ; #DEBUG_LINE_NO:1876
  ElseIf vibStrength == 4 ; #DEBUG_LINE_NO:1877
    Return "strongly" ; #DEBUG_LINE_NO:1878
  ElseIf vibStrength == 3 ; #DEBUG_LINE_NO:1879
    Return "" ; #DEBUG_LINE_NO:1880
  ElseIf vibStrength == 2 ; #DEBUG_LINE_NO:1881
    Return "softly" ; #DEBUG_LINE_NO:1882
  ElseIf vibStrength == 1 ; #DEBUG_LINE_NO:1883
    Return "agonizingly softly" ; #DEBUG_LINE_NO:1884
  EndIf
  Return "Invalid" ; #DEBUG_LINE_NO:1886
EndFunction

String Function BuildVibrationString(Actor akActor, Int vibStrength, Bool vPlug, Bool aPlug, Bool vPiercings, Bool nPiercings)
  If (vPiercings || nPiercings) && (vPlug || aPlug) ; #DEBUG_LINE_NO:1891
    Return "Your piercing(s) (In addition to the plug(s) within you) begin to vibrate " + Self.GetVibrationStrength(vibStrength) + "!" ; #DEBUG_LINE_NO:1892
  ElseIf vPiercings && !(vPlug || aPlug) ; #DEBUG_LINE_NO:1893
    Return "Your clitoral piercing begins to vibrate " + Self.GetVibrationStrength(vibStrength) + "!" ; #DEBUG_LINE_NO:1894
  ElseIf nPiercings && !(vPlug || aPlug) ; #DEBUG_LINE_NO:1895
    Return "Your nipple piercings begins to vibrate " + Self.GetVibrationStrength(vibStrength) + "!" ; #DEBUG_LINE_NO:1896
  Else
    Return "The plug(s) within you begin to vibrate " + Self.GetVibrationStrength(vibStrength) + "!" ; #DEBUG_LINE_NO:1898
  EndIf
EndFunction

String Function BuildPostVibrationString(Actor akActor, Int vibStrength, Bool vPlug, Bool aPlug, Bool vPiercings, Bool nPiercings)
  Bool Desperate = Aroused.GetActorExposure(akActor) >= Self.ArousalThreshold("Desperate") ; #DEBUG_LINE_NO:1903
  If (vPiercings || nPiercings) && (vPlug || aPlug) ; #DEBUG_LINE_NO:1904
    If Desperate ; #DEBUG_LINE_NO:1905
      Return "You let out a frustrated moan as your piercing and plugs abruptly cease to vibrate." ; #DEBUG_LINE_NO:1906
    Else
      Return "Your piercings and plugs all cease to vibrate." ; #DEBUG_LINE_NO:1908
    EndIf
  ElseIf vPiercings && !(vPlug || aPlug) ; #DEBUG_LINE_NO:1910
    If Desperate ; #DEBUG_LINE_NO:1911
      Return "You let out a frustrated moan as your clitoral piercing ceases to vibrate." ; #DEBUG_LINE_NO:1912
    Else
      Return "Your clitoral piercing ceases to vibrate." ; #DEBUG_LINE_NO:1914
    EndIf
  ElseIf nPiercings && !(vPlug || aPlug) ; #DEBUG_LINE_NO:1916
    If Desperate ; #DEBUG_LINE_NO:1917
      Return "You breathe a sigh of relief when the rings piercing your nipples cease to vibrate." ; #DEBUG_LINE_NO:1918
    Else
      Return "Your nipple piercings cease to vibrate." ; #DEBUG_LINE_NO:1920
    EndIf
  ElseIf Desperate
    Return "Your pussy clenches around the plugs as they abruptly cease vibrating." ; #DEBUG_LINE_NO:1924
  Else
    Return "The plugs cease vibrating." ; #DEBUG_LINE_NO:1926
  EndIf
EndFunction

Int Function VibrateEffect(Actor akActor, Int vibStrength, Int duration, Bool teaseOnly, Bool silent)
  Return Self.VibrateEffectV2(akActor, vibStrength, duration, teaseOnly, silent, False) ; #DEBUG_LINE_NO:1937
EndFunction

Int Function VibrateEffectV2(Actor akActor, Int vibStrength, Int duration, Bool teaseOnly, Bool silent, Bool AllowActorInScene)
  Bool[] cameraState
  Sound vibSoundSelect
  If akActor == PlayerRef && PlayerRef.IsInCombat() ; #DEBUG_LINE_NO:1944
    Self.Log("Vibrate effect called on Player, but player is in combat. Don't start VibrateEffect.", 0) ; #DEBUG_LINE_NO:1945
    Return -2 ; #DEBUG_LINE_NO:1946
  EndIf
  If !Self.IsValidActorV2(akActor, AllowActorInScene) ; #DEBUG_LINE_NO:1948
    Self.Log("Actor is not valid. Don't start VibrateEffect.", 0) ; #DEBUG_LINE_NO:1949
    Return -2 ; #DEBUG_LINE_NO:1950
  EndIf
  Self.AcquireAndSpinlock() ; #DEBUG_LINE_NO:1952
  If duration == 0 ; #DEBUG_LINE_NO:1953
    duration = Utility.RandomInt(5, 20) ; #DEBUG_LINE_NO:1954
  EndIf
  Bool wasVibrating = Self.IsVibrating(akActor) ; #DEBUG_LINE_NO:1956
  Int newDuration = Self.GetVibrating(akActor) + duration ; #DEBUG_LINE_NO:1957
  Self.SetVibrating(akActor, newDuration) ; #DEBUG_LINE_NO:1958
  If wasVibrating ; #DEBUG_LINE_NO:1959
    Self.Log("Actor already in VibratorFaction. Extending duration of existing event: " + newDuration as String, 0) ; #DEBUG_LINE_NO:1960
    DeviceMutex = False ; #DEBUG_LINE_NO:1961
    Return -2 ; #DEBUG_LINE_NO:1962
  EndIf
  Bool vPlug = akActor.WornHasKeyword(zad_DeviousPlugVaginal) ; #DEBUG_LINE_NO:1965
  Bool aPlug = akActor.WornHasKeyword(zad_DeviousPlugAnal) ; #DEBUG_LINE_NO:1966
  Bool nPiercings = akActor.WornHasKeyword(zad_DeviousPiercingsNipple) ; #DEBUG_LINE_NO:1967
  Bool vPiercings = akActor.WornHasKeyword(zad_DeviousPiercingsVaginal) ; #DEBUG_LINE_NO:1968
  DeviceMutex = False ; #DEBUG_LINE_NO:1970
  Float numVibratorsMult = 0.0 ; #DEBUG_LINE_NO:1971
  If vPlug ; #DEBUG_LINE_NO:1973
    numVibratorsMult += 0.699999988 ; #DEBUG_LINE_NO:1974
  EndIf
  If aPlug ; #DEBUG_LINE_NO:1976
    numVibratorsMult += 0.300000012 ; #DEBUG_LINE_NO:1977
  EndIf
  If nPiercings ; #DEBUG_LINE_NO:1980
    numVibratorsMult += 0.25 ; #DEBUG_LINE_NO:1981
  EndIf
  If vPiercings ; #DEBUG_LINE_NO:1983
    numVibratorsMult += 0.5 ; #DEBUG_LINE_NO:1984
  EndIf
  If numVibratorsMult == 0.0 ; #DEBUG_LINE_NO:1987
    numVibratorsMult = 1.0 ; #DEBUG_LINE_NO:1988
  EndIf
  If akActor.WornHasKeyword(zad_DeviousBlindfold) ; #DEBUG_LINE_NO:1990
    numVibratorsMult *= 1.149999976 ; #DEBUG_LINE_NO:1991
  EndIf
  If !wasVibrating ; #DEBUG_LINE_NO:1993
    Self.SendModEvent("DeviceVibrateEffectStart", akActor.GetLeveledActorBase().GetName(), vibStrength as Float * numVibratorsMult) ; #DEBUG_LINE_NO:1994
  EndIf
  Int actorCame = 0 ; #DEBUG_LINE_NO:1996
  Self.Log((("VibrateEffect(" + vibStrength as String) + " for " + duration as String) + "). VibrateMult: " + numVibratorsMult as String, 0) ; #DEBUG_LINE_NO:1997
  String msg = Self.BuildVibrationString(akActor, vibStrength, vPlug, aPlug, vPiercings, nPiercings) ; #DEBUG_LINE_NO:2000
  If vibStrength == 5 ; #DEBUG_LINE_NO:2001
    vibSoundSelect = VibrateVeryStrongSound ; #DEBUG_LINE_NO:2002
  ElseIf vibStrength == 4 ; #DEBUG_LINE_NO:2003
    vibSoundSelect = VibrateStrongSound ; #DEBUG_LINE_NO:2004
  ElseIf vibStrength == 3 ; #DEBUG_LINE_NO:2005
    vibSoundSelect = VibrateStandardSound ; #DEBUG_LINE_NO:2006
  ElseIf vibStrength == 2 ; #DEBUG_LINE_NO:2007
    vibSoundSelect = VibrateWeakSound ; #DEBUG_LINE_NO:2008
  ElseIf vibStrength == 1 ; #DEBUG_LINE_NO:2009
    vibSoundSelect = VibrateVeryWeakSound ; #DEBUG_LINE_NO:2010
  Else
    Self.Error("Vibrator received invalid strength setting.") ; #DEBUG_LINE_NO:2012
    Return -2 ; #DEBUG_LINE_NO:2013
  EndIf
  If !silent && akActor == PlayerRef ; #DEBUG_LINE_NO:2015
    Self.NotifyPlayer(msg, False) ; #DEBUG_LINE_NO:2016
  EndIf
  Float vibSoundVol = Config.VolumeVibrator ; #DEBUG_LINE_NO:2020
  If akActor != PlayerRef ; #DEBUG_LINE_NO:2021
    vibSoundVol = Config.VolumeVibratorNPC ; #DEBUG_LINE_NO:2022
  EndIf
  Int vsID = vibSoundSelect.Play(akActor as ObjectReference) ; #DEBUG_LINE_NO:2024
  Sound.SetInstanceVolume(vsID, vibSoundVol) ; #DEBUG_LINE_NO:2025
  Int msID = MoanSound.Play(akActor as ObjectReference) ; #DEBUG_LINE_NO:2026
  Sound.SetInstanceVolume(msID, Self.GetMoanVolume(akActor, -1)) ; #DEBUG_LINE_NO:2027
  Int timeVibrated = 0 ; #DEBUG_LINE_NO:2028
  Int vibAnimStarted = 0 ; #DEBUG_LINE_NO:2029
  sslBaseExpression expression = SexLab.RandomExpressionByTag("Pleasure") ; #DEBUG_LINE_NO:2033
  Self.ApplyExpression_v2(akActor, expression, 15, Math.Ceiling(Aroused.GetActorExposure(akActor) as Float * 0.600000024), False) ; #DEBUG_LINE_NO:2034
  If Utility.RandomInt(0, 100) <= 10 * vibStrength ; #DEBUG_LINE_NO:2035
    If !EnableVRSupport ; #DEBUG_LINE_NO:2036
      Self.PlayThirdPersonAnimation(akActor, Self.AnimSwitchKeyword(akActor, "Horny"), 3, True) ; #DEBUG_LINE_NO:2037
    EndIf
  EndIf
  Float combatModifier = 1.0 ; #DEBUG_LINE_NO:2042
  If akActor.GetCombatState() != 0 ; #DEBUG_LINE_NO:2043
    combatModifier = 0.5 ; #DEBUG_LINE_NO:2044
  EndIf
  While Self.IsValidActorV2(akActor, AllowActorInScene) && timeVibrated < Self.GetVibrating(akActor) && (akActor.WornHasKeyword(zad_DeviousPlug) || akActor.WornHasKeyword(zad_DeviousPiercingsNipple) || akActor.WornHasKeyword(zad_DeviousPiercingsVaginal)) ; #DEBUG_LINE_NO:2048
    If timeVibrated % 2 == 0 ; #DEBUG_LINE_NO:2050
      akActor.CreateDetectionEvent(akActor, vibStrength * 10) ; #DEBUG_LINE_NO:2052
    EndIf
    If timeVibrated >= 3 && vibStrength >= 3 && Aroused.GetActorExposure(akActor) >= 99 ; #DEBUG_LINE_NO:2059
      Int cumChance = ((vibStrength * 2) as Float * combatModifier) as Int ; #DEBUG_LINE_NO:2060
      Self.Log(("CumChance:" + cumChance as String) + ", vibStrength:" + vibStrength as String, 0) ; #DEBUG_LINE_NO:2061
      If Utility.RandomInt(0, 100) <= cumChance ; #DEBUG_LINE_NO:2062
        If vibAnimStarted != 0 ; #DEBUG_LINE_NO:2066
          Self.EndThirdPersonAnimation(akActor, cameraState, True) ; #DEBUG_LINE_NO:2067
          vibAnimStarted = 0 ; #DEBUG_LINE_NO:2068
        EndIf
        actorCame += 1 ; #DEBUG_LINE_NO:2070
        If teaseOnly ; #DEBUG_LINE_NO:2071
          Self.ApplyExpressionRaw(akActor, zadprebuildedexpressions.GetPrebuildExpression_Orgasm1(), 65, 100, False) ; #DEBUG_LINE_NO:2072
          Sound.StopInstance(vsID) ; #DEBUG_LINE_NO:2073
          Sound.StopInstance(msID) ; #DEBUG_LINE_NO:2074
          Self.StopVibrating(akActor) ; #DEBUG_LINE_NO:2075
          If !silent && akActor == PlayerRef ; #DEBUG_LINE_NO:2076
            Self.NotifyPlayer("The vibrations abruptly stop just short of bringing you to orgasm.", False) ; #DEBUG_LINE_NO:2077
          EndIf
          Self.EdgeActor(akActor) ; #DEBUG_LINE_NO:2079
          Self.ResetExpressionRaw(akActor, 65) ; #DEBUG_LINE_NO:2080
          Self.SendModEvent("DeviceVibrateEffectStop", akActor.GetLeveledActorBase().GetName(), vibStrength as Float * numVibratorsMult) ; #DEBUG_LINE_NO:2081
          Return -1 ; #DEBUG_LINE_NO:2082
        EndIf
        Self.ApplyExpressionRaw(akActor, zadprebuildedexpressions.GetPrebuildExpression_Orgasm2(), 65, 100, False) ; #DEBUG_LINE_NO:2084
        If !silent && akActor == PlayerRef ; #DEBUG_LINE_NO:2085
          Self.NotifyPlayer("The plugs bring you to a thunderous climax.", False) ; #DEBUG_LINE_NO:2086
        EndIf
        Sound.StopInstance(msID) ; #DEBUG_LINE_NO:2088
        Self.ActorOrgasm(akActor, -1, vsID) ; #DEBUG_LINE_NO:2089
        Self.ResetExpressionRaw(akActor, 65) ; #DEBUG_LINE_NO:2090
        If Self.IsVibrating(akActor) ; #DEBUG_LINE_NO:2091
          msID = MoanSound.Play(akActor as ObjectReference) ; #DEBUG_LINE_NO:2092
          Sound.SetInstanceVolume(msID, Self.GetMoanVolume(akActor, -1)) ; #DEBUG_LINE_NO:2093
        EndIf
      EndIf
    EndIf
    If timeVibrated % (6 - vibStrength) == 0 ; #DEBUG_LINE_NO:2101
      Self.UpdateExposure(akActor, 5.0 * combatModifier * numVibratorsMult, True) ; #DEBUG_LINE_NO:2103
      Sound.SetInstanceVolume(msID, Self.GetMoanVolume(akActor, -1)) ; #DEBUG_LINE_NO:2105
    EndIf
    If vibAnimStarted == 0 && Utility.RandomInt(0, 100) <= 3 + vibStrength * 2 && !Self.IsAnimating(akActor) ; #DEBUG_LINE_NO:2111
      Self.ApplyExpression(akActor, expression, (Aroused.GetActorExposure(akActor) as Float * 0.75) as Int, True) ; #DEBUG_LINE_NO:2113
      If !EnableVRSupport ; #DEBUG_LINE_NO:2115
        cameraState = Self.StartThirdPersonAnimation(akActor, Self.AnimSwitchKeyword(akActor, "Horny"), True) ; #DEBUG_LINE_NO:2116
      EndIf
      vibAnimStarted = timeVibrated + 1 ; #DEBUG_LINE_NO:2118
    EndIf
    If vibAnimStarted != 0 && timeVibrated - vibAnimStarted >= 6 ; #DEBUG_LINE_NO:2124
      Self.ApplyExpression_v2(akActor, expression, 15, timeVibrated / duration * 75, False) ; #DEBUG_LINE_NO:2126
      Self.EndThirdPersonAnimation(akActor, cameraState, True) ; #DEBUG_LINE_NO:2127
      vibAnimStarted = 0 ; #DEBUG_LINE_NO:2128
    EndIf
    If akActor == PlayerRef ; #DEBUG_LINE_NO:2134
      
    EndIf
    Utility.Wait(1.0) ; #DEBUG_LINE_NO:2140
    timeVibrated += 1 ; #DEBUG_LINE_NO:2141
  EndWhile
  Sound.StopInstance(vsID) ; #DEBUG_LINE_NO:2144
  Sound.StopInstance(msID) ; #DEBUG_LINE_NO:2145
  If Self.IsValidActorV2(akActor, AllowActorInScene) ; #DEBUG_LINE_NO:2146
    Self.ResetExpressionRaw(akActor, 15) ; #DEBUG_LINE_NO:2148
  EndIf
  If vibAnimStarted != 0 ; #DEBUG_LINE_NO:2153
    Self.EndThirdPersonAnimation(akActor, cameraState, True) ; #DEBUG_LINE_NO:2154
    vibAnimStarted = 0 ; #DEBUG_LINE_NO:2155
  EndIf
  Self.StopVibrating(akActor) ; #DEBUG_LINE_NO:2157
  If !silent && akActor == PlayerRef ; #DEBUG_LINE_NO:2158
    Self.NotifyPlayer(Self.BuildPostVibrationString(akActor, vibStrength, vPlug, aPlug, vPiercings, nPiercings), False) ; #DEBUG_LINE_NO:2159
  EndIf
  Self.SendModEvent("DeviceVibrateEffectStop", akActor.GetLeveledActorBase().GetName(), vibStrength as Float * numVibratorsMult) ; #DEBUG_LINE_NO:2161
  Self.UpdateArousalTimeRate(akActor, vibStrength as Float * numVibratorsMult) ; #DEBUG_LINE_NO:2162
  Aroused.GetActorArousal(akActor) ; #DEBUG_LINE_NO:2163
  Return actorCame ; #DEBUG_LINE_NO:2164
EndFunction

Function AttrDrain(Actor akActor, String attr)
  Float randomize = Utility.RandomInt(1, 75) as Float / 100.0 ; #DEBUG_LINE_NO:2169
  Int drain = (akActor.GetAv(attr) * randomize) as Int ; #DEBUG_LINE_NO:2170
  Self.Log(((attr + "DrainEffect(): Draining " + drain as String) + "(" + randomize as String) + " %)", 0) ; #DEBUG_LINE_NO:2171
  akActor.DamageAv(attr, drain as Float) ; #DEBUG_LINE_NO:2172
EndFunction

Function HealthDrainEffect(Actor akActor)
  Self.AttrDrain(akActor, "Health") ; #DEBUG_LINE_NO:2177
EndFunction

Function ManaDrainEffect(Actor akActor)
  Self.AttrDrain(akActor, "Magicka") ; #DEBUG_LINE_NO:2182
EndFunction

Function StaminaDrainEffect(Actor akActor)
  Self.AttrDrain(akActor, "Stamina") ; #DEBUG_LINE_NO:2187
EndFunction

Bool Function ShouldEdgeActor(Actor akActor)
  If Self.ActorHasKeyword(akActor, zad_EffectEdgeOnly) ; #DEBUG_LINE_NO:2192
    Return True ; #DEBUG_LINE_NO:2193
  ElseIf Self.ActorHasKeyword(akActor, zad_EffectEdgeRandom) && Utility.RandomInt(0, 100) >= 25 ; #DEBUG_LINE_NO:2194
    Return True ; #DEBUG_LINE_NO:2195
  EndIf
  Return False ; #DEBUG_LINE_NO:2197
EndFunction

Bool Function ActorHasKeyword(Actor akActor, Keyword kwd)
  Return akActor.HasMagicEffectWithKeyword(kwd) || akActor.WornHasKeyword(kwd) ; #DEBUG_LINE_NO:2201
EndFunction

Function ApplyMagickaPenalty(Actor akActor)
  zad_splMagickaPenalty.RemoteCast(akActor as ObjectReference, akActor, akActor as ObjectReference) ; #DEBUG_LINE_NO:2205
  Float currentMag = akActor.GetActorValue("Magicka") ; #DEBUG_LINE_NO:2206
  akActor.DamageActorValue("Magicka", currentMag * 0.25) ; #DEBUG_LINE_NO:2207
EndFunction

Function SpellCastVibrate(Actor akActor, Form tmp)
  Spell theSpell = tmp as Spell ; #DEBUG_LINE_NO:2211
  If akActor.WornHasKeyword(zad_DeviousPlug) && Self.ActorHasKeyword(akActor, zad_EffectVibrateOnSpellCast) ; #DEBUG_LINE_NO:2212
    Self.SendModEvent("EventOnCast", "", 0.0) ; #DEBUG_LINE_NO:2213
    Self.Log("OnSpellCast()", 0) ; #DEBUG_LINE_NO:2214
    If akActor == PlayerRef && akActor.GetCombatState() >= 1 ; #DEBUG_LINE_NO:2215
      If Utility.GetCurrentRealTime() > SpellCastVibrateCooldown ; #DEBUG_LINE_NO:2217
        SpellCastVibrateCooldown = Utility.GetCurrentRealTime() + 60.0 ; #DEBUG_LINE_NO:2218
      Else
        Return  ; #DEBUG_LINE_NO:2220
      EndIf
      Self.Log("Player is in combat while trying to cast spell: Applying magicka penalty.", 0) ; #DEBUG_LINE_NO:2222
      Self.ApplyMagickaPenalty(akActor) ; #DEBUG_LINE_NO:2224
      Return  ; #DEBUG_LINE_NO:2225
    EndIf
    Shout isShout = tmp as Shout ; #DEBUG_LINE_NO:2227
    Int cost = theSpell.GetEffectiveMagickaCost(akActor) ; #DEBUG_LINE_NO:2229
    If cost <= 1 && !isShout ; #DEBUG_LINE_NO:2230
      Return  ; #DEBUG_LINE_NO:2231
    EndIf
    Int duration = (cost as Float * 0.100000001) as Int ; #DEBUG_LINE_NO:2233
    Int vibStrength = cost / 50 ; #DEBUG_LINE_NO:2234
    If isShout != None || cost > 500 ; #DEBUG_LINE_NO:2235
      Self.Log("Spell is in fact a shout.", 0) ; #DEBUG_LINE_NO:2237
      vibStrength = 2 ; #DEBUG_LINE_NO:2238
      duration = 5 ; #DEBUG_LINE_NO:2239
    EndIf
    If Self.ActorHasKeyword(akActor, zad_EffectTrainOnSpellCast) ; #DEBUG_LINE_NO:2241
      Self.TrainingViolation(akActor, "SpellCast") ; #DEBUG_LINE_NO:2242
      If vibStrength <= 1 ; #DEBUG_LINE_NO:2243
        vibStrength = 3 ; #DEBUG_LINE_NO:2244
      Else
        vibStrength *= 2 ; #DEBUG_LINE_NO:2246
      EndIf
      duration /= 2 ; #DEBUG_LINE_NO:2248
    EndIf
    If vibStrength > 5 ; #DEBUG_LINE_NO:2250
      vibStrength = 5 ; #DEBUG_LINE_NO:2251
    EndIf
    If vibStrength < 1 ; #DEBUG_LINE_NO:2253
      vibStrength = 1 ; #DEBUG_LINE_NO:2254
    EndIf
    If duration <= 0 ; #DEBUG_LINE_NO:2256
      duration = 1 ; #DEBUG_LINE_NO:2257
    EndIf
    If Utility.RandomInt(0, 100) <= vibStrength * 15 ; #DEBUG_LINE_NO:2260
      
    EndIf
    Self.VibrateEffect(akActor, vibStrength, duration, Self.ShouldEdgeActor(akActor), False) ; #DEBUG_LINE_NO:2265
  EndIf
EndFunction

Bool Function IsValidActor(Actor akActor)
  Return Self.IsValidActorV2(akActor, False) ; #DEBUG_LINE_NO:2272
EndFunction

Bool Function IsValidActorV2(Actor akActor, Bool OverrideSceneCheck)
  If !akActor.Is3DLoaded() || akActor.isChild() || akActor.IsDisabled() || akActor.IsDead() ; #DEBUG_LINE_NO:2277
    Return False ; #DEBUG_LINE_NO:2278
  EndIf
  If akActor.GetCurrentScene() != None && OverrideSceneCheck == False ; #DEBUG_LINE_NO:2280
    Return False ; #DEBUG_LINE_NO:2281
  EndIf
  Return True ; #DEBUG_LINE_NO:2283
EndFunction

Function DisableControls()
  Game.DisablePlayerControls(True, True, False, False, False, True, True, False, 0) ; #DEBUG_LINE_NO:2287
EndFunction

Function UpdateControls()
  Self.Log("UpdateControls()", 0) ; #DEBUG_LINE_NO:2291
  Self.SendDDFunctionEvent("UpdateControls", PlayerRef) ; #DEBUG_LINE_NO:2292
  Bool movement = True ; #DEBUG_LINE_NO:2294
  Bool fighting = True ; #DEBUG_LINE_NO:2295
  Bool sneaking = True ; #DEBUG_LINE_NO:2296
  Bool menu = True ; #DEBUG_LINE_NO:2297
  Bool activate = True ; #DEBUG_LINE_NO:2298
  Int cameraState = Game.GetCameraState() ; #DEBUG_LINE_NO:2299
  If PlayerRef.WornHasKeyword(zad_DeviousBlindfold) && (Config.BlindfoldMode == 1 || Config.BlindfoldMode == 0) && (cameraState == 8 || cameraState == 9) ; #DEBUG_LINE_NO:2300
    movement = False ; #DEBUG_LINE_NO:2301
    sneaking = False ; #DEBUG_LINE_NO:2302
  EndIf
  If Self.IsBound(PlayerRef) ; #DEBUG_LINE_NO:2304
    If PlayerRef.WornHasKeyword(zad_BoundCombatDisableKick) ; #DEBUG_LINE_NO:2305
      fighting = False ; #DEBUG_LINE_NO:2306
    Else
      fighting = Config.UseBoundCombat ; #DEBUG_LINE_NO:2308
    EndIf
  EndIf
  If PlayerRef.WornHasKeyword(zad_DeviousPetSuit) ; #DEBUG_LINE_NO:2311
    sneaking = False ; #DEBUG_LINE_NO:2312
  EndIf
  If PlayerRef.WornHasKeyword(zad_DeviousPonyGear) ; #DEBUG_LINE_NO:2314
    sneaking = False ; #DEBUG_LINE_NO:2315
  EndIf
  Game.DisablePlayerControls(!movement, !fighting, False, False, !sneaking, !menu, !activate, False, 0) ; #DEBUG_LINE_NO:2317
  Game.EnablePlayerControls(movement, fighting, True, True, sneaking, menu, activate, True, 0) ; #DEBUG_LINE_NO:2318
EndFunction

Bool Function IsAnimating(Actor akActor)
  If akActor.GetSitState() != 0 || akActor.IsOnMount() ; #DEBUG_LINE_NO:2326
    Return True ; #DEBUG_LINE_NO:2327
  EndIf
  Return akActor.IsInFaction(zadAnimatingFaction) || akActor.IsInFaction(SexLab.AnimatingFaction) ; #DEBUG_LINE_NO:2329
EndFunction

Function SetAnimating(Actor akActor, Bool IsAnimating)
  If IsAnimating ; #DEBUG_LINE_NO:2334
    akActor.AddToFaction(zadAnimatingFaction) ; #DEBUG_LINE_NO:2335
    akActor.SetFactionRank(zadAnimatingFaction, 1) ; #DEBUG_LINE_NO:2336
  Else
    akActor.RemoveFromFaction(zadAnimatingFaction) ; #DEBUG_LINE_NO:2338
  EndIf
EndFunction

Bool Function IsVibrating(Actor akActor)
  Return akActor.IsInFaction(zadVibratorFaction) ; #DEBUG_LINE_NO:2344
EndFunction

Function SetVibrating(Actor akActor, Int duration)
  Bool oor = False ; #DEBUG_LINE_NO:2348
  If duration < 0 ; #DEBUG_LINE_NO:2349
    oor = True ; #DEBUG_LINE_NO:2350
    duration = 1 ; #DEBUG_LINE_NO:2351
  ElseIf duration >= 127 ; #DEBUG_LINE_NO:2352
    oor = True ; #DEBUG_LINE_NO:2353
    duration = 126 ; #DEBUG_LINE_NO:2354
  EndIf
  If oor ; #DEBUG_LINE_NO:2356
    Self.Warn(("SetVibrating(" + duration as String) + ") received out of range value") ; #DEBUG_LINE_NO:2357
  EndIf
  akActor.SetFactionRank(zadVibratorFaction, duration) ; #DEBUG_LINE_NO:2360
EndFunction

Function StopVibrating(Actor akActor)
  akActor.SetFactionRank(zadVibratorFaction, 0) ; #DEBUG_LINE_NO:2365
  akActor.RemoveFromFaction(zadVibratorFaction) ; #DEBUG_LINE_NO:2366
EndFunction

Int Function GetVibrating(Actor akActor)
  If !akActor.IsInFaction(zadVibratorFaction) ; #DEBUG_LINE_NO:2371
    Return 0 ; #DEBUG_LINE_NO:2372
  EndIf
  Return akActor.GetFactionRank(zadVibratorFaction) ; #DEBUG_LINE_NO:2374
EndFunction

Function ApplyGagEffect(Actor akActor)
  ExpLibs.ApplyGagEffect(akActor) ; #DEBUG_LINE_NO:2378
EndFunction

Function RemoveGagEffect(Actor akActor)
  ExpLibs.RemoveGagEffect(akActor) ; #DEBUG_LINE_NO:2382
EndFunction

Function SendGagEffectEvent(Actor akActor, Bool isRemove)
  Int Handle = modevent.Create("DDI_GagExpressionStateChange") ; #DEBUG_LINE_NO:2386
  If Handle ; #DEBUG_LINE_NO:2387
    modevent.PushForm(Handle, akActor as Form) ; #DEBUG_LINE_NO:2388
    modevent.PushBool(Handle, isRemove) ; #DEBUG_LINE_NO:2389
    modevent.Send(Handle) ; #DEBUG_LINE_NO:2390
  EndIf
EndFunction

String Function MakeSingularIfPlural(String theString)
  Int len = stringutil.getLength(theString) ; #DEBUG_LINE_NO:2395
  If stringutil.GetNthChar(theString, len - 1) == "S" ; #DEBUG_LINE_NO:2396
    Return stringutil.Substring(theString, 0, len - 1) ; #DEBUG_LINE_NO:2397
  EndIf
  Return theString ; #DEBUG_LINE_NO:2399
EndFunction

String Function AnimSwitchKeyword(Actor akActor, String idleName)
  If idleName == "Horny" || idleName == "Horny01" || idleName == "Horny02" || idleName == "Horny03" ; #DEBUG_LINE_NO:2406
    Return "DDZazHornyA" ; #DEBUG_LINE_NO:2407
  ElseIf idleName == "Edged" ; #DEBUG_LINE_NO:2408
    Return "DDZazHornyD" ; #DEBUG_LINE_NO:2409
  ElseIf idleName == "Orgasm" ; #DEBUG_LINE_NO:2410
    Return "DDZazHornyE" ; #DEBUG_LINE_NO:2411
  ElseIf idleName == "OutOfBreath" ; #DEBUG_LINE_NO:2412
    Return "ft_out_of_breath_reg" ; #DEBUG_LINE_NO:2413
  EndIf
  Self.Error("Failed to find valid animation for presentation.") ; #DEBUG_LINE_NO:2415
EndFunction

Function RepopulateNpcs()
  If RepopulateMutex ; #DEBUG_LINE_NO:2419
    Self.Log("RepopulateNpcs() is already processing.", 0) ; #DEBUG_LINE_NO:2420
    Return  ; #DEBUG_LINE_NO:2421
  EndIf
  RepopulateMutex = True ; #DEBUG_LINE_NO:2423
  Self.Log("RepopulateNpcs()", 0) ; #DEBUG_LINE_NO:2424
  If Utility.GetCurrentRealTime() - lastRepopulateTime <= 5.0 ; #DEBUG_LINE_NO:2425
    Self.Log("Aborting repopulation of NPC slots: Hit throttle.", 0) ; #DEBUG_LINE_NO:2426
    RepopulateMutex = False ; #DEBUG_LINE_NO:2427
    Return  ; #DEBUG_LINE_NO:2428
  EndIf
  lastRepopulateTime = Utility.GetCurrentRealTime() ; #DEBUG_LINE_NO:2430
  If zadNPCQuest.IsProcessing ; #DEBUG_LINE_NO:2431
    Self.Log("Waiting, since NPC Events is currently processing.", 0) ; #DEBUG_LINE_NO:2432
    Int timeout = 0 ; #DEBUG_LINE_NO:2433
    While zadNPCQuest.IsProcessing && timeout <= 24 ; #DEBUG_LINE_NO:2434
      Utility.Wait(5.0) ; #DEBUG_LINE_NO:2435
      timeout += 1 ; #DEBUG_LINE_NO:2436
    EndWhile
    If timeout >= 24 ; #DEBUG_LINE_NO:2438
      Self.Warn("RepopulateNpcs() spinlock timed out!!") ; #DEBUG_LINE_NO:2439
      zadNPCQuest.IsProcessing = False ; #DEBUG_LINE_NO:2440
    EndIf
  EndIf
  If !zadNPCSlots.IsStopping() && !zadNPCSlots.IsStarting() ; #DEBUG_LINE_NO:2443
    If zadNPCSlots.IsRunning() ; #DEBUG_LINE_NO:2444
      zadNPCSlots.Stop() ; #DEBUG_LINE_NO:2445
    EndIf
    If Config.NumNpcs > 0 ; #DEBUG_LINE_NO:2447
      Utility.Wait(2.0) ; #DEBUG_LINE_NO:2450
      zadNPCSlots.Start() ; #DEBUG_LINE_NO:2451
    Else
      Self.Log("Not repopulating NPC slots: Feature is disabled.", 0) ; #DEBUG_LINE_NO:2453
    EndIf
  Else
    Self.Warn("Not repopulating NPC slots: Quest is changing state.") ; #DEBUG_LINE_NO:2456
  EndIf
  RepopulateMutex = False ; #DEBUG_LINE_NO:2458
EndFunction

Function PlugPanelGag(Actor akActor)
  If akActor.GetFactionRank(zadGagPanelFaction) <= -1 ; #DEBUG_LINE_NO:2462
    Self.Error("PlugPanelGag called on actor that is not wearing a panel gag.") ; #DEBUG_LINE_NO:2463
    Return  ; #DEBUG_LINE_NO:2464
  EndIf
  Self.SendModEvent("GagPanelStateChange", akActor.GetLeveledActorBase().GetName(), 1.0) ; #DEBUG_LINE_NO:2466
  akActor.RemoveItem(zad_gagPanelPlug as Form, 1, False, None) ; #DEBUG_LINE_NO:2467
  akActor.SetFactionRank(zadGagPanelFaction, 1) ; #DEBUG_LINE_NO:2468
EndFunction

Function UnPlugPanelGag(Actor akActor)
  If akActor.GetFactionRank(zadGagPanelFaction) <= -1 ; #DEBUG_LINE_NO:2472
    Self.Error("UnPlugPanelGag called on actor that is not wearing a panel gag.") ; #DEBUG_LINE_NO:2473
    Return  ; #DEBUG_LINE_NO:2474
  EndIf
  Self.SendModEvent("GagPanelStateChange", akActor.GetLeveledActorBase().GetName(), 0.0) ; #DEBUG_LINE_NO:2476
  akActor.AddItem(zad_gagPanelPlug as Form, 1, False) ; #DEBUG_LINE_NO:2477
  akActor.SetFactionRank(zadGagPanelFaction, 0) ; #DEBUG_LINE_NO:2478
EndFunction

Bool Function IsBound(Actor akActor)
  Return akActor.WornHasKeyword(zad_DeviousHeavyBondage) || akActor.WornHasKeyword(zad_DeviousArmbinder) || akActor.WornHasKeyword(zad_DeviousArmbinderElbow) || akActor.WornHasKeyword(zad_DeviousYoke) || akActor.WornHasKeyword(zad_DeviousYokeBB) || akActor.WornHasKeyword(zad_DeviousStraitJacket) ; #DEBUG_LINE_NO:2483
EndFunction

Bool Function NeedsBoundAnim(Actor akActor)
  Return akActor.WornHasKeyword(zad_DeviousCuffsFront) || akActor.WornHasKeyword(zad_DeviousElbowTie) || akActor.WornHasKeyword(zad_DeviousArmbinder) || akActor.WornHasKeyword(zad_DeviousArmbinderElbow) || akActor.WornHasKeyword(zad_DeviousYoke) || akActor.WornHasKeyword(zad_DeviousYokeBB) || akActor.WornHasKeyword(zad_DeviousPetSuit) ; #DEBUG_LINE_NO:2487
EndFunction

Function ToggleCompass(Bool show)
  Float alpha = 0.0 ; #DEBUG_LINE_NO:2491
  If show ; #DEBUG_LINE_NO:2492
    alpha = 100.0 ; #DEBUG_LINE_NO:2493
  EndIf
  ui.SetNumber("HUD Menu", "_root.HUDMovieBaseInstance.CompassShoutMeterHolder._alpha", alpha) ; #DEBUG_LINE_NO:2496
  ui.SetNumber("HUD Menu", "_root.HUDMovieBaseInstance.CompassShoutMeterHolder.Compass.DirectionRect._alpha", alpha) ; #DEBUG_LINE_NO:2497
  ui.SetNumber("HUD Menu", "_root.HUDMovieBaseInstance.CompassShoutMeterHolder.Compass.CompassFrame._alpha", alpha) ; #DEBUG_LINE_NO:2498
EndFunction

Function TrainingViolation(Actor akActor, String violation)
  If akActor == PlayerRef ; #DEBUG_LINE_NO:2502
    Self.Log("TrainingViolation(player, " + violation + ")", 0) ; #DEBUG_LINE_NO:2503
    Self.SendModEvent("TrainingViolation", violation, 0.0) ; #DEBUG_LINE_NO:2504
  EndIf
EndFunction

Function ShockActor(Actor akActor)
  If akActor == PlayerRef ; #DEBUG_LINE_NO:2510
    Self.NotifyPlayer("The plugs within you let out a powerful electrical shock!", False) ; #DEBUG_LINE_NO:2511
    Self.Log("zadLibs: ShockActor", 0) ; #DEBUG_LINE_NO:2512
  Else
    Self.NotifyNPC(akActor.GetLeveledActorBase().GetName() + " squirms uncomfortably as electricity runs through her.", False) ; #DEBUG_LINE_NO:2514
  EndIf
  ShockEffect.RemoteCast(akActor as ObjectReference, akActor, akActor as ObjectReference) ; #DEBUG_LINE_NO:2516
  Aroused.SetActorExposure(akActor, Utility.RandomInt(10, 20)) ; #DEBUG_LINE_NO:2517
EndFunction

Form Function GetRenderedDeviceInstance(Actor akActor, Int Slot, Keyword kwd)
  Form f1 = akActor.GetWornForm(Slot) ; #DEBUG_LINE_NO:2522
  If f1 as Bool && f1.HasKeyWord(kwd) ; #DEBUG_LINE_NO:2523
    Return f1 ; #DEBUG_LINE_NO:2524
  EndIf
  Return None ; #DEBUG_LINE_NO:2526
EndFunction

Form Function GetWornHeavyBondageInstance(Actor akActor)
  Return Self.GetRenderedDeviceInstance(akActor, 65536, zad_DeviousHeavyBondage) ; #DEBUG_LINE_NO:2533
EndFunction

Function UpdateArousalTimeRate(Actor akActor, Float val)
  If Aroused.GetActorTimerate(akActor) <= 50.0 ; #DEBUG_LINE_NO:2538
    Aroused.UpdateActorTimeRate(akActor, val) ; #DEBUG_LINE_NO:2539
  EndIf
EndFunction

Function ChastityBeltStruggle(Actor akActor)
  If !akActor.WornHasKeyword(zad_DeviousBelt) || akActor.WornHasKeyword(zad_DeviousHeavyBondage) || BoundCombat.HasCompatibleDevice(akActor) ; #DEBUG_LINE_NO:2545
    Return  ; #DEBUG_LINE_NO:2546
  EndIf
  Utility.Wait(0.100000001) ; #DEBUG_LINE_NO:2549
  If Self.IsAnimating(akActor) ; #DEBUG_LINE_NO:2551
    Return  ; #DEBUG_LINE_NO:2552
  EndIf
  If akActor == PlayerRef && PlayerRef.IsInCombat() ; #DEBUG_LINE_NO:2555
    Return  ; #DEBUG_LINE_NO:2556
  EndIf
  If EnableVRSupport ; #DEBUG_LINE_NO:2560
    Self.PlayThirdPersonAnimation(akActor, "DDChastityBeltStruggle0" + Utility.RandomInt(1, 2) as String, Utility.RandomInt(3, 4), True) ; #DEBUG_LINE_NO:2561
  Else
    Self.PlayThirdPersonAnimation(akActor, "DDChastityBeltStruggle0" + Utility.RandomInt(1, 2) as String, Utility.RandomInt(5, 30), True) ; #DEBUG_LINE_NO:2563
  EndIf
  Aroused.UpdateActorExposure(akActor, 5, "") ; #DEBUG_LINE_NO:2565
  If akActor == PlayerRef ; #DEBUG_LINE_NO:2566
    Self.notify("You tug at your chastity belt, but it won't come off!", False) ; #DEBUG_LINE_NO:2567
  EndIf
  Self.SexlabMoan(akActor, -1, None) ; #DEBUG_LINE_NO:2569
  If Aroused.GetActorExposure(akActor) > 75 ; #DEBUG_LINE_NO:2570
    Self.PlayHornyAnimation(akActor) ; #DEBUG_LINE_NO:2571
    If akActor == PlayerRef ; #DEBUG_LINE_NO:2572
      Self.notify("You are incredibly horny and try to force a finger inside your belt.", False) ; #DEBUG_LINE_NO:2573
    EndIf
  EndIf
EndFunction

Function Trip(Actor akActor)
  Utility.Wait(0.100000001) ; #DEBUG_LINE_NO:2580
  If Self.IsAnimating(akActor) ; #DEBUG_LINE_NO:2582
    Return  ; #DEBUG_LINE_NO:2583
  EndIf
  If akActor == PlayerRef && PlayerRef.IsInCombat() ; #DEBUG_LINE_NO:2586
    Return  ; #DEBUG_LINE_NO:2587
  EndIf
  If EnableVRSupport ; #DEBUG_LINE_NO:2593
    Self.PlayThirdPersonAnimation(akActor, "ft_fall_over_reg_1", 2, True) ; #DEBUG_LINE_NO:2594
  Else
    Self.PlayThirdPersonAnimation(akActor, "ft_fall_over_reg_1", 8, True) ; #DEBUG_LINE_NO:2596
  EndIf
  Self.SexlabMoan(akActor, -1, None) ; #DEBUG_LINE_NO:2599
EndFunction

Function CatchBreath(Actor akActor)
  Utility.Wait(0.100000001) ; #DEBUG_LINE_NO:2604
  If Self.IsAnimating(akActor) ; #DEBUG_LINE_NO:2606
    Return  ; #DEBUG_LINE_NO:2607
  EndIf
  If akActor == PlayerRef && PlayerRef.IsInCombat() ; #DEBUG_LINE_NO:2610
    Return  ; #DEBUG_LINE_NO:2611
  EndIf
  If EnableVRSupport ; #DEBUG_LINE_NO:2617
    Self.PlayThirdPersonAnimation(akActor, "ft_out_of_breath_reg", 2, True) ; #DEBUG_LINE_NO:2618
  Else
    Self.PlayThirdPersonAnimation(akActor, "ft_out_of_breath_reg", 5, True) ; #DEBUG_LINE_NO:2620
  EndIf
  Self.SexlabMoan(akActor, -1, None) ; #DEBUG_LINE_NO:2623
EndFunction

Function ApplyBoundAnim(Actor akActor, Idle theIdle)
  Self.Log("ApplyBoundAnim()", 0) ; #DEBUG_LINE_NO:2629
  If akActor.GetEquippedWeapon(False) ; #DEBUG_LINE_NO:2630
    akActor.UnequipItem(akActor.GetEquippedWeapon(False) as Form, False, True) ; #DEBUG_LINE_NO:2631
  EndIf
  If akActor.GetEquippedWeapon(True) ; #DEBUG_LINE_NO:2633
    akActor.UnequipItem(akActor.GetEquippedWeapon(True) as Form, False, True) ; #DEBUG_LINE_NO:2634
  EndIf
  If akActor.GetEquippedShield() ; #DEBUG_LINE_NO:2636
    akActor.UnequipItem(akActor.GetEquippedShield() as Form, False, True) ; #DEBUG_LINE_NO:2637
  EndIf
  If akActor.GetEquippedSpell(0) ; #DEBUG_LINE_NO:2639
    akActor.UnequipSpell(akActor.GetEquippedSpell(0), 0) ; #DEBUG_LINE_NO:2640
  EndIf
  If akActor.GetEquippedSpell(1) ; #DEBUG_LINE_NO:2642
    akActor.UnequipSpell(akActor.GetEquippedSpell(1), 1) ; #DEBUG_LINE_NO:2643
  EndIf
  If akActor.IsWeaponDrawn() ; #DEBUG_LINE_NO:2645
    If Config.UseBoundCombat ; #DEBUG_LINE_NO:2646
      Return  ; #DEBUG_LINE_NO:2647
    EndIf
    akActor.SheatheWeapon() ; #DEBUG_LINE_NO:2649
  EndIf
  If akActor.IsOnMount() ; #DEBUG_LINE_NO:2651
    akActor.Dismount() ; #DEBUG_LINE_NO:2652
  EndIf
EndFunction

Function PauseDialogue()
  DialogueGagDisable.Mod(1.0) ; #DEBUG_LINE_NO:2660
  DialogueArmbinderDisable.Mod(1.0) ; #DEBUG_LINE_NO:2661
EndFunction

Function ResumeDialogue()
  DialogueGagDisable.Mod(-1.0) ; #DEBUG_LINE_NO:2669
  DialogueArmbinderDisable.Mod(-1.0) ; #DEBUG_LINE_NO:2670
EndFunction

Function ResetDialogue()
  DialogueGagDisable.SetValueInt(0) ; #DEBUG_LINE_NO:2679
  DialogueArmbinderDisable.SetValueInt(0) ; #DEBUG_LINE_NO:2680
EndFunction

Function AddToDisableDialogueFaction(Actor akActor)
  akActor.AddToFaction(zadDisableDialogueFaction) ; #DEBUG_LINE_NO:2687
EndFunction

Function RemoveFromDisableDialogueFaction(Actor akActor)
  akActor.RemoveFromFaction(zadDisableDialogueFaction) ; #DEBUG_LINE_NO:2694
EndFunction

Function HideBreasts(Actor akActor)
  If Config.BreastNodeManagement ; #DEBUG_LINE_NO:2699
    Bool gender = akActor.GetLeveledActorBase().GetSex() == 1 ; #DEBUG_LINE_NO:2700
    Self.SetNodeHidden(akActor, gender, "NPC L Breast", True, "DDi") ; #DEBUG_LINE_NO:2701
    Self.SetNodeHidden(akActor, gender, "NPC R Breast", True, "DDi") ; #DEBUG_LINE_NO:2702
    If netimmerse.HasNode(akActor as ObjectReference, "3BA", False) || netimmerse.HasNode(akActor as ObjectReference, "3BBB", False) ; #DEBUG_LINE_NO:2703
      Self.SetNodeHidden(akActor, gender, "L Breast01", True, "DDi") ; #DEBUG_LINE_NO:2704
      Self.SetNodeHidden(akActor, gender, "L Breast02", True, "DDi") ; #DEBUG_LINE_NO:2705
      Self.SetNodeHidden(akActor, gender, "L Breast03", True, "DDi") ; #DEBUG_LINE_NO:2706
      Self.SetNodeHidden(akActor, gender, "R Breast01", True, "DDi") ; #DEBUG_LINE_NO:2707
      Self.SetNodeHidden(akActor, gender, "R Breast02", True, "DDi") ; #DEBUG_LINE_NO:2708
      Self.SetNodeHidden(akActor, gender, "R Breast03", True, "DDi") ; #DEBUG_LINE_NO:2709
    EndIf
  EndIf
EndFunction

Function ShowBreasts(Actor akActor)
  Bool gender = akActor.GetLeveledActorBase().GetSex() == 1 ; #DEBUG_LINE_NO:2720
  Self.SetNodeHidden(akActor, gender, "NPC L Breast", False, "DDi") ; #DEBUG_LINE_NO:2721
  Self.SetNodeHidden(akActor, gender, "NPC R Breast", False, "DDi") ; #DEBUG_LINE_NO:2722
  If netimmerse.HasNode(akActor as ObjectReference, "3BA", False) || netimmerse.HasNode(akActor as ObjectReference, "3BBB", False) ; #DEBUG_LINE_NO:2723
    Self.SetNodeHidden(akActor, gender, "L Breast01", False, "DDi") ; #DEBUG_LINE_NO:2724
    Self.SetNodeHidden(akActor, gender, "L Breast02", False, "DDi") ; #DEBUG_LINE_NO:2725
    Self.SetNodeHidden(akActor, gender, "L Breast03", False, "DDi") ; #DEBUG_LINE_NO:2726
    Self.SetNodeHidden(akActor, gender, "R Breast01", False, "DDi") ; #DEBUG_LINE_NO:2727
    Self.SetNodeHidden(akActor, gender, "R Breast02", False, "DDi") ; #DEBUG_LINE_NO:2728
    Self.SetNodeHidden(akActor, gender, "R Breast03", False, "DDi") ; #DEBUG_LINE_NO:2729
  EndIf
EndFunction

Function HideBelly(Actor akActor)
  If Config.BellyNodeManagement ; #DEBUG_LINE_NO:2735
    Bool gender = akActor.GetLeveledActorBase().GetSex() == 1 ; #DEBUG_LINE_NO:2736
    Self.SetNodeHidden(akActor, gender, "NPC Belly", True, "DDi") ; #DEBUG_LINE_NO:2737
  EndIf
EndFunction

Function ShowBelly(Actor akActor)
  Bool gender = akActor.GetLeveledActorBase().GetSex() == 1 ; #DEBUG_LINE_NO:2744
  Self.SetNodeHidden(akActor, gender, "NPC Belly", False, "DDi") ; #DEBUG_LINE_NO:2745
EndFunction

Function SetNodeHidden(Actor akActor, Bool isFemale, String nodeName, Bool value, String modkey)
  If Config.GotSLIF ; #DEBUG_LINE_NO:2749
    If value ; #DEBUG_LINE_NO:2750
      slif_main.hideNode(akActor, "Devious Devices Integration", nodeName, 0.01, modkey) ; #DEBUG_LINE_NO:2751
    Else
      slif_main.showNode(akActor, "Devious Devices Integration", nodeName) ; #DEBUG_LINE_NO:2753
    EndIf
  ElseIf value
    xpmselib.SetNodeScale(akActor, isFemale, nodeName, 0.01, modkey) ; #DEBUG_LINE_NO:2757
  Else
    xpmselib.SetNodeScale(akActor, isFemale, nodeName, 1.0, modkey) ; #DEBUG_LINE_NO:2759
  EndIf
EndFunction

String Function LookupDeviceType(Keyword kwd)
  If kwd == zad_DeviousPlug ; #DEBUG_LINE_NO:2765
    Return "Plug" ; #DEBUG_LINE_NO:2766
  ElseIf kwd == zad_DeviousHeavyBondage ; #DEBUG_LINE_NO:2767
    Return "WristRestraint" ; #DEBUG_LINE_NO:2768
  ElseIf kwd == zad_DeviousBelt ; #DEBUG_LINE_NO:2769
    Return "Belt" ; #DEBUG_LINE_NO:2770
  ElseIf kwd == zad_DeviousBra ; #DEBUG_LINE_NO:2771
    Return "Bra" ; #DEBUG_LINE_NO:2772
  ElseIf kwd == zad_DeviousCollar ; #DEBUG_LINE_NO:2773
    Return "Collar" ; #DEBUG_LINE_NO:2774
  ElseIf kwd == zad_DeviousArmCuffs ; #DEBUG_LINE_NO:2775
    Return "ArmCuffs" ; #DEBUG_LINE_NO:2776
  ElseIf kwd == zad_DeviousLegCuffs ; #DEBUG_LINE_NO:2777
    Return "LegCuffs" ; #DEBUG_LINE_NO:2778
  ElseIf kwd == zad_DeviousArmbinder ; #DEBUG_LINE_NO:2779
    Return "armbinder" ; #DEBUG_LINE_NO:2780
  ElseIf kwd == zad_DeviousYoke ; #DEBUG_LINE_NO:2781
    Return "yoke" ; #DEBUG_LINE_NO:2782
  ElseIf kwd == zad_DeviousCorset ; #DEBUG_LINE_NO:2783
    Return "corset" ; #DEBUG_LINE_NO:2784
  ElseIf kwd == zad_DeviousClamps ; #DEBUG_LINE_NO:2785
    Return "Clamps" ; #DEBUG_LINE_NO:2786
  ElseIf kwd == zad_DeviousGloves ; #DEBUG_LINE_NO:2787
    Return "Gloves" ; #DEBUG_LINE_NO:2788
  ElseIf kwd == zad_DeviousHood ; #DEBUG_LINE_NO:2789
    Return "Hood" ; #DEBUG_LINE_NO:2790
  ElseIf kwd == zad_DeviousSuit ; #DEBUG_LINE_NO:2791
    Return "Suit" ; #DEBUG_LINE_NO:2792
  ElseIf kwd == zad_DeviousGag ; #DEBUG_LINE_NO:2793
    Return "Gag" ; #DEBUG_LINE_NO:2794
  ElseIf kwd == zad_DeviousGagPanel ; #DEBUG_LINE_NO:2795
    Return "gagPanel" ; #DEBUG_LINE_NO:2796
  ElseIf kwd == zad_DeviousPlugVaginal ; #DEBUG_LINE_NO:2797
    Return "PlugVaginal" ; #DEBUG_LINE_NO:2798
  ElseIf kwd == zad_DeviousPlugAnal ; #DEBUG_LINE_NO:2799
    Return "PlugAnal" ; #DEBUG_LINE_NO:2800
  ElseIf kwd == zad_DeviousHarness ; #DEBUG_LINE_NO:2801
    Return "Harness" ; #DEBUG_LINE_NO:2802
  ElseIf kwd == zad_DeviousBlindfold ; #DEBUG_LINE_NO:2803
    Return "blindfold" ; #DEBUG_LINE_NO:2804
  ElseIf kwd == zad_DeviousBoots ; #DEBUG_LINE_NO:2805
    Return "Boots" ; #DEBUG_LINE_NO:2806
  ElseIf kwd == zad_DeviousPiercingsNipple ; #DEBUG_LINE_NO:2807
    Return "PiercingsNipple" ; #DEBUG_LINE_NO:2808
  ElseIf kwd == zad_DeviousPiercingsVaginal ; #DEBUG_LINE_NO:2809
    Return "PiercingsVaginal" ; #DEBUG_LINE_NO:2810
  ElseIf kwd == zad_DeviousArmbinderElbow ; #DEBUG_LINE_NO:2811
    Return "ArmBinderElbow" ; #DEBUG_LINE_NO:2812
  ElseIf kwd == zad_DeviousYokeBB ; #DEBUG_LINE_NO:2813
    Return "YokeBB" ; #DEBUG_LINE_NO:2814
  ElseIf kwd == zad_DeviousCuffsFront ; #DEBUG_LINE_NO:2815
    Return "CuffsFront" ; #DEBUG_LINE_NO:2816
  ElseIf kwd == zad_DeviousStraitJacket ; #DEBUG_LINE_NO:2817
    Return "StraitJacket" ; #DEBUG_LINE_NO:2818
  ElseIf kwd == zad_DeviousBondageMittens ; #DEBUG_LINE_NO:2819
    Return "Mittens" ; #DEBUG_LINE_NO:2820
  ElseIf kwd == zad_DeviousHobbleSkirt ; #DEBUG_LINE_NO:2821
    Return "HobbleSkirt" ; #DEBUG_LINE_NO:2822
  ElseIf kwd == zad_DeviousHobbleSkirtRelaxed ; #DEBUG_LINE_NO:2823
    Return "HobbleSkirt" ; #DEBUG_LINE_NO:2824
  EndIf
  Self.Log("LookupDeviceType received invalid keyword " + kwd as String, 0) ; #DEBUG_LINE_NO:2826
EndFunction

Function strip(Actor a, Bool animate)
  Self.stripweapons(a, True) ; #DEBUG_LINE_NO:2835
  Form frm = a.GetWornForm(4096) ; #DEBUG_LINE_NO:2836
  If frm as Bool && !frm.HasKeyWordString("SexLabNoStrip") ; #DEBUG_LINE_NO:2837
    a.UnequipItem(frm, False, True) ; #DEBUG_LINE_NO:2838
  EndIf
  frm = a.GetWornForm(32) ; #DEBUG_LINE_NO:2840
  If frm as Bool && !frm.HasKeyWordString("SexLabNoStrip") ; #DEBUG_LINE_NO:2841
    a.UnequipItem(frm, False, True) ; #DEBUG_LINE_NO:2842
  EndIf
  If a.WornHasKeyword(zad_DeviousHeavyBondage) ; #DEBUG_LINE_NO:2844
    animate = False ; #DEBUG_LINE_NO:2845
  EndIf
  SexLab.StripActor(a, None, animate, False) ; #DEBUG_LINE_NO:2847
EndFunction

Bool Function hasAnyWeaponEquipped(Actor a)
  If !a.GetEquippedObject(0) && !a.GetEquippedObject(1) ; #DEBUG_LINE_NO:2851
    Return False ; #DEBUG_LINE_NO:2852
  EndIf
  Return True ; #DEBUG_LINE_NO:2854
EndFunction

Function stripweapons(Actor a, Bool unequiponly)
  Int i = 10 ; #DEBUG_LINE_NO:2858
  Form loc_lefthand = None ; #DEBUG_LINE_NO:2860
  Form loc_righthand = None ; #DEBUG_LINE_NO:2861
  While i > 0 ; #DEBUG_LINE_NO:2863
    i -= 1 ; #DEBUG_LINE_NO:2864
    loc_lefthand = a.GetEquippedObject(0) ; #DEBUG_LINE_NO:2865
    If loc_lefthand ; #DEBUG_LINE_NO:2866
      a.UnequipItem(loc_lefthand, False, True) ; #DEBUG_LINE_NO:2867
    EndIf
    If loc_lefthand as Spell ; #DEBUG_LINE_NO:2869
      a.UnequipSpell(loc_lefthand as Spell, 0) ; #DEBUG_LINE_NO:2870
    EndIf
    loc_righthand = a.GetEquippedObject(1) ; #DEBUG_LINE_NO:2873
    If loc_righthand ; #DEBUG_LINE_NO:2874
      a.UnequipItem(loc_righthand, False, True) ; #DEBUG_LINE_NO:2875
    EndIf
    If loc_righthand as Spell ; #DEBUG_LINE_NO:2877
      a.UnequipSpell(loc_righthand as Spell, 1) ; #DEBUG_LINE_NO:2878
    EndIf
    If (loc_lefthand as Bool || loc_righthand as Bool) || a.IsWeaponDrawn() ; #DEBUG_LINE_NO:2880
      a.SheatheWeapon() ; #DEBUG_LINE_NO:2881
      Utility.Wait(0.100000001) ; #DEBUG_LINE_NO:2882
    Else
      Return  ; #DEBUG_LINE_NO:2884
    EndIf
  EndWhile
EndFunction

Function StartBoundEffects(Actor akTarget)
  If !akTarget.WornHasKeyword(zad_DeviousHeavyBondage) && !akTarget.WornHasKeyword(zad_DeviousHobbleSkirt) && !akTarget.WornHasKeyword(zad_DeviousPonyGear) ; #DEBUG_LINE_NO:2890
    Return  ; #DEBUG_LINE_NO:2891
  EndIf
  Self.stripweapons(akTarget, True) ; #DEBUG_LINE_NO:2893
  If akTarget != PlayerRef ; #DEBUG_LINE_NO:2894
    BoundCombat.EvaluateAA(akTarget) ; #DEBUG_LINE_NO:2895
    Return  ; #DEBUG_LINE_NO:2897
  EndIf
  Self.Log("OnEffectStart(): Bound Effects", 0) ; #DEBUG_LINE_NO:2899
  Self.PlayBoundIdle() ; #DEBUG_LINE_NO:2900
  Self.RegisterForSingleUpdate(8.0) ; #DEBUG_LINE_NO:2901
  If akTarget == PlayerRef ; #DEBUG_LINE_NO:2902
    Self.UpdateControls() ; #DEBUG_LINE_NO:2903
  EndIf
EndFunction

Function StopBoundEffects(Actor akTarget)
  Self.Log("OnEffectFinish(): Bound Effects", 0) ; #DEBUG_LINE_NO:2908
  If akTarget == PlayerRef ; #DEBUG_LINE_NO:2911
    Self.UnregisterForUpdate() ; #DEBUG_LINE_NO:2912
    Self.UpdateControls() ; #DEBUG_LINE_NO:2913
  EndIf
  BoundCombat.EvaluateAA(akTarget) ; #DEBUG_LINE_NO:2915
EndFunction

Event OnUpdate()
  If Config.debugSigTerm ; #DEBUG_LINE_NO:2919
    Config.debugSigTerm = False ; #DEBUG_LINE_NO:2920
    Self.DDI_DebugTerminate() ; #DEBUG_LINE_NO:2921
    Self.RegisterForSingleUpdate(20.0) ; #DEBUG_LINE_NO:2923
    Return  ; #DEBUG_LINE_NO:2924
  EndIf
  If Game.IsMenuControlsEnabled() || Game.IsFightingControlsEnabled() ; #DEBUG_LINE_NO:2941
    If !Self.IsAnimating(PlayerRef) ; #DEBUG_LINE_NO:2942
      Self.UpdateControls() ; #DEBUG_LINE_NO:2943
    EndIf
  EndIf
  Self.RegisterForSingleUpdate(8.0) ; #DEBUG_LINE_NO:2946
EndEvent

Function PlayBoundIdle()
  BoundCombat.EvaluateAA(PlayerRef) ; #DEBUG_LINE_NO:2950
  If !Self.IsAnimating(PlayerRef) && !PlayerRef.IsInFaction(SexLabAnimatingFaction) ; #DEBUG_LINE_NO:2951
    Self.ApplyBoundAnim(PlayerRef, None) ; #DEBUG_LINE_NO:2952
  EndIf
EndFunction

Bool Function UseRubberSounds()
  Return Config.RubberSoundMode > 0 ; #DEBUG_LINE_NO:2957
EndFunction

Float Function GetRubberSoundOffset()
  Float offset = 0.0 ; #DEBUG_LINE_NO:2961
  If Config.RubberSoundMode == 1 ; #DEBUG_LINE_NO:2962
    offset = 30.0 ; #DEBUG_LINE_NO:2963
  ElseIf Config.RubberSoundMode == 2 ; #DEBUG_LINE_NO:2964
    offset = 10.0 ; #DEBUG_LINE_NO:2965
  ElseIf Config.RubberSoundMode == 3 ; #DEBUG_LINE_NO:2966
    offset = 0.0 ; #DEBUG_LINE_NO:2967
  EndIf
  Return offset ; #DEBUG_LINE_NO:2969
EndFunction

Function UnsetStoredDevice(Actor akActor, Keyword zad_DeviousDevice)
  storageutil.UnsetFormValue(akActor as Form, "zad_Equipped" + Self.LookupDeviceType(zad_DeviousDevice) + "_Inventory") ; #DEBUG_LINE_NO:2973
  storageutil.UnsetFormValue(akActor as Form, "zad_Equipped" + Self.LookupDeviceType(zad_DeviousDevice) + "_Rendered") ; #DEBUG_LINE_NO:2974
  storageutil.UnsetFormValue(akActor as Form, "zad_Equipped" + Self.LookupDeviceType(zad_DeviousDevice) + "_Key") ; #DEBUG_LINE_NO:2975
EndFunction

Function RestoreSettings(Actor akActor)
  Float originalExposureRate = Self.GetOriginalRate(akActor) ; #DEBUG_LINE_NO:2979
  Self.Log("Restoring original exposure rate to " + originalExposureRate as String, 0) ; #DEBUG_LINE_NO:2980
  Aroused.SetActorExposureRate(akActor, originalExposureRate) ; #DEBUG_LINE_NO:2981
  storageutil.UnSetFloatValue(akActor as Form, "zad.StoredExposureRate") ; #DEBUG_LINE_NO:2982
EndFunction

Function MuteOverEncumberedMSG()
  Game.SetGameSettingString("sOverEncumbered", "") ; #DEBUG_LINE_NO:2992
  Game.SetGameSettingString("sNoFastTravelOverencumbered", "You cannot fast travel in these restraints.") ; #DEBUG_LINE_NO:2993
EndFunction

Function EquipDevice(Actor akActor, Armor deviceInventory, Armor deviceRendered, Keyword zad_DeviousDevice, Bool skipEvents, Bool skipMutex)
  Self.LockDevice(akActor, deviceInventory, False) ; #DEBUG_LINE_NO:3005
EndFunction

Function removeDevice(Actor akActor, Armor deviceInventory, Armor deviceRendered, Keyword zad_DeviousDevice, Bool destroyDevice, Bool skipEvents, Bool skipMutex)
  Self.UnlockDevice(akActor, deviceInventory, deviceRendered, zad_DeviousDevice, destroyDevice, False) ; #DEBUG_LINE_NO:3010
EndFunction

Bool Function ManipulateGenericDevice(Actor akActor, Armor device, Bool equipOrUnequip, Bool skipEvents, Bool skipMutex)
  Bool manipulated = False ; #DEBUG_LINE_NO:3017
  Keyword kw = Self.GetDeviceKeyword(device) ; #DEBUG_LINE_NO:3018
  Armor loc_rd = zadnativefunctions.GetRenderDevice(device) ; #DEBUG_LINE_NO:3019
  If loc_rd.HasKeyWord(zad_BlockGeneric) || loc_rd.HasKeyWord(zad_QuestItem) ; #DEBUG_LINE_NO:3020
    Self.Warn("ManipulateGenericDevice called on non-generic device.") ; #DEBUG_LINE_NO:3021
  Else
    Form tmpInv = storageutil.GetFormValue(akActor as Form, "zad_Equipped" + Self.LookupDeviceType(kw) + "_Inventory", None) ; #DEBUG_LINE_NO:3024
    If !equipOrUnequip && tmpInv as Bool ; #DEBUG_LINE_NO:3025
      Form tmpRend = storageutil.GetFormValue(akActor as Form, "zad_Equipped" + Self.LookupDeviceType(kw) + "_Rendered", None) ; #DEBUG_LINE_NO:3026
      Self.removeDevice(akActor, tmpInv as Armor, tmpRend as Armor, kw, False, skipEvents, skipMutex) ; #DEBUG_LINE_NO:3027
      Return True ; #DEBUG_LINE_NO:3028
    EndIf
    If equipOrUnequip ; #DEBUG_LINE_NO:3030
      Self.EquipDevice(akActor, device, loc_rd, kw, skipEvents, skipMutex) ; #DEBUG_LINE_NO:3031
    Else
      Self.removeDevice(akActor, device, loc_rd, kw, False, skipEvents, skipMutex) ; #DEBUG_LINE_NO:3033
    EndIf
    manipulated = True ; #DEBUG_LINE_NO:3035
  EndIf
  Return manipulated ; #DEBUG_LINE_NO:3037
EndFunction

Bool Function ManipulateGenericDeviceByKeyword(Actor akActor, Keyword kw, Bool equipOrUnequip, Bool skipEvents, Bool skipMutex)
  Form tmpInv = storageutil.GetFormValue(akActor as Form, "zad_Equipped" + Self.LookupDeviceType(kw) + "_Inventory", None) ; #DEBUG_LINE_NO:3044
  If !equipOrUnequip && tmpInv as Bool ; #DEBUG_LINE_NO:3045
    Form tmpRend = storageutil.GetFormValue(akActor as Form, "zad_Equipped" + Self.LookupDeviceType(kw) + "_Rendered", None) ; #DEBUG_LINE_NO:3046
    If tmpInv.HasKeyWord(zad_BlockGeneric) || tmpRend.HasKeyWord(zad_BlockGeneric) || tmpInv.HasKeyWord(zad_QuestItem) || tmpRend.HasKeyWord(zad_QuestItem) ; #DEBUG_LINE_NO:3047
      Self.Warn("ManipulateGenericDeviceByKeyword called on non-generic device!") ; #DEBUG_LINE_NO:3048
      Return False ; #DEBUG_LINE_NO:3049
    EndIf
    Self.removeDevice(akActor, tmpInv as Armor, tmpRend as Armor, kw, False, skipEvents, skipMutex) ; #DEBUG_LINE_NO:3051
    Return True ; #DEBUG_LINE_NO:3052
  EndIf
  Int iFormIndex = akActor.GetNumItems() ; #DEBUG_LINE_NO:3054
  Bool breakFlag = False ; #DEBUG_LINE_NO:3055
  While iFormIndex > 0 && !breakFlag ; #DEBUG_LINE_NO:3056
    iFormIndex -= 1 ; #DEBUG_LINE_NO:3057
    Form kForm = akActor.GetNthForm(iFormIndex) ; #DEBUG_LINE_NO:3058
    If kForm.HasKeyWord(zad_InventoryDevice) && ((akActor.IsEquipped(kForm) || akActor != PlayerRef) && !equipOrUnequip || !akActor.IsEquipped(kForm) && equipOrUnequip) ; #DEBUG_LINE_NO:3059
      ObjectReference tmpORef = akActor.placeAtMe(kForm, 1, False, True) ; #DEBUG_LINE_NO:3060
      zadequipscript tmpZRef = tmpORef as zadequipscript ; #DEBUG_LINE_NO:3061
      If tmpZRef != None && tmpZRef.zad_DeviousDevice == kw && (akActor.GetItemCount(tmpZRef.deviceRendered as Form) > 0 && !equipOrUnequip || equipOrUnequip) ; #DEBUG_LINE_NO:3062
        If !tmpZRef.HasKeyWord(zad_BlockGeneric) && !tmpZRef.HasKeyWord(zad_QuestItem) ; #DEBUG_LINE_NO:3063
          breakFlag = True ; #DEBUG_LINE_NO:3064
          If equipOrUnequip ; #DEBUG_LINE_NO:3065
            Self.EquipDevice(akActor, kForm as Armor, tmpZRef.deviceRendered, tmpZRef.zad_DeviousDevice, skipEvents, skipMutex) ; #DEBUG_LINE_NO:3066
          Else
            Self.removeDevice(akActor, kForm as Armor, tmpZRef.deviceRendered, tmpZRef.zad_DeviousDevice, False, skipEvents, skipMutex) ; #DEBUG_LINE_NO:3068
          EndIf
        EndIf
      EndIf
      tmpORef.delete() ; #DEBUG_LINE_NO:3072
    EndIf
  EndWhile
  Return breakFlag ; #DEBUG_LINE_NO:3075
EndFunction

Bool Function ForceEquipDevice(Actor akActor, Armor deviceInventory, Armor deviceRendered, Keyword zad_DeviousDevice, Bool skipEvents, Bool skipMutex)
  If Self.WearingConflictingDevice(akActor, deviceRendered, zad_DeviousDevice) ; #DEBUG_LINE_NO:3082
    Self.Log("ForceEquipDevice() called for one device, while already wearing a device of the same type:" + zad_DeviousDevice as String, 0) ; #DEBUG_LINE_NO:3083
    If Self.ManipulateGenericDeviceByKeyword(akActor, zad_DeviousDevice, False, skipEvents, skipMutex) ; #DEBUG_LINE_NO:3084
      Self.Log("Force equip: Conflicting item removed successfully!", 0) ; #DEBUG_LINE_NO:3085
    Else
      Self.Log("Force equip: Failed to remove worn item. Aborting.", 0) ; #DEBUG_LINE_NO:3087
      Return False ; #DEBUG_LINE_NO:3088
    EndIf
  EndIf
  If akActor.GetItemCount(deviceInventory as Form) <= 0 ; #DEBUG_LINE_NO:3091
    akActor.AddItem(deviceInventory as Form, 1, True) ; #DEBUG_LINE_NO:3092
  EndIf
  akActor.EquipItemEx(deviceInventory as Form, 0, False, True) ; #DEBUG_LINE_NO:3094
  Return True ; #DEBUG_LINE_NO:3095
EndFunction

Function ManipulateDevice(Actor akActor, Armor device, Bool equipOrUnequip, Bool skipEvents)
  Keyword deviceKeyword
  Armor deviceRendered
  Self.Log("WARNING: ManipulateDevice() is deprecated and will be removed in a later DD version! Use LockDevice() instead.", 0) ; #DEBUG_LINE_NO:3100
  If device == beltPadded ; #DEBUG_LINE_NO:3104
    deviceRendered = beltPaddedRendered ; #DEBUG_LINE_NO:3105
    deviceKeyword = zad_DeviousBelt ; #DEBUG_LINE_NO:3106
  ElseIf device == beltIron ; #DEBUG_LINE_NO:3107
    deviceRendered = beltIronRendered ; #DEBUG_LINE_NO:3108
    deviceKeyword = zad_DeviousBelt ; #DEBUG_LINE_NO:3109
  ElseIf device == plugIron ; #DEBUG_LINE_NO:3110
    deviceRendered = plugIronRendered ; #DEBUG_LINE_NO:3111
    deviceKeyword = zad_DeviousPlug ; #DEBUG_LINE_NO:3112
  ElseIf device == plugPrimitive ; #DEBUG_LINE_NO:3113
    deviceRendered = plugPrimitiveRendered ; #DEBUG_LINE_NO:3114
    deviceKeyword = zad_DeviousPlug ; #DEBUG_LINE_NO:3115
  ElseIf device == plugInflatable ; #DEBUG_LINE_NO:3116
    deviceRendered = plugInflatableRendered ; #DEBUG_LINE_NO:3117
    deviceKeyword = zad_DeviousPlug ; #DEBUG_LINE_NO:3118
  ElseIf device == plugSoulgem ; #DEBUG_LINE_NO:3119
    deviceRendered = plugSoulgemRendered ; #DEBUG_LINE_NO:3120
    deviceKeyword = zad_DeviousPlug ; #DEBUG_LINE_NO:3121
  ElseIf device == braPadded ; #DEBUG_LINE_NO:3122
    deviceRendered = braPaddedRendered ; #DEBUG_LINE_NO:3123
    deviceKeyword = zad_DeviousBra ; #DEBUG_LINE_NO:3124
  ElseIf device == cuffsPaddedArms ; #DEBUG_LINE_NO:3125
    deviceRendered = cuffsPaddedArmsRendered ; #DEBUG_LINE_NO:3126
    deviceKeyword = zad_DeviousArmCuffs ; #DEBUG_LINE_NO:3127
  ElseIf device == cuffsPaddedLegs ; #DEBUG_LINE_NO:3128
    deviceRendered = cuffsPaddedLegsRendered ; #DEBUG_LINE_NO:3129
    deviceKeyword = zad_DeviousLegCuffs ; #DEBUG_LINE_NO:3130
  ElseIf device == cuffsPaddedCollar ; #DEBUG_LINE_NO:3131
    deviceRendered = cuffsPaddedCollarRendered ; #DEBUG_LINE_NO:3132
    deviceKeyword = zad_DeviousCollar ; #DEBUG_LINE_NO:3133
  ElseIf device == cuffsPaddedComplete ; #DEBUG_LINE_NO:3134
    deviceRendered = cuffsPaddedCompleteRendered ; #DEBUG_LINE_NO:3135
    deviceKeyword = zad_DeviousArmCuffs ; #DEBUG_LINE_NO:3136
  ElseIf device == collarPosture ; #DEBUG_LINE_NO:3137
    deviceRendered = collarPostureRendered ; #DEBUG_LINE_NO:3138
    deviceKeyword = zad_DeviousCollar ; #DEBUG_LINE_NO:3139
  ElseIf device == collarPostureLeather ; #DEBUG_LINE_NO:3140
    deviceRendered = collarPostureLeatherRendered ; #DEBUG_LINE_NO:3141
    deviceKeyword = zad_DeviousCollar ; #DEBUG_LINE_NO:3142
  ElseIf device == armbinder ; #DEBUG_LINE_NO:3143
    deviceRendered = armbinderRendered ; #DEBUG_LINE_NO:3144
    deviceKeyword = zad_DeviousArmbinder ; #DEBUG_LINE_NO:3145
  ElseIf device == gagBall ; #DEBUG_LINE_NO:3146
    deviceRendered = gagBallRendered ; #DEBUG_LINE_NO:3147
    deviceKeyword = zad_DeviousGag ; #DEBUG_LINE_NO:3148
  ElseIf device == gagPanel ; #DEBUG_LINE_NO:3149
    deviceRendered = gagPanelRendered ; #DEBUG_LINE_NO:3150
    deviceKeyword = zad_DeviousGag ; #DEBUG_LINE_NO:3151
  ElseIf device == gagRing ; #DEBUG_LINE_NO:3152
    deviceRendered = gagRingRendered ; #DEBUG_LINE_NO:3153
    deviceKeyword = zad_DeviousGag ; #DEBUG_LINE_NO:3154
  ElseIf device == gagStrapBall ; #DEBUG_LINE_NO:3155
    deviceRendered = gagStrapBallRendered ; #DEBUG_LINE_NO:3156
    deviceKeyword = zad_DeviousGag ; #DEBUG_LINE_NO:3157
  ElseIf device == gagStrapRing ; #DEBUG_LINE_NO:3158
    deviceRendered = gagStrapRingRendered ; #DEBUG_LINE_NO:3159
    deviceKeyword = zad_DeviousGag ; #DEBUG_LINE_NO:3160
  ElseIf device == blindfold ; #DEBUG_LINE_NO:3161
    deviceRendered = blindfoldRendered ; #DEBUG_LINE_NO:3162
    deviceKeyword = zad_DeviousBlindfold ; #DEBUG_LINE_NO:3163
  ElseIf device == cuffsLeatherArms ; #DEBUG_LINE_NO:3164
    deviceRendered = cuffsLeatherArmsRendered ; #DEBUG_LINE_NO:3165
    deviceKeyword = zad_DeviousArmCuffs ; #DEBUG_LINE_NO:3166
  ElseIf device == cuffsLeatherLegs ; #DEBUG_LINE_NO:3167
    deviceRendered = cuffsLeatherLegsRendered ; #DEBUG_LINE_NO:3168
    deviceKeyword = zad_DeviousLegCuffs ; #DEBUG_LINE_NO:3169
  ElseIf device == cuffsLeatherCollar ; #DEBUG_LINE_NO:3170
    deviceRendered = cuffsLeatherCollarRendered ; #DEBUG_LINE_NO:3171
    deviceKeyword = zad_DeviousCollar ; #DEBUG_LINE_NO:3172
  ElseIf device == harnessBody ; #DEBUG_LINE_NO:3173
    deviceRendered = harnessBodyRendered ; #DEBUG_LINE_NO:3174
    deviceKeyword = zad_DeviousHarness ; #DEBUG_LINE_NO:3175
  ElseIf device == harnessCollar ; #DEBUG_LINE_NO:3176
    deviceRendered = harnessCollarRendered ; #DEBUG_LINE_NO:3177
    deviceKeyword = zad_DeviousCollar ; #DEBUG_LINE_NO:3178
  ElseIf device == plugIronVag ; #DEBUG_LINE_NO:3179
    deviceRendered = plugIronVagRendered ; #DEBUG_LINE_NO:3180
    deviceKeyword = zad_DeviousPlugVaginal ; #DEBUG_LINE_NO:3181
  ElseIf device == plugIronAn ; #DEBUG_LINE_NO:3182
    deviceRendered = plugIronAnRendered ; #DEBUG_LINE_NO:3183
    deviceKeyword = zad_DeviousPlugAnal ; #DEBUG_LINE_NO:3184
  ElseIf device == plugPrimitiveVag ; #DEBUG_LINE_NO:3185
    deviceRendered = plugPrimitiveVagRendered ; #DEBUG_LINE_NO:3186
    deviceKeyword = zad_DeviousPlugVaginal ; #DEBUG_LINE_NO:3187
  ElseIf device == plugPrimitiveAn ; #DEBUG_LINE_NO:3188
    deviceRendered = plugPrimitiveAnRendered ; #DEBUG_LINE_NO:3189
    deviceKeyword = zad_DeviousPlugAnal ; #DEBUG_LINE_NO:3190
  ElseIf device == plugSoulgemVag ; #DEBUG_LINE_NO:3191
    deviceRendered = plugSoulgemVagRendered ; #DEBUG_LINE_NO:3192
    deviceKeyword = zad_DeviousPlugVaginal ; #DEBUG_LINE_NO:3193
  ElseIf device == plugSoulgemAn ; #DEBUG_LINE_NO:3194
    deviceRendered = plugSoulgemAnRendered ; #DEBUG_LINE_NO:3195
    deviceKeyword = zad_DeviousPlugAnal ; #DEBUG_LINE_NO:3196
  ElseIf device == plugInflatableVag ; #DEBUG_LINE_NO:3197
    deviceRendered = plugInflatableVagRendered ; #DEBUG_LINE_NO:3198
    deviceKeyword = zad_DeviousPlugVaginal ; #DEBUG_LINE_NO:3199
  ElseIf device == plugInflatableAn ; #DEBUG_LINE_NO:3200
    deviceRendered = plugInflatableAnRendered ; #DEBUG_LINE_NO:3201
    deviceKeyword = zad_DeviousPlugAnal ; #DEBUG_LINE_NO:3202
  ElseIf device == beltPaddedOpen ; #DEBUG_LINE_NO:3203
    deviceRendered = beltPaddedOpenRendered ; #DEBUG_LINE_NO:3204
    deviceKeyword = zad_DeviousBelt ; #DEBUG_LINE_NO:3205
  ElseIf device == plugChargeableVag ; #DEBUG_LINE_NO:3206
    deviceRendered = plugChargeableRenderedVag ; #DEBUG_LINE_NO:3207
    deviceKeyword = zad_DeviousPlugVaginal ; #DEBUG_LINE_NO:3208
  ElseIf device == plugTrainingVag ; #DEBUG_LINE_NO:3209
    deviceRendered = plugTrainingRenderedVag ; #DEBUG_LINE_NO:3210
    deviceKeyword = zad_DeviousPlugVaginal ; #DEBUG_LINE_NO:3211
  ElseIf device == piercingNSoul ; #DEBUG_LINE_NO:3212
    deviceRendered = piercingNSoulRendered ; #DEBUG_LINE_NO:3213
    deviceKeyword = zad_DeviousPiercingsNipple ; #DEBUG_LINE_NO:3214
  ElseIf device == piercingVSoul ; #DEBUG_LINE_NO:3215
    deviceRendered = piercingVSoulRendered ; #DEBUG_LINE_NO:3216
    deviceKeyword = zad_DeviousPiercingsVaginal ; #DEBUG_LINE_NO:3217
  ElseIf device == collarRestrictive ; #DEBUG_LINE_NO:3218
    deviceRendered = collarRestrictiveRendered ; #DEBUG_LINE_NO:3219
    deviceKeyword = zad_DeviousCollar ; #DEBUG_LINE_NO:3220
  ElseIf device == corset ; #DEBUG_LINE_NO:3221
    deviceRendered = corsetRendered ; #DEBUG_LINE_NO:3222
    deviceKeyword = zad_DeviousCorset ; #DEBUG_LINE_NO:3223
  ElseIf device == glovesRestrictive ; #DEBUG_LINE_NO:3224
    deviceRendered = glovesRestrictiveRendered ; #DEBUG_LINE_NO:3225
    deviceKeyword = zad_DeviousGloves ; #DEBUG_LINE_NO:3226
  ElseIf device == yoke ; #DEBUG_LINE_NO:3227
    deviceRendered = yokeRendered ; #DEBUG_LINE_NO:3228
    deviceKeyword = zad_DeviousYoke ; #DEBUG_LINE_NO:3229
  Else
    Self.Error("ManipulateDevice did not recognize device type that it received as an argument.") ; #DEBUG_LINE_NO:3231
    Return  ; #DEBUG_LINE_NO:3232
  EndIf
  If equipOrUnequip ; #DEBUG_LINE_NO:3234
    Self.EquipDevice(akActor, device, deviceRendered, deviceKeyword, skipEvents, False) ; #DEBUG_LINE_NO:3235
  Else
    Self.removeDevice(akActor, device, deviceRendered, deviceKeyword, False, skipEvents, False) ; #DEBUG_LINE_NO:3237
  EndIf
EndFunction

Function RegisterDevices()
  Self.RegisterGenericDevice(braPadded, "bra,metal,padded, DDi") ; #DEBUG_LINE_NO:3246
  Self.RegisterGenericDevice(cuffsPaddedArms, "cuffs,arms,metal,padded, DDi") ; #DEBUG_LINE_NO:3247
  Self.RegisterGenericDevice(cuffsPaddedLegs, "cuffs,legs,metal,padded, DDi") ; #DEBUG_LINE_NO:3248
  Self.RegisterGenericDevice(cuffsPaddedCollar, "collar,metal,padded,short, DDi") ; #DEBUG_LINE_NO:3249
  Self.RegisterGenericDevice(collarPosture, "collar,metal,posture,padded, DDi") ; #DEBUG_LINE_NO:3250
  Self.RegisterGenericDevice(armbinder, "armbinder,leather,black, DDi") ; #DEBUG_LINE_NO:3251
  Self.RegisterGenericDevice(gagBall, "gag,harness,ball,leather,black, DDi") ; #DEBUG_LINE_NO:3252
  Self.RegisterGenericDevice(gagPanel, "gag,harness,panel,leather,black, DDi") ; #DEBUG_LINE_NO:3253
  Self.RegisterGenericDevice(gagRing, "gag,harness,ring,leather,black, DDi") ; #DEBUG_LINE_NO:3254
  Self.RegisterGenericDevice(gagStrapBall, "gag,strap,ball,leather,black, DDi") ; #DEBUG_LINE_NO:3255
  Self.RegisterGenericDevice(gagStrapRing, "gag,strap,ring,leather,black, DDi") ; #DEBUG_LINE_NO:3256
  Self.RegisterGenericDevice(blindfold, "blindfold,leather,black, DDi") ; #DEBUG_LINE_NO:3257
  Self.RegisterGenericDevice(cuffsLeatherArms, "cuffs,arms,leather,black, DDi") ; #DEBUG_LINE_NO:3258
  Self.RegisterGenericDevice(cuffsLeatherLegs, "cuffs,legs,leather,black, DDi") ; #DEBUG_LINE_NO:3259
  Self.RegisterGenericDevice(cuffsLeatherCollar, "collar,leather,black,short, DDi") ; #DEBUG_LINE_NO:3260
  Self.RegisterGenericDevice(collarPostureLeather, "collar,leather,black,posture, DDi") ; #DEBUG_LINE_NO:3261
  Self.RegisterGenericDevice(harnessBody, "harness,leather,black,full, DDi") ; #DEBUG_LINE_NO:3262
  Self.RegisterGenericDevice(harnessCollar, "collar,harness,leather,black, DDi") ; #DEBUG_LINE_NO:3263
  Self.RegisterGenericDevice(plugIronVag, "plug,vaginal,iron,simple, DDi") ; #DEBUG_LINE_NO:3264
  Self.RegisterGenericDevice(plugIronAn, "plug,anal,iron,simple, DDi") ; #DEBUG_LINE_NO:3265
  Self.RegisterGenericDevice(plugPrimitiveVag, "plug,vaginal,primitive,simple, DDi") ; #DEBUG_LINE_NO:3266
  Self.RegisterGenericDevice(plugPrimitiveAn, "plug,anal,primitive,simple, DDi") ; #DEBUG_LINE_NO:3267
  Self.RegisterGenericDevice(plugSoulgemVag, "plug,vaginal,soulgem,magic, DDi") ; #DEBUG_LINE_NO:3268
  Self.RegisterGenericDevice(plugSoulgemAn, "plug,anal,soulgem,magic, DDi") ; #DEBUG_LINE_NO:3269
  Self.RegisterGenericDevice(plugInflatableVag, "plug,pump,vaginal,inflatable, DDi") ; #DEBUG_LINE_NO:3270
  Self.RegisterGenericDevice(plugInflatableAn, "plug,pump,anal,inflatable, DDi") ; #DEBUG_LINE_NO:3271
  Self.RegisterGenericDevice(beltPaddedOpen, "belt,metal,padded,open, DDi") ; #DEBUG_LINE_NO:3272
  Self.RegisterGenericDevice(beltPadded, "belt,metal,padded,full, DDi") ; #DEBUG_LINE_NO:3273
  Self.RegisterGenericDevice(beltIron, "belt,metal,iron,full, DDi") ; #DEBUG_LINE_NO:3274
  Self.RegisterGenericDevice(piercingNSoul, "piercing,nipple,soulgem, DDi") ; #DEBUG_LINE_NO:3275
  Self.RegisterGenericDevice(piercingVSoul, "piercing,vaginal,soulgem, DDi") ; #DEBUG_LINE_NO:3276
  Self.RegisterGenericDevice(collarRestrictive, "DDi,koffii,collar,restrictive,metal,black") ; #DEBUG_LINE_NO:3278
  Self.RegisterGenericDevice(corset, "DDi,restrictive,corset,leather,black") ; #DEBUG_LINE_NO:3279
  Self.RegisterGenericDevice(glovesRestrictive, "DDi,restrictive,gloves,leather,black") ; #DEBUG_LINE_NO:3280
  Self.RegisterGenericDevice(yoke, "DDi,yoke,metal") ; #DEBUG_LINE_NO:3281
  Self.Log("Finished registering devices.", 0) ; #DEBUG_LINE_NO:3283
EndFunction

Function RegisterGenericDevice(Armor inventoryDevice, String tags)
  If inventoryDevice == None || inventoryDevice.HasKeyWord(zad_BlockGeneric) || inventoryDevice.HasKeyWord(zad_QuestItem) || !inventoryDevice.HasKeyWord(zad_InventoryDevice) ; #DEBUG_LINE_NO:3288
    Return  ; #DEBUG_LINE_NO:3289
  EndIf
  Keyword kw = Self.GetDeviceKeyword(inventoryDevice) ; #DEBUG_LINE_NO:3292
  If kw == None ; #DEBUG_LINE_NO:3293
    Return  ; #DEBUG_LINE_NO:3294
  EndIf
  storageutil.FormListAdd(kw as Form, "zad.GenericDevice", inventoryDevice as Form, False) ; #DEBUG_LINE_NO:3297
  storageutil.StringListClear(inventoryDevice as Form, "zad.deviceTags") ; #DEBUG_LINE_NO:3298
  String[] tagArray = papyrusutil.StringSplit(tags, ",") ; #DEBUG_LINE_NO:3299
  Int i = tagArray.Length ; #DEBUG_LINE_NO:3300
  While i > 0 ; #DEBUG_LINE_NO:3301
    i -= 1 ; #DEBUG_LINE_NO:3302
    Self.Log("adding tag " + tagArray[i] + " for " + inventoryDevice.GetName(), 0) ; #DEBUG_LINE_NO:3303
    storageutil.StringListAdd(inventoryDevice as Form, "zad.deviceTags", tagArray[i], True) ; #DEBUG_LINE_NO:3304
  EndWhile
EndFunction

Armor Function GetGenericDeviceByKeyword(Keyword kw)
  Int n = storageutil.FormListCount(kw as Form, "zad.GenericDevice") ; #DEBUG_LINE_NO:3310
  Armor device = None ; #DEBUG_LINE_NO:3311
  While device == None && n > 0 ; #DEBUG_LINE_NO:3312
    Int i = Utility.RandomInt(0, n - 1) ; #DEBUG_LINE_NO:3314
    device = storageutil.FormListGet(kw as Form, "zad.GenericDevice", i) as Armor ; #DEBUG_LINE_NO:3315
    If device == None ; #DEBUG_LINE_NO:3316
      Self.Log("Found a none stored generic item, clearing...", 0) ; #DEBUG_LINE_NO:3318
      storageutil.FormListRemoveAt(kw as Form, "zad.GenericDevice", i) ; #DEBUG_LINE_NO:3319
      n = storageutil.FormListCount(kw as Form, "zad.GenericDevice") ; #DEBUG_LINE_NO:3320
    EndIf
  EndWhile
  Return device ; #DEBUG_LINE_NO:3324
EndFunction

Armor Function GetDeviceByTags(Keyword kw, String tags, Bool requireAll, String tagsToSuppress, Bool fallBack)
  Self.Log(("GetDeviceByTags(" + kw as String) + ", " + tags + ")", 0) ; #DEBUG_LINE_NO:3331
  String[] tagArray = papyrusutil.StringSplit(tags, ",") ; #DEBUG_LINE_NO:3332
  String[] supArray = papyrusutil.StringSplit(tagsToSuppress, ",") ; #DEBUG_LINE_NO:3333
  Form[] resultList = new Form[64] ; #DEBUG_LINE_NO:3334
  Int n = 0 ; #DEBUG_LINE_NO:3335
  Int i = storageutil.FormListCount(kw as Form, "zad.GenericDevice") ; #DEBUG_LINE_NO:3337
  While i > 0 ; #DEBUG_LINE_NO:3338
    i -= 1 ; #DEBUG_LINE_NO:3339
    Armor current = storageutil.FormListGet(kw as Form, "zad.GenericDevice", i) as Armor ; #DEBUG_LINE_NO:3340
    If (current as Bool && Self.HasTags(current, tagArray, requireAll)) && !Self.HasTags(current, supArray, False) ; #DEBUG_LINE_NO:3341
      resultList[n] = current as Form ; #DEBUG_LINE_NO:3342
      n += 1 ; #DEBUG_LINE_NO:3343
    ElseIf current == None ; #DEBUG_LINE_NO:3344
      storageutil.FormListRemoveAt(kw as Form, "zad.GenericDevice", i) ; #DEBUG_LINE_NO:3345
    EndIf
  EndWhile
  If n < 1 && fallBack ; #DEBUG_LINE_NO:3349
    Self.Log("No devices found with tags, falling back to random device", 0) ; #DEBUG_LINE_NO:3350
    Return Self.GetGenericDeviceByKeyword(kw) ; #DEBUG_LINE_NO:3351
  ElseIf n < 1 ; #DEBUG_LINE_NO:3352
    Self.Log("No devices found with tags", 0) ; #DEBUG_LINE_NO:3353
    Return None ; #DEBUG_LINE_NO:3354
  EndIf
  Return resultList[Utility.RandomInt(0, n - 1)] as Armor ; #DEBUG_LINE_NO:3356
EndFunction

Bool Function HasTag(Armor item, String tag)
  Int i = storageutil.StringListCount(item as Form, "zad.deviceTags") ; #DEBUG_LINE_NO:3361
  While i > 0 ; #DEBUG_LINE_NO:3362
    i -= 1 ; #DEBUG_LINE_NO:3363
    If storageutil.StringListGet(item as Form, "zad.deviceTags", i) == tag ; #DEBUG_LINE_NO:3364
      Return True ; #DEBUG_LINE_NO:3365
    EndIf
  EndWhile
  Return False ; #DEBUG_LINE_NO:3368
EndFunction

Bool Function HasTags(Armor item, String[] tags, Bool requireAll)
  If tags.Length < 1 ; #DEBUG_LINE_NO:3373
    Return False ; #DEBUG_LINE_NO:3374
  EndIf
  Int i = storageutil.StringListCount(item as Form, "zad.deviceTags") ; #DEBUG_LINE_NO:3377
  Int n = 0 ; #DEBUG_LINE_NO:3378
  While i > 0 ; #DEBUG_LINE_NO:3380
    i -= 1 ; #DEBUG_LINE_NO:3381
    Int j = tags.Length ; #DEBUG_LINE_NO:3382
    String currentTag = storageutil.StringListGet(item as Form, "zad.deviceTags", i) ; #DEBUG_LINE_NO:3383
    While j > 0 ; #DEBUG_LINE_NO:3384
      j -= 1 ; #DEBUG_LINE_NO:3385
      If currentTag == tags[j] ; #DEBUG_LINE_NO:3386
        If !requireAll ; #DEBUG_LINE_NO:3387
          Return True ; #DEBUG_LINE_NO:3388
        EndIf
        n += 1 ; #DEBUG_LINE_NO:3390
        j = 0 ; #DEBUG_LINE_NO:3391
      EndIf
    EndWhile
  EndWhile
  Return n == tags.Length ; #DEBUG_LINE_NO:3395
EndFunction

Function CorsetMagic(Actor akActor)
  Return  ; #DEBUG_LINE_NO:3399
EndFunction

Bool Function UpdateCorsetState(Actor akActor)
  Return False ; #DEBUG_LINE_NO:3403
EndFunction

Function enableVA()
  If EnableVRSupport ; #DEBUG_LINE_NO:3407
    zadinterfacevrik.EnableVR(vrikActionsAPI) ; #DEBUG_LINE_NO:3408
  EndIf
EndFunction

Function disableVA()
  If EnableVRSupport ; #DEBUG_LINE_NO:3413
    zadinterfacevrik.DisableVR(vrikActionsAPI) ; #DEBUG_LINE_NO:3414
  EndIf
EndFunction
