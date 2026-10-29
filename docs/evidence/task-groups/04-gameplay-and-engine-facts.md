# 04 组证据：玩法规则与引擎事实查询

- 任务：A07（纯玩法规则）+ B04（引擎事实查询），前置组 02。
- 日期：2026-10-29（审计后重写）。
- 任务书：[04-gameplay-and-engine-facts.md](../../superpowers/plans/2026-10-27-task-books/04-gameplay-and-engine-facts.md)。
- 目标版本：Skyrim SE 1.6.1170 / SKSE 2.2.6 / CommonLibSSE-NG（仓库内 vendored）。
- 范围：本组七个 CPP、配套头文件、测试、探针和构建项；未改 `src/main.cpp`、`src/boundary/`。

## 1. 交付文件

| 模块 | 实现 | 头文件 |
| --- | --- | --- |
| M06 | `src/engine/GameData.cpp`（292 行） | `include/slsf/engine/GameData.h`（68 行） |
| M07 | `src/engine/ActorQueries.cpp`（141 行） | `include/slsf/engine/ActorQueries.h`（42 行） |
| M17 | `src/core/gameplay/Anonymity.cpp`（160 行） | `include/slsf/core/gameplay/Anonymity.h`（73 行） |
| M18 | `src/core/gameplay/Visibility.cpp`（637 行） | `include/slsf/core/gameplay/Visibility.h`（205 行） |
| M19 | `src/core/gameplay/Witness.cpp`（393 行） | `include/slsf/core/gameplay/Witness.h`（191 行） |
| M20 | `src/core/gameplay/AnimationClassifier.cpp`（493 行） | `include/slsf/core/gameplay/AnimationClassifier.h`（129 行） |
| M21 | `src/core/gameplay/Comments.cpp`（87 行） | `include/slsf/core/gameplay/Comments.h`（47 行） |

其他新增/修改：

- `include/slsf/core/gameplay/Common.h`（152 行）：`Tri` 三态、`RuleDecision`、`NumericDecision`、运行时关系分组 `ClassifyRelationshipRank`。
- `include/slsf/engine/EngineFacts.h`（115 行）：`PluginType`、`LoadedPlugin`、`EngineContext`、`ActorSnapshot`、`IEngineBackend`。
- `tests/core/GameplayRulesTests.cpp`（1389 行）：表驱动玩法规则测试，固定 `ScriptedRandom` 序列。
- `tests/core/EngineFactsTests.cpp`（653 行）：`FakeEngineBackend` 记录上下文/收集/查询调用次数和顺序。
- `tests/probe/TargetProbe.cpp`（135 行）：目标版本 ABI/契约探针。
- `xmake.lua`：新增 `SLSFEngine`、`SLSFEngineProbe`、`SLSFEngineAsan`，测试依赖加入 engine；`tests/core/xmake.lua` 同步依赖。
- `.scratch/native-migration/spec.md`：同步 A07/B04 状态并链接本报告。
- 未把 `SLSFEngine` 链入插件；边界适配器和 Lifecycle 组装留给 06 组。

## 2. 已实现的玩法规则

### 2.1 匿名（Anonymity）

- 每个 `RegionId` 独立保存 `localAnonymous/startedAnonymous/recognitionDays/lastEnter/lastExit`；`ObserveRegion(previous,current,now)` 只在区域实际变化时消费进入/离开：每次实际进入、实际离开各计 1（已进入过的区域间切换为 2；离开未进入过的区域只计当前 Enter 的 1），不会创建默认记忆，也不会把绝对游戏时间当停留时间。
- 离开：`timeSpent=lastExit-lastEnter`；短于 0.125 天且本地匿名则只更新 `lastExit`；否则加 `timeSpent`（本地匿名或进入时匿名再减 0.125），上限 365，保留旧规则不设 0 下界。
- 进入：`away=max(0,now-lastExit)`；首次进入且 `now>lastExit`（或离开足够久，`away>R`）时置本地/本轮匿名，`R=max(7, 2R-away)`；`now=0` 的首帧与旧脚本一样不置匿名；已在本地匿名时同样衰减。
- 每小时 `AdvanceHourly` 仅在停留达到 0.125 天时清除 `localAnonymous`，不改变 `startedAnonymous` 或记忆；缺区域返回 `Missing`，时间倒流返回 `Stale`。
- `Evaluate`：动态匿名用本地标志、领主豁免和低名气区域三态；静态匿名用 `HidesIdentity`、DD Hood、头槽 31 的 Bikini/DoesNotHideIdentity 关键字链；必要事实未知返回 `NotReady`，不会把未知当 false。
- 旧来源：`E:/source/repos/SLSF Reloaded/Scripts/Source/SLSF_Reloaded_DynamicAnonymity.psc`。

### 2.2 可见性（Visibility）

- 槽位固定为 31 头、32 身体、33 手、37 脚、45 项圈、46/47 背部、49 腰带、53 大腿、54 小腿、56 胸罩、58 束具；`ItemFacts` 保存每槽的 present/关键字/DD 事实。
- 身体纹身按旧分支 A/B/C/D：SLS+AND、仅 AND、SLS 无 AND、两者皆无；含 `-NONE-`、Chest、Pelvis、Ass、Back、Thigh、Calves 的子类判定。Back 的上下槽条件为“下槽开放且（上槽缺失或上槽开放）”；上槽缺失不能绕过下槽 `DoesNotCoverBack`。
- 每个子类都先用 `SlaveTats` 安装、已应用数量、目标槽 `applied` 判可见；`appliedCountKnown` 为假时保守返回未知，不把“数量事实不可用”当成 0。上限由适配器按旧配置（默认 Body 20、Face/Hands/Feet 10）裁剪后填入槽数组。
- 脸部仅在 `AnonymityEnabled && !DynamicAnonymity` 且玩家匿名时直接不可见；动态匿名/匿名关闭时走 HidesIdentity、DD Hood、头槽链。`AnonymityEnabled` 已知为假时不再要求 `DynamicAnonymity` 已知。
- 手/脚分别使用 `CoversHands`/`DoesNotCoverHands`、`CoversFeet`/`DoesNotCoverFeet`，槽 33/37。
- 肛门/阴道/口腔精液区分“是否可见”和“可见计数”：前者先要求来源计数 > 0，后者保留旧脚本无计数门槛的语义；口腔含身份/DD Hood 门槛；男性阴道可见在所有状态（含 Stale/Invalid）均不返回 Known。
- 束缚：DD 未装时三个 bound 标志归零；`beltWorn/collarWorn` 仍回传输入事实，四个 `*Visible` 与 `wearingUnhideable` 归零（旧脚本提前返回、保留旧全局值；这是刻意近似，详见第 9 节）。HeavyBondage 时 `heavilyBound=true`，`visiblyBound` 受匿名门控；Lockable 下按 45/49/56/58 的 DD 关键字与 AND 等级、身体槽空置条件组合；49 腰带缺失时回退 58；重度分支按旧脚本先清零再置位。`FromTriChecked` 对所有非 Ready 快照拒绝从事实推导 Known，Stale/Incompatible 返回 `NotReady`，Invalid 返回 `Invalid`。
- 旧来源：`E:/source/repos/SLSF Reloaded/Scripts/Source/SLSF_Reloaded_VisibilityManager.psc`（IsPlayerAnonymous 205-232、CheckAppliedTattoos 234-318、IsBodyTattooVisible 402-521、Face 523、Hand/Foot 558+、Cum 584-770、CheckBondage 776-906）。

### 2.3 目击与名气候选（Witness）

- `NpcScanner` 按旧顺序过滤：位置无效/死亡/战斗/性场景活动 → 需要 LOS 时距离和 `HasLOS` → `IsDetectedBy` → 关系已知；每个幸存 NPC 消耗一次 `UniformInt(1,100)`，成功后再遇到候选仍继续掷，单次扫描最多一次成功；fresh `Run` 开头重置成功标志，暂停恢复则保留。
- 暂停批次可恢复：`NpcScanResult::resumeIndex` 记录未决目标，下一次用同一 Actor 列表调用 `Run` 时从该位置继续，保留已成功标志、不重掷已处理 NPC；不同列表则重新开始。
- 关系分组：rank < 0 敌人、0 中立、1..3 朋友、>=4 情人；使用运行时等级，不使用 BGSRelationship 静态 0..8。
- 未知事实只暂停依赖它的后续候选，已处理候选结果保留；不会先掷骰再补未知。
- `FameGainRoller` 在检查候选前**无条件**掷 `FriendLoverBetrayRoll(1,100)`；候选按枚举/ESP 顺序 0..24 建立；朋友/情人高名气修正值 `BetrayRollModifier` 跨候选粘滞：每个可选类别按当前名气更新（朋友名气/2、情人名气/4），非禁止类别也更新，后续类别使用同一修正值；只有依赖的类别/名气未知才返回 `NotReady`。
- `CandidateSelector`：0 个候选直接 Ready；1 个不掷骰直接选中；多个先掷 `1..count`，等于 count 时按输入顺序全选；否则最多 50 次 `0..count-1`，用 `selectedMask` 去重，耗尽则 `shortfall`，重复抽取不增加已选数。
- 旧来源：`SLSF_Reloaded_NPCScan.psc`、`SLSF_Reloaded_FameManager.psc`（FameGainRoll 749-1282）。

### 2.4 动画分类（AnimationClassifier）

- 顺序保留：无效/匿名/非法位置/10 秒起始宽限/场景结束/BodySearch/LeadIn；LeadIn 只有在允许 Foreplay 名气时继续。
- 单 Actor 只触发 `SingleActorScan`，不进入 SubAnalyze、不掷吻别骰；多 Actor 先掷一次 `KissAndTellRoll(1,100)`，再按运行时关系分组得出 AllFriends/AllLovers，掷骰不通过则不评分。
- `SubAnalyze` 按旧追加顺序输出：Oral、Anal、Group、Whore/Slut、Submissive/Dominant、Bound、Masochist、Sadist、Likes Women、Likes Men、Bestiality、Gentle、按 Actor 顺序的种族类别（重复种族保留重复项）、DD Sadist；关系门槛、受虐/施虐标志置位、受害者/强奸放行表达式的短路顺序均按旧脚本。
- 未知事实清空已确定类别并返回 `Deferred/NotReady`；`Status::Invalid` 明确返回 `Invalid` 且不掷骰。
- `AnimationRace::Unknown` 会把三种种族喜好标为未知并暂停；已知但不匹配的种族必须由适配器填 `AnimationRace::Other`，否则会产生旧脚本没有的过度暂停。
- 10 秒宽限由执行器提供 `AnimationFacts::startGraceElapsed`；未知或未到时间时纯规则返回 Deferred，常量 `kAnimationStartGraceSeconds=10.0` 供调度器使用。
- 夜间/前戏的名气减半仍由既有 `GainRules` 的 `night && foreplay` 负责；本组只输出 `foreplay` 和类别。
- 旧来源：`SLSF_Reloaded_SexAnimationAnalyzer.psc` 25-276。

### 2.5 评论（Comments）

- 先算裸体评论机会：PublicWhore 且禁用裸体评论 → -1；否则未装 AND → 100；该事实未知/非法时整体返回对应状态，不进入后续 RNG。
- 匿名或战斗状态不掷骰；匿名下一次延迟 10 秒，其余（含战斗）5 秒。
- 正常路径每周期一次 `UniformInt(1,100)`，与整数化 `CommentFrequency` 比较；结果带 `nextDelaySeconds` 供执行器调度。
- 旧来源：`SLSF_Reloaded_CommentManager.psc`（47 行）。

## 3. 引擎查询与 DTO

- `IEngineBackend` 只暴露 `Context()`、`CollectLoadedPlugins()`、`QueryActor(actor,player)`；`GameDataService` 与 `ActorQueriesService` 都先检查 `EngineContext{mainThread,inGame}`，非法上下文不调用后端。
- 插件表保留 `LoadedPlugin{name,type}`，类型区分 Unknown/Esm/Esp/Esl；`IsPluginLoaded` 和 `MakeStableKey` 对插件名做 ASCII 大小写无关规范化，`StableFormKey{canonicalPlugin,localFormId}` 保留本地 FormID；FormID 0 与大于 0xFFFFFF 拒绝。
- `ActorSnapshot` 包含 Actor/玩家/位置/loaded/validLocation/dead/inCombat/sexLabActive/LOS/playerDetected/distance/relationship/detection/装备，所有可缺事实用 `Fact`；关系等级校验 -4..4，距离必须有限且非负，detection 非负。Store 规范化 Actor/玩家 key 并校验 FormID；Query 还校验后端返回值与请求的 Actor/玩家一致。
- `GameDataService::RefreshDirty(player)` 对每个 dirty 目标恰好一次 `QueryActor`；成功 Ready 更新缓存并移除 dirty；非 Ready 快照（含 Missing）缓存并保留 dirty；后端 `Error::NotFound` 映射为 Missing，作为当前加载期的终止态缓存并移除 dirty，避免永久污染后续刷新；其余后端错误、Actor 不匹配、非法事实写 Unknown 快照并保留 dirty。适配器确定 Form 永久缺失时应返回 `Error::NotFound`，再次看到该 Actor 时需 `MarkDirty`。
- `RefreshDirty`/`RefreshTargets` 先规范化并校验 player FormID；非法 key 在任何后端调用前返回 `Invalid`。`ActorSnapshot::status` 的 Ready/非 Ready 契约写在 `EngineFacts.h`：Ready 完整，非 Ready 是保留 dirty 的部分观测，永久缺失通过后端 `Error::NotFound` 表达。
- `RefreshTargets` 只刷新调用方给出的 Actor，不消费无关历史 dirty；`Forget` 删除缓存和 dirty 位；`RefreshPlugins` 清缓存但保留规范化后的 dirty 集合。
- 缓存 Key 在 Store/MarkDirty/Find/Forget 统一规范化；`Find` 返回内部指针，Store/Clear/RefreshDirty/Forget/RefreshPlugins 后失效，`QueryCached` 立即复制。服务无内部互斥锁，只能在游戏主线程或统一执行器线程调用。
- `ActorQueriesService::QueryAll`/连续查询覆盖当前业务 Actor 列表；`QueryCached` 只读缓存；`ToNpcScanTargetFacts` 把引擎快照复制成玩法 DTO，距离只在 `distanceKnown` 为真且有限非负时设为已知。`QueryAll` 中途失败会保留之前已成功写入的缓存，这是可观察的部分副作用。
- 已安装/动态/未知保持区分：插件类型、mod 安装事实、关系事实、距离事实各自带 `FactState`，不会被合并成布尔值。

## 4. ABI 与游戏行为来源

| 事实 | 来源 | 探针结果 |
| --- | --- | --- |
| 目标运行时 | `SKSE::RUNTIME_SSE_LATEST` / Skyrim 1.6.1170 | `probe_target_runtime=1.6.1170.0` |
| BGSRelationship 等级 0..8、size 0x40 | `lib/commonlibsse-ng/include/RE/B/BGSRelationship.h` | `static_assert` 通过；`probe_relationship_sizeof=64`，`probe_relationship_static_levels=0..8` |
| 静态等级到运行时 rank | SKSE 2.2.6 `E:/source/repos/skse64/skse64/GameData.h:430-456`、`GameData.cpp:228-236` 只说明索引 `value[]`；**4-level 映射是假设，未由该源码证明** | `probe_relationship_runtime_mapping=assumed:4-level;unverified` |
| 关系缺失 | 适配器契约：空 Actor/无记录表示为 `Fact::Unknown` | `probe_relationship_unknown_dto=unknown-preserved` |
| 检测签名 | CommonLib `RE/A/Actor.h`：`RequestDetectionLevel(Actor*, DETECTION_PRIORITY)` | `static_assert` 通过；行为未验证 |
| 视线签名 | CommonLib `Actor::HasLineOfSight(TESObjectREFR*, bool&)` | `static_assert` 通过；行为未验证 |
| 装备槽 | CommonLib 装备槽/旧 Visibility 脚本 | `static_assert` Body32/Collar45/Belt49/Bra56/Harness58，且 engine/gameplay 槽数一致 |
| 玩法公式 | `E:/source/repos/SLSF Reloaded/Scripts/Source/SLSF_Reloaded_*.psc`（见各节） | 表驱动用例覆盖典型值和边界 |

探针完整输出：

```text
probe_target_runtime=1.6.1170.0
probe_relationship_sizeof=64
probe_relationship_static_levels=0..8
probe_relationship_runtime_mapping=assumed:4-level;unverified
probe_dto_relationship=rank4->Lover;distance=10
probe_relationship_unknown_dto=unknown-preserved
probe_context_contract=unit-tested:legal:(mainThread&&inGame);else-InvalidState-no-backend-call
probe_detection_signature=RequestDetectionLevel(Actor*,DETECTION_PRIORITY)->int32
probe_los_signature=HasLineOfSight(TESObjectREFR*,bool&)->bool
probe_runtime_behaviour=unverified:relationship-record,detection-level,line-of-sight
probe_runtime_game_queries=pending:no-running-game-backend
probe_result=STATIC_PASS
```

**实机缺口**：构建机已安装 Skyrim SE 1.6.1170（`E:\st\steamapps\common\Skyrim Special Edition\SkyrimSE.exe`，存在 `skse64_1_6_1170.dll`），但本次没有启动游戏、也没有运行中的 CommonLib `IEngineBackend`。因此关系记录的运行时 -4..4、detection 返回值、LOS 返回值、距离和逐 NPC 可见性仍是 **pending**；静态签名、枚举、sizeof、DTO 翻译和纯规则已通过。探针不伪造这些结果。

## 5. 测试方法与命令结果

测试策略：玩法规则为确定性状态机/表驱动，固定输入和固定 `ScriptedRandom` 序列，同时断言随机调用的区间、次数和掩码；全量 Catch2 用 `--rng-seed 1170 --order rand` 打乱用例顺序。引擎层用 `FakeEngineBackend` 记录调用次数/顺序；ASan 覆盖数值和生命周期；解析器模糊测试作为既有回归，本组未修改解析器。

| 命令 | 退出码 | 结果 | 日志 |
| --- | --- | --- | --- |
| `xmake build SLSFCoreTests SLSFEngineProbe SLSFReloadedNG` | 0 | core 测试、探针和插件均构建成功 | `build/test-results/04-build.log` |
| `SLSFCoreTests.exe --rng-seed 1170 --order rand` | 0 | 120451 assertions / 178 cases | `build/test-results/04-full.log` |
| `SLSFCoreTests.exe "[gameplay]" --rng-seed 1170 --order rand` | 0 | 362 assertions / 13 cases | `build/test-results/04-gameplay.log` |
| `SLSFCoreTests.exe "[engine]" --rng-seed 1170 --order rand` | 0 | 252 assertions / 18 cases | `build/test-results/04-engine.log` |
| `tools/run-native-tests.ps1 -Mode core -Seed 1170` | 0 | 20 shards / 178 cases，JUnit `core.xml` | `build/test-results/04-core.log` |
| `... -Mode boundary -Seed 1170` | 0 | 4 shards / 4 cases / 22 assertions | `build/test-results/04-boundary.log` |
| `... -Mode asan -Seed 1170 -Jobs 1` | 0 | core 120451/178；boundary 22/4；预期的负向 OOB 被捕获 | `build/test-results/04-asan.log` |
| `... -Mode fuzz -Target StateCodecFuzz -Runs 100000 -Seed 1170` | 0 | 100000 runs，cov 885，corpus 239 | `build/test-results/04-fuzz.log` |
| `SLSFEngineProbe.exe` | 0 | `probe_result=STATIC_PASS` | `build/test-results/04-probe.log` |

日志均为本次重跑生成的 transcript，含 `COMMAND:` 与 `EXIT_CODE=0` 头尾；runner 另在 `build/test-results/{core,boundary,core-asan,boundary-asan}-shards/shard-NN.log` 保存分片输出。JUnit 位于 `build/test-results/core.xml`、`boundary.xml`、`core-asan.xml`、`boundary-asan.xml`。

ASan 负向探针在 `tests/asan/AsanProbe.cpp:15` 故意越界，输出 `AddressSanitizer: heap-buffer-overflow`，runner 仍判定通过；这是预期的检测能力验证。

## 6. 独立审计与修复

四个只读子代理分别审计玩法/引擎/报告，全部返回 PASS WITH FINDINGS，无 P0/P1/P2。报告审计另给出 8 项 P3（F-01..F-08）：F-03 已修代码并加回归，其余文档/覆盖/边界项已修正或明确记录。

| 审计 | 范围 | 主要发现与处置 |
| --- | --- | --- |
| `5c53da01` | 匿名/可见性 | Back 上下槽短路、Stale/Incompatible 产生 Known、Face 多余要求 dynamic、TattooEarly 把未知数量当 0、男性阴道绕过状态、未进入区域离开创建记忆——全部修复并加回归用例 |
| `942fe46f` | 目击/动画/评论 | 扫描器首个跳过即中止、复用成功标志、betray 修正值不粘滞、暂停批次不能恢复、RNG 区间断言不足——全部修复；动画 Invalid 映射和 race 契约已补 |
| `b179468a` | 引擎/ABI/线程 | NaN/Inf 距离、NotFound 永久 dirty、Refresh 波及无关 dirty、Key 大小写、FormID 边界、玩家未校验、ASCII locale、槽数未绑定、报告把假设当证明——除运行时行为待实机外均已修复或明确记录 |
| `2a6d2138` | 报告逐项 | 最终 PASS WITH FINDINGS（0 P0/P1/P2，8 项 P3）；旧计数/旧日志/旧探针输出已修正；“SKSE 证明 4-level”已改为假设；F-03 已修代码，其余见下 |

关键修复：

- `Visibility::BackSlots` 改为 `TriAnd(lowerOpen, TriOr(TriNot(Present(upper)), upperOpen))`；上槽缺失但下槽遮背时返回 false。
- `FromTriChecked` 对 Stale/Incompatible 也拒绝推导，统一为 NotReady；Invalid 仍为 Invalid。
- `AnonymityRules::ObserveRegion` 只在目标区域已有记忆时执行 Leave，避免未进入区域的离开把绝对游戏时间写入 recognition。
- `NpcScanner` 逐目标 skip/continue，未知目标暂停，`resumeIndex` + 同一 Actor 列表恢复；不重掷已处理候选。
- `UpdateBetrayModifier` 让朋友/情人高名气修正值在所有可选类别间粘滞，`BetrayGate` 使用同一修正值；补“仅 Oral 被禁止时 Whore 的高名气仍修正 Oral”用例。
- `AssGate`/`VaginalGate` 改用带 `installed` 门的 `AndCondition`，未安装 AND 时 `rankIsOne` 不能单独产生可见；补“未装 AND 但 rank=true”的肛门/阴道回归用例。
- 引擎校验拒绝非有限/负距离、无效玩家 FormID、Actor/玩家 key 不匹配（Query）；`RefreshDirty`/`RefreshTargets` 先校验 player，后端 `Error::NotFound` 终止 dirty，其他失败保留；新增 `RefreshTargets`/`Forget`，Key 端到端规范化，ASCII 折叠不依赖 locale。
- `TargetProbe` 用 `static_assert` 验证函数签名/枚举/槽位，删除无断言硬编码契约输出，统一 `probe_result=STATIC_PASS` 与 `runtime_mapping=assumed...unverified`。

报告审计 P3 处置：F-01（区域切换计数措辞）、F-02（fresh Run 与 resume 的成功标志）、F-04（DD 未装分支完整偏差）、F-05（NaN/Inf 覆盖范围）、F-07（审计状态）、F-08（首次进入/重度可见边界）均已修正；F-06 无 transcript，已标注未复现；F-03 已修代码并加回归（见上）。

## 7. 失败输入与修复记录

| 输入/现象 | 来源 | 修复与复验 |
| --- | --- | --- |
| 早期 `Status::NotFound`/C2838 记录 | 无可用 transcript，未复现 | 标注为历史记录、不作为已证失败；当前类型只有 `Status::Missing`，构建 exit 0 |
| 测试文件误写 `facts.匿名` | 首次编译失败 | 改为 `facts.anonymous` |
| CandidateSelector 用例样本 `{2,0}` 仅选中 1 个且 wanted 区间断言错 | gameplay 测试 exit 1 | 改为 `{2,0,2}`，断言 `{1,3}`、`{0,2}`，selected 2 个 |
| Back 上槽缺失、下槽遮背仍返回可见 | 审计复现 | 修正 `BackSlots`；branch A/B 回归用例均断言 false |
| Stale 快照的 bondage/cum 返回 Known | 审计复现 | `FromTriChecked` 拒绝所有非 Ready；stale 用例断言 NotReady |
| 死亡/无效 NPC 后续候选未掷骰 | 审计复现 | 逐目标 skip/continue；用例验证后续有效候选仍处理 |
| 复用扫描器第二次不评分 | 审计复现 | fresh `Run` 重置 success，resume 保留；两次扫描/恢复用例通过 |
| 高名气修正值只对当前类别生效 | 审计复现（roll 60/chance 25） | 改为跨类别粘滞；用例候选 `{Whore,Oral}` |
| `TattooEarly` 把 `appliedCountKnown=false` 当 0 | 审计复现 | 改为 Unknown/NotReady；加用例 |
| 未进入区域 Leave 写入 R=绝对时间 | 审计复现 | 仅已有记忆时 Leave；加用例 |
| NaN/+Inf 距离被接受 | 引擎审计 | `std::isfinite` + 非负；RefreshDirty 覆盖 NaN/±Inf，Store/Query 覆盖 NaN/+Inf（同一 isfinite 分支） |
| 玩家 key 与请求不符仍缓存 | 引擎审计 | Store 校验玩家 FormID；Query 校验 expectedPlayer；新增用例 InvalidState 且不缓存 |

## 8. 旧报告逐项核验

| 旧报告项 | 核验结果 |
| --- | --- |
| 行数 189/127/155/622/326/493/87、测试 1077/439、探针 121 | 过期；已改为 292/141/160/637/393/493/87、1389/653、135 |
| 120246/170、gameplay 257/12、engine 152/11 | 过期；当前 120451/178、362/13、252/18 |
| `probe_relationship_levels=4,3,...`、`probe_relationship_encoding=...`、`probe_detection_contract`、`probe_los_contract`、`probe_result=PASS` | 旧探针含无断言的硬编码行；已改为 static_assert 签名 + `assumed:4-level;unverified` + `STATIC_PASS` |
| “构建机未安装 Skyrim” | 错误；已安装 1.6.1170 于 `E:\st\...`；真实限制是没有运行中的游戏后端 |
| “SKSE 源码证明静态 0..8 → 运行时 4..-4” | 不支持；源码只显示 `value[]` 索引，映射仍是假设，待实机读关系记录 |
| dirty 失败语义（错误/不匹配/非法保留 dirty） | 不完整；已补后端 `Error::NotFound` 终止 dirty，非 Ready 快照（含 Missing）保留重试，其余失败保留 |
| 扫描器/关系门槛描述 | 缺少暂停批次恢复和 betray 修正值粘滞；已补 |
| 线程安全/缓存指针生命周期 | 原报告未说明；现明确无内部锁、主线程/执行器线程、Find 指针失效条件 |
| 测试日志 `04-*.log` | 旧日志来自旧二进制；本次全部重跑并生成带命令/退出码的新 transcript |

结论：除明确标注 pending 的实机行为外，旧报告的事实性错误均已修正；本报告中的数字、命令、日志和探针输出来自当前源码构建。

## 9. 已知偏差、待运行项与下组产物

已记录偏差：

- `Visibility` 的 DD 未安装分支仍从输入返回 `beltWorn/collarWorn`，并把 `collarVisible/braVisible/beltVisible/harnessVisible/wearingUnhideable` 置 false；旧脚本在该分支提前返回，只清 `IsVisiblyBound/IsLightlyBound/IsHeavilyBound`，保留这些全局旧值。此差异是刻意保留的可观测近似，已在测试和本报告记录。
- 10 秒动画宽限是执行器提供的事实门，纯核心不持有真实时钟；未到时间返回 Deferred，调度器负责重排。
- ESL 的 compact FormID 解码尚未在真实记录采集器中实现；当前稳定键接口保留 `LoadedPlugin.type` 和 `localFormId`，由后续适配器提供实际 FormID 映射。
- 插件 `SLSFReloadedNG` 尚未链接 `SLSFEngine`；边界层和 Lifecycle 组装属于 06 组。
- `ActorQueriesService::QueryAll` 中途失败会保留此前缓存写入；这是已记录的部分副作用，不声称原子。
- 引擎服务无内部互斥锁，线程安全由调用方（主线程/统一执行器）保证。
- 核心玩法 DTO 信任 `ActorQueriesService` 已校验的 -4..4 关系等级；直接构造 `NpcScanTargetFacts`/`AnimationActorFacts` 的适配器必须自行保证范围。

下组直接可用的产物：

- `SLSFEngine.lib`、`SLSFEngineAsan.lib` 及 `include/slsf/engine/{EngineFacts,GameData,ActorQueries}.h`：接 CommonLib 后端、插件表、dirty 刷新和 `ToNpcScanTargetFacts`。
- `AnonymityRules::ObserveRegion/AdvanceHourly`：区域切换和每小时采样接入执行器；每次实际进入/离开各计一次（首次或未进入过的 previous 只计 Enter）。
- `AnimationFacts::startGraceElapsed` + `kAnimationStartGraceSeconds=10.0`：执行器在动画开始后 10 真实秒填 true。
- `CommentDecision::nextDelaySeconds`：10 秒匿名/5 秒常规调度。
- `Witness` 的 `NpcScanner/CandidateSelector/FameGainRoller`、暂停恢复和 `RuleContext` 随机源：接实际事件和窗口随机序列。
- 探针构建目标 `SLSFEngineProbe`；实机运行需 1.6.1170 + SKSE 2.2.6，并补 `QueryActor` 的真实实现后复跑关系/detection/LOS/逐 NPC 可见性。
- 测试与日志：`tests/core/GameplayRulesTests.cpp`、`tests/core/EngineFactsTests.cpp`、`tests/probe/TargetProbe.cpp`、`build/test-results/04-*.log`。

实机待运行项（接口、文件、环境、操作）：

- 接口：`IEngineBackend::Context/CollectLoadedPlugins/QueryActor`；`ActorSnapshot` 的 relationship/detection/LOS/distance/equipment 字段；CommonLib `RE::Actor::RequestDetectionLevel`、`RE::Actor::HasLineOfSight`、`RE::BGSRelationship::GetRelationship`；Papyrus `GetRelationshipRank` 或 CK 数据作对照。
- 文件：`include/slsf/engine/EngineFacts.h`、`src/engine/ActorQueries.cpp`、`tests/probe/TargetProbe.cpp`；真实 `QueryActor` 适配器尚未创建（06 组，建议 `src/engine/CommonLibBackend.cpp`）。
- 环境：Windows + Skyrim SE 1.6.1170（`E:/st/steamapps/common/Skyrim Special Edition/SkyrimSE.exe`）+ SKSE 2.2.6（`skse64_1_6_1170.dll`）+ 仓库内 vendored CommonLibSSE-NG；查询必须在游戏主线程、加载完成后执行（`mainThread=true, inGame=true`）。
- 操作：用 SKSE 启动并载入存档；以实际 Player/附近 NPC 构造请求，逐 NPC 调 `QueryActor`；对照实机关系等级（-4..4）、`RequestDetectionLevel(player,kNormal)`、`HasLineOfSight`、距离和 `ToNpcScanTargetFacts`；把输出写入 `build/test-results/04-probe-live.log` 并替换第 4 节 pending 行。
- 通过标准：关系编码与 Papyrus/CK 对照一致；detection/LOS 与游戏行为一致；逐 NPC 可见性经 NPCScan 规则复现；无法取得的事实在 DTO 中保持 Unknown，不伪造成 Known。

## 10. 验收对照

- 旧版典型/边界场景：Anonymity、Visibility、Witness、Animation、Comments 五组表驱动用例覆盖进入/离开、短访、衰减、四个身体分支、脸/手/脚、精液可见计数、束缚、NPC 过滤/单次成功、暂停恢复、候选顺序/重复/50 次短缺、动画顺序/单 Actor/关系未知、评论各门控。
- 候选集合等于逐类别直接判定：`FameGainRoller` 先建候选再交给选择器，测试断言类别/顺序/掩码。
- 固定随机序列得到类别和 shortfall：`ScriptedRandom` 断言每次区间和调用数；51 次抽取示例得到 50 次尝试和短缺；候选掩码用例断言选中集合。
- dirty 刷新更新事实缓存：engine 用例断言每目标一次查询、缓存替换、部分保留 dirty、后端 NotFound 终止、RefreshTargets 不波及无关 dirty。
- 未知只暂停依赖评分：Witness/Animation/Comments 用例覆盖未知关系和未知事实下的 RNG 次数、状态和已确定结果保留。
- ABI/探针：静态断言和纯 DTO 运行探针通过；实机查询明确列为 pending，未伪造结果。

## 11. 复现命令

```powershell
cd 'E:/source/repos/SLSF Reloaded NG'
& 'C:/Program Files/xmake/xmake.exe' build SLSFCoreTests SLSFEngineProbe SLSFReloadedNG
./build/windows/x64/releasedbg/SLSFCoreTests.exe --rng-seed 1170 --order rand
./build/windows/x64/releasedbg/SLSFCoreTests.exe "[gameplay]" --rng-seed 1170 --order rand
./build/windows/x64/releasedbg/SLSFCoreTests.exe "[engine]" --rng-seed 1170 --order rand
powershell -NoProfile -ExecutionPolicy Bypass -File tools/run-native-tests.ps1 -Mode core -Seed 1170
powershell -NoProfile -ExecutionPolicy Bypass -File tools/run-native-tests.ps1 -Mode boundary -Seed 1170
powershell -NoProfile -ExecutionPolicy Bypass -File tools/run-native-tests.ps1 -Mode asan -Seed 1170 -Jobs 1
powershell -NoProfile -ExecutionPolicy Bypass -File tools/run-native-tests.ps1 -Mode fuzz -Target StateCodecFuzz -Runs 100000 -Seed 1170
./build/windows/x64/releasedbg/SLSFEngineProbe.exe
```

随机种子：`1170`。玩法测试内部随机序列由 `ScriptedRandom` 固定，不依赖时间/环境；全量 Catch2 的 `--order rand` 固定到 seed 1170。模糊测试的 corpus 目录会在多次运行间演化，因此 cov/corpus 数字记录的是本次实际值（100000 runs，cov 885，corpus 239）。
