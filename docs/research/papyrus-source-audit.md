# SLSF Reloaded Papyrus 源码迁移审计

审阅对象：`E:/source/repos/SLSF Reloaded/Scripts/Source` 的全部 17 个 PSC。本文保留原字段、调用链和声明定位，供实现时查证业务。范围与已选行为统一见[项目规格](../../.scratch/native-migration/spec.md)；后续 PSC/PEX/ESP 合同核对见[证据](../evidence/legacy-contracts.md)。

AND hooks、Faction 与旧动画启发式按原源码记录。当前项目使用 P+ 明确角色和行为，移除 AND；外部事实与换档边界见[生命周期设计](../superpowers/specs/2026-10-26-integration-state-lifecycle-design.md)。

## 结论

业务与存储可重建为 C++ SKSE 服务，Papyrus 承担公开调用与必要传输。后续合同从以下盘点中确定了 86 个公开函数、52 个事件输入、7 个回应和 25 类声望。124 个函数与 483 个属性是源码声明总数。

### 资产缺口与当前进度

- 源码目录缺 SLSF_Reloaded_MCM.psc，存在 MCM.pex。新界面使用 SKSE 菜单；旧 Config 读取点用于核查配置含义。PEX 与 ESP/VMAD 提供公开签名及绑定证据，源码一致性按资产哈希和提取结果判断。
- PlayerScript 声明 SLSF_Reloaded_TimeManager Property TimeManager Auto（E:/source/repos/SLSF Reloaded/Scripts/Source/SLSF_Reloaded_PlayerScript.psc:L3-L14），但本文 17 个 PSC 中没有该 ScriptName，且此 PSC 正文未见调用它。要在 PEX/VMAD 审计中核对它是否是遗留绑定、额外已编译脚本或未解析类型。
- 当前已实现数值、类别设置、地点和 codec 纯核心，见[02 组报告](../evidence/task-groups/02-state-location-and-codec.md)。SKSE 入口仍为最小初始化，运行时及 Papyrus bridge 接入在后续任务完成。

## 覆盖与计数

已完整读取 17 份 PSC，包括 DataManager（2052 行）、FameManager（2782 行）和 ModEventListener（1143 行）。逐行核对得到 17 个 ScriptName、73 个事件声明、124 个有效 Function 声明和 483 个有效 Property 声明。函数正则的 130 个命中包含 4 处注释正文（ModEventListener L180、L677、L804、L814）和 2 个块注释声明（Logger L26、L39），扣除后为 124。属性计数也排除了 Logger L9-L13 的 4 个块注释声明。Hidden 仅控制编辑器显示，相应属性仍计入属性统计，并按实际调用方式核对。

下表函数/事件为 PSC 中声明数；属性计数包括 Auto/AutoReadOnly 与 Hidden，不包括局部变量。文件均位于绝对路径根目录 E:/source/repos/SLSF Reloaded/Scripts/Source/。

| 文件 | 继承 | Functions | Events | Properties | 角色摘要 |
|---|---|---:|---:|---:|---|
| SLSF_Reloaded_CommentManager.psc | Quest | 1 | 2 | 7 | 控制评论概率、战斗抑制与 Public Whore/AND 相关全局值 |
| SLSF_Reloaded_PlayerScript.psc | ReferenceAlias | 2 | 10 | 18 | 事件枢纽：安装检测、SexLab/AND hooks、位置与装备变化、NPC 扫描 |
| SLSF_Reloaded_FameManager.psc | Quest | 30 | 1 | 81 | 25 类名望资格判定、增减、传播、提示及全局同步 |
| SLSF_Reloaded_MasterClock.psc | Quest | 5 | 2 | 14 | 以 0.25 参数注册游戏时间更新，按分段值累计欠账并追补 decay/spread/periodic 更新 |
| SLSF_Reloaded_LocationManager.psc | Quest | 14 | 0 | 26 | 默认/自定义地点、Cell/Location 归属、注册表与索引压缩 |
| SLSF_Reloaded_Data_Importer.psc | Quest | 2 | 0 | 8 | JsonUtil 导入地点、Mod/flags、名望及传播概率 |
| SLSF_Reloaded_DynamicAnonymity.psc | Quest | 9 | 2 | 23 | 按进入/离开时间、名望阈值和 Thane 状态维护本地匿名度 |
| SLSF_Reloaded_VisibilityManager.psc | Quest | 22 | 1 | 51 | 性行为可见性、SlaveTats、身份遮挡、Cum、DD/AND/SLS 状态 |
| SLSF_Reloaded_DataManager.psc | Quest | 18 | 1 | 125 | 名望与 spread flags 存储、外部 Mod/类别 flags 注册及派生状态 |
| SLSF_Reloaded_LegacyOverwrite.psc | Quest | 1 | 0 | 5 | 写旧 SLSF 的 StorageUtil 列表与当前地点全局变量 |
| SLSF_Reloaded_ModEventListener.psc | Quest | 1 | 53 | 8 | 注册 52 个外部 ModEvent 入口，处理协议并发出答复事件 |
| SLSF_Reloaded_NPCScan.psc | ActiveMagicEffect | 0 | 1 | 8 | 扫描单个施法目标，按关系、战斗、SexLab、LOS 与概率触发名望 |
| SLSF_Reloaded_SexAnimationAnalyzer.psc | ReferenceAlias | 2 | 0 | 15 | 延迟读取 SexLab 动画/参与者/标签并逐类判定 Fame |
| SLSF_Reloaded_Logger.psc | Quest | 3 | 0 | 3 | 3 个 global native 日志入口 |
| SLSF_Reloaded_ModIntegration.psc | Quest | 11 | 0 | 79 | 识别外部插件、解析 Form/Keyword/Faction，并提供兼容查询 |
| SLSF_Reloaded_Data_Exporter.psc | Quest | 2 | 0 | 6 | JsonUtil 导出名望、flags、地点表单与 spread chance |
| SLSF_Reloaded_Uninstall.psc | Quest | 1 | 0 | 6 | 停止更新/Quest 并清理可选集成引用 |
| **合计** | **14 Quest / 2 ReferenceAlias / 1 ActiveMagicEffect** | **124** | **73** | **483** |  |

完整 ScriptName/继承声明位于每个 PSC 第 1 行。关键实例类型：PlayerScript extends ReferenceAlias（E:/source/repos/SLSF Reloaded/Scripts/Source/SLSF_Reloaded_PlayerScript.psc:L1），SexAnimationAnalyzer extends ReferenceAlias（E:/source/repos/SLSF Reloaded/Scripts/Source/SLSF_Reloaded_SexAnimationAnalyzer.psc:L1），NPCScan extends ActiveMagicEffect（E:/source/repos/SLSF Reloaded/Scripts/Source/SLSF_Reloaded_NPCScan.psc:L1）；其余 14 个均 extends Quest。这是旧实现结构，新的原生调度和事件处理无需保留所有内部脚本附件。

## 角色、节拍与状态/数据流

### 调度与输入事件

- MasterClock OnInit 等 2 秒后订阅 RegisterForUpdateGameTime(0.25)；每次更新根据 Utility.GetCurrentGameTime() 与上次时刻，用 QuarterHourSplit=0.010417 换算欠账检查次数，并以 HalfHourSync 每两次调用一次 HalfHourChecks。它维护 DecayTicker/SpreadTicker，并调用 FameManager 更新；见 E:/source/repos/SLSF Reloaded/Scripts/Source/SLSF_Reloaded_MasterClock.psc:L8-L31、L33-L72、L74-L127。游戏时间倒退时只重置 LastTimeChecked 后返回（L42-L46）。FameManager 中旧的内部倒计时代码在块注释中，当前活动节拍来自 MasterClock（E:/source/repos/SLSF Reloaded/Scripts/Source/SLSF_Reloaded_FameManager.psc:L125-L148）。
- CommentManager 初次 10 秒后更新；每次检查匿名状态、战斗状态和随机评论概率，并重新注册 5 秒单次更新（E:/source/repos/SLSF Reloaded/Scripts/Source/SLSF_Reloaded_CommentManager.psc:L13-L39）。
- DynamicAnonymity 每游戏小时更新一次；位置变化时由 PlayerScript 先调用 CompareLocations 记录进入/离开、再检查新地点匿名度，随后同步地点名望全局变量（E:/source/repos/SLSF Reloaded/Scripts/Source/SLSF_Reloaded_DynamicAnonymity.psc:L35-L82；E:/source/repos/SLSF Reloaded/Scripts/Source/SLSF_Reloaded_PlayerScript.psc:L99-L107）。动态匿名合并停留时长、名望阈值及 Thane/DLC2/OrcFriends 条件（E:/source/repos/SLSF Reloaded/Scripts/Source/SLSF_Reloaded_DynamicAnonymity.psc:L130-L197、L199-L258、L292-L359）。
- PlayerScript 的 OnPlayerLoadGame 重新检查插件、重注册监听、清理卸载 Mod 和重算 tattoo slots（E:/source/repos/SLSF Reloaded/Scripts/Source/SLSF_Reloaded_PlayerScript.psc:L44-L59）。装备/卸下事件更新 Skooma 全局值或启动 VisibilityManager.FullUpdate（L109-L154）；RunNPCDetect 清零成功门闩并施放扫描法术（L156-L159）。
- NPCScan 是 ActiveMagicEffect 目标筛选与单次门闩：过滤无效地点、死亡/战斗目标、SexLab active、可配置 LOS；按关系 rank 决定 FameChance，最多一次成功后调用 FameGainRoll，见 E:/source/repos/SLSF Reloaded/Scripts/Source/SLSF_Reloaded_NPCScan.psc:L13-L85。

### 名望主流程

- DataManager 提供 21 个默认地点和 21 个自定义地点的名望数组组，以及 spread flag、dynamic anonymity flag、外部 Mod 名称及布尔类别数组（E:/source/repos/SLSF Reloaded/Scripts/Source/SLSF_Reloaded_DataManager.psc:L8-L138）。自定义地点是固定 21 槽：LocationManager 与 DataManager 按索引访问；注册/注销会压缩地点、名望与匿名数组（E:/source/repos/SLSF Reloaded/Scripts/Source/SLSF_Reloaded_LocationManager.psc:L15-L35、L468-L573）。
- GetFameValue(LocationName, FameCategory)/SetFameValue(...) 将字符串地点/类别映射成固定数组下标；设置名望也同步 spread 可用位与匿名位，见 E:/source/repos/SLSF Reloaded/Scripts/Source/SLSF_Reloaded_DataManager.psc:L446-L548、L550-L1022。外部 Mod 上限 100，类别状态数组持久存在 DataManager 对象（L149-L183、L1470-L1572）。
- MasterClock 周期调用 FameManager.UpdateFame，传入乘子补偿检查积压；玩家不匿名时发起 NPC 扫描；另处理衰减暂停计时、可选类别清除、Data.FameCheck（E:/source/repos/SLSF Reloaded/Scripts/Source/SLSF_Reloaded_FameManager.psc:L125-L227）。FameGainRoll 汇总可获得类别、朋友/恋人告密约束与外部 Mod flags，再随机抽类别（L749-L1282）；GainFame 按当前值、倍率、昼夜/Foreplay 等写值并设置衰减暂停（L1284-L1372）。DecayFame 与 SpreadFameRoll/SpreadFame 分别依配置减值或跨地点扩散（L1374-L1601、L1603-L2621）。
- 每次位置改变会调用 UpdateGlobals，把当前地点各名望写入旧界面/评论使用的 GlobalVariable；无效地点时全部归零（E:/source/repos/SLSF Reloaded/Scripts/Source/SLSF_Reloaded_FameManager.psc:L2726-L2782）。GlobalVariable 是 ESP 表单接口，不只是 C++ 私有状态。
- 25 个 FameType 字符串和位置输入以 ModEventListener 文档为语义合同；默认位置可接受区域别名，例如 Eastmarch/Windhelm、Haafingar/Solitude 等（E:/source/repos/SLSF Reloaded/Scripts/Source/SLSF_Reloaded_ModEventListener.psc:L87-L153；位置规范化见 E:/source/repos/SLSF Reloaded/Scripts/Source/SLSF_Reloaded_LocationManager.psc:L37-L76）。字符串拼写、空格、别名与 FameType/配置索引顺序均应逐项对照发布 PEX/ESP VMAD 与旧 PSC 调用点；本审计不依赖旧 MCM 界面源码。

## Papyrus 兼容面与外部 ModEvent 合同

以下保留源码声明，公开与内部函数的最终划分见 `contracts/legacy-api.json`。公开 ScriptName/类型、签名、实例/Global 和事件协议用于后续兼容实现。

### 直接函数 API

- DataManager 的 18 个 Function：FameCheck()，SetDefaults()，CheckSpreadableFame()，GetFameValue(String LocationName, String FameCategory) -> Int，SetFameValue(String LocationName, String FameCategory, Int FameValue)，DecayDynamicAnonymityCheck(Int LocationIndex, Bool IsDefaultLocation)，GetExternalFlags(String FlagName) -> Bool，RegisterExternalMod(String ModName)，CountExternalMods() -> Int，CleanExternalModList()，UnregisterExternalMod(String ModName)，GetModFlagState(String ModName, String FlagName) -> Bool，DoesFlagExist(String FlagName) -> String，IsModRegistered(String ModName) -> Bool，SetExternalFlags(String ModName, String FlagName, Bool FlagValue)，CheckSexWorker() -> Bool，CheckFlags()，FameOverviewCheck()。定义与公共状态见 E:/source/repos/SLSF Reloaded/Scripts/Source/SLSF_Reloaded_DataManager.psc:L140-L2052。
- FameManager 的 30 个函数：Startup()，UpdateFame(Int UpdateMultiplier=1)，WipeCuckFame()，WipeUnfaithfulFame()，WipeAirheadFame()，WipeCumDumpFame()，CheckExternalFlags()；CanGainWhoreFame()、CanGainSlutFame(String FameLocation)、CanGainExhibitionistFame(String FameLocation)、CanGainOralFame(String FameLocation)、CanGainAnalFame(String FameLocation)、CanGainNastyFame(String FameLocation)、CanGainPregnantFame(String FameLocation)、CanGainBoundFame(String FameLocation)、CanGainTattooFame(String FameLocation)、CanGainCumDumpFame()；CheckTattooExtraFame(String ExtraFame) -> Bool，FameGainRoll(String FameLocation, Bool CalledExternally=False, Bool FameFromFriend=False, Bool FameFromLover=False)，GainFame(String Category, String LocationName, Bool IsForeplay)，DecayFame(Int DecayMultiplier=1)，SpreadFameRoll()，SpreadFame(String SpreadFromLocation)，FameGainNotification()，FameDecayNotification()，CalculateFameSpread(Float FromFame, Float TargetFame) -> Int，FameSpreadNotification()，ClearFame(String LocationToClear)，ClearAllFame()，UpdateGlobals()。定义见 E:/source/repos/SLSF Reloaded/Scripts/Source/SLSF_Reloaded_FameManager.psc:L97-L2782。
- LocationManager 的 14 个函数：IsLocationValid(String CheckedLocation) -> Bool，LocationCanBeRegistered(String LocationToRegister, Bool ExternalRegister=False) -> Bool，UpdateCustomLocationCount()，FetchLocationName(Location LocationRef) -> String，CurrentLocationName() -> String，GetLocalizedFameLocation(Location LocationRef) -> String，GetFameLocation(Location LocationRef) -> String，IsLocationExcluded(Location LocationRef) -> Bool，RegisterCustomLocation()，AddToLocationList(Location LocationRef)，RemoveFromLocationList(String FameLocation)，UnregisterCustomLocation(Int LocationIndexToUnregister)，RegisterCustomLocationExternal(String LocationToRegister, Location LocationRefToRegister)，UnregisterCustomLocationExternal(String LocationToUnregister)。见 E:/source/repos/SLSF Reloaded/Scripts/Source/SLSF_Reloaded_LocationManager.psc:L37-L625。
- 其他直接接口：PlayerScript RunNPCDetect()（E:/source/repos/SLSF Reloaded/Scripts/Source/SLSF_Reloaded_PlayerScript.psc:L156-L159）；Analyzer AnimationAnalyze(Int threadID)、AnimationSubAnalyze(Actor[] Actors, sslThreadController PlayerController, Int PlayerThread, Bool Foreplay, String PlayerLocation, Bool Friend=False, Bool Lover=False)（E:/source/repos/SLSF Reloaded/Scripts/Source/SLSF_Reloaded_SexAnimationAnalyzer.psc:L25-L276）；DynamicAnonymity 的 Compare/SetLast*/CheckRecognition*/Get/CheckAnonymityStatus/ThaneStatus（E:/source/repos/SLSF Reloaded/Scripts/Source/SLSF_Reloaded_DynamicAnonymity.psc:L66-L359）；VisibilityManager 的匿名、tattoo、cum 可见性计数与 bondage 检查（E:/source/repos/SLSF Reloaded/Scripts/Source/SLSF_Reloaded_VisibilityManager.psc:L163-L906）。Export/Import 的 RunExport(String FileName)、ExportData(String FileName)、RunImport(String FileName)、ImportData(String FileName) 见各自文件 L16-L114 / L15-L139。
- Native/global 面：Logger 的 3 个 global native 是 Log(String LogString, Int Severity=0)、EnableLogging(Bool EnableLogging)、SetLogLevel(Int LogLevel)，定义在 E:/source/repos/SLSF Reloaded/Scripts/Source/SLSF_Reloaded_Logger.psc:L16-L24。ModIntegration 的 GetFHUData() -> sr_InflateQuest、GetPWData() -> PW_MainLoopScript、GetLegacyConfig() -> SLSF_Configuration 是 Global 函数（E:/source/repos/SLSF Reloaded/Scripts/Source/SLSF_Reloaded_ModIntegration.psc:L330-L339）。其余是实例函数。

### 52 个已注册 ModEvent 输入

监听器在 Init 时注册以下 52 个事件；事件名、目标 Handler、参数类型与次序来自 E:/source/repos/SLSF Reloaded/Scripts/Source/SLSF_Reloaded_ModEventListener.psc:L13-L85 及 L192-L1143。发送端必须保持事件名和 Push 顺序。

| 协议组 | ModEvent 名 → Listener handler（参数顺序） |
|---|---|
| 自动名望 | SLSF_Reloaded_SendFameGainRoll → OnExternalFameGainRoll(String EventLocation)；SLSF_Reloaded_SendFameGain → OnExternalFameGain(String Category, String EventLocation, Int MinFame, Int MaxFame) |
| 手动增加 | SLSF_Reloaded_SendManualFameGain → OnExternalManualFameGain(String Category, String EventLocation, Int MinIncrease, Int MaxIncrease)；SLSF_Reloaded_SendManualFameGainAllInLocation → OnExternalManualFameGainAllInLocation(String EventLocation, Int MinIncrease, Int MaxIncrease)；SLSF_Reloaded_SendManualFameGainAll → OnExternalManualFameGainAll(Int MinIncrease, Int MaxIncrease) |
| 衰减 | SLSF_Reloaded_SendFameDecay → OnExternalFameDecay()；SLSF_Reloaded_SendManualFameDecay → OnExternalManualFameDecay(String EventLocation, String Category, Int MinDecay, Int MaxDecay)；SLSF_Reloaded_SendManualFameDecayAllInLocation → OnExternalManualFameDecayAllInLocation(String EventLocation, Int MinDecay, Int MaxDecay)；SLSF_Reloaded_SendManualFameDecayAll → OnExternalManualFameDecayAll(Int MinDecay, Int MaxDecay) |
| 传播 | SLSF_Reloaded_SendFameSpreadRoll → OnExternalFameSpreadRoll()；SLSF_Reloaded_SendFameSpread → OnExternalFameSpread(Location SpreadFromLocation)；SLSF_Reloaded_SendFameSpreadByName → OnExternalFameSpreadByName(String SpreadFromLocation)；SLSF_Reloaded_SendManualFameSpread → OnExternalManualFameSpread(String SpreadFromLocation, String SpreadToLocation, String Category, Int PercentToSpread) |
| 地点注册 | SLSF_Reloaded_SendLocationRegister → OnExternalLocationRegister(Location LocationToRegister)；SLSF_Reloaded_SendLocationUnregister → OnExternalLocationUnregister(Location LocationToUnregister) |
| Mod 注册 | SLSF_Reloaded_RegisterMod → OnExternalModRegister(String ModName="-EMPTY-")；SLSF_Reloaded_UnregisterMod → OnExternalModUnregister(String ModName="-EMPTY-")。插件名要求含扩展名，见 L804-L839。 |
| 类别 flag | SLSF_Reloaded_Set{Slut,Whore,Exhibitionist,Oral,Anal,Nasty,Pregnant,Dominant,Submissive,Sadist,Masochist,Gentle,LikesMen,LikesWomen,LikesOrc,LikesKhajiit,LikesArgonian,Bestiality,Group,Bound,Tattoo,CumDump,Unfaithful,Cuck,Airhead,WhoreEvent,SexWorker}Flag → 对应 OnExternal{类别}Flag(String ModName, Bool Flag)；WhoreEvent 已 deprecated，SexWorker 是替代名称（L885-L994）。 |
| 类别启用 | SLSF_Reloaded_EnableFameType → OnEnableFameType(String FameType, String ModName, Bool Flag) |
| 请求/查询 | SLSF_Reloaded_RequestLocation → OnRequestLocation(Bool Strict)；SLSF_Reloaded_RequestRecognitionTime → OnRequestRecognitionTime(String FameLocation)；SLSF_Reloaded_RequestFame → OnRequestFame(String LocationName, String Category)；SLSF_Reloaded_RequestCumVisibility → OnRequestCumVisibility(String CumLocation)；SLSF_Reloaded_RequestFlagState → OnRequestFlagState(String FlagName)；SLSF_Reloaded_RequestModFlagState → OnRequestModFlagState(String ModName, String FlagName)；SLSF_Reloaded_RequestModRegisterState → OnRequestModRegisterState(String ModName) |

七个查询回应也属于协议：SLSF_Reloaded_ReturnRequestedLocation(String)，SLSF_Reloaded_ReturnRecognitionTime(Float)，SLSF_Reloaded_ReturnRequestedFame(String LocationName, String Category, Int RequestedFame)，SLSF_Reloaded_ReturnRequestedCum(Bool)，SLSF_Reloaded_ReturnFlagState(String FlagName, String FlagExists, Bool RequestedFlagState)，SLSF_Reloaded_ReturnModFlagState(String ModName, Bool IsModRegistered, String FlagName, String FlagExists, Bool RequestedModFlagState)，SLSF_Reloaded_ReturnModRegisteredState(String ModName, Bool IsModRegistered)。Handler 用 ModEvent.PushString/Float/Int/Bool 按此顺序发送，见 E:/source/repos/SLSF Reloaded/Scripts/Source/SLSF_Reloaded_ModEventListener.psc:L1018-L1143。

### 内部/第三方事件挂钩

PlayerScript 监听 SexLab 的 HookAnimationStart(Int threadID, Bool hasPlayer)、HookLeadInEnd(Int threadID, Bool hasPlayer)、HookAnimationEnd(Int threadID, Bool hasPlayer)，并可监听 AND 的 AdvancedNudityDetectionUpdate()；注册和回调见 E:/source/repos/SLSF Reloaded/Scripts/Source/SLSF_Reloaded_PlayerScript.psc:L30-L65、L73-L97。旧分析器等待 10 秒后读取当前 PlayerController，检查 SexSceneEnded 与 BodySearch/LeadIn 标签，再读取参与者，见 E:/source/repos/SLSF Reloaded/Scripts/Source/SLSF_Reloaded_SexAnimationAnalyzer.psc:L25-L121。

NPCScan 的 ActiveMagicEffect 事件签名是 OnEffectStart(Actor akTarget, Actor akCaster)（E:/source/repos/SLSF Reloaded/Scripts/Source/SLSF_Reloaded_NPCScan.psc:L13-L85）。玩家对象事件包括 OnLocationChange(Location, Location)、OnObjectEquipped(Form, ObjectReference)、OnObjectUnequipped(Form, ObjectReference) 等，签名见 E:/source/repos/SLSF Reloaded/Scripts/Source/SLSF_Reloaded_PlayerScript.psc:L44-L154。订阅入口、附着脚本与签名都要纳入版本/加载测试。

## 实施资料

原数组、flags、计时器和脚本附件用于解释业务。原生数据布局、兼容载体、持久化和实施顺序统一在[项目规格](../../.scratch/native-migration/spec.md)及其任务说明维护。

原时间语义见[时间审计](time-and-polling-semantics.md)；普通 callback 与 typed ModEvent 的传输区别见[Devious Curses 审计](devious-curses-integration-audit.md)。对应实现结果记录在 evidence。
