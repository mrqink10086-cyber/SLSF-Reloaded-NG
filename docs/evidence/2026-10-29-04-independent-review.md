# 04 组独立审核与逐项对抗验证

日期：2026-10-29。范围：A07 玩法规则、B04 引擎事实查询，以及对应头文件、测试、probe 和构建配置。

结论：B04 目标游戏验证尚未完成，列为 P2 验收缺口。另有两项 P3：DD 未安装时的旧状态保留约定待明确，以及未知事实处理的精度观察。Standards 另记录一项可选的重复逻辑整理建议。内存、堆栈和线程审查在当前代码范围内未发现已证实的安全缺陷。

## 审核基线与方法

- 初始化基线：`5222a3c31841426bb26c3635a37638abb8f707e5`。
- 03 组封存树：`28b4160cbba5afc84ce869d3f990648510db74fa`，用于区分本组增量。
- 本次固定树：`2b3912e81d62b4a26dbe43b20b92e18f5ce522cf`。相对 03 的源码、测试及构建增量为 21 个文件。
- 三名 `gpt-6-luna / xhigh` 子代理分别独立审核 Standards/内存/堆栈/线程、Spec/交付记录、算法；第四名 fresh 子代理逐项反证，根代理核对原脚本完整控制流并复放归档探针。
- 固定树和差分使用独立 Git index；真实 index、HEAD、生产源码、原测试及现有任务文档保持原状。本次产物为审核报告、探针和日志。

规范来源为[项目规格](../../.scratch/native-migration/spec.md)、[设计](../../.scratch/native-migration/design.md)、[优化方案](../../.scratch/native-migration/optimization-plan.md)、[04 任务书](../superpowers/plans/2026-10-27-task-books/04-gameplay-and-engine-facts.md)及其中引用的 A07/B04 计划。独立报告与复放材料见[归档说明](review-04/README.md)。

## Standards

**文档规范违规：0。可选 Fowler 异味：1。**

引擎事实通过拥有自身字符串和容器的 DTO 进入核心，缓存读取处复制结果，服务遵循调用方串行化约定。十二项 Fowler 基线的逐项判断见 [Standards 与安全报告](review-04/standards-memory.md)。

唯一可选整理项是 [Visibility.cpp](<E:/source/repos/SLSF Reloaded NG/src/core/gameplay/Visibility.cpp:238>) 中 A/B 分支的 `TattooSubcategory` 判断有重复；`None`、`Back`、`Thigh` 等分支公式存在业务差异。它不构成已证实的规则错误，也不要求为本次交付新增抽象。

## Spec

### F1 · P2：B04 目标运行时验证仍为部分交付

[B04 计划](../superpowers/plans/2026-10-27-runtime-and-compat-plan.md:41)和 [04 任务书](../superpowers/plans/2026-10-27-task-books/04-gameplay-and-engine-facts.md:23)要求在 Skyrim 1.6.1170 验证实际关系等级、感知、LOS、距离及逐 NPC 可见性，并记录查询上下文和 Unknown 条件。

当前 [TargetProbe.cpp](<E:/source/repos/SLSF Reloaded NG/tests/probe/TargetProbe.cpp:92>) 使用合成 `ActorSnapshot`，核对 ABI、函数签名、上下文布尔约定和 DTO 翻译；[探针结尾](<E:/source/repos/SLSF Reloaded NG/tests/probe/TargetProbe.cpp:131>) 明确输出 `probe_runtime_game_queries=pending:no-running-game-backend`、`probe_result=STATIC_PASS`。根代理重建并执行所得结果相同，见[本次探针日志](review-04/target-probe.log)。

因此 B04 的接口与假后端测试已经交付，实机语义验收仍为 partial。交付报告已如实披露 pending。后续 06 组负责插件装配，不改变本组原有实机验收要求。

处理要求：在合法主线程和已加载游戏上下文中运行真实后端，逐项记录上述查询和 Unknown 结果；取得这份证据后再关闭 B04 的实机验收。

### F2 · P3：DD 未安装分支改变旧可见状态的保留语义

总规格以旧脚本玩法为基线，见[项目规格](../../.scratch/native-migration/spec.md:17)。旧 [CheckBondage](<E:/source/repos/SLSF Reloaded/Scripts/Source/SLSF_Reloaded_VisibilityManager.psc:776>) 在 DD 未安装时只清三个 bound Global，然后返回，保留原来的 collar/bra/belt/harness 可见值及 `IsWearingUnhideable`。

新 [EvaluateBondage](<E:/source/repos/SLSF Reloaded NG/src/core/gameplay/Visibility.cpp:489>) 把这五项结果直接输出为 `Ready:false`。[04 交付记录](task-groups/04-gameplay-and-engine-facts.md:207)称这是刻意保留的近似，当前规格和任务书未记录这项玩法变更。

差异的触发前提是原标志已有 true 值，而本轮 DD 为 KnownFalse；DD 从未安装且原值为 false 时，两种行为可以相同。当前确认的是状态保留约定和纯规则输出的差异；生产输出端尚未接线，本次没有声称已在游戏中观察到结果变化。因此按有条件的兼容边界列 P3。

处理要求：明确该分支各字段的“保留/更新”结果，并由输出端按原状态语义应用；若产品选择统一清零，应把该行为作为规格变更记录。

## 算法对抗结果

### A1 · 已撤回：重度束缚漏算不可隐藏装备

初审和第一版手写参考逻辑把旧脚本 [871 行](<E:/source/repos/SLSF Reloaded/Scripts/Source/SLSF_Reloaded_VisibilityManager.psc:871>)的扫描误认为共同路径。根代理检查完整控制流后发现，它位于 [809 行](<E:/source/repos/SLSF Reloaded/Scripts/Source/SLSF_Reloaded_VisibilityManager.psc:809>)的 `ElseIf lockable` 内，外层分支在 [889 行](<E:/source/repos/SLSF Reloaded/Scripts/Source/SLSF_Reloaded_VisibilityManager.psc:889>)结束。

`heavy=true` 会跳过该扫描，保留 803 行初始化的 false，与 C++ 一致。两名代理均已修正参考逻辑和探针，A1 不计入缺陷。见[算法报告](review-04/algorithms.md)、[fresh 对抗报告](review-04/adversarial.md)及[修正后的独立 control 日志](review-04/adversarial-heavy-bondage.log)。

### O1 · P3 观察：未知 AND 状态导致更保守的 readiness

公开 DTO 输入为 Chest tattoo 已应用、Chest rank 已知为 1、SLS 已安装、相关装备槽已知为空时，实际调用 `BodyTattooVisible` 得到：

| AND 安装事实 | 返回 |
| --- | --- |
| KnownTrue | `Ready:true` |
| KnownFalse | `Ready:true` |
| Unknown | `NotReady:false` |

[分支入口](<E:/source/repos/SLSF Reloaded NG/src/core/gameplay/Visibility.cpp:223>)在分支分析前暂停。输入使用独立 Fact 字段，两个代理都实际执行了三个输入，见[独立日志](review-04/adversarial-body-tattoo-unknown.log)。

这是可确认的结果可用性精度损失。逐项反证后将初始 P2 降为 P3 观察：现有要求强调按事实依赖暂停，但没有明确要求对所有未知分支求值并合并。后续可单独优化这个有明确输入的场景；本审核不增加通用符号推理框架要求，也未将它计为已观察到的玩家计分错误。

### 其他算法与性能

匿名时间规则、NPC 扫描顺序、暂停续跑、每次至多一次成功、候选顺序、去重和 50 次尝试、动画顺序及评论门控未找到新的已证实语义错误。检查路径和旧脚本依据见[算法报告](review-04/algorithms.md)。

单次 Clang `-O2` 探针记录 `FameGainRoller` 在 25 类全部 eligible 的 10,000 次调用中分配 180,000 次，包含返回容器及动态存储；候选枚举上限 25、选择尝试上限 50。它是局部成本记录，不构成性能缺陷，也不构成引入池或 arena 的依据。完整输入、计数方法和耗时见[性能日志](review-04/algorithm-performance-probe.log)。早期被编译器折叠为零耗时的常量查找循环已作废，最终探针使用运行时参数和 noinline 调用。

## 内存、堆栈与线程

| 审核面 | 本次证据与结论 |
| --- | --- |
| 堆与所有权 | DTO 拥有字符串和容器；服务借用 backend/RNG 引用；当前调用点未找到悬空使用。核心和引擎均纳入本次 ASan 构建。 |
| 缓存借用 | `Find` 的返回值受后续修改约束；`QueryCached` 返回复制值。dirty/target 集合先复制再执行刷新，当前循环的容器变更未破坏自身迭代。 |
| 池与 arena | 本组没有自定义 pool/arena、raw owning allocation 或手动释放路径。动态数据使用 STL 容器。 |
| 堆栈 | 未发现递归或大型可变栈对象。布局探针测得 `ActorSnapshot=2144 B`；这是固定对象大小，不能当作栈帧或调用链峰值。 |
| 线程与重入 | 服务明确由主线程或所属执行器串行调用，内部无互斥锁；本组没有可证实的并发调用或 race 路径。未运行 TSan；真实后端回调、重入和构造/销毁顺序随 06 组接线验证。 |

完整依据见[安全报告](review-04/standards-memory.md)和[fresh 裁决](review-04/adversarial.md)。现有 ASan 配置关闭了 vector/string 容器注解，能检测本次执行中的常规地址错误，但覆盖范围不等同于全部容器逻辑边界。

## 逐项反证与交付核对

| 初审或交付主张 | 最终裁决 |
| --- | --- |
| Standards 无已确认规范违规 | 保留；A/B 重复判断仅为可选 Fowler 异味。 |
| 没有已证实的内存、堆栈、线程缺陷 | 在上述代码和执行范围内保留；对象大小、串行约定和 ASan 分别提供不同证据。 |
| B04 静态 probe 已通过 | 保留 STATIC_PASS；实机验收仍 partial，列 F1。 |
| DD 未装分支的清零属于可接受近似 | 保留已记录的行为差异；实现记录不足以代替规格变更，列 F2。 |
| heavy+hood 旧结果应为 true | 撤回；参考逻辑误读 ElseIf 范围，修正后与 C++ 相同。 |
| Unknown AND 导致不必要暂停，应列 P2 | 三组输出保留，降为 O1；不增加通用分支合流要求。 |
| RefreshTargets 可能接受不同 player 的有效快照 | 条件一致性建议；没有真实 backend 或明确返回键合同证明现有触发，不列玩法缺陷。 |
| 既有四次审计的关闭声明 | 核对当前报告所列修正和回归；原始四份 transcript 未交付，不据汇总声明声称复核了原审计过程。 |
| 04 交付日志数字 | 原报告日志与本次复跑分开记录；fuzz corpus 随运行演化，cov/corpus 差异不等于测试矛盾。 |

## 本次执行结果

| 检查 | 结果 |
| --- | --- |
| 普通核心 / ASan 核心 | 各 178 cases、120451 assertions，通过 |
| 普通边界 / ASan 边界 | 各 4 cases、22 assertions，通过 |
| 差分 | 8 cases、17161 assertions，通过 |
| Codec fuzz | 100000 runs，seed 1170，通过 |
| ASan 正负探针 | 正例成功；负例产生预期 heap-buffer-overflow |
| EngineProbe 与插件构建 | 通过；probe 执行结果 STATIC_PASS |
| 归档源码复放 | 五份独立探针重新编译、运行通过；语义输出与最终裁决一致 |
| 合同负例 | 11/11 拒绝 |
| 任务书 | 11 组、25 任务、37 CPP 唯一归属、11 桌面提示、101 本地链接，通过 |
| 审核材料校验 | 6 份 Markdown、57 个本地链接、13 个行号锚点及围栏通过；30 件归档产物与最终原件一致 |

JUnit 的核心 XML 有 268 个展开节点，对应 Catch2 的 178 个 test cases；普通和 ASan 的 120451 个 assertions 相同，均无重复节点、failure 或 error。日志见[全量执行](review-04/existing-checks.log)、[JUnit 汇总](review-04/junit-summary.txt)和[归档复放说明](review-04/README.md)。

Standards：0 项规范违规、1 项可选异味；Spec：1 项 P2 验收缺口、1 项 P3 状态兼容边界；算法另有 1 项 P3 readiness 观察。
