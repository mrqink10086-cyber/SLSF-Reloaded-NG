ScriptName zadclibs Extends Quest

;-- Variables ---------------------------------------
Bool _initiated = False

;-- Properties --------------------------------------
slautilscr Property Aroused Auto
Actor Property SelectedUser Auto hidden
ReferenceAlias Property UserRef Auto
zadcanims Property cAnims Auto
zadlibs Property libs Auto
Faction Property zadcNGInContraptionFaction Auto
Activator Property zadc_BondageChair Auto
Activator Property zadc_BondagePole Auto
FormList Property zadc_CellFurnitureList Auto
Keyword Property zadc_FurnitureDevice Auto
Key Property zadc_FurnitureKey Auto
Keyword Property zadc_FurnitureKit Auto
Armor Property zadc_NoWaitItem Auto
Activator Property zadc_Pillory Auto
Activator Property zadc_Pillory2 Auto
Armor Property zadc_PrisonerChainsInventory Auto
Armor Property zadc_PrisonerChainsRendered Auto
Activator Property zadc_RestraintPost Auto
Activator Property zadc_ShackleWallIron Auto
Activator Property zadc_TorturePole01 Auto
Activator Property zadc_TorturePole02 Auto
Activator Property zadc_TorturePole03 Auto
Activator Property zadc_TorturePole04 Auto
Activator Property zadc_TorturePole05 Auto
Activator Property zadc_WoodenHorse Auto
Activator Property zadc_cross Auto
Activator Property zadc_gallowspole_overhead Auto
Activator Property zadc_gallowspole_strappado Auto
Activator Property zadc_gallowspole_suspend_hogtie Auto
Activator Property zadc_gallowspole_upside_down Auto
Activator Property zadc_gallowspole_woodenhorse Auto
MiscObject Property zadc_kit_bondagechair Auto
MiscObject Property zadc_kit_bondagepost Auto
MiscObject Property zadc_kit_cross Auto
MiscObject Property zadc_kit_gallowspolehogtie Auto
MiscObject Property zadc_kit_gallowspoleoverhead Auto
MiscObject Property zadc_kit_gallowspolestrappado Auto
MiscObject Property zadc_kit_gallowspoleupsidedown Auto
MiscObject Property zadc_kit_gallowspolewoodenhorse Auto
MiscObject Property zadc_kit_pillory Auto
MiscObject Property zadc_kit_pillory2 Auto
MiscObject Property zadc_kit_restraintpost Auto
MiscObject Property zadc_kit_torturepole Auto
MiscObject Property zadc_kit_torturepole2 Auto
MiscObject Property zadc_kit_torturepole3 Auto
MiscObject Property zadc_kit_torturepole4 Auto
MiscObject Property zadc_kit_torturepole5 Auto
MiscObject Property zadc_kit_wallshackle Auto
MiscObject Property zadc_kit_woodenhorse Auto
MiscObject Property zadc_kit_xcross Auto
Quest Property zadc_nearbyfurniture Auto
Outfit Property zadc_outfit_naked Auto
Package Property zadc_pk_donothing Auto
Activator Property zadc_xcross Auto

;-- Functions ---------------------------------------

; Skipped compiler generated GetState

; Skipped compiler generated GotoState

Bool Function GetIsFurnitureDevice(ObjectReference FurnitureDevice)
  If FurnitureDevice as Bool && FurnitureDevice.HasKeyword(zadc_FurnitureDevice) ; #DEBUG_LINE_NO:74
    Return True ; #DEBUG_LINE_NO:75
  EndIf
  Return False ; #DEBUG_LINE_NO:77
EndFunction

Actor Function GetUser(ObjectReference FurnitureDevice)
  If !Self.GetIsFurnitureDevice(FurnitureDevice) ; #DEBUG_LINE_NO:82
    Return None ; #DEBUG_LINE_NO:83
  EndIf
  zadcfurniturescript fs = FurnitureDevice as zadcfurniturescript ; #DEBUG_LINE_NO:85
  If fs as Bool && fs.User as Bool ; #DEBUG_LINE_NO:86
    Return fs.User ; #DEBUG_LINE_NO:87
  EndIf
  Return None ; #DEBUG_LINE_NO:89
EndFunction

Bool Function IsValidActor(Actor akActor)
  Return Self.IsValidActorV2(akActor, False) ; #DEBUG_LINE_NO:95
EndFunction

Bool Function IsValidActorV2(Actor akActor, Bool OverrideSceneCheck)
  If !akActor || akActor.IsChild() || akActor.IsDisabled() || akActor.IsDead() || akActor.IsGhost() ; #DEBUG_LINE_NO:103
    Return False ; #DEBUG_LINE_NO:104
  EndIf
  If (!libs.SexLab.ActorLib.CanAnimate(akActor) || Self.GetDevice(akActor) as Bool) || Self.IsAnimating(akActor) ; #DEBUG_LINE_NO:106
    Return False ; #DEBUG_LINE_NO:107
  EndIf
  If akActor.GetCurrentScene() != None && OverrideSceneCheck == False ; #DEBUG_LINE_NO:109
    Return False ; #DEBUG_LINE_NO:110
  EndIf
  Return True ; #DEBUG_LINE_NO:112
EndFunction

ObjectReference Function GetDevice(Actor act)
  ObjectReference obj = storageutil.GetFormValue(act as Form, "DDC_DeviceUsed", None) as ObjectReference ; #DEBUG_LINE_NO:117
  Return obj ; #DEBUG_LINE_NO:118
EndFunction

Key Function GetDeviceKey(ObjectReference FurnitureDevice)
  If !Self.GetIsFurnitureDevice(FurnitureDevice) ; #DEBUG_LINE_NO:123
    Return None ; #DEBUG_LINE_NO:124
  EndIf
  zadcfurniturescript fs = FurnitureDevice as zadcfurniturescript ; #DEBUG_LINE_NO:126
  If fs ; #DEBUG_LINE_NO:127
    Return fs.DeviceKey ; #DEBUG_LINE_NO:128
  EndIf
  Return None ; #DEBUG_LINE_NO:130
EndFunction

ObjectReference Function GetClosestFurnitureDevice()
  If zadc_nearbyfurniture.IsRunning() ; #DEBUG_LINE_NO:136
    zadc_nearbyfurniture.Stop() ; #DEBUG_LINE_NO:137
    zadc_nearbyfurniture.Start() ; #DEBUG_LINE_NO:138
  Else
    zadc_nearbyfurniture.Start() ; #DEBUG_LINE_NO:140
  EndIf
  ReferenceAlias device = zadc_nearbyfurniture.GetAlias(0) as ReferenceAlias ; #DEBUG_LINE_NO:142
  If device ; #DEBUG_LINE_NO:143
    Return device.GetReference() ; #DEBUG_LINE_NO:144
  EndIf
  Return None ; #DEBUG_LINE_NO:146
EndFunction

ObjectReference Function GetLinkedDevice(ObjectReference FurnitureDevice, Int Position)
  zadcfurniturescript fs = FurnitureDevice as zadcfurniturescript ; #DEBUG_LINE_NO:151
  If fs as Bool && fs.LinkedDevices.Length >= Position ; #DEBUG_LINE_NO:152
    Return fs.LinkedDevices[Position] ; #DEBUG_LINE_NO:153
  EndIf
  Return None ; #DEBUG_LINE_NO:155
EndFunction

FormList Function FindFurnitureDevicesInCell()
  zadc_CellFurnitureList.Revert() ; #DEBUG_LINE_NO:161
  Cell kCell = libs.PlayerRef.GetParentCell() ; #DEBUG_LINE_NO:162
  Int i = kCell.GetNumRefs(24) ; #DEBUG_LINE_NO:164
  While i > 0 ; #DEBUG_LINE_NO:165
    i -= 1 ; #DEBUG_LINE_NO:166
    ObjectReference o = kCell.GetNthRef(i, 24) ; #DEBUG_LINE_NO:167
    If o.HasKeyword(zadc_FurnitureDevice) ; #DEBUG_LINE_NO:168
      zadc_CellFurnitureList.AddForm(o as Form) ; #DEBUG_LINE_NO:169
    EndIf
  EndWhile
  Return zadc_CellFurnitureList ; #DEBUG_LINE_NO:172
EndFunction

Function SetOverridePose(ObjectReference FurnitureDevice, Package pose)
  storageutil.SetFormValue(FurnitureDevice as Form, "DDC_OverridePose", pose as Form) ; #DEBUG_LINE_NO:179
EndFunction

Package Function GetOverridePose(ObjectReference FurnitureDevice)
  Package pose = storageutil.GetFormValue(FurnitureDevice as Form, "DDC_OverridePose", None) as Package ; #DEBUG_LINE_NO:184
  If pose ; #DEBUG_LINE_NO:185
    Return pose ; #DEBUG_LINE_NO:186
  EndIf
  Return None ; #DEBUG_LINE_NO:188
EndFunction

Function ClearOverridePose(ObjectReference FurnitureDevice)
  storageutil.UnSetFormValue(FurnitureDevice as Form, "DDC_OverridePose") ; #DEBUG_LINE_NO:193
EndFunction

Bool Function SetTimedRelease(ObjectReference FurnitureDevice, Int Hours, Bool ResetStartTime)
  zadcfurniturescript fs = FurnitureDevice as zadcfurniturescript ; #DEBUG_LINE_NO:200
  If fs ; #DEBUG_LINE_NO:201
    fs.isSelfBondage = True ; #DEBUG_LINE_NO:202
    fs.SelfBondageReleaseTimer = Hours as Float ; #DEBUG_LINE_NO:203
    If ResetStartTime ; #DEBUG_LINE_NO:204
      fs.ReleaseTimerStartedAt = libs.GameDaysPassed.GetValue() ; #DEBUG_LINE_NO:205
    EndIf
  EndIf
EndFunction

Bool Function LockActor(Actor akActor, ObjectReference FurnitureDevice, Package OverridePose)
  Return Self.LockActorV2(akActor, FurnitureDevice, OverridePose, False) ; #DEBUG_LINE_NO:213
EndFunction

Bool Function LockActorV2(Actor akActor, ObjectReference FurnitureDevice, Package OverridePose, Bool AllowActorInScene)
  If !Self.IsValidActorV2(akActor, AllowActorInScene) || !Self.GetIsFurnitureDevice(FurnitureDevice) ; #DEBUG_LINE_NO:221
    Return False ; #DEBUG_LINE_NO:222
  EndIf
  zadcfurniturescript fs = FurnitureDevice as zadcfurniturescript ; #DEBUG_LINE_NO:224
  If !fs || fs.User as Bool ; #DEBUG_LINE_NO:225
    Return False ; #DEBUG_LINE_NO:226
  EndIf
  If OverridePose ; #DEBUG_LINE_NO:228
    Self.SetOverridePose(FurnitureDevice, OverridePose) ; #DEBUG_LINE_NO:229
  EndIf
  fs.LockActor(akActor) ; #DEBUG_LINE_NO:231
  If OverridePose ; #DEBUG_LINE_NO:232
    Self.ClearOverridePose(FurnitureDevice) ; #DEBUG_LINE_NO:233
  EndIf
  Return True ; #DEBUG_LINE_NO:235
EndFunction

Bool Function UnlockActor(Actor akActor)
  zadcfurniturescript fs = Self.GetDevice(akActor) as zadcfurniturescript ; #DEBUG_LINE_NO:240
  If fs as Bool && fs.User as Bool ; #DEBUG_LINE_NO:241
    fs.UnlockActor() ; #DEBUG_LINE_NO:242
  EndIf
EndFunction

Bool Function PlaySexScene(Actor User, Actor Partner, String AnimationName)
  If User.WornHasKeyword(libs.zad_DeviousBelt) || !Self.GetIsFemale(User) ; #DEBUG_LINE_NO:250
    Return False ; #DEBUG_LINE_NO:252
  EndIf
  zadcfurniturescript fs = Self.GetDevice(User) as zadcfurniturescript ; #DEBUG_LINE_NO:254
  If (fs as Bool && fs.User as Bool) && fs.SexAnimations.Length > 0 ; #DEBUG_LINE_NO:255
    If fs.SexScene(Partner, AnimationName) ; #DEBUG_LINE_NO:256
      Return True ; #DEBUG_LINE_NO:257
    EndIf
  EndIf
  Return False ; #DEBUG_LINE_NO:260
EndFunction

Bool Function GetIsTransportable(ObjectReference FurnitureDevice)
  If !Self.GetIsFurnitureDevice(FurnitureDevice) ; #DEBUG_LINE_NO:265
    Return False ; #DEBUG_LINE_NO:266
  EndIf
  zadcfurniturescript fs = FurnitureDevice as zadcfurniturescript ; #DEBUG_LINE_NO:268
  If fs as Bool && fs.CanBePickedUp ; #DEBUG_LINE_NO:269
    Return True ; #DEBUG_LINE_NO:270
  EndIf
  Return False ; #DEBUG_LINE_NO:272
EndFunction

Bool Function GetHasBlueprint(ObjectReference FurnitureDevice)
  If !Self.GetIsFurnitureDevice(FurnitureDevice) ; #DEBUG_LINE_NO:277
    Return False ; #DEBUG_LINE_NO:278
  EndIf
  zadcfurniturescript fs = FurnitureDevice as zadcfurniturescript ; #DEBUG_LINE_NO:280
  If fs as Bool && fs.Blueprint != None ; #DEBUG_LINE_NO:281
    Return True ; #DEBUG_LINE_NO:282
  EndIf
  Return False ; #DEBUG_LINE_NO:284
EndFunction

MiscObject Function GetBlueprint(ObjectReference FurnitureDevice)
  If !Self.GetIsFurnitureDevice(FurnitureDevice) ; #DEBUG_LINE_NO:289
    Return None ; #DEBUG_LINE_NO:290
  EndIf
  zadcfurniturescript fs = FurnitureDevice as zadcfurniturescript ; #DEBUG_LINE_NO:292
  If fs as Bool && fs.Blueprint != None ; #DEBUG_LINE_NO:293
    Return fs.Blueprint ; #DEBUG_LINE_NO:294
  EndIf
  Return None ; #DEBUG_LINE_NO:296
EndFunction

Bool Function SetIsTransportable(ObjectReference FurnitureDevice)
  If !Self.GetIsFurnitureDevice(FurnitureDevice) ; #DEBUG_LINE_NO:301
    Return False ; #DEBUG_LINE_NO:302
  EndIf
  zadcfurniturescript fs = FurnitureDevice as zadcfurniturescript ; #DEBUG_LINE_NO:304
  If fs ; #DEBUG_LINE_NO:305
    fs.CanBePickedUp = True ; #DEBUG_LINE_NO:306
    Return True ; #DEBUG_LINE_NO:307
  EndIf
  Return False ; #DEBUG_LINE_NO:309
EndFunction

Bool Function SetBlueprint(ObjectReference FurnitureDevice, MiscObject Blueprint)
  If !Self.GetIsFurnitureDevice(FurnitureDevice) || !Blueprint.HasKeyword(zadc_FurnitureKit) ; #DEBUG_LINE_NO:314
    Return False ; #DEBUG_LINE_NO:315
  EndIf
  zadcfurniturescript fs = FurnitureDevice as zadcfurniturescript ; #DEBUG_LINE_NO:317
  If fs ; #DEBUG_LINE_NO:318
    fs.Blueprint = Blueprint ; #DEBUG_LINE_NO:319
    Return True ; #DEBUG_LINE_NO:320
  EndIf
  Return False ; #DEBUG_LINE_NO:322
EndFunction

ObjectReference Function BobTheBuilder(Activator FurnitureToBuild, ObjectReference WhereToBuild)
  If !WhereToBuild ; #DEBUG_LINE_NO:327
    WhereToBuild = libs.PlayerRef as ObjectReference ; #DEBUG_LINE_NO:328
  EndIf
  Self.CloseMenus() ; #DEBUG_LINE_NO:330
  ObjectReference newFurniture = WhereToBuild.PlaceAtMe(FurnitureToBuild as Form, 1, True, False) ; #DEBUG_LINE_NO:331
  Self.AlignObject(newFurniture, WhereToBuild) ; #DEBUG_LINE_NO:332
  Utility.Wait(0.5) ; #DEBUG_LINE_NO:333
  Self.MoveObjectByVector(WhereToBuild, newFurniture, 100.0) ; #DEBUG_LINE_NO:334
  (newFurniture as zadcfurniturescript).CanBePickedUp = True ; #DEBUG_LINE_NO:335
  libs.Log((((("Placed contraption " + newFurniture.GetBaseObject().GetName() + " in cell " + newFurniture.GetParentCell() as String) + " at location (" + newFurniture.X as String) + " " + newFurniture.Y as String) + " " + newFurniture.Z as String) + ").", 0) ; #DEBUG_LINE_NO:336
  Return newFurniture ; #DEBUG_LINE_NO:337
EndFunction

Event OnInit()
  Self.RegisterForSingleUpdate(10.0) ; #DEBUG_LINE_NO:343
EndEvent

Event OnUpdate()
  If !_initiated ; #DEBUG_LINE_NO:348
    Self.InitLibrary() ; #DEBUG_LINE_NO:349
    _initiated = True ; #DEBUG_LINE_NO:350
  EndIf
EndEvent

Function InitLibrary()
  Self.RegisterForModEvent("zad_RegisteredMCMKeys", "OnRegisterMCMKeys") ; #DEBUG_LINE_NO:355
  Self.UnregisterForAllKeys() ; #DEBUG_LINE_NO:356
  Self.RegisterForKey(libs.Config.FurnitureNPCActionKey) ; #DEBUG_LINE_NO:357
  cAnims.LoadAnimations() ; #DEBUG_LINE_NO:358
EndFunction

Function OnRegisterMCMKeys(String eventName, String strArg, Float numArg, Form sender)
  Self.UnregisterForAllKeys() ; #DEBUG_LINE_NO:365
  Self.RegisterForKey(libs.Config.FurnitureNPCActionKey) ; #DEBUG_LINE_NO:366
EndFunction

Function Reinitialize()
  Self.Reset() ; #DEBUG_LINE_NO:370
  Utility.Wait(0.5) ; #DEBUG_LINE_NO:371
  Self.Stop() ; #DEBUG_LINE_NO:372
  Utility.Wait(0.5) ; #DEBUG_LINE_NO:373
  Self.Start() ; #DEBUG_LINE_NO:374
EndFunction

Function OnKeyDown(Int KeyCode)
  If ui.IsMenuOpen("Console") || ui.IsMenuOpen("Console Native UI Menu") ; #DEBUG_LINE_NO:378
    Return  ; #DEBUG_LINE_NO:379
  EndIf
  If KeyCode == libs.Config.FurnitureNPCActionKey ; #DEBUG_LINE_NO:381
    Self.FurnitureAction() ; #DEBUG_LINE_NO:382
  EndIf
  If KeyCode == 209 ; #DEBUG_LINE_NO:384
    Self.Test() ; #DEBUG_LINE_NO:385
  EndIf
EndFunction

Bool Function GetIsFemale(Actor act)
  Return act.GetLeveledActorBase().GetSex() == 1 ; #DEBUG_LINE_NO:390
EndFunction

Function Test()
  Actor usr = Game.GetCurrentCrosshairRef() as Actor ; #DEBUG_LINE_NO:394
  Self.PlaySexScene(usr, libs.PlayerRef, "") ; #DEBUG_LINE_NO:395
  Return  ; #DEBUG_LINE_NO:396
  Self.Reinitialize() ; #DEBUG_LINE_NO:397
  Return  ; #DEBUG_LINE_NO:398
  Utility.Wait(1.0) ; #DEBUG_LINE_NO:399
  Self.FindFurnitureDevicesInCell() ; #DEBUG_LINE_NO:400
  libs.notify(("Found: " + zadc_CellFurnitureList.GetSize() as String) + " devices!", False) ; #DEBUG_LINE_NO:401
  Self.LockActor(libs.PlayerRef, zadc_CellFurnitureList.GetAt(0) as ObjectReference, None) ; #DEBUG_LINE_NO:402
  Return  ; #DEBUG_LINE_NO:404
  ObjectReference objr = Game.GetCurrentCrosshairRef() ; #DEBUG_LINE_NO:405
  Actor act = objr as Actor ; #DEBUG_LINE_NO:406
  If act as Bool && act == Game.GetPlayer() ; #DEBUG_LINE_NO:407
    Return  ; #DEBUG_LINE_NO:409
  EndIf
  libs.equipDevice(act, libs.gagBall, libs.gagBallRendered, libs.zad_DeviousGag, False, True) ; #DEBUG_LINE_NO:411
  libs.equipDevice(act, libs.zad_armBinderHisec_Inventory, libs.zad_armBinderHisec_Rendered, libs.zad_DeviousHeavyBondage, False, True) ; #DEBUG_LINE_NO:412
  libs.equipDevice(act, libs.collarRestrictive, libs.collarRestrictiveRendered, libs.zad_DeviousCollar, False, True) ; #DEBUG_LINE_NO:413
EndFunction

Bool Function IsAnimating(Actor akActor)
  If akActor.IsOnMount() ; #DEBUG_LINE_NO:418
    Return True ; #DEBUG_LINE_NO:419
  EndIf
  Return akActor.IsInFaction(libs.zadAnimatingFaction) || akActor.IsInFaction(libs.SexLab.AnimatingFaction) ; #DEBUG_LINE_NO:421
EndFunction

Function FurnitureAction()
  Self.FurnitureActionV2(False) ; #DEBUG_LINE_NO:427
  Return  ; #DEBUG_LINE_NO:427
EndFunction

Function FurnitureActionV2(Bool AllowActorInScene)
  ObjectReference objr = Game.GetCurrentCrosshairRef() ; #DEBUG_LINE_NO:433
  Actor act = objr as Actor ; #DEBUG_LINE_NO:434
  If act as Bool && act == Game.GetPlayer() ; #DEBUG_LINE_NO:435
    Return  ; #DEBUG_LINE_NO:437
  EndIf
  If objr.HasKeyword(zadc_FurnitureDevice) && SelectedUser as Bool ; #DEBUG_LINE_NO:439
    If !Self.GetUser(objr) ; #DEBUG_LINE_NO:440
      Debug.notification("Locking " + SelectedUser.GetActorBase().GetName() + " into device.") ; #DEBUG_LINE_NO:441
      objr.Activate(SelectedUser as ObjectReference, False) ; #DEBUG_LINE_NO:442
      Return  ; #DEBUG_LINE_NO:443
    Else
      Debug.notification("This device is occupied.") ; #DEBUG_LINE_NO:445
    EndIf
  EndIf
  If objr.HasKeyword(zadc_FurnitureDevice) && !SelectedUser ; #DEBUG_LINE_NO:448
    
  EndIf
  If act ; #DEBUG_LINE_NO:452
    ObjectReference obj = Self.GetDevice(act) ; #DEBUG_LINE_NO:454
    If obj ; #DEBUG_LINE_NO:455
      Debug.notification("Freeing " + act.GetActorBase().GetName()) ; #DEBUG_LINE_NO:457
      obj.Activate(Game.GetPlayer() as ObjectReference, False) ; #DEBUG_LINE_NO:459
      Return  ; #DEBUG_LINE_NO:460
    EndIf
    If !SelectedUser ; #DEBUG_LINE_NO:462
      If !Self.IsValidActorV2(act, AllowActorInScene) ; #DEBUG_LINE_NO:464
        Debug.notification(act.GetLeveledActorBase().GetName() + " is invalid or busy.") ; #DEBUG_LINE_NO:465
        Return  ; #DEBUG_LINE_NO:466
      EndIf
      SelectedUser = act ; #DEBUG_LINE_NO:468
      Debug.notification("Selected " + act.GetActorBase().GetName() + " for furniture placement.") ; #DEBUG_LINE_NO:469
      actorutil.AddPackageOverride(SelectedUser, zadc_pk_donothing, 95, 0) ; #DEBUG_LINE_NO:470
      SelectedUser.EvaluatePackage() ; #DEBUG_LINE_NO:471
      If !SelectedUser.WornHasKeyword(libs.zad_DeviousHeavyBondage) && !SelectedUser.WornHasKeyword(libs.zad_DeviousLegCuffs) && !SelectedUser.WornHasKeyword(libs.zad_DeviousArmCuffs) && Self.GetIsFemale(SelectedUser) ; #DEBUG_LINE_NO:472
        libs.equipDevice(SelectedUser, zadc_PrisonerChainsInventory, zadc_PrisonerChainsRendered, libs.zad_DeviousHeavyBondage, False, True) ; #DEBUG_LINE_NO:474
      EndIf
      Return  ; #DEBUG_LINE_NO:476
    ElseIf SelectedUser == act ; #DEBUG_LINE_NO:479
      Debug.notification("Unselected " + act.GetActorBase().GetName() + " for furniture placement.") ; #DEBUG_LINE_NO:481
      If SelectedUser.IsEquipped(zadc_PrisonerChainsRendered as Form) ; #DEBUG_LINE_NO:482
        libs.removeDevice(SelectedUser, zadc_PrisonerChainsInventory, zadc_PrisonerChainsRendered, libs.zad_DeviousHeavyBondage, True, False, True) ; #DEBUG_LINE_NO:483
        Utility.Wait(1.0) ; #DEBUG_LINE_NO:484
      EndIf
      actorutil.RemovePackageOverride(SelectedUser, zadc_pk_donothing) ; #DEBUG_LINE_NO:486
      SelectedUser.EvaluatePackage() ; #DEBUG_LINE_NO:487
      SelectedUser = None ; #DEBUG_LINE_NO:488
      Return  ; #DEBUG_LINE_NO:489
    Else
      If !Self.IsValidActorV2(act, AllowActorInScene) ; #DEBUG_LINE_NO:492
        Debug.notification(act.GetLeveledActorBase().GetName() + " is invalid or busy.") ; #DEBUG_LINE_NO:493
        Return  ; #DEBUG_LINE_NO:494
      EndIf
      Actor OldTarget = SelectedUser ; #DEBUG_LINE_NO:497
      SelectedUser = act ; #DEBUG_LINE_NO:498
      Debug.notification("Selected " + act.GetActorBase().GetName() + " for furniture placement.") ; #DEBUG_LINE_NO:499
      actorutil.AddPackageOverride(SelectedUser, zadc_pk_donothing, 95, 0) ; #DEBUG_LINE_NO:500
      SelectedUser.EvaluatePackage() ; #DEBUG_LINE_NO:501
      If !SelectedUser.WornHasKeyword(libs.zad_DeviousHeavyBondage) && !SelectedUser.WornHasKeyword(libs.zad_DeviousLegCuffs) && !SelectedUser.WornHasKeyword(libs.zad_DeviousArmCuffs) && Self.GetIsFemale(SelectedUser) ; #DEBUG_LINE_NO:502
        libs.equipDevice(SelectedUser, zadc_PrisonerChainsInventory, zadc_PrisonerChainsRendered, libs.zad_DeviousHeavyBondage, False, True) ; #DEBUG_LINE_NO:504
      EndIf
      If OldTarget.IsEquipped(zadc_PrisonerChainsRendered as Form) ; #DEBUG_LINE_NO:506
        libs.removeDevice(OldTarget, zadc_PrisonerChainsInventory, zadc_PrisonerChainsRendered, libs.zad_DeviousHeavyBondage, True, False, True) ; #DEBUG_LINE_NO:507
      EndIf
      actorutil.RemovePackageOverride(OldTarget, zadc_pk_donothing) ; #DEBUG_LINE_NO:509
      OldTarget.EvaluatePackage() ; #DEBUG_LINE_NO:510
      Return  ; #DEBUG_LINE_NO:511
    EndIf
  EndIf
EndFunction

Function CloseMenus()
  Game.DisablePlayerControls(True, True, False, False, False, True, True, False, 0) ; #DEBUG_LINE_NO:518
  Game.EnablePlayerControls(True, True, True, True, True, True, True, True, 0) ; #DEBUG_LINE_NO:519
EndFunction

Function AlignObject(ObjectReference ObjectToAlign, ObjectReference ObjectToAlignWith)
  ObjectToAlign.MoveTo(ObjectToAlignWith, 0.0, 0.0, 0.0, True) ; #DEBUG_LINE_NO:523
  ObjectToAlign.SetAngle(0.0, ObjectToAlignWith.GetAngleY(), ObjectToAlignWith.GetAngleZ()) ; #DEBUG_LINE_NO:524
EndFunction

Function MoveObjectByVector(ObjectReference ObjectToMove, ObjectReference ObjectToMoveFrom, Float Distance)
  Float Angle = ObjectToMoveFrom.GetAngleZ() ; #DEBUG_LINE_NO:529
  If Angle < 90.0 ; #DEBUG_LINE_NO:532
    Angle = 90.0 - Angle ; #DEBUG_LINE_NO:533
  Else
    Angle = 450.0 - Angle ; #DEBUG_LINE_NO:535
  EndIf
  Float X = Math.Sin(Angle) * Distance ; #DEBUG_LINE_NO:537
  Float Y = Math.Cos(Angle) * Distance ; #DEBUG_LINE_NO:538
  ObjectToMove.MoveTo(ObjectToMoveFrom, X, Y, 0.0, False) ; #DEBUG_LINE_NO:539
EndFunction

Function StripOutfit(Actor akActor)
  If akActor != libs.PlayerRef ; #DEBUG_LINE_NO:544
    ActorBase actB = akActor.GetLeveledActorBase() ; #DEBUG_LINE_NO:545
    storageutil.SetFormValue(akActor as Form, "DDC_Outfit1", actB.GetOutfit(False) as Form) ; #DEBUG_LINE_NO:546
    storageutil.SetFormValue(akActor as Form, "DDC_Outfit2", actB.GetOutfit(True) as Form) ; #DEBUG_LINE_NO:547
    actB.SetOutfit(zadc_outfit_naked, False) ; #DEBUG_LINE_NO:548
    actB.SetOutfit(zadc_outfit_naked, True) ; #DEBUG_LINE_NO:549
  EndIf
  If storageutil.FormListCount(akActor as Form, "DCC_Gear") > 0 ; #DEBUG_LINE_NO:551
    Return  ; #DEBUG_LINE_NO:552
  EndIf
  Form[] Gear = libs.SexLab.StripActor(akActor, None, False, False) ; #DEBUG_LINE_NO:554
  Int i = 0 ; #DEBUG_LINE_NO:555
  While i < Gear.Length ; #DEBUG_LINE_NO:556
    storageutil.FormListAdd(akActor as Form, "DCC_Gear", Gear[i], True) ; #DEBUG_LINE_NO:557
    i += 1 ; #DEBUG_LINE_NO:558
  EndWhile
EndFunction

Function RestoreOutfit(Actor akActor)
  If akActor != libs.PlayerRef ; #DEBUG_LINE_NO:564
    ActorBase actB = akActor.GetLeveledActorBase() ; #DEBUG_LINE_NO:565
    Outfit Outfit1 = storageutil.GetFormValue(akActor as Form, "DDC_Outfit1", None) as Outfit ; #DEBUG_LINE_NO:566
    Outfit Outfit2 = storageutil.GetFormValue(akActor as Form, "DDC_Outfit2", None) as Outfit ; #DEBUG_LINE_NO:567
    If Outfit1 ; #DEBUG_LINE_NO:568
      actB.SetOutfit(Outfit1, False) ; #DEBUG_LINE_NO:569
    EndIf
    If Outfit2 ; #DEBUG_LINE_NO:571
      actB.SetOutfit(Outfit2, True) ; #DEBUG_LINE_NO:572
    EndIf
    Int Count = Outfit1.GetNumParts() ; #DEBUG_LINE_NO:574
    While Count > 0 ; #DEBUG_LINE_NO:575
      akActor.EquipItem(Outfit1.GetNthPart(Count - 1), False, False) ; #DEBUG_LINE_NO:576
      Count -= 1 ; #DEBUG_LINE_NO:577
    EndWhile
  EndIf
  If storageutil.FormListCount(akActor as Form, "DCC_Gear") == 0 ; #DEBUG_LINE_NO:580
    Return  ; #DEBUG_LINE_NO:581
  EndIf
  libs.SexLab.UnstripActor(akActor, storageutil.FormListToArray(akActor as Form, "DCC_Gear"), False) ; #DEBUG_LINE_NO:583
  storageutil.FormListClear(akActor as Form, "DCC_Gear") ; #DEBUG_LINE_NO:584
EndFunction

Function ClearDevice(Actor act)
  storageutil.UnSetFormValue(act as Form, "DDC_DeviceUsed") ; #DEBUG_LINE_NO:588
EndFunction

Function StoreDevice(Actor act, ObjectReference obj)
  storageutil.SetFormValue(act as Form, "DDC_DeviceUsed", obj as Form) ; #DEBUG_LINE_NO:592
EndFunction

Function StoreBondage(Actor akActor, Keyword[] InvalidDevices, Bool Force)
  If !akActor.WornHasKeyword(libs.zad_Lockable) ; #DEBUG_LINE_NO:603
    Return  ; #DEBUG_LINE_NO:605
  EndIf
  Bool NeedsAAReset = False ; #DEBUG_LINE_NO:608
  Int i = 31 ; #DEBUG_LINE_NO:609
  While i >= 0 ; #DEBUG_LINE_NO:610
    Form Item = akActor.GetWornForm(Armor.GetMaskForSlot(i + 30)) ; #DEBUG_LINE_NO:611
    If Item as Bool && Item.HasKeyword(libs.zad_Lockable) ; #DEBUG_LINE_NO:612
      If Force ; #DEBUG_LINE_NO:614
        akActor.UnequipItem(Item, False, True) ; #DEBUG_LINE_NO:615
        storageutil.FormListAdd(akActor as Form, "DDC_StoredBondage", Item, True) ; #DEBUG_LINE_NO:616
        If Item.HasKeyword(libs.zad_DeviousHeavyBondage) || Item.HasKeyword(libs.zad_DeviousHobbleSkirt) || Item.HasKeyword(libs.zad_DeviousPonyGear) ; #DEBUG_LINE_NO:617
          NeedsAAReset = True ; #DEBUG_LINE_NO:619
        EndIf
        If Item.HasKeyword(libs.zad_DeviousGag) ; #DEBUG_LINE_NO:621
          libs.RemoveGagEffect(akActor) ; #DEBUG_LINE_NO:623
        EndIf
      Else
        Int Z = InvalidDevices.Length ; #DEBUG_LINE_NO:626
        While Z > 0 ; #DEBUG_LINE_NO:627
          Z -= 1 ; #DEBUG_LINE_NO:628
          If Item.HasKeyword(InvalidDevices[Z]) ; #DEBUG_LINE_NO:629
            akActor.UnequipItem(Item, False, True) ; #DEBUG_LINE_NO:631
            storageutil.FormListAdd(akActor as Form, "DDC_StoredBondage", Item, True) ; #DEBUG_LINE_NO:632
            If Item.HasKeyword(libs.zad_DeviousHeavyBondage) || Item.HasKeyword(libs.zad_DeviousHobbleSkirt) || Item.HasKeyword(libs.zad_DeviousPonyGear) ; #DEBUG_LINE_NO:633
              NeedsAAReset = True ; #DEBUG_LINE_NO:635
            EndIf
          EndIf
        EndWhile
      EndIf
    EndIf
    i -= 1 ; #DEBUG_LINE_NO:641
  EndWhile
  If NeedsAAReset
    
  EndIf
EndFunction

Function RestoreBondage(Actor akActor)
  Bool NeedsAAReset = False ; #DEBUG_LINE_NO:651
  Int i = storageutil.FormListCount(akActor as Form, "DDC_StoredBondage") ; #DEBUG_LINE_NO:652
  While i > 0 ; #DEBUG_LINE_NO:653
    i -= 1 ; #DEBUG_LINE_NO:654
    Armor Item = storageutil.FormListGet(akActor as Form, "DDC_StoredBondage", i) as Armor ; #DEBUG_LINE_NO:655
    If Item ; #DEBUG_LINE_NO:656
      akActor.EquipItem(Item as Form, True, True) ; #DEBUG_LINE_NO:657
      If Item.HasKeyword(libs.zad_DeviousHeavyBondage) || Item.HasKeyword(libs.zad_DeviousHobbleSkirt) || Item.HasKeyword(libs.zad_DeviousPonyGear) ; #DEBUG_LINE_NO:658
        NeedsAAReset = True ; #DEBUG_LINE_NO:660
      EndIf
    EndIf
  EndWhile
  storageutil.FormListClear(akActor as Form, "DDC_StoredBondage") ; #DEBUG_LINE_NO:664
  If NeedsAAReset ; #DEBUG_LINE_NO:665
    libs.BoundCombat.EvaluateAA(akActor) ; #DEBUG_LINE_NO:666
  EndIf
EndFunction

Function SetNiOverrideOverride(Actor aktarget)
  String NINODE_ROOT = "NPC" ; #DEBUG_LINE_NO:671
  String NIOO_KEY = "DDC" ; #DEBUG_LINE_NO:672
  Int isFemale = aktarget.GetLeveledActorBase().GetSex() ; #DEBUG_LINE_NO:674
  If nioverride.HasNodeTransformPosition(aktarget as ObjectReference, False, isFemale as Bool, NINODE_ROOT, "internal") ; #DEBUG_LINE_NO:675
    Float[] pos = nioverride.GetNodeTransformPosition(aktarget as ObjectReference, False, isFemale as Bool, NINODE_ROOT, "internal") ; #DEBUG_LINE_NO:676
    pos[0] = -pos[0] ; #DEBUG_LINE_NO:677
    pos[1] = -pos[1] ; #DEBUG_LINE_NO:678
    pos[2] = -pos[2] ; #DEBUG_LINE_NO:679
    nioverride.AddNodeTransformPosition(aktarget as ObjectReference, False, isFemale as Bool, NINODE_ROOT, NIOO_KEY, pos) ; #DEBUG_LINE_NO:680
    nioverride.UpdateNodeTransform(aktarget as ObjectReference, False, isFemale as Bool, NINODE_ROOT) ; #DEBUG_LINE_NO:681
  EndIf
EndFunction

Function ResetNiOverrideOverride(Actor aktarget)
  String NINODE_ROOT = "NPC" ; #DEBUG_LINE_NO:686
  String NIOO_KEY = "DDC" ; #DEBUG_LINE_NO:687
  Int isFemale = aktarget.GetLeveledActorBase().GetSex() ; #DEBUG_LINE_NO:688
  If nioverride.HasNodeTransformPosition(aktarget as ObjectReference, False, isFemale as Bool, NINODE_ROOT, NIOO_KEY) ; #DEBUG_LINE_NO:689
    nioverride.RemoveNodeTransformPosition(aktarget as ObjectReference, False, isFemale as Bool, NINODE_ROOT, NIOO_KEY) ; #DEBUG_LINE_NO:690
    nioverride.UpdateNodeTransform(aktarget as ObjectReference, False, isFemale as Bool, NINODE_ROOT) ; #DEBUG_LINE_NO:691
  EndIf
EndFunction
