.info
	.source "TIF__0831D4D0.psc"
	.modifyTime 1730603776 ;Sun Nov 03 11:16:16 2024 Local
	.compileTime 1730603778 ;Sun Nov 03 11:16:18 2024 Local
	.user "James"
	.computer "HEX_V"
.endInfo
.userFlagsRef
	.flag conditional 1	; 0x00000001
	.flag hidden 0	; 0x00000000
.endUserFlagsRef
.objectTable
	.object tif__0831d4d0 TopicInfo
		.userFlags 1	; Flags: 0x00000001
		.docString ""
		.autoState 
		.variableTable
			.variable ::SexLab_var sexlabframework
				.userFlags 0	; Flags: 0x00000000
				.initialValue None
			.endVariable
		.endVariableTable
		.propertyTable
			.property SexLab sexlabframework auto
				.userFlags 0	; Flags: 0x00000000
				.docString ""
				.autoVar ::SexLab_var
			.endProperty
		.endPropertyTable
		.stateTable
			.state 
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
				.function Fragment_0
					.userFlags 0	; Flags: 0x00000000
					.docString ""
					.return None
					.paramTable
						.param akSpeakerRef ObjectReference
					.endParamTable
					.localTable
						.local ::temp0 actor
						.local ::NoneVar None
						.local ::temp1 Int
						.local ::temp2 Bool
						.local akSpeaker actor
						.local EventHandle Int
					.endLocalTable
					.code
						Cast ::temp0 akSpeakerRef                                ;@line 7
						Assign akSpeaker ::temp0                                 ;@line 7
						CallStatic debug Notification ::NoneVar "That's not fair! I need to be used..."  ;@line 9
						CallStatic modevent Create ::temp1 "SLSF_Reloaded_SendFameGain"  ;@line 12
						Assign EventHandle ::temp1                               ;@line 12
						CallStatic modevent PushString ::NoneVar EventHandle "Airhead"  ;@line 13
						CallStatic modevent PushString ::NoneVar EventHandle "Current"  ;@line 14
						CallStatic modevent PushInt ::NoneVar EventHandle 0      ;@line 15
						CallStatic modevent PushInt ::NoneVar EventHandle 150    ;@line 16
						CallStatic modevent Send ::temp2 EventHandle             ;@line 17
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
			.endState
		.endStateTable
	.endObject
.endObjectTable
