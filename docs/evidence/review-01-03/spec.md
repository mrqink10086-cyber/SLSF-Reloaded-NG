# 01–03 独立规格审核

审查快照：`5222a3c31841426bb26c3635a37638abb8f707e5` → `7a43e4e4b3a67b3ac5fa2f4d33990ac4e0101e70`；聚焦 `include/slsf/core`、`src/core`、`tests/core`、`tests/boundary`、`xmake.lua`，并按任务规格和交付报告逐项核对。此报告只读审核；本轮未运行 xmake 或测试，未改生产源码、测试、项目规格及交付报告。根代理报告当前 `tools/run-native-tests.ps1 -Mode all` 通过，核心 130 cases。

## 摘要（少于 400 words）

01/02 的合同、地区状态、provider 汇总及 codec v2 交付，与当前源码和修复记录一致；未发现仍未关闭的 01/02 规格问题。03 的图、标量/AVX2 内核、常规 `Advance` 批次及主要结算规则有明确实现和测试证据，但“兼容 Spread/SpreadRoll 先结算实际经过时间”的完整行为没有由现有核心接口保证：`ApplySpreadPulse` 直接读未结算的 L/R；只有调用方先另行调用 `Advance` 才符合规格。另有一项确定的 A06 性能验收未满足：按类别配置时仍在逐格衰减循环内构造字符串并查 map。03 报告还把低于 10 的手动传播记为“拒绝”，而项目规格要求无操作；并声称失败时输入队列不变，实际排序会改写调用方队列。后两项分别是有条件的 API 行为差异和报告精度问题。`DecayTimeNeeded` 到 `decayPerDay` 的配置换算属于明确记录的配置接入交接，不列为 01–03 缺陷。规格表仍把已有实现的 A05/A06 标成待实现，属于状态文档过期。

## 发现

### S1 — Spread/SpreadRoll 脉冲入口没有保证先结算实际时间（条件性 P1）

- **要求**：`.scratch/native-migration/spec.md:48-49` 要求先处理真实经过时间，再从同一快照做一跳脉冲；A06 还要求由一个入口拥有已消费时刻并统一提交（`docs/superpowers/plans/2026-10-27-foundation-and-core-plan.md:61-69`）。
- **实现与调用路径**：`ApplySpreadPulse` 是目前唯一的核心脉冲入口（`include/slsf/core/reputation/Settlement.h:64-76`）。它在 `src/core/reputation/Settlement.cpp:344-352` 直接复制当前 L/R 并算入边，之后在 `:378-388` 更新目标 R、保护期限和 `remoteSettledAt`；没有调用 `DecayAccount` 或 `Advance`，且签名不接收 RNG/`AdvanceInputs`。因此合法调用 `Set(L=40, t=0)` 后直接在 `t=1` 脉冲时，来源的到期 L 未先衰减，目标的 `remoteSettledAt` 却被推进到 1；若之后来源撤回，结算会把目标视作从 1 起才经历 R 退去时间。
- **现有测试与反证**：`tests/core/SettlementTests.cpp:489-509` 直接 `Set` 后在 `t=1` 调用脉冲，且该用例设 `decayPerDay=0`，没有覆盖需要实际衰减的输入。另一个测试 `:545-569` 先 `Advance(t=1)` 再脉冲，证明人工组合可以工作；它没有证明脉冲入口或上层组合契约会强制此顺序。报告 `docs/evidence/task-groups/03-diffusion-and-settlement.md:137` 只说明内容变更后的同 tick 传播需显式 `Advance`，没有写明每次兼容脉冲都必须先结算。
- **影响与边界**：若 A07/事件调用方总是先按相同 `now` 成功完成所需结算，结果正确；目前没有核心封装或可检查的已结算前置条件。一个可达的精确例子是目标 `R=80, settledAt=0, remoteSettledAt=0`，来源 `L=40`、边系数 `0.5`、门槛 `≤20`，参数 `decayPerDay=0, relaxationPerDay=1`，并在 `t=1` 脉冲：按“先结算再脉冲”，旧 R 先退到 79，本批候选为 20，最终 `max(79,20)=79`；当前脉冲直接取 `max(80,20)=80`，并把目标时钟推进至 1，旧 R 多保留 1 点。建议后续接入时提供满足该顺序的编排入口（同时避免意外重复普通传播），或明确并验证调用前提，并加入此边界测试。此项针对 A05/A06 核心契约，不要求现在实现 A07 或游戏运行时。

### S2 — A06 的类别衰减参数仍在逐类别循环中动态解析（P2，性能验收未完成）

- **要求**：A06 明确要求共同时间系数移出类别循环并读取预编译类别参数（`docs/superpowers/plans/2026-10-27-foundation-and-core-plan.md:65-69`；优化计划 `.scratch/native-migration/optimization-plan.md:84-89`；03 任务书 `docs/superpowers/plans/2026-10-27-task-books/03-diffusion-and-settlement.md:18-26`）。
- **实现与调用路径**：`DecayAccount` 在 `src/core/reputation/Settlement.cpp:116-150` 遍历每个类别槽；每个有正值且未被保护的槽都会调用 `LegacyFameMultiplier` 和 `RuleDecayRoll`（`:136-140`）。当 `UseGlobalFameMultiplier` 关闭时，`src/core/reputation/GainRules.cpp:36-49` 每个槽构造 `std::string` 类别键并查 `std::map`；`RuleDecayRoll` 也在 `GainRules.cpp:154-186` 按次读取衰减档位配置。虽然 `periods` 已在循环外算好（`Settlement.cpp:111-115`），类别倍率/档位仍未从类型化数组预编译读取。`ConfigurationData` 已有 25 项倍率数组（`include/slsf/core/Configuration.h:35-42`），但结算仍使用 `Commands.h:30-37` 的 map 型 `Configuration`。
- **可达条件与后果**：旧式按类别倍率开关设为非全局、且多个类别有待衰减值时，循环会重复构造 key/查表；最多每账户每批 25 次。它不改变算术结果，但没有达到明确的 A06 热路径验收。03 报告的结算耗时记录（`docs/evidence/task-groups/03-diffusion-and-settlement.md:57-65`）没有证明此处已使用类型化参数。若类型化配置转换要留给 B07，应把本项记录为 A06 性能交付未完成的交接，而不能视为已满足该条验收。

### S3 — 手动传播阈值的可观察结果与规格不同（条件性 P2）

- **要求**：项目规格要求来源 `<10` 时“无操作”（`.scratch/native-migration/spec.md:46`）。
- **实现与路径**：`RuleManualSpreadFame` 对此返回 `InvalidArgument`（`src/core/reputation/GainRules.cpp:109-112`）；`ApplyManualSpread` 原样向上传播错误并不写目标（`src/core/reputation/Settlement.cpp:231-248`）。`tests/core/GainRulesTests.cpp:194-200` 与 `tests/core/SettlementTests.cpp:459-463` 将该错误结果固定为预期；03 报告也写成“source<10 拒绝”（`docs/evidence/task-groups/03-diffusion-and-settlement.md:28`）。
- **影响与边界**：状态数值确实不变，所以若调用适配层吞掉该错误，对玩家表现等价于无操作；直接调用核心 API 的调用方却会观察到失败，与“无操作”不同。当前没有下游映射证据。应在交接前决定核心是否改为成功的 no-op 结果，或在报告中明确适配层负责吞错；本审核不假设尚未完成的游戏层。

### R1 — 03 报告把失败事务描述得比实际保证更强（低影响报告不准确）

- 03 报告称失败时状态与队列都不变（`docs/evidence/task-groups/03-diffusion-and-settlement.md:26`），但 `Advance` 在创建 staging 副本前对调用方拥有的 `inputs.scheduledChanges` 做原地 `stable_sort`（`src/core/reputation/Settlement.cpp:449-483`）；之后任一到期命令失败会在 `:494-498` 返回，而不恢复原顺序。可达例：队列先放无效 `t=2` 命令，再放有效 `t=1.5` 命令，`Advance(t=2)` 会排序后失败；`State` 不变，但入参 vector 顺序已改变。现有回归测试只断言状态不变（`tests/core/SettlementTests.cpp:250-267`），没有断言失败后队列顺序。
- 这是对报告“队列不变”的直接反证。排序是预期的规范化，头文件仅承诺成功调用消费到期项、保留未来项（`include/slsf/core/Commands.h:67-69`）；未发现队列原序属于业务要求，因此不据此认定玩法缺陷。将报告限定为“状态未提交、失败不消费到期项”即可。

## 交付主张矩阵

| 报告主张 | 规格/实现核对 | 审核结论 |
|---|---|---|
| 01：A01 合同清单及负例、A02 共享类型与构建隔离（`docs/evidence/task-groups/01-contracts-and-build.md:58-114,118-161,264-313`） | 合同数据和规则数量列明，负例和独立 header/build 目标有按日期记录的命令/日志。报告 2026-10-28 后续修订继续更新测试批次。 | 未发现 01 范围内未关闭的规格缺口。首版 `-Mode all` 在 `:296` 明确因 differential/fuzz/bench 尚未接入而退出 2，是当时历史记录，不与后续分模式成功冲突。未在本审核重跑 xmake。 |
| 02：A03/A04/A08 的 RegionId 数据、owner 归属、provider 类别汇总、v1/v2 codec（`docs/evidence/task-groups/02-state-location-and-codec.md:89-101,103-127`） | 报告列明 unresolved region、注册优先级、零类别注册、目标单写、RegionId 命令、CategoryMask 等修复；补充报告提供反例、回归和文件路径（`docs/evidence/2026-10-28-01-02-supplement-review.md:27-60`）。v2 保存本档数据，v1 兼容；JSON/graphEdits 重读留给 B05/B07（02 报告 `:95-101`）。 | 未发现现有源码反驳这些交付主张。B05/B07 的配置重读、保存回调及游戏接入是明确交接项，不是 01/02 缺陷。 |
| 03：固定有向图、CSR、每目标多来源 max、门槛、冻结一跳、来源撤回及 AVX2（`docs/evidence/task-groups/03-diffusion-and-settlement.md:20-35,69-79`） | 实现和用例位于 `src/core/reputation/DiffusionGraph.cpp:62-260`、`DiffusionKernel.cpp:42-66,144-211`、`DiffusionKernelAvx2.cpp:20-51`；测试映射列出 25/20、来源下降、链式一跳和 scalar/AVX2 差分。 | 主要计算主张有源码与测试支撑；没有发现传播数值规则的反例。参数默认 `spreadRetention` 未使用等有意设计/留档事项，按报告 `:34,122-127` 的说明不另算缺陷。 |
| 03：普通 `Advance` 顺序、时间消费、保护窗口、gain/manual decay 和批次 RNG（`03...md:24-29,67-95`） | `Advance` 处理到期命令、逐账户衰减后冻结行并传播；对应实现 `Settlement.cpp:419-590`、`GainRules.cpp:70-186`，测试映射覆盖保护、回拨、同区间一次及差分输入。 | 常规批次主张有实现和测试。S1 是兼容脉冲组合缺口；S3 是手动 spread 边界返回值不同。 |
| 03：失败 staging 时“状态与队列都不变”（`03...md:26,102-108`） | staging 保证 State 不半提交；但队列原位排序早于 staging，失败不会恢复排序。 | **报告主张不准确**；见 R1。 |
| 03：测试/差分/ASan/fuzz/基准通过（`03...md:44-65,81-97`） | 03 报告保留 2026-10-28 的 119 cases / 119,204 assertions；02 后续报告记录 130 / 119,295（`02...md:135-142`）；根代理的当前 all-mode 检查同为 130 core cases。差分报告明确共享 RNG 流用于逐项对照（03 `:83`）。 | 数量是历史快照演进，不能把 119 与 130 当作互相矛盾；差分是确定性一致性检验，不应被描述为独立 RNG 的统计验证，报告本身已注明。 |
| 03：A05/A06 完成状态 | `.scratch/native-migration/spec.md:91-92` 仍标“待实现”；但 03 报告称已完成，当前相应源文件和测试均存在。 | 项目进度表过期，应由文档负责人另行同步；按本审核只写报告的限制未修改该表。 |

## 明确排除的交接项

- `Parameters.decayPerDay` 当前默认为 `2.0`，而保护期限另从 `DecayTimeNeeded` 读配置：`Settlement.cpp:87-115,161-167`。当用户改为例如 72 个半小时批次时，若配置编译层仍给 `decayPerDay=2.0`，衰减频率会与 36 小时保护期限不匹配。03 报告已把这两个来源列为待配置接入统一（`docs/evidence/task-groups/03-diffusion-and-settlement.md:34,120`）；配置 JSON/编译通知由 B07 负责（`.scratch/native-migration/spec.md:53,101`）。这是明确的 B07 配置映射交接，不作为当前 01–03 缺陷。
- 尚未接入 B02 同步事务/发布、B05 存档回调、B07 JSON、B06 公开查询及游戏实机加载不计入本审核发现；报告 01–03 已将其标为下组任务。
- 夜间 AND 偏差、自动传播替换旧随机公式、门槛从跨边后比较等由项目/传播设计明确批准，并在 03 报告偏差表中记录，不属于未申报漂移（`docs/evidence/task-groups/03-diffusion-and-settlement.md:122-129`）。
