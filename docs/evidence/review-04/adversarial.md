# 04 组独立对抗复核

## 范围与裁决

固定审核树 `2b3912e81d62b4a26dbe43b20b92e18f5ce522cf`，基线 `5222a3c31841426bb26c3635a37638abb8f707e5`，03 组树 `28b4160cbba5afc84ce869d3f990648510db74fa`。只读复核已有的 standards/memory、spec、algorithms 报告，交付证据、任务书、规格、实现、旧 Papyrus 和原测试；独立编译并运行两个只读 gameplay probe。未运行共享 xmake 或原项目测试，未改生产代码及原测试。

| 主张 | 对抗结果 | 严重性 / 边界 |
|---|---|---|
| HeavyBondage 下漏算 unhideable（algorithms.md A1） | **撤回。** 初审把 Papyrus 的 `ElseIf` 作用域读错，旧值也为 false；修正后的公开入口结果一致。 | 无发现 |
| Chest tattoo 的 AND Unknown 过度暂停（A2） | **保留事实，降为 P3 readiness 精度观察。** 两个可行分支都 Ready:true，但未知安装状态目前只造成 NotReady；游戏适配器是否会产生该事实组合尚未验证。 | 核心 DTO/readiness；没有已接线的玩家影响 |
| DD 未安装时重置旧可见输出 | **保留为 P3 兼容决策缺口。** 只有此前输出为 true、随后 DD 变为 KnownFalse 时才出现状态差；初值为 false 时相同。 | 核心结果状态语义；运行时调用方未接线 |
| RefreshTargets 返回不同 Player | **仅条件接口建议。** 源码路径确实少了请求身份比较，但尚无明确 backend 身份契约或真实 backend 反例。 | 引擎 seam；非已证游戏缺陷 |
| Standards、内存、线程安全 | 未发现新的硬违规、安全错误或竞争证据；应保留现有边界限定。`sizeof` 不能当作最大栈帧/调用链峰值。 | 仅涵盖 04 源码及声明的调用契约 |
| B04 目标运行时验证 | **部分交付裁定正确。** 静态 ABI/DTO probe 不是 1.6.1170 实机查询。 | 运行时 adapter/backend 待接入 |

## 算法报告逐项反证

### A1：HeavyBondage 反例不成立，撤回

最终算法报告已将这项候选撤回（`E:/source/repos/SLSF Reloaded NG/build/diagnostics/review-04/algorithms.md:9-13`）。初稿曾将旧脚本的 hood 扫描解释为 Heavy 分支后的共同逻辑；这不符合完整控制流：旧脚本在 `E:/source/repos/SLSF Reloaded/Scripts/Source/SLSF_Reloaded_VisibilityManager.psc:799-809` 先把 `IsWearingUnhideable` 置 false，再进入 `If Heavy ... ElseIf Lockable`；扫描在 `E:/source/repos/SLSF Reloaded/Scripts/Source/SLSF_Reloaded_VisibilityManager.psc:871-875`，仍处于 Lockable 的 `ElseIf` 内；外层 `Else`/`EndIf` 在 `E:/source/repos/SLSF Reloaded/Scripts/Source/SLSF_Reloaded_VisibilityManager.psc:885-889`。Heavy 为 true 时 Lockable 扫描被跳过，重置后的值保持 false。`IsVisiblyBound` 则由 Heavy 分支设 true，匿名门控见 `E:/source/repos/SLSF Reloaded/Scripts/Source/SLSF_Reloaded_VisibilityManager.psc:891-893`。

因此初审 probe 的 `legacy_expected...=1` 是错误 oracle：其源码把期望值手写为 `constexpr true`（现更正为 `false`，见 `E:/source/repos/SLSF Reloaded NG/build/diagnostics/review-04/adversarial-heavy-bondage.cpp:24-35`）。更正后用例 `Ready, DD=true, Heavy=true, Lockable=false, Hood=true, Anonymous=false` 的原生输出 `wearingUnhideable=0, visiblyBound=1` 与 Papyrus 一致；重放记录在 `E:/source/repos/SLSF Reloaded NG/build/diagnostics/review-04/adversarial-heavy-bondage.log:1-4`。C++ 分支 `E:/source/repos/SLSF Reloaded NG/src/core/gameplay/Visibility.cpp:516-533` 及对应测试 `E:/source/repos/SLSF Reloaded NG/tests/core/GameplayRulesTests.cpp:678-700` 在这个情形没有发现偏差。**撤回 A1，不应把它计为 P2 或实现缺陷。**

### A2：事实保留，严重度下调

最终算法报告将 A2 定为低优先级 readiness 精度观察（`E:/source/repos/SLSF Reloaded NG/build/diagnostics/review-04/algorithms.md:15-23`）。行为观察成立。`E:/source/repos/SLSF Reloaded NG/src/core/gameplay/Visibility.cpp:223-227` 在分析子类前，只要 SLS 或 AND 安装状态为 Unknown 就返回 NotReady；但 Chest 的 A 分支在 `E:/source/repos/SLSF Reloaded NG/src/core/gameplay/Visibility.cpp:238-250` 由 Chest/Bra rank 判定，SLS=true 且 AND=false 的 C 分支在 `E:/source/repos/SLSF Reloaded NG/src/core/gameplay/Visibility.cpp:334-342` 仅依赖身体、背部装备。旧 Chest 分支为 `E:/source/repos/SLSF Reloaded/Scripts/Source/SLSF_Reloaded_VisibilityManager.psc:411-425,472-480`；SLS=true/AND=false 分支为 `E:/source/repos/SLSF Reloaded/Scripts/Source/SLSF_Reloaded_VisibilityManager.psc:508-513`。

独立 probe 构造已安装且已应用的 Chest 纹身、SLS=true、Chest rank=true、身体/上下背槽均 absent，分别运行 AND=true/false/Unknown。结果前两者均 Ready:true，Unknown 为 NotReady:false：`E:/source/repos/SLSF Reloaded NG/build/diagnostics/review-04/adversarial-body-tattoo-unknown.log:1-3`。这是公开 DTO 可表达的组合：安装状态与各 rank 分别是 `Fact` 字段（`E:/source/repos/SLSF Reloaded NG/include/slsf/core/gameplay/Visibility.h:89-94`）；既有测试也允许 AND=false 时 rank 保持 Known(true)，并明确该 rank 只有安装后才参与判断（`E:/source/repos/SLSF Reloaded NG/tests/core/GameplayRulesTests.cpp:626-644`）。未发现要求 Unknown installation 必须清空 ranks 的 DTO 不变量。但没有真实适配器证明该组合会在游戏中出现。

优化计划规定“部分未知只暂停依赖它的候选”（`E:/source/repos/SLSF Reloaded NG/.scratch/native-migration/optimization-plan.md:95-99`），任务书也要求部分未知只暂停依赖字段的新计分（`E:/source/repos/SLSF Reloaded NG/docs/superpowers/plans/2026-10-27-task-books/04-gameplay-and-engine-facts.md:25-30`）。故保留为合同精度问题；由于本例不会产生错误的 Ready 布尔值，仅损失确定结果的可用性，且没有接线消费者，**从初审 P2 下调为 P3 readiness 精度观察**。不把它扩张成所有三态分支都必须推理合并的通用要求。

### 其他算法、分配与成本

最终算法报告的其他审核面在 `E:/source/repos/SLSF Reloaded NG/build/diagnostics/review-04/algorithms.md:47-58`：对匿名、其余可见性、目击/名气、动画和评论未发现额外反例；本轮未找到新的可重放差异，但这是有限范围复核，不等价于对所有 DTO 组合的穷举。特别是 DD 未安装分支应按下节单列，不能由“其他束缚路径未发现差异”覆盖。

性能报告 `E:/source/repos/SLSF Reloaded NG/build/diagnostics/review-04/algorithms.md:60-66` 的数字应按单机单次微基准理解：10,000 次全 25 类 eligible 的 `FameGainRoller::Run` 产生 180,000 次全局 `new/new[]` 计数（18 次/调用），耗时 5,557 μs；25M 次 mask 测试耗时 21,746,000 ns。报告已说明这不是插件整体/帧预算结论，也未给出阈值；**保留为观察，不构成性能 finding**。

## 与规格和交付报告的对照

### DD 未安装分支：状态兼容缺口，不是普遍数值错

总规格规定除选定传播重写和明确旧错误修复外，玩法以旧脚本为基线（`E:/source/repos/SLSF Reloaded NG/.scratch/native-migration/spec.md:17`）。旧脚本 `CheckBondage` 的 DD=false 分支只将三个 bound Global 清零后返回（`E:/source/repos/SLSF Reloaded/Scripts/Source/SLSF_Reloaded_VisibilityManager.psc:776-790`），因此保留此前的 `DD_*Visible` 和 `IsWearingUnhideable` 值。新规则 `E:/source/repos/SLSF Reloaded NG/src/core/gameplay/Visibility.cpp:489-500` 则为可见字段和 unhideable 返回 false。交付报告承认该差异并称为“刻意近似”（`E:/source/repos/SLSF Reloaded NG/docs/evidence/task-groups/04-gameplay-and-engine-facts.md:205-212`），但未引用规格批准。

触发必须是**旧值先为 true，之后 DD 状态变为 KnownFalse**；DD 从未安装且初值为 false 时，两者一致。新 `VisibilityFacts` 没有 previous outputs 输入（`E:/source/repos/SLSF Reloaded NG/include/slsf/core/gameplay/Visibility.h:170-181`），纯规则无法自行表达“保持旧 Global 不写”。因此确认的是旧状态保留语义未映射到当前 API、且报告尚未给出批准依据；不能说每次 DD=false 都有数值错误，也没有证据证明游戏中发生该转变。**P3 兼容决策缺口**：由项目规格 owner 决定保留状态语义或明确批准重置，确定后同步 DTO/调用约定与证据报告。

### RefreshTargets Player 身份检查：保留条件建议

`IEngineBackend::QueryActor(actor, player)` 的接口位于 `E:/source/repos/SLSF Reloaded NG/include/slsf/engine/EngineFacts.h:103-112`。直接 `ActorQueriesService::Query` 比对返回 actor 和 player（`E:/source/repos/SLSF Reloaded NG/src/engine/ActorQueries.cpp:19-29,94-105`），而 `GameDataService::RefreshTargetsImpl` 在 `E:/source/repos/SLSF Reloaded NG/src/engine/GameData.cpp:222-240` 规范化两者后仅比较 actor；`ValidateSnapshot` 只检查 player key 格式（`E:/source/repos/SLSF Reloaded NG/src/engine/GameData.cpp:37-43`）。所以该入口从源码上能接纳“有效但不同 player”的返回快照。

这仍**不是已证 backend 或游戏行为缺陷**：现有接口没有明确写明 backend 必须按原请求 player 生成返回身份；用假 backend 注入不匹配只会证明校验差异，不会证明玩法触发。四项历史修复里“玩家未校验”的关闭记录应限定为 Query 路径（交付报告 `E:/source/repos/SLSF Reloaded NG/docs/evidence/task-groups/04-gameplay-and-engine-facts.md:154-165`）。若 backend 契约意图覆盖所有入口，则 `RefreshTargetsImpl` 是一致性缺口，应加请求 player 校验和负例；在契约补齐前保留**条件接口建议，不分配缺陷严重度**。

### A07/B04 矩阵及目标运行时

- A07 五个纯规则模块、DTO、接口与测试确已交付（`E:/source/repos/SLSF Reloaded NG/docs/evidence/task-groups/04-gameplay-and-engine-facts.md:9-30,32-80`）。算法核对没有支持“整体算法失败”的证据；A2 是低优先级 readiness 精度观察，DD 分支是需明确决策的旧状态兼容边界。A1 已撤回。不能把后续 06 组未接线当作当前数值错误。
- B04 的 `IEngineBackend`、`GameDataService`、`ActorQueriesService` 和 DTO seam 存在，但 CommonLib 查询 backend 未实现/未接插件，故 B04 **部分交付**裁决正确（`E:/source/repos/SLSF Reloaded NG/docs/evidence/task-groups/04-gameplay-and-engine-facts.md:82-92,209-230`）。任务书明确要求在目标运行时探测关系、感知、LOS、距离及 unknown（`E:/source/repos/SLSF Reloaded NG/docs/superpowers/plans/2026-10-27-runtime-and-compat-plan.md:37-43`）。`STATIC_PASS` 只说明 ABI/签名/槽位和合成 DTO 检查；日志仍写 `probe_runtime_game_queries=pending:no-running-game-backend`（`E:/source/repos/SLSF Reloaded NG/build/diagnostics/review-04/target-probe.log:1-13`）。关系 `0..8 → -4..4` 映射仍是已披露假设，不是已验证事实。
- 当前报告的既有复测日志支持 178 core cases / 120451 assertions、boundary 4 cases / 22 assertions、ASan 同批 core/boundary、100000 次 seed 1170 fuzz、probe/plugin 构建退出 0（`E:/source/repos/SLSF Reloaded NG/build/diagnostics/review-04/existing-checks.log:1-3,84-117,198-219,243-248,301-319`）。它们不排除未覆盖的规则边界；ASan 的 heap-buffer-overflow 在 `E:/source/repos/SLSF Reloaded NG/build/diagnostics/review-04/existing-checks.log:107-116` 是故意的负向探针，不能当生产缺陷，也不能证明未覆盖代码安全。

## 四组历史审计修复声明

交付报告 `E:/source/repos/SLSF Reloaded NG/docs/evidence/task-groups/04-gameplay-and-engine-facts.md:148-168,172-199` 汇总四个只读审计及修复；本轮只按当前源码和回归断言核查这些结果，**没有四份原始 transcript，不能独立证明先前审计过程或每条历史发现确曾发生**。

- `5c53da01` 的若干修复有当前回归支撑：Back 上槽缺失仍检查下槽在 `E:/source/repos/SLSF Reloaded NG/tests/core/GameplayRulesTests.cpp:418-440`；Stale/Unknown tattoo readiness 在 `E:/source/repos/SLSF Reloaded NG/tests/core/GameplayRulesTests.cpp:547-560`；AND rank 由 installed 门控在 `E:/source/repos/SLSF Reloaded NG/tests/core/GameplayRulesTests.cpp:626-644`。本次 A1 源码反证也说明 test 名称“full legacy reset”并非独立 oracle；应以旧脚本实际 `If/ElseIf` 作用域为准。
- `942fe46f` 的 scanner skip/暂停恢复、存活候选抽骰、fresh run 与成功标志由 `E:/source/repos/SLSF Reloaded NG/tests/core/GameplayRulesTests.cpp:761-859` 覆盖；候选 RNG 顺序及 50 次尝试见 `E:/source/repos/SLSF Reloaded NG/tests/core/GameplayRulesTests.cpp:946-970,1102-1122`。当前没有找到反例。
- `b179468a` 的数值、dirty、FormID/Key、线程上下文声明见 `E:/source/repos/SLSF Reloaded NG/docs/evidence/task-groups/04-gameplay-and-engine-facts.md:154-166,184-198`；ActorQueries 的 player mismatch 回归在 `E:/source/repos/SLSF Reloaded NG/tests/core/EngineFactsTests.cpp:546-560`。该测试不覆盖独立 `RefreshTargetsImpl` player 配对，范围见前述条件建议。
- `2a6d2138` 是报告内容/计数/探针修订的汇总；目前仅有汇总报告，不能恢复原 transcript。报告自己记录 F-06 未复现（`E:/source/repos/SLSF Reloaded NG/docs/evidence/task-groups/04-gameplay-and-engine-facts.md:168`），应继续理解为“明确未复现”，不转写成已证缺陷修复。

新观察并未反证上述已列明的具体回归修复；不过历史审计的“无 P0/P1/P2”只是原始审计结论摘要，不代表对 DTO 组合和运行时集成的穷尽证明。A1 是算法初审自身的 oracle 错误，而非后来实现回归。

## Standards、内存、栈与线程安全结论边界

初审 `E:/source/repos/SLSF Reloaded NG/build/diagnostics/review-04/standards-memory.md:5-24` 未发现第一方硬标准违规；branch A/B tattoo switch 的重复是可选 Fowler smell，等式有意不同，不是缺陷。当前复核没有相反证据。

内存/线程报告 `E:/source/repos/SLSF Reloaded NG/build/diagnostics/review-04/standards-memory.md:26-42` 的核心结论可以保留为“已审源码范围未见问题”：DTO 拥有 strings/vectors，服务借用 backend/RNG，`Find`/`Plugins` 指针及引用有失效边界，cache refresh 先复制 dirty/target keys；`GameDataService` 文档要求由主线程或统一执行器单 owner 调用（`E:/source/repos/SLSF Reloaded NG/include/slsf/engine/GameData.h:22-23`）。这并未验证生产构造销毁顺序、真实 backend 同步回调/重入、线程调用或对象借用寿命。

`ActorSnapshot=2144` bytes 等只是 `sizeof` 固定对象尺寸（初审报告 `E:/source/repos/SLSF Reloaded NG/build/diagnostics/review-04/standards-memory.md:31-32`），不表示最大栈帧、调用链峰值或线程栈余量；本轮没有汇编 stack-frame 或运行时高水位测量。无递归、大型可变栈对象、raw owning pointer/pool 的源码观察，加上现有 ASan 覆盖，只支持“当前检查范围内未见栈/内存错误”，不能扩大为实际 adapter 生命周期和并发安全已验证。`NpcScanner`、规则对象和 RNG 都要求调用方遵守串行所有权；报告已将其写成契约而非锁保护保证。

## 最终 finding 清单

1. **P3 — A2：Body Chest tattoo 安装 Unknown 时过度暂停。** 公共 DTO 反例可重放；保留 readiness 精度观察，不声称已观察到玩家结果。
2. **P3 — DD false 状态保留语义未决。** 只有历史输出 true 转 DD KnownFalse 时有差；交付报告称“刻意近似”但未找到规格批准记录。需作项目决策，不把它表述成所有 DD 未安装场景都错误。
3. **条件建议 — GameData refresh 返回 player 身份。** `RefreshTargetsImpl` 未检查请求 player；除非 backend 返回身份契约确认必须相等，否则不计缺陷。

未发现 A1 重度束缚偏差、其他可复现玩法回归、记录级内存/线程问题或标准硬违规。B04 仍是已披露的实机验证待办。
