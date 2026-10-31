# 第 08 组独立审核与对抗验证

历史快照：以下裁决针对 2026-10-31 的源码及归档 hash，不代表修复后的工作树。后续实现、回归与独立复核见 [2026-11-01 修复报告](2026-11-01-08-fixes.md)。本文件保留原发现和复现输出。

日期：2026-10-31。裁决：**Standards 未发现硬性违规；Spec 不通过，包含 1 项需求缺项、2 项已复现的行为缺陷。08 组不能按完整任务书验收。**

本次以当前工作区源码为依据。组内交接报告只用于提取待验证主张；通过的测试、历史审核结论和最小编译 probe 都不代替实现审查。

## 范围与复现身份

- 基线：`main`，HEAD `741cf5b67b53cdeed5669f673482c4d2a9ff74b7`。08 组仍在工作区，包括未跟踪文件，因此这个 HEAD 本身不包含被审实现。
- 审核包含三种外部状态适配器、公共 wire、PapyrusCalls 的本次变更及测试，并追到分类器、生产入口、引擎身份查找、任务书和项目需求。
- 三个独立子代理均按要求使用 **gpt-6-luna / xhigh**，分别负责 Spec、Standards/安全、算法/事实映射。主审独立构建、执行普通与 ASan 测试，并编译独立反例程序。没有把子代理发现直接当作游戏内复现。
- 未修改生产源码、原测试、原交接报告或 Git 提交。仅新增本审核文档及证据归档。
- 被审源码与要求的 SHA256 清单：[source-sha256.txt](</E:/source/repos/SLSF Reloaded NG/docs/evidence/audits/2026-10-31-group08/source-sha256.txt>)。

## Standards

独立标准审查未发现可举证的硬性仓库标准违反；本组新增集成源码未增加普通说明注释，符合 AGENTS 默认零注释要求。Fowler 气味核查未形成需要报告的新增问题。

在源码审查及本次测试中未发现已确认的越界、悬空访问、重复释放或无界栈增长。线程与对象寿命结论以原报告第 197 行的串行执行、析构约束得到落实为前提；生产装配尚未验证这些约束，不能据此推断任意线程调用安全。

Standards 计数：硬性违规 **0**；已确认内存/栈/线程缺陷 **0**。

## Spec

### G08-S1 / P1：实际事实采集器缺失，本组需求被推迟

08 任务书明确要求接入 P+ 普通通知、公开 VM 事实和前戏 readiness，保留计分宽限，并实现实际 DDNG/其他外部事实快照及未知后恢复行为，见[任务书第 19 行](</E:/source/repos/SLSF Reloaded NG/docs/superpowers/plans/2026-10-27-task-books/08-pplus-ddng-and-external-state.md:19>)及[第 24 行](</E:/source/repos/SLSF Reloaded NG/docs/superpowers/plans/2026-10-27-task-books/08-pplus-ddng-and-external-state.md:24>)。C03/C04/C07 的细化要求分别位于[集成计划第 23 行](</E:/source/repos/SLSF Reloaded NG/docs/superpowers/plans/2026-10-27-integrations-and-delivery-plan.md:23>)、[第 31 行](</E:/source/repos/SLSF Reloaded NG/docs/superpowers/plans/2026-10-27-integrations-and-delivery-plan.md:31>)、[第 51 行](</E:/source/repos/SLSF Reloaded NG/docs/superpowers/plans/2026-10-27-integrations-and-delivery-plan.md:51>)。

当前交付实现了 wire 消费器、快照投影和可控 VM 下的请求状态机；缺少实际产生这些 wire 的 provider：

| 分支 | 实际调用目标 | 缺少的交付 |
|---|---|---|
| P+ | [PPlusOptions](</E:/source/repos/SLSF Reloaded NG/include/slsf/integrations/SexLabPPlus.h:110>) 默认 `SLSF_Reloaded_PPlusProbe.CollectScene` | 未找到该 collector。现存 `SLSFGroup08PPlusProbe.Probe(SexLabThread, Actor)` 只返回 bool，不能提供角色、28 项交互、标签作用域和 epoch 帧。 |
| DDNG | [DdngOptions](</E:/source/repos/SLSF Reloaded NG/include/slsf/integrations/DeviousDevices.h:92>) 默认 `SLSF_Reloaded_DDProbe.CollectScene` | 未找到该 collector。现存 bool probe 只遍历设备并检查 belt/gag keyword，不能提供完整设备/束缚位、请求角色集合及装备 revision。生产入口只做能力探测。 |
| 其他模组 | [ExternalModsOptions](</E:/source/repos/SLSF Reloaded NG/include/slsf/integrations/ExternalMods.h:71>) 默认 `SLSF_Reloaded_ExternalProbe.Collect` | 未找到该 collector。怀孕、膨胀、Public Whore 等实际公开查询及旧阈值转换仍没有对应提供方实现。 |

报告在[第 342 行](</E:/source/repos/SLSF Reloaded NG/docs/evidence/task-groups/08-pplus-ddng-and-external-state.md:342>)主动把全量 collector 推给下组，因此这不是仅缺目标游戏验收。该处拟定的单个 `ExternalProbe.CollectPPlus/CollectDdng` 名称也与当前三个默认调用目标不同，后续需统一协议入口。

本项 P1 仅由本组实际 provider/事实采集器缺失支撑。下列端到端通过标准也未闭合，但不额外计入本项缺陷：通知 thread ID/`strArg` 解析与生产事件桥、真实 VM 宿主、宽限的执行器时钟、注册恢复及评分路径对 `BlocksNewScoring` 的消费。这些需相应装配和下游业务层接入；分类器已有 grace/leadIn 入口也不等于端到端行为已经实现。前戏切换的适配器错误另见 G08-S2。

**边界：** B03 的生产泵和 C02 的真实 VM 宿主是既有跨组待办，本审核不重复把它们列成 08 新缺陷。但是 C03/C04/C07 自己要求的 provider 和上述行为不能仅凭假 VM/手工 wire 测试判完成。若选择分期交付，应正式缩小验收范围，当前任务书未授权这种延期。

**本项完成条件：** 补齐采用的 collector，按生产请求/结果约定验证真实事实采集。DDNG 若继续采用 Papyrus 采集，应说明与 C04 v2 接口要求的对应关系并验证实际设备数据。事件桥、宽限执行和未知恢复等端到端验收由相应负责组继续闭环。

### G08-S2 / P2：LeadInEnd 被忽略，旧阶段仍保持 Ready

[SexLabPPlus::OnSceneEvent](</E:/source/repos/SLSF Reloaded NG/src/integrations/SexLabPPlus.cpp:468>) 只将 `AnimationStart/SceneStart/StageStart/StageChange` 视为开始或刷新事件；`LeadInEnd` 在[第 491 行](</E:/source/repos/SLSF Reloaded NG/src/integrations/SexLabPPlus.cpp:491>)返回空请求，不失效旧快照，也不安排有界重查。C03 明确要求接收普通 `AnimationStart/LeadInEnd/AnimationEnd`；本地 P+ 审计记录 `LeadInEnd` 在主场景切换完成前发出，不能以此时旧帧作为新阶段 Ready。

主审探针先完成带 LeadIn 标签的 stage 1 帧，再发送 `SexLabLeadInEnd`：

```text
lead_in_end returned_request=0 request_count=1 ready=1 old_stage=1
synthetic_stage_start request_count=2 waiting=1
```

第二行是控制组：代码认识的合成 `StageStart` 才使适配器发起第二次请求并进入 Waiting。C03 列出的普通通知不包含 `StageStart`，不能假设真实事件流随后会用它补救漏掉的 LeadInEnd。现有[回归用例第 1166 行](</E:/source/repos/SLSF Reloaded NG/tests/core/ExternalStateTests.cpp:1166>)反而把 LeadInEnd 无操作固化为期望。

**影响：** 适配器交付旧阶段 Ready 快照；接入评分后可能使用过期标签或交互。当前没有生产事件桥，因此本结论是模块路径复现，不声称已在游戏中计错分。

**修正方向：** 处理实际 LeadInEnd，失效旧阶段并在 scene epoch 下按新阶段 readiness 有界重查；回归应覆盖通知先于主阶段就绪的真实次序，不能只把旧帧重新取回便判 Ready。

### G08-S3 / P2：未知交互被投影成已知缺席，分类继续运行

[InteractionState](</E:/source/repos/SLSF Reloaded NG/src/integrations/SexLabPPlus.cpp:176>) 能返回 `Tri::Unknown`，但 `HasInteraction` 只保留 `Tri::True`。[ApplyPPlusToAnimation 第 413 行](</E:/source/repos/SLSF Reloaded NG/src/integrations/SexLabPPlus.cpp:413>) 用这些 bool 合成 Oral/Anal 标签，没有保存相应 Unknown。下游[TagTri](</E:/source/repos/SLSF Reloaded NG/src/core/gameplay/AnimationClassifier.cpp:13>) 把标签不存在转换成 false，Oral/Anal 分类据此执行。

项目要求部分字段未知时暂停依赖这些字段的计分，见[design.md 第 115 行](</E:/source/repos/SLSF Reloaded NG/.scratch/native-migration/design.md:115>)和[spec.md 第 28 行](</E:/source/repos/SLSF Reloaded NG/.scratch/native-migration/spec.md:28>)。

独立反例使用合法 Available 帧、两个角色、恰好一个玩家、没有 Oral 标签；仅改变玩家 `pOral` 的 unknown bit 23。其他分类前提明确设为已知，避免未知关系或 grace 掩盖本缺陷：

```text
oral unknown_mask=0 parsed=1 tri=0 applied=1 tag=0 ready=1 kind=4 category=0 category_count=3
oral unknown_mask=800000 parsed=1 tri=2 applied=1 tag=0 ready=1 kind=4 category=0 category_count=3
```

此处 `ready` 来自 `AnimationResult::IsReady()`，表示分类结果可继续使用；`kind=4` 为 `Analyzed`。该反例验证 Apply 后的分类路径，不代表已验证适配器的 `ExternalGate::Ready`。已知 false 与 Unknown 得到相同的可分析结果，没有 Oral 类别，也没有等待。Unknown 在适配器到分类器的事实边界丢失。

**修正方向：** 保留与分类相关的三态事实或按依赖传播未就绪；汇总时已知 true 可以决定 OR 为 true，但没有 true 且存在相关 unknown 时不能降为 false。新增回归应走 Apply 后的真实分类路径。

Spec 计数：需求缺项 **1**、已复现行为缺陷 **2**；最严重项为 G08-S1。没有把下文条件风险混入此计数。

## 算法、内存、堆栈和线程审查

| 检查项 | 源码/反证结果 | 结论边界 |
|---|---|---|
| P+ 交互与作用域 | 28 位索引与本地上游 `SexLabThread.psc` 相符；Thread 标签不进入 Scene/Stage 分类，交互掩码越界/重叠被拒绝。 | 三态投影有 G08-S3；未证明任意角色交互需要改成玩家专属，不另报方向或作用域错误；角色字段投影见 C6。 |
| DDNG 位定义 | bondage 位与上游 `API.h` 一致，`Total=0x200` 被拒绝；设备掩码为 18 位。 | 证明消费者位定义，不能证明尚未存在的 collector 正确采集每个槽位。 |
| 外部模组映射 | 八个模组的插件名映射与旧 PSC 相符，Bimbos 对应 `CustomComments.esp`。 | 布尔值/数值由 wire 提供；实际查询及旧阈值转换仍属 G08-S1。 |
| 时间/状态机 | 超时、重试、取消、旧 epoch、装备/状态变更由模块状态机处理；普通与 ASan 用例均通过。 | 不覆盖真实通知时序；G08-S2 是现有测试固化错误期望的反例。 |
| 输入边界 | 共用解析器限制 256 KiB、4096 行、32 字段/行、8192 字节/字段；角色至多 64、标签至多 512。整数/浮点及枚举有检查，拒绝非有限浮点。 | 不是无上限信任外部输入；身份范围另见 C1。 |
| 算法成本 | 重复角色比较和请求集合 permutation 最坏 O(A²)，A≤64；标签排序及复制受上述上限约束。 | 未做真实性能分布采样，没有把这些有界成本认定为生产瓶颈。 |
| 堆栈 | wire 解析为迭代过程，无按输入尺寸建立的栈数组及输入递归；字符串/集合位于堆。 | 未发现输入导致的无界栈增长。 |
| PapyrusCalls 所有权 | 调用节点用 shared_ptr 固定存活期，先登记再 Submit，atomic delivered 使终止交付只有一个赢家；外部回调在锁外调用。 | 覆盖同步 completion、取消/迟到结果等所查路径；不扩大为任意对象析构并发安全。 |
| 适配器锁与回调 | 状态更新在锁内，完成与 transport 取消在锁外，所查重入路径未见锁内外部调用造成的确定死锁。 | 原报告第 197 行的串行执行器/析构合同仍是前提。 |
| ASan | 本次普通/ASan core 与 boundary 全通过。 | ASan 不检测所有数据竞争；本次未执行真实游戏回调并发或线程消毒器验证。 |

### 条件风险与未定合同

- **C1：稳定键范围。** 三种 parser 接受 actor local ID `0` 和 `FFFFFFFF`；独立反例均返回成功。但[LookupLoadedRecord](</E:/source/repos/SLSF Reloaded NG/include/slsf/engine/RecordLookup.h:10>) 拒绝 `0`、大于 `0x00FFFFFF` 的键，light plugin 还限 `0xFFF`。DDNG/External 的请求集合比对约束实际可达性，故列为协议校验缺口，不声称合法生产请求已触发错分。
- **C2：模块缺行语义。** Available 外部帧可以完全省略八个 `mod` 行；查询 FHU 得到 `FactState::Missing`，不是 KnownFalse。探针为 `omitted_mod available=1 fact_state=3 known_false=0`。应明确缺行代表未安装还是不完整；当前不能把它报成“Unknown 已经转换为 false”。
- **C3：析构并发。** 三个适配器的异步 completion 捕获裸 `this`；若回调与析构并发，cancel/invalidate 不等于等待回调退出，存在 UAF 条件。但原报告第 197 行明确要求统一执行器串行调用、析构前停事件源、回调不销毁 adapter/PapyrusCalls。当前未接线，未找到违反该合同的生产调用点，不列为已确认内存缺陷；后续装配必须实际落实该合同。
- **C4：异常合同。** VM port、epoch supplier 和取消调用未被声明 noexcept，所查提交/完成路径也未统一隔离这些调用抛出的异常。真实实现尚未接入，因此列为未定义接口合同，不假设当前必然抛异常或额外要求全局吞异常。
- **C5：玩家标志完整性。** P+ parser 接受零个/多个 player；后续分类器会因玩家数不等于一而 Invalid，当前未证实错误计分，不升级为新缺陷。
- **C6：角色字段映射。** [P+ 解析器第 104 行](</E:/source/repos/SLSF Reloaded NG/src/integrations/SexLabPPlus.cpp:104>)保存 `dominant/submissive/masochist`，但 Apply 未将它们投影到分类事实。[现有分类器第 107 行](</E:/source/repos/SLSF Reloaded NG/src/core/gameplay/AnimationClassifier.cpp:107>)仍按默认配置、性别、strapon、束缚等旧规则推导类别；[P+ 研究第 45 行](</E:/source/repos/SLSF Reloaded NG/docs/research/sexlab-pplus-2.18.1-integration-audit.md:45>)要求读取角色后按 SLSF 规则映射。应明确真实角色与旧规则的映射和优先级；当前未证明必须直接以这些 bool 覆盖旧分类，因此不另报已确认错分。

## 原报告逐项对抗核验

“成立”仅表示表中限定的代码/日志主张成立；不自动包含真实提供方或游戏内行为。

| 原报告行 | 主张 | 核验裁决与依据 |
|---|---|---|
| 4、349 | C++ 实现及审核完成，运行时 probe 待执行 | **部分成立。** 消费器/状态机存在，测试通过；G08-S1/S2/S3 使完整任务验收不成立。 |
| 10–20 | 交付文件及职责 | 文件存在；第 16 行把 18 位 device 掩码写成 bondage，需更正。 |
| 19、334–335 | 55 个本组用例、最终 356 例 | 用例/结果数量与本次重跑相符；旧 353 等数字不是本次证据。 |
| 22 | 未修改公开 API、冻结签名 | 08 差异集中于集成模块和 PapyrusCalls，没有发现该主张的反例。 |
| 28–32 | availability、gate、默认 timing 及阻止计分映射 | 公共类型/函数成立；阻止业务的端到端效果仍需消费者接线。 |
| 36–37 | P+ 必需字段、掩码检查和角色身份 | 字段/掩码检查成立；“已验证角色”不能扩张成引擎解析成功，C1/C5 未被 parser 拒绝。 |
| 38–42 | 标签作用域、epoch、旧 End 不覆盖新场景 | 所查实现及对应模块用例支持。未证明真实 P+ 通知流已经接入。 |
| 43 | 合成 Oral/Anal、排序去重、Facial | Oral/Anal 和排序存在；**Facial 合成不存在**；三态有 G08-S3。 |
| 44 | active、有角色后应用玩家事实 | 正常帧支持；未知交互不满足完整事实语义，提前拒绝也不重置既有 facts。 |
| 48 | DD 必需 active/provider | **不符。** parser 没有 active 必需项；Available 要求 provider 与 revision。 |
| 49 | DD 使用 device 行 | **不符。** 实际为 `actor\tplugin\tlocalId\tbondage\tdevices`。 |
| 50 | bondage 为 18 位、Total 哨兵检查 | 哨兵拒绝成立；**18 位属于 devices，bondage 有效范围为 0..0x1ff。** |
| 51 | chastity 合成 belt/bra | 位到事实的代码与对应回归支持；不等于实际设备采集通过。 |
| 52–53 | 装备变更/旧 epoch 及 Unknown/Missing/Incompatible | 模块实现/测试支持；真实事件源未接。 |
| 57–62 | External 字段、八模组映射、PublicWhore/Bimbos 和坏数字 | 映射及解析检查存在；必需字段随 availability 分支变化，八模组完整性未要求，见 C2。 |
| 68 | 同场景复用、旧等待者 Changed | 所查替换/共享请求代码及回归支持，未发现静默遗失的新增反例。 |
| 69 | 有界重试/耗尽 Failed | 状态机支持，需调用者驱动 Advance；不证明生产泵已运行。 |
| 70 | 一次终止、epoch 过期拒绝 | 所查正常/取消/解析失败/迟到路径与回归支持；接口异常合同见 C4。 |
| 71 | 结束后旧事件不会重新激活 | 旧 epoch/停用事件模块检查支持；不覆盖漏接 LeadInEnd 的 G08-S2。 |
| 72 | Reset/析构取消，避免回调悬挂 | 发起取消成立；不能由取消推导任意并发析构安全，见 C3。 |
| 86–101 | build 和 seed 1170 普通测试通过 | **独立重跑成立**，见下表；不是采信原日志。 |
| 103–131 | 所列回归用例 | 存在并通过；LeadIn 相关用例明确固化忽略真实通知，facial 用例只证明不合成 Oral。 |
| 141–146 | ASan 通过及负向 probe | 本次 core/boundary ASan 独立通过；未重新执行原报告的 ASan 负向探针，不把其历史日志视为本次执行。 |
| 148–158 | 2000 次确定性变异、坏输入拒绝 | 变异用例包含在本次 356 例中，所列检查存在。它是有界坏输入测试，不是外部独立语义 oracle；没覆盖本次全部反例。 |
| 161–164 | 开发期重复完成/编译错误已修 | 当前相应路径/回归支持修正；开发期先失败的先后史只由历史记录说明，本次不复原。 |
| 166 | 无需保留的未修复失败输入 | **不成立。** 本次存在 G08-S2/S3 可重放反例。 |
| 172 | 历史修复 1：旧等待者一次 Changed | 当前状态机/回归支持。 |
| 173 | 历史修复 2：AnimationStart 不复用旧场景 | 当前替换路径/回归支持。 |
| 174 | 历史修复 3：aFacial 只合成 Facial | **不符。** 已不合成 Oral，但也没有合成 Facial；独立输出 `facial known=1 applied=1 tag=0`。现分类器没有 Facial 类别，不宣称现有评分损失。 |
| 175 | 历史修复 4：Anal chastity 参与 belt | 实现/回归支持。 |
| 176 | 历史修复 5：结果自带 epoch 也检查 | PapyrusCalls 实现/回归支持。 |
| 177 | 历史修复 6：旧请求不重新 active | 所查 token/epoch 路径支持。 |
| 178 | 历史修复 7：Available active=0 关闭场景 | adapter 处理路径支持。 |
| 179 | 历史修复 8：停用/无角色时先重置旧事实 | **不符。** Apply 第 376–387 行先返回，第 392 行才重置。探针 `inactive applied=0 old_player_fact=1`；遵守 applied=false 的调用方不应消费该旧事实，因此不另报已确认错分。 |
| 180 | 历史修复 9：未知/重复键及对象整帧拒绝 | 解析实现/坏输入回归支持；不包含 C1/C2 的未定义或遗漏检查。 |
| 181、189–192 | 历史修复 10：shared_ptr/atomic、锁外回调 | 当前 PapyrusCalls 路径支持，见安全表；不扩大对象寿命合同。 |
| 193、195 | 有界非递归；薄 PSC + C++ 分层 | 解析器有界成立；**薄 PSC collector 尚未交付**，不能把架构说明当作已存在的采集实现。 |
| 197 | 串行执行器、epoch、析构合同 | 明确的前置条件；当前缺生产装配，尚未证明运行时落实。 |
| 205–225 | 版本清单、SHA、DLL 导出 | 作为历史环境记录/导出日志可核对；本次未启动游戏，不把它当当前游戏行为或 P+ 2.18 安装成功。DD `GetAPI` 导出存在。 |
| 229–273 | P+ 2.18 编译通过、2.16 缺 API | 最小 probe/历史编译日志支持；本次不重跑编译。只能证明使用到的脚本表面，不能证明全量 collector 或 28 位实际采样。 |
| 277–320 | DDNG probe 编译通过 | 最小 belt/gag bool probe 及历史日志支持；不是完整设备快照或 C++ v2 设备数据验证。 |
| 322–326 | 已采用真实 provider、避免未导出的 API | **未完成/表述冲突。** provider 仅为拟实现方案；第 225 行已确认 GetAPI 导出，第 326 行不能笼统称 C++ API 未导出。虚方法通过该入口取得接口对象调用。 |
| 328、336 | 没有游戏运行时 probe | 与实际证据边界一致，不把编译结果冒充运行时。 |
| 334–337 | 已逐项复核、C++ 契约可复核 | 数字/部分代码主张成立；本表与独立反例说明其复核未覆盖已确认缺陷和报告误述。 |
| 341–345 | provider、事件桥、gate 消费留给下组 | 如实承认未完成；其中 provider 是 08/C03/C04/C07 自身要求，见 G08-S1。其余接线按负责组闭环，不重复计为本次新缺陷。 |

## 本次独立执行证据

主审重新构建 `SLSFCoreTests`、`SLSFBoundaryTests`、`SLSFReloadedNG`、`SLSFCoreTestsAsan`、`SLSFBoundaryTestsAsan`，全部退出 0。测试统一使用 `--reporter compact --rng-seed 1170 --colour-mode none`，ASan 运行使用本机 LLVM runtime。

| 可执行文件 | cases | assertions | 退出码 | 本次日志 |
|---|---:|---:|---:|---|
| Core | 356 | 145637 | 0 | [test-core.log](</E:/source/repos/SLSF Reloaded NG/docs/evidence/audits/2026-10-31-group08/test-core.log>) |
| Boundary | 4 | 22 | 0 | [test-boundary.log](</E:/source/repos/SLSF Reloaded NG/docs/evidence/audits/2026-10-31-group08/test-boundary.log>) |
| Core ASan | 356 | 145637 | 0 | [test-core-asan.log](</E:/source/repos/SLSF Reloaded NG/docs/evidence/audits/2026-10-31-group08/test-core-asan.log>) |
| Boundary ASan | 4 | 22 | 0 | [test-boundary-asan.log](</E:/source/repos/SLSF Reloaded NG/docs/evidence/audits/2026-10-31-group08/test-boundary-asan.log>) |

独立反例程序链接上述当前 `SLSFCore.lib`，使用真实 parser/Apply/分类器和适配器，只有 VM transport 为可控替身。源码：[probe.cpp](</E:/source/repos/SLSF Reloaded NG/docs/evidence/audits/2026-10-31-group08/probe.cpp>)；完整输出：[results.log](</E:/source/repos/SLSF Reloaded NG/docs/evidence/audits/2026-10-31-group08/results.log>)。已用归档的 [replay.cmd](</E:/source/repos/SLSF Reloaded NG/docs/evidence/audits/2026-10-31-group08/replay.cmd>) 再次重放，退出 0，结果一致；其 MSVC 路径对应本机。没有执行目标游戏、实际 PEX 或真实 provider 取帧。

**最终计数：Standards 0 项硬性违规，未确认内存/栈/线程缺陷；Spec 3 项，最严重为本组实际事实采集器缺失。**
