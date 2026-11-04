.info
	.source "QF_CC_CheerleaderSpellQuest_088AC9A9.psc"
	.modifyTime 1730603584 ;Sun Nov 03 11:13:04 2024 Local
	.compileTime 1730603585 ;Sun Nov 03 11:13:05 2024 Local
	.user "James"
	.computer "HEX_V"
.endInfo
.userFlagsRef
	.flag conditional 1	; 0x00000001
	.flag hidden 0	; 0x00000000
.endUserFlagsRef
.objectTable
	.object qf_cc_cheerleaderspellquest_088ac9a9 Quest
		.userFlags 1	; Flags: 0x00000001
		.docString ""
		.autoState 
		.variableTable
			.variable ::Target4_var referencealias
				.userFlags 0	; Flags: 0x00000000
				.initialValue None
			.endVariable
			.variable ::Alias_Cheerleader_Ally5_var referencealias
				.userFlags 0	; Flags: 0x00000000
				.initialValue None
			.endVariable
			.variable ::CC_CheerleaderAliasQuest_var Quest
				.userFlags 0	; Flags: 0x00000000
				.initialValue None
			.endVariable
			.variable ::exitIdle_var idle
				.userFlags 0	; Flags: 0x00000000
				.initialValue None
			.endVariable
			.variable ::thisQuest_var Quest
				.userFlags 0	; Flags: 0x00000000
				.initialValue None
			.endVariable
			.variable ::PlayerRef_var actor
				.userFlags 0	; Flags: 0x00000000
				.initialValue None
			.endVariable
			.variable ::CC_PowerBimboCheerleader_var spell
				.userFlags 0	; Flags: 0x00000000
				.initialValue None
			.endVariable
			.variable ::Target3_var referencealias
				.userFlags 0	; Flags: 0x00000000
				.initialValue None
			.endVariable
			.variable ::Alias_Player_var referencealias
				.userFlags 0	; Flags: 0x00000000
				.initialValue None
			.endVariable
			.variable ::CC_IsBimboCheerleading_var globalvariable
				.userFlags 0	; Flags: 0x00000000
				.initialValue None
			.endVariable
			.variable ::Target1_var referencealias
				.userFlags 0	; Flags: 0x00000000
				.initialValue None
			.endVariable
			.variable ::Target5_var referencealias
				.userFlags 0	; Flags: 0x00000000
				.initialValue None
			.endVariable
			.variable ::Alias_Cheerleader_Ally3_var referencealias
				.userFlags 0	; Flags: 0x00000000
				.initialValue None
			.endVariable
			.variable ::mouthpiece_var objectreference
				.userFlags 0	; Flags: 0x00000000
				.initialValue None
			.endVariable
			.variable leftHandWeapon weapon
				.userFlags 0	; Flags: 0x00000000
				.initialValue None
			.endVariable
			.variable ::CC_CheerleaderBuff_var spell
				.userFlags 0	; Flags: 0x00000000
				.initialValue None
			.endVariable
			.variable rightHandWeapon weapon
				.userFlags 0	; Flags: 0x00000000
				.initialValue None
			.endVariable
			.variable ::CC_CheerleaderSpellQuest_Cheers_1_var topic
				.userFlags 0	; Flags: 0x00000000
				.initialValue None
			.endVariable
			.variable ::Target2_var referencealias
				.userFlags 0	; Flags: 0x00000000
				.initialValue None
			.endVariable
			.variable ::Alias_Cheerleader_Ally1_var referencealias
				.userFlags 0	; Flags: 0x00000000
				.initialValue None
			.endVariable
			.variable ::Alias_Cheerleader_Ally2_var referencealias
				.userFlags 0	; Flags: 0x00000000
				.initialValue None
			.endVariable
			.variable ::Alias_Cheerleader_Ally4_var referencealias
				.userFlags 0	; Flags: 0x00000000
				.initialValue None
			.endVariable
		.endVariableTable
		.propertyTable
			.property Target5 referencealias auto
				.userFlags 0	; Flags: 0x00000000
				.docString ""
				.autoVar ::Target5_var
			.endProperty
			.property Alias_Cheerleader_Ally4 referencealias auto
				.userFlags 0	; Flags: 0x00000000
				.docString ""
				.autoVar ::Alias_Cheerleader_Ally4_var
			.endProperty
			.property CC_CheerleaderAliasQuest Quest auto
				.userFlags 0	; Flags: 0x00000000
				.docString ""
				.autoVar ::CC_CheerleaderAliasQuest_var
			.endProperty
			.property CC_CheerleaderSpellQuest_Cheers_1 topic auto
				.userFlags 0	; Flags: 0x00000000
				.docString ""
				.autoVar ::CC_CheerleaderSpellQuest_Cheers_1_var
			.endProperty
			.property Target2 referencealias auto
				.userFlags 0	; Flags: 0x00000000
				.docString ""
				.autoVar ::Target2_var
			.endProperty
			.property Alias_Player referencealias auto
				.userFlags 0	; Flags: 0x00000000
				.docString ""
				.autoVar ::Alias_Player_var
			.endProperty
			.property Target1 referencealias auto
				.userFlags 0	; Flags: 0x00000000
				.docString ""
				.autoVar ::Target1_var
			.endProperty
			.property Alias_Cheerleader_Ally1 referencealias auto
				.userFlags 0	; Flags: 0x00000000
				.docString ""
				.autoVar ::Alias_Cheerleader_Ally1_var
			.endProperty
			.property Target4 referencealias auto
				.userFlags 0	; Flags: 0x00000000
				.docString ""
				.autoVar ::Target4_var
			.endProperty
			.property thisQuest Quest auto
				.userFlags 0	; Flags: 0x00000000
				.docString ""
				.autoVar ::thisQuest_var
			.endProperty
			.property CC_PowerBimboCheerleader spell auto
				.userFlags 0	; Flags: 0x00000000
				.docString ""
				.autoVar ::CC_PowerBimboCheerleader_var
			.endProperty
			.property mouthpiece objectreference auto
				.userFlags 0	; Flags: 0x00000000
				.docString ""
				.autoVar ::mouthpiece_var
			.endProperty
			.property Alias_Cheerleader_Ally2 referencealias auto
				.userFlags 0	; Flags: 0x00000000
				.docString ""
				.autoVar ::Alias_Cheerleader_Ally2_var
			.endProperty
			.property exitIdle idle auto
				.userFlags 0	; Flags: 0x00000000
				.docString ""
				.autoVar ::exitIdle_var
			.endProperty
			.property PlayerRef actor auto
				.userFlags 0	; Flags: 0x00000000
				.docString ""
				.autoVar ::PlayerRef_var
			.endProperty
			.property Alias_Cheerleader_Ally3 referencealias auto
				.userFlags 0	; Flags: 0x00000000
				.docString ""
				.autoVar ::Alias_Cheerleader_Ally3_var
			.endProperty
			.property Target3 referencealias auto
				.userFlags 0	; Flags: 0x00000000
				.docString ""
				.autoVar ::Target3_var
			.endProperty
			.property Alias_Cheerleader_Ally5 referencealias auto
				.userFlags 0	; Flags: 0x00000000
				.docString ""
				.autoVar ::Alias_Cheerleader_Ally5_var
			.endProperty
			.property CC_IsBimboCheerleading globalvariable auto
				.userFlags 0	; Flags: 0x00000000
				.docString ""
				.autoVar ::CC_IsBimboCheerleading_var
			.endProperty
			.property CC_CheerleaderBuff spell auto
				.userFlags 0	; Flags: 0x00000000
				.docString ""
				.autoVar ::CC_CheerleaderBuff_var
			.endProperty
		.endPropertyTable
		.stateTable
			.state 
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
				.function Fragment_3
					.userFlags 0	; Flags: 0x00000000
					.docString ""
					.return None
					.paramTable
					.endParamTable
					.localTable
						.local ::NoneVar None
					.endLocalTable
					.code
						CallMethod stopCheerleading self ::NoneVar               ;@line 38
					.endCode
				.endFunction
				.function OnUpdate
					.userFlags 0	; Flags: 0x00000000
					.docString ""
					.return None
					.paramTable
					.endParamTable
					.localTable
						.local ::temp10 Float
						.local ::temp11 Float
						.local ::temp12 Bool
						.local ::NoneVar None
						.local ::temp13 actor
						.local ::temp14 actor
						.local ::temp15 objectreference
						.local ::temp16 objectreference
					.endLocalTable
					.code
						CallMethod GetValue ::CC_IsBimboCheerleading_var ::temp10  ;@line 120
						Cast ::temp11 1                                          ;@line 120
						CompareEQ ::temp12 ::temp10 ::temp11                     ;@line 120
						JumpF ::temp12 _label0                                   ;@line 120
						CallMethod Disable ::mouthpiece_var ::NoneVar False      ;@line 123
						CallMethod Enable ::mouthpiece_var ::NoneVar False       ;@line 124
						CallMethod GetActorReference ::Alias_Cheerleader_Ally1_var ::temp13  ;@line 127
						Cast ::temp13 ::temp13                                   ;@line 127
						CallMethod GetActorReference ::Alias_Cheerleader_Ally1_var ::temp14  ;@line 127
						Cast ::temp14 ::temp14                                   ;@line 127
						Cast ::temp15 ::temp13                                   ;@line 127
						Cast ::temp16 ::temp14                                   ;@line 127
						CallMethod Cast ::CC_CheerleaderBuff_var ::NoneVar ::temp15 ::temp16  ;@line 127
						CallMethod GetActorReference ::Alias_Cheerleader_Ally2_var ::temp13  ;@line 128
						Cast ::temp14 ::temp13                                   ;@line 128
						CallMethod GetActorReference ::Alias_Cheerleader_Ally2_var ::temp13  ;@line 128
						Cast ::temp13 ::temp13                                   ;@line 128
						Cast ::temp15 ::temp14                                   ;@line 128
						Cast ::temp16 ::temp13                                   ;@line 128
						CallMethod Cast ::CC_CheerleaderBuff_var ::NoneVar ::temp15 ::temp16  ;@line 128
						CallMethod GetActorReference ::Alias_Cheerleader_Ally3_var ::temp14  ;@line 129
						Cast ::temp13 ::temp14                                   ;@line 129
						CallMethod GetActorReference ::Alias_Cheerleader_Ally3_var ::temp14  ;@line 129
						Cast ::temp14 ::temp14                                   ;@line 129
						Cast ::temp15 ::temp13                                   ;@line 129
						Cast ::temp16 ::temp14                                   ;@line 129
						CallMethod Cast ::CC_CheerleaderBuff_var ::NoneVar ::temp15 ::temp16  ;@line 129
						CallMethod GetActorReference ::Alias_Cheerleader_Ally4_var ::temp13  ;@line 130
						Cast ::temp14 ::temp13                                   ;@line 130
						CallMethod GetActorReference ::Alias_Cheerleader_Ally4_var ::temp13  ;@line 130
						Cast ::temp13 ::temp13                                   ;@line 130
						Cast ::temp15 ::temp14                                   ;@line 130
						Cast ::temp16 ::temp13                                   ;@line 130
						CallMethod Cast ::CC_CheerleaderBuff_var ::NoneVar ::temp15 ::temp16  ;@line 130
						CallMethod GetActorReference ::Alias_Cheerleader_Ally5_var ::temp14  ;@line 131
						Cast ::temp13 ::temp14                                   ;@line 131
						CallMethod GetActorReference ::Alias_Cheerleader_Ally5_var ::temp14  ;@line 131
						Cast ::temp14 ::temp14                                   ;@line 131
						Cast ::temp15 ::temp13                                   ;@line 131
						Cast ::temp16 ::temp14                                   ;@line 131
						CallMethod Cast ::CC_CheerleaderBuff_var ::NoneVar ::temp15 ::temp16  ;@line 131
						CallMethod Say ::mouthpiece_var ::NoneVar ::CC_CheerleaderSpellQuest_Cheers_1_var ::PlayerRef_var True  ;@line 134
						CallMethod EquipSpell ::PlayerRef_var ::NoneVar ::CC_PowerBimboCheerleader_var 2  ;@line 137
						Cast ::temp11 6                                          ;@line 139
						CallMethod RegisterForSingleUpdate self ::NoneVar ::temp11  ;@line 139
						Jump _label0                                             ;@line 139
					_label0:
					.endCode
				.endFunction
				.function stopCheerleading
					.userFlags 0	; Flags: 0x00000000
					.docString ""
					.return None
					.paramTable
					.endParamTable
					.localTable
						.local ::NoneVar None
						.local ::temp17 Float
						.local ::temp18 Bool
						.local ::temp19 Int
						.local EventHandle Int
					.endLocalTable
					.code
						CallMethod UnregisterForUpdate self ::NoneVar            ;@line 148
						Cast ::temp17 0                                          ;@line 149
						CallMethod SetValue ::CC_IsBimboCheerleading_var ::NoneVar ::temp17  ;@line 149
						CallMethod Disable ::mouthpiece_var ::NoneVar False      ;@line 152
						CallStatic game EnablePlayerControls ::NoneVar True True True True True True True True 0  ;@line 153
						CallMethod PlayIdle ::PlayerRef_var ::temp18 ::exitIdle_var  ;@line 156
						CallStatic utility wait ::NoneVar 0.100000               ;@line 157
						CallStatic game SetPlayerAIDriven ::NoneVar False        ;@line 158
						CallMethod Clear ::Alias_Cheerleader_Ally1_var ::NoneVar  ;@line 161
						CallMethod Clear ::Alias_Cheerleader_Ally2_var ::NoneVar  ;@line 162
						CallMethod Clear ::Alias_Cheerleader_Ally3_var ::NoneVar  ;@line 163
						CallMethod Clear ::Alias_Cheerleader_Ally4_var ::NoneVar  ;@line 164
						CallMethod Clear ::Alias_Cheerleader_Ally5_var ::NoneVar  ;@line 165
						CallMethod Stop ::CC_CheerleaderAliasQuest_var ::NoneVar  ;@line 168
						CallMethod Reset ::CC_CheerleaderAliasQuest_var ::NoneVar  ;@line 169
						Cast ::temp17 0                                          ;@line 172
						CallMethod SendModEvent self ::NoneVar "CC_MakeBimboDumber" "Test Event OnInit Activation" ::temp17  ;@line 172
						CallStatic modevent Create ::temp19 "SLSF_Reloaded_SendFameGain"  ;@line 175
						Assign EventHandle ::temp19                              ;@line 175
						CallStatic modevent PushString ::NoneVar EventHandle "Airhead"  ;@line 176
						CallStatic modevent PushString ::NoneVar EventHandle "Current"  ;@line 177
						CallStatic modevent PushInt ::NoneVar EventHandle 0      ;@line 178
						CallStatic modevent PushInt ::NoneVar EventHandle 150    ;@line 179
						CallStatic modevent Send ::temp18 EventHandle            ;@line 180
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
				.function StartCheerleading
					.userFlags 0	; Flags: 0x00000000
					.docString ""
					.return None
					.paramTable
					.endParamTable
					.localTable
						.local ::temp0 Float
						.local ::NoneVar None
						.local ::temp1 actorbase
						.local ::temp2 String
						.local ::temp3 Bool
						.local ::temp4 weapon
						.local ::temp6 actor
						.local ::temp7 objectreference
						.local ::temp8 actor
						.local ::temp9 objectreference
						.local ::temp5 form
					.endLocalTable
					.code
						Cast ::temp0 1                                           ;@line 65
						CallMethod SetValue ::CC_IsBimboCheerleading_var ::NoneVar ::temp0  ;@line 65
						CallMethod Enable ::mouthpiece_var ::NoneVar False       ;@line 66
						CallMethod GetActorBase ::PlayerRef_var ::temp1          ;@line 67
						CallMethod GetName ::temp1 ::temp2                       ;@line 67
						CallMethod SetDisplayName ::mouthpiece_var ::temp3 ::temp2 False  ;@line 67
						CallMethod GetEquippedWeapon ::PlayerRef_var ::temp4 False  ;@line 70
						Assign leftHandWeapon ::temp4                            ;@line 70
						CallMethod GetEquippedWeapon ::PlayerRef_var ::temp4 True  ;@line 71
						Assign rightHandWeapon ::temp4                           ;@line 71
						Cast ::temp4 None                                        ;@line 73
						CompareEQ ::temp3 leftHandWeapon ::temp4                 ;@line 73
						Not ::temp3 ::temp3                                      ;@line 73
						JumpF ::temp3 _label1                                    ;@line 73
						Cast ::temp5 leftHandWeapon                              ;@line 74
						CallMethod UnequipItem ::PlayerRef_var ::NoneVar ::temp5 False False  ;@line 74
						Jump _label1                                             ;@line 74
					_label1:
						Cast ::temp4 None                                        ;@line 77
						CompareEQ ::temp3 rightHandWeapon ::temp4                ;@line 77
						Not ::temp3 ::temp3                                      ;@line 77
						JumpF ::temp3 _label2                                    ;@line 77
						Cast ::temp5 rightHandWeapon                             ;@line 78
						CallMethod UnequipItem ::PlayerRef_var ::NoneVar ::temp5 False False  ;@line 78
						Jump _label2                                             ;@line 78
					_label2:
						CallStatic game ForceThirdPerson ::NoneVar               ;@line 83
						CallStatic game SetPlayerAIDriven ::NoneVar True         ;@line 84
						CallStatic game DisablePlayerControls ::NoneVar False False True False False True False True 0  ;@line 85
						CallMethod Say ::mouthpiece_var ::NoneVar ::CC_CheerleaderSpellQuest_Cheers_1_var ::PlayerRef_var True  ;@line 88
						CallMethod Stop ::CC_CheerleaderAliasQuest_var ::NoneVar  ;@line 91
						CallMethod Reset ::CC_CheerleaderAliasQuest_var ::NoneVar  ;@line 92
						CallMethod Start ::CC_CheerleaderAliasQuest_var ::temp3  ;@line 93
						CallMethod GetActorReference ::Target1_var ::temp6       ;@line 96
						Cast ::temp7 ::temp6                                     ;@line 96
						CallMethod ForceRefTo ::Alias_Cheerleader_Ally1_var ::NoneVar ::temp7  ;@line 96
						CallMethod GetActorReference ::Target2_var ::temp6       ;@line 97
						Cast ::temp7 ::temp6                                     ;@line 97
						CallMethod ForceRefTo ::Alias_Cheerleader_Ally2_var ::NoneVar ::temp7  ;@line 97
						CallMethod GetActorReference ::Target3_var ::temp6       ;@line 98
						Cast ::temp7 ::temp6                                     ;@line 98
						CallMethod ForceRefTo ::Alias_Cheerleader_Ally3_var ::NoneVar ::temp7  ;@line 98
						CallMethod GetActorReference ::Target4_var ::temp6       ;@line 99
						Cast ::temp7 ::temp6                                     ;@line 99
						CallMethod ForceRefTo ::Alias_Cheerleader_Ally4_var ::NoneVar ::temp7  ;@line 99
						CallMethod GetActorReference ::Target5_var ::temp6       ;@line 100
						Cast ::temp7 ::temp6                                     ;@line 100
						CallMethod ForceRefTo ::Alias_Cheerleader_Ally5_var ::NoneVar ::temp7  ;@line 100
						CallMethod GetActorReference ::Alias_Cheerleader_Ally1_var ::temp6  ;@line 103
						Cast ::temp6 ::temp6                                     ;@line 103
						CallMethod GetActorReference ::Alias_Cheerleader_Ally1_var ::temp8  ;@line 103
						Cast ::temp8 ::temp8                                     ;@line 103
						Cast ::temp7 ::temp6                                     ;@line 103
						Cast ::temp9 ::temp8                                     ;@line 103
						CallMethod Cast ::CC_CheerleaderBuff_var ::NoneVar ::temp7 ::temp9  ;@line 103
						CallMethod GetActorReference ::Alias_Cheerleader_Ally2_var ::temp6  ;@line 104
						Cast ::temp8 ::temp6                                     ;@line 104
						CallMethod GetActorReference ::Alias_Cheerleader_Ally2_var ::temp6  ;@line 104
						Cast ::temp6 ::temp6                                     ;@line 104
						Cast ::temp7 ::temp8                                     ;@line 104
						Cast ::temp9 ::temp6                                     ;@line 104
						CallMethod Cast ::CC_CheerleaderBuff_var ::NoneVar ::temp7 ::temp9  ;@line 104
						CallMethod GetActorReference ::Alias_Cheerleader_Ally3_var ::temp8  ;@line 105
						Cast ::temp6 ::temp8                                     ;@line 105
						CallMethod GetActorReference ::Alias_Cheerleader_Ally3_var ::temp8  ;@line 105
						Cast ::temp8 ::temp8                                     ;@line 105
						Cast ::temp7 ::temp6                                     ;@line 105
						Cast ::temp9 ::temp8                                     ;@line 105
						CallMethod Cast ::CC_CheerleaderBuff_var ::NoneVar ::temp7 ::temp9  ;@line 105
						CallMethod GetActorReference ::Alias_Cheerleader_Ally4_var ::temp6  ;@line 106
						Cast ::temp8 ::temp6                                     ;@line 106
						CallMethod GetActorReference ::Alias_Cheerleader_Ally4_var ::temp6  ;@line 106
						Cast ::temp6 ::temp6                                     ;@line 106
						Cast ::temp7 ::temp8                                     ;@line 106
						Cast ::temp9 ::temp6                                     ;@line 106
						CallMethod Cast ::CC_CheerleaderBuff_var ::NoneVar ::temp7 ::temp9  ;@line 106
						CallMethod GetActorReference ::Alias_Cheerleader_Ally5_var ::temp8  ;@line 107
						Cast ::temp6 ::temp8                                     ;@line 107
						CallMethod GetActorReference ::Alias_Cheerleader_Ally5_var ::temp8  ;@line 107
						Cast ::temp8 ::temp8                                     ;@line 107
						Cast ::temp7 ::temp6                                     ;@line 107
						Cast ::temp9 ::temp8                                     ;@line 107
						CallMethod Cast ::CC_CheerleaderBuff_var ::NoneVar ::temp7 ::temp9  ;@line 107
						CallMethod EquipSpell ::PlayerRef_var ::NoneVar ::CC_PowerBimboCheerleader_var 2  ;@line 110
						Cast ::temp0 6                                           ;@line 113
						CallMethod RegisterForSingleUpdate self ::NoneVar ::temp0  ;@line 113
					.endCode
				.endFunction
				.function Fragment_0
					.userFlags 0	; Flags: 0x00000000
					.docString ""
					.return None
					.paramTable
					.endParamTable
					.localTable
						.local ::NoneVar None
					.endLocalTable
					.code
						CallMethod StartCheerleading self ::NoneVar              ;@line 51
					.endCode
				.endFunction
			.endState
		.endStateTable
	.endObject
.endObjectTable
