# 01–03 独立审核与逐项对抗验证

日期：2026-10-28。

01/02 此前六项修复保持成立。03 保留 **3 项 P2**：到期命令的时间边界、类别衰减参数的热路径读取、固定图 CSR 的重复编译。自定义地区查找另列为优化建议。当前检查未发现已确认的内存安全缺陷。

## 审核范围与方法

- 基点：`5222a3c31841426bb26c3635a37638abb8f707e5`；工作区固定快照：`7a43e4e4b3a67b3ac5fa2f4d33990ac4e0101e70`。比较包含未跟踪的当前交付文件，限定 01–03 的共享类型、合同、构建、地点、声望状态、provider、codec、图、增减规则、传播内核和结算及其测试。
- 三个 `gpt-6-luna / xhigh` 代理分别审核 Standards/内存、Spec/交付主张、算法；另两个全新同模型、同推理档位代理分别反证规格/内存报告和算法报告。检查以项目规格、任务书及已选设计为准；实现注释和现有测试用于解释当前行为。
- 文件读取和搜索主要使用 FastCtx；探针与审核脚本使用 Git Bash。全套检查沿用项目现有 PowerShell 测试工具。探针、日志和逐项报告归档在 [review-01-03](review-01-03/README.md)。
- 本轮未修改生产源码、现有测试或构建配置，未暂存或提交。复查的生产、测试和构建文件仍与固定快照相同。

## Standards

**0 项已确认的规范违规。** 独立审核逐项检查了 12 类 Fowler 基线。重复的类别计数更新、整数类别参数、档位分支和结算文件职责被列为四项可讨论的设计观察；尚无证据将它们升级为需修复的问题。内存与资源检查结果见下表；没有为当前代码引入池或 arena 的要求。

## Spec

**3 项 P2，最高为到期命令的时间语义偏差。** 现有测试通过，但未覆盖“先结算至命令发生时刻，再应用该命令”的反例。A06 的预编译类别参数要求及绑定阶段建立 CSR 的设计也尚未完整落实。01/02 六项修复继续关闭。脉冲组合、跨档配置映射和公开无操作返回约定按既定组别交接。

| 编号 | 结论与触发条件 | 要求、实现与对抗结果 |
|---|---|---|
| F1 / P2 | 地区尚未结算到命令时刻时，到期 `Set` 被计入发生前的衰减区间 | [Settlement.cpp:484](../../src/core/reputation/Settlement.cpp:484) 先应用全部到期命令，再在 [:522](../../src/core/reputation/Settlement.cpp:522) 结算旧区间。单地区、空图、固定最小随机数、`decayPerDay=2`、初始值 0，`Set=10, at=0.5`：`Advance(0.5)` 得到 **9**，先结算至 0.5 再 Set 得到 **10**；延迟至 1 时为 **8 / 9**。[设计:65](../../.scratch/native-migration/design.md:65) 和 [A06:65](../superpowers/plans/2026-10-27-foundation-and-core-plan.md:65) 要求先处理旧时间并切分业务边界。头文件及测试确实约定了当前“命令先行”顺序，但未找到覆盖已选设计的批准决定；对抗审核将原报告 P1 降为 P2，保留具体偏差。对未消费区间内的业务边界，应先结算、按同刻稳定顺序应用命令，再推进至 now，统一提交。 |
| F2 / P2 | 衰减类别循环仍构造配置键并查 map | [Settlement.cpp:136](../../src/core/reputation/Settlement.cpp:136) 对合格类别调用倍率和衰减规则；[GainRules.cpp:36](../../src/core/reputation/GainRules.cpp:36) 在非全局倍率路径构造类别键，衰减规则另按次读档位配置。[A06:67](../superpowers/plans/2026-10-27-foundation-and-core-plan.md:67) 明确要求读取预编译类别参数。对抗审核确认这是性能验收缺口，未发现算术错误。B07 负责配置编译接入，A06 的数值循环应消费类型化参数。 |
| F3 / P2 | 固定图在每次 Advance，包括同刻无操作时重编译 | [Settlement.cpp:444](../../src/core/reputation/Settlement.cpp:444) 的 CSR 编译早于无操作判断；编译逐边解析稳定键、排序并分配 CSR。[优化方案:76](../../.scratch/native-migration/optimization-plan.md:76) 将派生 CSR 放在绑定/恢复阶段重建。对抗审核保留设计偏差：图及地区绑定版本不变时应复用编译结果。默认 21 地区/78 有向边的无操作约 **3.96 μs**；512/1536 压力输入约 **164.8 μs**，用于展示增长，不是业务规模或延迟门槛。 |

## 算法优化建议

**O1：按地区索引判断自定义地区。** [IsCustomAccount](../../src/core/reputation/Settlement.cpp:410) 每个地区线性扫描 `customRegions`，整批为 O(N×C)。512 地区、40 次推进，整体 Advance 从无自定义地区的 306.4 μs 增至全部自定义时的 648.8 μs。该比较还包含暂存状态时复制自定义记录的 O(C) 成本，不能将 2.12 倍全部归因于查找。对抗审核将原 P2 降为优化建议；绑定/恢复时生成按 RegionId 寻址的自定义地区标记，是可测量的改进方向。

## 内存、栈、池与 arena

| 检查面 | 实际实现与证据 | 结果 |
|---|---|---|
| 所有权与借用 | 状态、图和快照拥有 vector/string；不可变快照用 `shared_ptr<const ...>`；span/string_view 在同步构建或查询内消费，输出复制稳定身份 | 未发现悬空借用；销毁动态绑定输入后读取返回图的探针通过 |
| SIMD 边界与布局 | NumericRow 为 32 个 double、32 字节对齐、256 字节；AVX2 访问 0–27，随后清零 25–31 | 精确两行缓冲、末行作为来源的 ASan+UBSan 探针通过；该机器实际执行 AVX2 |
| 解析器 | Reader 在读取前检查剩余长度；长度和计数受 codec 限制 | 有效 v2 数据的全部截断前缀均被拒绝；现有解析器 fuzz 100000 次通过 |
| 栈 | 地点森林与 owner 链使用显式 vector 路径/栈，数值行缓冲放在容器中 | 未发现无界递归或大型可变栈分配；深链用例通过 |
| 池、arena 与释放 | 当前纯核心使用标准 RAII 容器，没有自建 pool、arena、pmr 或手工分配/释放路径 | 本范围没有对应 reset/release 生命周期需要审核 |
| 提交与失败 | Advance 对工作状态暂存后统一提交；借用输入不保留到调用后 | 未发现半提交造成的内存问题；输入队列排序的报告偏差另见 D1 |

项目 ASan 目标关闭了 STL vector/string 容器注解；独立 ASan+UBSan 探针没有定义这两个关闭宏。两类检查均记录在 [内存报告](review-01-03/standards-memory.md)，覆盖范围分别列明。

## 报告逐项对抗后的处置

| 原报告条目 | 最终处置 |
|---|---|
| 算法 A1：时间边界，原 P1 | 保留 F1，降为 P2；确定性反例排除了图、随机流和重绑差异，头文件/测试与需求的顺序冲突也应一并统一 |
| 算法 A2：每次编译 CSR | 保留 F3；重编译发生在无操作返回前，默认及扩展规模分别记录 |
| 算法 A3：自定义地区线性查找，原 P2 | 收窄为 O1；保留复杂度事实，撤回将整体倍数完全归因于扫描及强制增加缓存的推论 |
| Spec S1：脉冲未自行结算，原条件性 P1 | 作为 B02 交接约束。ApplySpreadPulse 是明确排除衰减的单跳原语；[B02](../superpowers/plans/2026-10-27-runtime-and-compat-plan.md:19) 负责结算、修改和发布的组合。公开 Spread/SpreadRoll 路径仍须先结算；当前尚无生产调用方证明组合顺序出错，不计入已确认核心缺陷 |
| Spec S2：逐类别动态解析配置 | 保留 F2，限定实际走 map 的配置路径；未声称算术错误 |
| Spec S3：来源 <10 返回 InvalidArgument | 核心状态确实无变化；未找到规范要求内部 Result 必须成功或当前适配器导致外部失败的证据。公开适配层应保持原无操作语义，不计入已确认玩法缺陷 |
| D1：失败时“状态与队列都不变” | 报告措辞应改为“状态未提交，失败不消费到期项”。[stable_sort](../../src/core/reputation/Settlement.cpp:455) 早于暂存，失败后队列顺序可能改变；未发现保留原队列顺序的业务要求 |
| D2：A05/A06 仍标待实现 | 进度表应记录已有实现及 F1–F3 等待处理项；现状既非完全未实现，也未满足全部验收 |
| 负值 Set 后 R 演进 | 同一时刻保留 -42；正时间后 R 向无入边目标 0 推进。属于时间演进，撤回“Set 立即钳位”的疑点 |
| maxFame=200、非有限 gain 时间 | 所选支持域未确认这些输入；保留原探针观察，不据此增加当前修复要求 |
| 地区重绑与旧 CSR | 按稳定身份迁移后重新编译得到正确新索引；持有旧编译结果跨绑定使用属于调用约束，不是已复现缺陷 |
| scalar/AVX2、填充槽与特殊数值 | 定向逐位比较通过，结合差分基线未发现反例 |
| 四项 Fowler 可能异味、内存无当前发现 | 保留为观察及本次检查结果，没有升级为规范违规或泛化为所有运行时安全结论 |

01/02 六项逐项复核：暂缺地区完整保存、原注册优先级、零类别注册往返、单项仅更新目标地区、RegionId 命令、provider CategoryMask 均保持关闭。证据及限定见 [规格/内存对抗报告](review-01-03/adversarial.md)。

## 验证与复放

本轮在固定代码上执行：

```bash
cd '/e/source/repos/SLSF Reloaded NG'
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/run-native-tests.ps1 -Mode all -Seed 1170 -Runs 100000 -Jobs 20
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/verify-contracts.ps1 -NegativeFixtures
python tools/verify-task-books.py
```

| 检查 | 结果 |
|---|---|
| 普通 / ASan 核心 | 各 130 cases / 119295 assertions，通过 |
| 普通 / ASan 边界 | 各 4 cases / 22 assertions，通过 |
| 差分 | 6 cases / 17148 assertions，通过 |
| 解析器 fuzz | 100000 次，seed 1170，通过 |
| 合同与合同反例 | 124 函数、52 输入、7 回复、25 类别、58 records、23 rules；11/11 反例被拒绝 |
| 任务书 | 11 组、25 任务、37 CPP 唯一归属，11 任务书、11 桌面提示及 100 链接通过 |
| 独立内存探针 | Clang 20 ASan+UBSan，实际执行 AVX2，退出 0 |
| 算法探针 | 有限时刻反例、重绑、特殊数值及成本探针已运行；退出 0 表示探针执行成功，F1 的差异保留为错误证据 |

核心 JUnit 有 146 个展开节点，对应 Catch2 的 130 个测试用例；展开节点名称唯一，failure/error 均为 0。ASan 负探针按预期报告人工制造的越界，未计为生产缺陷。全套检查约 21 秒。

原始报告：[Standards/内存](review-01-03/standards-memory.md)、[Spec](review-01-03/spec.md)、[算法](review-01-03/algorithms.md)。新代理逐项反证：[Spec/内存/01–02 修复](review-01-03/adversarial.md)、[算法](review-01-03/algorithm-counter.md)。[完整检查日志](review-01-03/existing-checks.log)、[JUnit 汇总](review-01-03/junit-summary.txt)与[探针复放说明](review-01-03/README.md)一并归档。原始报告的条件性判断和优先级以本报告的对抗后处置为准。
