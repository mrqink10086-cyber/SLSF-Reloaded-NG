.info
	.source "SLSFR_EventBridge.psc"
	.modifyTime 1773208267 ;Wed Mar 11 13:51:07 2026 Local
	.compileTime 1773208525 ;Wed Mar 11 13:55:25 2026 Local
	.user "15219"
	.computer "MMM"
.endInfo
.userFlagsRef
	.flag conditional 1	; 0x00000001
	.flag hidden 0	; 0x00000000
.endUserFlagsRef
.objectTable
	.object SLSFR_EventBridge Quest
		.userFlags 0	; Flags: 0x00000000
		.docString ""
		.autoState 
		.variableTable
			.variable _isInLeadIn Bool
				.userFlags 0	; Flags: 0x00000000
				.initialValue False
			.endVariable
			.variable ::SexLab_var sexlabframework
				.userFlags 0	; Flags: 0x00000000
				.initialValue None
			.endVariable
			.variable ::UpdateIntervalHours_var Float
				.userFlags 0	; Flags: 0x00000000
				.initialValue 1.000000
			.endVariable
		.endVariableTable
		.propertyTable
			.property SexLab sexlabframework auto
				.userFlags 0	; Flags: 0x00000000
				.docString ""
				.autoVar ::SexLab_var
			.endProperty
			.property UpdateIntervalHours Float auto
				.userFlags 0	; Flags: 0x00000000
				.docString ""
				.autoVar ::UpdateIntervalHours_var
			.endProperty
		.endPropertyTable
		.stateTable
			.state 
				.function SetTattooCfg native static
					.userFlags 0	; Flags: 0x00000000
					.docString ""
					.return None
					.paramTable
						.param bodySlots Int
						.param faceSlots Int
						.param handSlots Int
						.param footSlots Int
					.endParamTable
				.endFunction
				.function RunFullUpdate native static
					.userFlags 0	; Flags: 0x00000000
					.docString ""
					.return Int
					.paramTable
					.endParamTable
				.endFunction
				.function SetBodySubcat native static
					.userFlags 0	; Flags: 0x00000000
					.docString ""
					.return None
					.paramTable
						.param slot Int
						.param subcat Int
					.endParamTable
				.endFunction
				.function OnSexLabAnimEnd native static
					.userFlags 0	; Flags: 0x00000000
					.docString ""
					.return None
					.paramTable
						.param locName String
						.param actorCount Int
						.param hasMale Bool
						.param hasFemale Bool
						.param hasCreature Bool
						.param isVictim Bool
						.param isAggressor Bool
						.param isForeplay Bool
						.param hasOrc Bool
						.param hasKhajiit Bool
						.param hasArgonian Bool
						.param tags String
					.endParamTable
				.endFunction
				.function OnGameTimePassed native static
					.userFlags 0	; Flags: 0x00000000
					.docString ""
					.return None
					.paramTable
						.param hours Float
					.endParamTable
				.endFunction
				.function SetTattooData native static
					.userFlags 0	; Flags: 0x00000000
					.docString ""
					.return None
					.paramTable
						.param area Int
						.param appliedLo Int
						.param appliedHi Int
						.param excludedLo Int
						.param excludedHi Int
					.endParamTable
				.endFunction
				.function OnSexLabOrgasm native static
					.userFlags 0	; Flags: 0x00000000
					.docString ""
					.return None
					.paramTable
						.param locName String
						.param isVictim Bool
					.endParamTable
				.endFunction
				.function OnSLOrgasm
					.userFlags 0	; Flags: 0x00000000
					.docString ""
					.return None
					.paramTable
						.param aiThreadID Int
						.param abHasPlayer Bool
					.endParamTable
					.localTable
						.local ::temp11 Bool
						.local ::temp12 sexlabthread
						.local ::temp13 actor
						.local ::temp14 String
						.local ::temp15 Bool
						.local ::NoneVar None
						.local thread sexlabthread
						.local playerRef actor
						.local locName String
						.local isVictim Bool
					.endLocalTable
					.code
						Not ::temp11 abHasPlayer                                 ;@line 150
						JumpF ::temp11 _label0                                   ;@line 150
						Return None                                              ;@line 151
						Jump _label0                                             ;@line 151
					_label0:
						CallMethod GetThread ::SexLab_var ::temp12 aiThreadID    ;@line 154
						Assign thread ::temp12                                   ;@line 154
						Cast ::temp12 None                                       ;@line 155
						CompareEQ ::temp11 thread ::temp12                       ;@line 155
						JumpF ::temp11 _label1                                   ;@line 155
						Return None                                              ;@line 156
						Jump _label1                                             ;@line 156
					_label1:
						CallStatic game GetPlayer ::temp13                       ;@line 159
						Assign playerRef ::temp13                                ;@line 159
						CallMethod GetLocationName self ::temp14                 ;@line 160
						Assign locName ::temp14                                  ;@line 160
						CallMethod isConsent thread ::temp11                     ;@line 161
						Not ::temp11 ::temp11                                    ;@line 161
						Cast ::temp11 ::temp11                                   ;@line 161
						JumpF ::temp11 _label2                                   ;@line 161
						CallMethod GetSubmissive thread ::temp15 playerRef       ;@line 161
						Cast ::temp11 ::temp15                                   ;@line 161
					_label2:
						Assign isVictim ::temp11                                 ;@line 161
						CallStatic SLSFR_EventBridge OnSexLabOrgasm ::NoneVar locName isVictim  ;@line 164
					.endCode
				.endFunction
				.function OnSLLeadInEnd
					.userFlags 0	; Flags: 0x00000000
					.docString ""
					.return None
					.paramTable
						.param aiThreadID Int
						.param abHasPlayer Bool
					.endParamTable
					.localTable
					.endLocalTable
					.code
						JumpF abHasPlayer _label3                                ;@line 184
						Assign _isInLeadIn False                                 ;@line 185
						Jump _label3                                             ;@line 185
					_label3:
					.endCode
				.endFunction
				.function OnSLStageStart
					.userFlags 0	; Flags: 0x00000000
					.docString ""
					.return None
					.paramTable
						.param aiThreadID Int
						.param abHasPlayer Bool
					.endParamTable
					.localTable
					.endLocalTable
					.code
					.endCode
				.endFunction
				.function OnSLLeadInStart
					.userFlags 0	; Flags: 0x00000000
					.docString ""
					.return None
					.paramTable
						.param aiThreadID Int
						.param abHasPlayer Bool
					.endParamTable
					.localTable
					.endLocalTable
					.code
						JumpF abHasPlayer _label4                                ;@line 178
						Assign _isInLeadIn True                                  ;@line 179
						Jump _label4                                             ;@line 179
					_label4:
					.endCode
				.endFunction
				.function AppendTag
					.userFlags 0	; Flags: 0x00000000
					.docString ""
					.return String
					.paramTable
						.param existing String
						.param tag String
					.endParamTable
					.localTable
						.local ::temp20 Bool
						.local ::temp21 String
					.endLocalTable
					.code
						CompareEQ ::temp20 existing ""                           ;@line 259
						JumpF ::temp20 _label5                                   ;@line 259
						Return tag                                               ;@line 260
						Jump _label5                                             ;@line 260
					_label5:
						StrCat ::temp21 existing ","                             ;@line 262
						StrCat ::temp21 ::temp21 tag                             ;@line 262
						Return ::temp21                                          ;@line 262
					.endCode
				.endFunction
				.function OnGameLoaded
					.userFlags 0	; Flags: 0x00000000
					.docString ""
					.return None
					.paramTable
					.endParamTable
					.localTable
						.local ::NoneVar None
					.endLocalTable
					.code
						CallMethod RegisterAll self ::NoneVar                    ;@line 47
					.endCode
				.endFunction
				.function GetLocationName
					.userFlags 0	; Flags: 0x00000000
					.docString ""
					.return String
					.paramTable
					.endParamTable
					.localTable
						.local ::temp30 actor
						.local ::temp31 location
						.local ::temp32 Bool
						.local loc location
						.local ::temp33 String
					.endLocalTable
					.code
						CallStatic game GetPlayer ::temp30                       ;@line 377
						CallMethod GetCurrentLocation ::temp30 ::temp31          ;@line 377
						Assign loc ::temp31                                      ;@line 377
						Cast ::temp31 None                                       ;@line 378
						CompareEQ ::temp32 loc ::temp31                          ;@line 378
						Not ::temp32 ::temp32                                    ;@line 378
						JumpF ::temp32 _label6                                   ;@line 378
						CallMethod GetName loc ::temp33                          ;@line 379
						Return ::temp33                                          ;@line 379
						Jump _label6                                             ;@line 379
					_label6:
						Return ""                                                ;@line 381
					.endCode
				.endFunction
				.function BuildTagString
					.userFlags 0	; Flags: 0x00000000
					.docString ""
					.return String
					.paramTable
						.param thread sexlabthread
					.endParamTable
					.localTable
						.local ::temp16 Bool
						.local ::temp18 Int
						.local ::temp19 Bool
						.local tags String
						.local ::temp17 String
					.endLocalTable
					.code
						Assign tags ""                                           ;@line 194
						CallMethod HasTag thread ::temp16 "Vaginal"              ;@line 196
						JumpF ::temp16 _label7                                   ;@line 196
						CallMethod AppendTag self ::temp17 tags "Vaginal"        ;@line 197
						Assign tags ::temp17                                     ;@line 197
						Jump _label7                                             ;@line 197
					_label7:
						CallMethod HasTag thread ::temp16 "Anal"                 ;@line 199
						JumpF ::temp16 _label8                                   ;@line 199
						CallMethod AppendTag self ::temp17 tags "Anal"           ;@line 200
						Assign tags ::temp17                                     ;@line 200
						Jump _label8                                             ;@line 200
					_label8:
						CallMethod HasTag thread ::temp16 "Oral"                 ;@line 202
						JumpF ::temp16 _label9                                   ;@line 202
						CallMethod AppendTag self ::temp17 tags "Oral"           ;@line 203
						Assign tags ::temp17                                     ;@line 203
						Jump _label9                                             ;@line 203
					_label9:
						CallMethod HasTag thread ::temp16 "Kissing"              ;@line 205
						JumpF ::temp16 _label10                                  ;@line 205
						CallMethod AppendTag self ::temp17 tags "Kissing"        ;@line 206
						Assign tags ::temp17                                     ;@line 206
						Jump _label10                                            ;@line 206
					_label10:
						CallMethod HasTag thread ::temp16 "Masturbation"         ;@line 208
						JumpF ::temp16 _label11                                  ;@line 208
						CallMethod AppendTag self ::temp17 tags "Masturbation"   ;@line 209
						Assign tags ::temp17                                     ;@line 209
						Jump _label11                                            ;@line 209
					_label11:
						CallMethod HasTag thread ::temp16 "Fisting"              ;@line 211
						JumpF ::temp16 _label12                                  ;@line 211
						CallMethod AppendTag self ::temp17 tags "Fisting"        ;@line 212
						Assign tags ::temp17                                     ;@line 212
						Jump _label12                                            ;@line 212
					_label12:
						CallMethod HasTag thread ::temp16 "Footjob"              ;@line 214
						JumpF ::temp16 _label13                                  ;@line 214
						CallMethod AppendTag self ::temp17 tags "Footjob"        ;@line 215
						Assign tags ::temp17                                     ;@line 215
						Jump _label13                                            ;@line 215
					_label13:
						CallMethod HasTag thread ::temp16 "Handjob"              ;@line 217
						JumpF ::temp16 _label14                                  ;@line 217
						CallMethod AppendTag self ::temp17 tags "Handjob"        ;@line 218
						Assign tags ::temp17                                     ;@line 218
						Jump _label14                                            ;@line 218
					_label14:
						CallMethod HasTag thread ::temp16 "Boobjob"              ;@line 220
						JumpF ::temp16 _label15                                  ;@line 220
						CallMethod AppendTag self ::temp17 tags "Boobjob"        ;@line 221
						Assign tags ::temp17                                     ;@line 221
						Jump _label15                                            ;@line 221
					_label15:
						CallMethod HasTag thread ::temp16 "Aggressive"           ;@line 225
						JumpF ::temp16 _label16                                  ;@line 225
						CallMethod AppendTag self ::temp17 tags "Aggressive"     ;@line 226
						Assign tags ::temp17                                     ;@line 226
						Jump _label16                                            ;@line 226
					_label16:
						CallMethod HasTag thread ::temp16 "Loving"               ;@line 228
						JumpF ::temp16 _label17                                  ;@line 228
						CallMethod AppendTag self ::temp17 tags "Loving"         ;@line 229
						Assign tags ::temp17                                     ;@line 229
						Jump _label17                                            ;@line 229
					_label17:
						CallMethod HasTag thread ::temp16 "Rough"                ;@line 231
						JumpF ::temp16 _label18                                  ;@line 231
						CallMethod AppendTag self ::temp17 tags "Rough"          ;@line 232
						Assign tags ::temp17                                     ;@line 232
						Jump _label18                                            ;@line 232
					_label18:
						CallMethod HasTag thread ::temp16 "Bound"                ;@line 234
						JumpF ::temp16 _label19                                  ;@line 234
						CallMethod AppendTag self ::temp17 tags "Bound"          ;@line 235
						Assign tags ::temp17                                     ;@line 235
						Jump _label19                                            ;@line 235
					_label19:
						CallMethod HasTag thread ::temp16 "Gangbang"             ;@line 237
						JumpF ::temp16 _label20                                  ;@line 237
						CallMethod AppendTag self ::temp17 tags "Gangbang"       ;@line 238
						Assign tags ::temp17                                     ;@line 238
						Jump _label20                                            ;@line 238
					_label20:
						CallMethod HasTag thread ::temp16 "Orgy"                 ;@line 240
						JumpF ::temp16 _label21                                  ;@line 240
						CallMethod AppendTag self ::temp17 tags "Orgy"           ;@line 241
						Assign tags ::temp17                                     ;@line 241
						Jump _label21                                            ;@line 241
					_label21:
						CallMethod IsSceneVaginal thread ::temp16                ;@line 245
						Cast ::temp16 ::temp16                                   ;@line 245
						JumpF ::temp16 _label22                                  ;@line 245
						CallStatic stringutil Find ::temp18 tags "Vaginal" 0     ;@line 245
						CompareLT ::temp19 ::temp18 0                            ;@line 245
						Cast ::temp16 ::temp19                                   ;@line 245
					_label22:
						JumpF ::temp16 _label23                                  ;@line 245
						CallMethod AppendTag self ::temp17 tags "Vaginal"        ;@line 246
						Assign tags ::temp17                                     ;@line 246
						Jump _label23                                            ;@line 246
					_label23:
						CallMethod IsSceneAnal thread ::temp19                   ;@line 248
						Cast ::temp19 ::temp19                                   ;@line 248
						JumpF ::temp19 _label24                                  ;@line 248
						CallStatic stringutil Find ::temp18 tags "Anal" 0        ;@line 248
						CompareLT ::temp16 ::temp18 0                            ;@line 248
						Cast ::temp19 ::temp16                                   ;@line 248
					_label24:
						JumpF ::temp19 _label25                                  ;@line 248
						CallMethod AppendTag self ::temp17 tags "Anal"           ;@line 249
						Assign tags ::temp17                                     ;@line 249
						Jump _label25                                            ;@line 249
					_label25:
						CallMethod IsSceneOral thread ::temp16                   ;@line 251
						Cast ::temp16 ::temp16                                   ;@line 251
						JumpF ::temp16 _label26                                  ;@line 251
						CallStatic stringutil Find ::temp18 tags "Oral" 0        ;@line 251
						CompareLT ::temp19 ::temp18 0                            ;@line 251
						Cast ::temp16 ::temp19                                   ;@line 251
					_label26:
						JumpF ::temp16 _label27                                  ;@line 251
						CallMethod AppendTag self ::temp17 tags "Oral"           ;@line 252
						Assign tags ::temp17                                     ;@line 252
						Jump _label27                                            ;@line 252
					_label27:
						Return tags                                              ;@line 255
					.endCode
				.endFunction
				.function ScanSlaveTats
					.userFlags 0	; Flags: 0x00000000
					.docString ""
					.return None
					.paramTable
					.endParamTable
					.localTable
						.local ::temp25 actor
						.local ::temp26 Int
						.local ::NoneVar None
						.local ::temp27 Bool
						.local playerRef actor
						.local bodySlots Int
						.local faceSlots Int
						.local handSlots Int
						.local footSlots Int
						.local appliedLo Int
						.local appliedHi Int
						.local i Int
						.local ::temp28 Bool
						.local ::temp29 Bool
					.endLocalTable
					.code
						CallStatic game GetPlayer ::temp25                       ;@line 311
						Assign playerRef ::temp25                                ;@line 311
						CallStatic slavetats SLOTS ::temp26 "Body"               ;@line 314
						Assign bodySlots ::temp26                                ;@line 314
						CallStatic slavetats SLOTS ::temp26 "Face"               ;@line 315
						Assign faceSlots ::temp26                                ;@line 315
						CallStatic slavetats SLOTS ::temp26 "Hands"              ;@line 316
						Assign handSlots ::temp26                                ;@line 316
						CallStatic slavetats SLOTS ::temp26 "Feet"               ;@line 317
						Assign footSlots ::temp26                                ;@line 317
						CallStatic SLSFR_EventBridge SetTattooCfg ::NoneVar bodySlots faceSlots handSlots footSlots  ;@line 320
						Assign appliedLo 0                                       ;@line 323
						Assign appliedHi 0                                       ;@line 324
						Assign i 0                                               ;@line 325
					_label32:
						CompareLT ::temp27 i bodySlots                           ;@line 326
						JumpF ::temp27 _label28                                  ;@line 326
						CallStatic slavetats get_applied_tattoo_in_slot ::temp26 playerRef "Body" i  ;@line 327
						CompareEQ ::temp28 ::temp26 0                            ;@line 327
						Not ::temp28 ::temp28                                    ;@line 327
						JumpF ::temp28 _label29                                  ;@line 327
						CompareLT ::temp29 i 32                                  ;@line 328
						JumpF ::temp29 _label30                                  ;@line 328
						CallStatic math LeftShift ::temp26 1 i                   ;@line 329
						CallStatic math LogicalOr ::temp26 appliedLo ::temp26    ;@line 329
						Assign appliedLo ::temp26                                ;@line 329
						Jump _label31                                            ;@line 329
					_label30:
						ISubtract ::temp26 i 32                                  ;@line 331
						CallStatic math LeftShift ::temp26 1 ::temp26            ;@line 331
						CallStatic math LogicalOr ::temp26 appliedHi ::temp26    ;@line 331
						Assign appliedHi ::temp26                                ;@line 331
					_label31:
						Jump _label29                                            ;@line 331
					_label29:
						IAdd ::temp26 i 1                                        ;@line 334
						Assign i ::temp26                                        ;@line 334
						Jump _label32                                            ;@line 334
					_label28:
						CallStatic SLSFR_EventBridge SetTattooData ::NoneVar 0 appliedLo appliedHi 0 0  ;@line 336
						Assign appliedLo 0                                       ;@line 339
						Assign i 0                                               ;@line 340
					_label35:
						CompareLT ::temp29 i faceSlots                           ;@line 341
						JumpF ::temp29 _label33                                  ;@line 341
						CallStatic slavetats get_applied_tattoo_in_slot ::temp26 playerRef "Face" i  ;@line 342
						CompareEQ ::temp28 ::temp26 0                            ;@line 342
						Not ::temp28 ::temp28                                    ;@line 342
						JumpF ::temp28 _label34                                  ;@line 342
						CallStatic math LeftShift ::temp26 1 i                   ;@line 343
						CallStatic math LogicalOr ::temp26 appliedLo ::temp26    ;@line 343
						Assign appliedLo ::temp26                                ;@line 343
						Jump _label34                                            ;@line 343
					_label34:
						IAdd ::temp26 i 1                                        ;@line 345
						Assign i ::temp26                                        ;@line 345
						Jump _label35                                            ;@line 345
					_label33:
						CallStatic SLSFR_EventBridge SetTattooData ::NoneVar 1 appliedLo 0 0 0  ;@line 347
						Assign appliedLo 0                                       ;@line 350
						Assign i 0                                               ;@line 351
					_label38:
						CompareLT ::temp27 i handSlots                           ;@line 352
						JumpF ::temp27 _label36                                  ;@line 352
						CallStatic slavetats get_applied_tattoo_in_slot ::temp26 playerRef "Hands" i  ;@line 353
						CompareEQ ::temp28 ::temp26 0                            ;@line 353
						Not ::temp28 ::temp28                                    ;@line 353
						JumpF ::temp28 _label37                                  ;@line 353
						CallStatic math LeftShift ::temp26 1 i                   ;@line 354
						CallStatic math LogicalOr ::temp26 appliedLo ::temp26    ;@line 354
						Assign appliedLo ::temp26                                ;@line 354
						Jump _label37                                            ;@line 354
					_label37:
						IAdd ::temp26 i 1                                        ;@line 356
						Assign i ::temp26                                        ;@line 356
						Jump _label38                                            ;@line 356
					_label36:
						CallStatic SLSFR_EventBridge SetTattooData ::NoneVar 2 appliedLo 0 0 0  ;@line 358
						Assign appliedLo 0                                       ;@line 361
						Assign i 0                                               ;@line 362
					_label41:
						CompareLT ::temp29 i footSlots                           ;@line 363
						JumpF ::temp29 _label39                                  ;@line 363
						CallStatic slavetats get_applied_tattoo_in_slot ::temp26 playerRef "Feet" i  ;@line 364
						CompareEQ ::temp28 ::temp26 0                            ;@line 364
						Not ::temp28 ::temp28                                    ;@line 364
						JumpF ::temp28 _label40                                  ;@line 364
						CallStatic math LeftShift ::temp26 1 i                   ;@line 365
						CallStatic math LogicalOr ::temp26 appliedLo ::temp26    ;@line 365
						Assign appliedLo ::temp26                                ;@line 365
						Jump _label40                                            ;@line 365
					_label40:
						IAdd ::temp26 i 1                                        ;@line 367
						Assign i ::temp26                                        ;@line 367
						Jump _label41                                            ;@line 367
					_label39:
						CallStatic SLSFR_EventBridge SetTattooData ::NoneVar 3 appliedLo 0 0 0  ;@line 369
					.endCode
				.endFunction
				.function ScanAndUpdate
					.userFlags 0	; Flags: 0x00000000
					.docString ""
					.return None
					.paramTable
					.endParamTable
					.localTable
						.local ::temp22 sexlabframework
						.local ::temp23 Bool
						.local ::temp24 Int
						.local hasST Bool
						.local ::NoneVar None
					.endLocalTable
					.code
						Cast ::temp22 None                                       ;@line 296
						CompareEQ ::temp23 ::SexLab_var ::temp22                 ;@line 296
						JumpF ::temp23 _label42                                  ;@line 296
						Return None                                              ;@line 297
						Jump _label42                                            ;@line 297
					_label42:
						CallStatic game GetModByName ::temp24 "SlaveTats.esp"    ;@line 301
						CompareEQ ::temp23 ::temp24 255                          ;@line 301
						Not ::temp23 ::temp23                                    ;@line 301
						Assign hasST ::temp23                                    ;@line 301
						JumpF hasST _label43                                     ;@line 302
						CallMethod ScanSlaveTats self ::NoneVar                  ;@line 303
						Jump _label43                                            ;@line 303
					_label43:
						CallStatic SLSFR_EventBridge RunFullUpdate ::temp24      ;@line 307
					.endCode
				.endFunction
				.function RegisterHooks
					.userFlags 0	; Flags: 0x00000000
					.docString ""
					.return None
					.paramTable
					.endParamTable
					.localTable
						.local ::NoneVar None
					.endLocalTable
					.code
						CallMethod RegisterForModEvent self ::NoneVar "HookAnimationEnd" "OnSLAnimEnd"  ;@line 58
						CallMethod RegisterForModEvent self ::NoneVar "HookAnimationStart" "OnSLAnimStart"  ;@line 59
						CallMethod RegisterForModEvent self ::NoneVar "HookOrgasmStart" "OnSLOrgasm"  ;@line 60
						CallMethod RegisterForModEvent self ::NoneVar "HookStageStart" "OnSLStageStart"  ;@line 61
						CallMethod RegisterForModEvent self ::NoneVar "HookLeadInStart" "OnSLLeadInStart"  ;@line 62
						CallMethod RegisterForModEvent self ::NoneVar "HookLeadInEnd" "OnSLLeadInEnd"  ;@line 63
					.endCode
				.endFunction
				.function OnSLAnimEnd
					.userFlags 0	; Flags: 0x00000000
					.docString ""
					.return None
					.paramTable
						.param aiThreadID Int
						.param abHasPlayer Bool
					.endParamTable
					.localTable
						.local ::temp0 Bool
						.local ::temp1 sexlabthread
						.local ::temp2 actor[]
						.local ::temp3 actor
						.local ::temp4 String
						.local ::temp5 Int
						.local ::NoneVar None
						.local thread sexlabthread
						.local positions actor[]
						.local playerRef actor
						.local locName String
						.local actorCount Int
						.local hasMale Bool
						.local hasFemale Bool
						.local hasCreature Bool
						.local hasOrc Bool
						.local hasKhajiit Bool
						.local hasArgonian Bool
						.local i Int
						.local ::temp6 Bool
						.local a actor
						.local ::temp7 Bool
						.local ::temp8 Bool
						.local ::temp9 Bool
						.local ::temp10 race
						.local sex Int
						.local r race
						.local raceName String
						.local isConsent Bool
						.local isSubmissive Bool
						.local isVictim Bool
						.local isAggressor Bool
						.local isForeplay Bool
						.local tags String
					.endLocalTable
					.code
						Not ::temp0 abHasPlayer                                  ;@line 71
						JumpF ::temp0 _label44                                   ;@line 71
						Return None                                              ;@line 72
						Jump _label44                                            ;@line 72
					_label44:
						CallMethod GetThread ::SexLab_var ::temp1 aiThreadID     ;@line 75
						Assign thread ::temp1                                    ;@line 75
						Cast ::temp1 None                                        ;@line 76
						CompareEQ ::temp0 thread ::temp1                         ;@line 76
						JumpF ::temp0 _label45                                   ;@line 76
						Return None                                              ;@line 77
						Jump _label45                                            ;@line 77
					_label45:
						CallMethod GetPositions thread ::temp2                   ;@line 81
						Assign positions ::temp2                                 ;@line 81
						CallStatic game GetPlayer ::temp3                        ;@line 82
						Assign playerRef ::temp3                                 ;@line 82
						CallMethod GetLocationName self ::temp4                  ;@line 85
						Assign locName ::temp4                                   ;@line 85
						ArrayLength ::temp5 positions                            ;@line 88
						Assign actorCount ::temp5                                ;@line 88
						Assign hasMale False                                     ;@line 89
						Assign hasFemale False                                   ;@line 90
						Assign hasCreature False                                 ;@line 91
						Assign hasOrc False                                      ;@line 92
						Assign hasKhajiit False                                  ;@line 93
						Assign hasArgonian False                                 ;@line 94
						Assign i 0                                               ;@line 96
					_label57:
						CompareLT ::temp0 i actorCount                           ;@line 97
						JumpF ::temp0 _label46                                   ;@line 97
						ArrayGetElement ::temp3 positions i                      ;@line 98
						Assign a ::temp3                                         ;@line 98
						CompareEQ ::temp6 a playerRef                            ;@line 99
						Not ::temp6 ::temp6                                      ;@line 99
						JumpF ::temp6 _label47                                   ;@line 99
						CallMethod GetSex ::SexLab_var ::temp5 a                 ;@line 100
						Assign sex ::temp5                                       ;@line 100
						CompareEQ ::temp7 sex 0                                  ;@line 102
						JumpF ::temp7 _label48                                   ;@line 102
						Assign hasMale True                                      ;@line 103
						Jump _label49                                            ;@line 103
					_label48:
						CompareEQ ::temp8 sex 1                                  ;@line 104
						Cast ::temp8 ::temp8                                     ;@line 104
						JumpT ::temp8 _label50                                   ;@line 104
						CompareEQ ::temp9 sex 2                                  ;@line 104
						Cast ::temp8 ::temp9                                     ;@line 104
					_label50:
						JumpF ::temp8 _label51                                   ;@line 104
						Assign hasFemale True                                    ;@line 105
						Jump _label49                                            ;@line 105
					_label51:
						CompareGTE ::temp9 sex 3                                 ;@line 106
						JumpF ::temp9 _label49                                   ;@line 106
						Assign hasCreature True                                  ;@line 107
						Jump _label49                                            ;@line 107
					_label49:
						CallMethod GetRace a ::temp10                            ;@line 111
						Assign r ::temp10                                        ;@line 111
						Cast ::temp10 None                                       ;@line 112
						CompareEQ ::temp8 r ::temp10                             ;@line 112
						Not ::temp8 ::temp8                                      ;@line 112
						JumpF ::temp8 _label52                                   ;@line 112
						CallMethod GetName r ::temp4                             ;@line 113
						Assign raceName ::temp4                                  ;@line 113
						CallStatic stringutil Find ::temp5 raceName "Orc" 0      ;@line 114
						CompareGTE ::temp9 ::temp5 0                             ;@line 114
						Cast ::temp9 ::temp9                                     ;@line 114
						JumpT ::temp9 _label53                                   ;@line 114
						CallStatic stringutil Find ::temp5 raceName "Orsimer" 0  ;@line 114
						CompareGTE ::temp7 ::temp5 0                             ;@line 114
						Cast ::temp9 ::temp7                                     ;@line 114
					_label53:
						JumpF ::temp9 _label54                                   ;@line 114
						Assign hasOrc True                                       ;@line 115
						Jump _label55                                            ;@line 115
					_label54:
						CallStatic stringutil Find ::temp5 raceName "Khajiit" 0  ;@line 116
						CompareGTE ::temp7 ::temp5 0                             ;@line 116
						JumpF ::temp7 _label56                                   ;@line 116
						Assign hasKhajiit True                                   ;@line 117
						Jump _label55                                            ;@line 117
					_label56:
						CallStatic stringutil Find ::temp5 raceName "Argonian" 0  ;@line 118
						CompareGTE ::temp7 ::temp5 0                             ;@line 118
						JumpF ::temp7 _label55                                   ;@line 118
						Assign hasArgonian True                                  ;@line 119
						Jump _label55                                            ;@line 119
					_label55:
						Jump _label52                                            ;@line 119
					_label52:
						Jump _label47                                            ;@line 119
					_label47:
						IAdd ::temp5 i 1                                         ;@line 123
						Assign i ::temp5                                         ;@line 123
						Jump _label57                                            ;@line 123
					_label46:
						CallMethod isConsent thread ::temp7                      ;@line 127
						Assign isConsent ::temp7                                 ;@line 127
						CallMethod GetSubmissive thread ::temp9 playerRef        ;@line 128
						Assign isSubmissive ::temp9                              ;@line 128
						Not ::temp8 isConsent                                    ;@line 129
						Cast ::temp6 ::temp8                                     ;@line 129
						JumpF ::temp6 _label58                                   ;@line 129
						Cast ::temp6 isSubmissive                                ;@line 129
					_label58:
						Assign isVictim ::temp6                                  ;@line 129
						Not ::temp0 isConsent                                    ;@line 130
						Cast ::temp9 ::temp0                                     ;@line 130
						JumpF ::temp9 _label59                                   ;@line 130
						Not ::temp7 isSubmissive                                 ;@line 130
						Cast ::temp9 ::temp7                                     ;@line 130
					_label59:
						Assign isAggressor ::temp9                               ;@line 130
						Assign isForeplay _isInLeadIn                            ;@line 133
						CallMethod BuildTagString self ::temp4 thread            ;@line 137
						Assign tags ::temp4                                      ;@line 137
						CallStatic SLSFR_EventBridge OnSexLabAnimEnd ::NoneVar locName actorCount hasMale hasFemale hasCreature isVictim isAggressor isForeplay hasOrc hasKhajiit hasArgonian tags  ;@line 140
						Assign _isInLeadIn False                                 ;@line 143
						CallMethod ScanAndUpdate self ::NoneVar                  ;@line 146
					.endCode
				.endFunction
				.function OnInit
					.userFlags 0	; Flags: 0x00000000
					.docString ""
					.return None
					.paramTable
					.endParamTable
					.localTable
						.local ::NoneVar None
					.endLocalTable
					.code
						CallMethod RegisterAll self ::NoneVar                    ;@line 41
					.endCode
				.endFunction
				.function OnSLAnimStart
					.userFlags 0	; Flags: 0x00000000
					.docString ""
					.return None
					.paramTable
						.param aiThreadID Int
						.param abHasPlayer Bool
					.endParamTable
					.localTable
					.endLocalTable
					.code
					.endCode
				.endFunction
				.function GotoState
					.userFlags 0	; Flags: 0x00000000
					.docString "Function that switches this object to the specified state"
					.return None
					.paramTable
						.param newState String
					.endParamTable
					.localTable
						.local ::NoneVar None
					.endLocalTable
					.code
						CallMethod onEndState self ::NoneVar                     ;@line ??
						Assign ::State newState                                  ;@line ??
						CallMethod onBeginState self ::NoneVar                   ;@line ??
					.endCode
				.endFunction
				.function OnUpdateGameTime
					.userFlags 0	; Flags: 0x00000000
					.docString ""
					.return None
					.paramTable
					.endParamTable
					.localTable
						.local ::NoneVar None
					.endLocalTable
					.code
						CallStatic SLSFR_EventBridge OnGameTimePassed ::NoneVar ::UpdateIntervalHours_var  ;@line 270
						CallMethod RegisterForSingleUpdateGameTime self ::NoneVar ::UpdateIntervalHours_var  ;@line 271
					.endCode
				.endFunction
				.function GetState
					.userFlags 0	; Flags: 0x00000000
					.docString "Function that returns the current state"
					.return String
					.paramTable
					.endParamTable
					.localTable
					.endLocalTable
					.code
						Return ::State                                           ;@line ??
					.endCode
				.endFunction
				.function RegisterAll
					.userFlags 0	; Flags: 0x00000000
					.docString ""
					.return None
					.paramTable
					.endParamTable
					.localTable
						.local ::NoneVar None
					.endLocalTable
					.code
						CallMethod RegisterHooks self ::NoneVar                  ;@line 51
						CallMethod RegisterForSingleUpdateGameTime self ::NoneVar ::UpdateIntervalHours_var  ;@line 52
					.endCode
				.endFunction
			.endState
		.endStateTable
	.endObject
.endObjectTable
