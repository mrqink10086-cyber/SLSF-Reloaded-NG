# 03 组证据：AVX2 传播、增减公式与时间结算

任务 A05/A06。实现与验证日期：2026-10-28。上游任务书：[03-diffusion-and-settlement.md](../../superpowers/plans/2026-10-27-task-books/03-diffusion-and-settlement.md)。

**当前交付**：已在现有逻辑接入事件时间分段、准备结果/CSR 复用、owner 图投影、类型化规则、自定义地区标记、工作缓冲及 AVX2 松弛。普通/ASan 核心各 147 用例、119,837 断言通过；随机差分 8 用例、17,161 断言，fuzz 100,000 次通过。完整实现、测量和独立复核见[03 整链实施报告](../2026-10-28-03-chain-implementation.md)。

以下 1–10 节保留首轮实现与 119 用例阶段的记录；当前接口和交接以第 11 节为准。

## 1. 交付范围

| 模块 | 文件 |
| --- | --- |
| M11 传播图 | `src/core/reputation/DiffusionGraph.cpp`、`include/slsf/core/reputation/DiffusionGraph.h` |
| M14 增减规则 | `src/core/reputation/GainRules.cpp`、`include/slsf/core/reputation/GainRules.h` |
| M15 传播内核 | `src/core/reputation/DiffusionKernel.cpp`、`DiffusionKernelAvx2.cpp`、`DiffusionKernelInternal.h`、`DiffusionKernel.h` |
| M16 时间结算 | `src/core/reputation/Settlement.cpp`、`Settlement.h` |
| 接口同步 | `include/slsf/core/Types.h`（`DiffusionBackend`）、`include/slsf/core/Commands.h`（`AdvanceInputs::backend`、一次性命令队列、`Advance(AdvanceInputs&)`）、`src/core/reputation/ReputationState.cpp`（`ClearAll` 同步清保护） |
| 构建 | `xmake.lua`（`DiffusionKernelAvx2.cpp` 单独启用 `/arch:AVX2`） |
| 参考与测试 | `tests/reference/DiffusionReference.{h,cpp}`、`tests/core/{DiffusionGraph,DiffusionKernel,GainRules,Settlement,Random}Tests.cpp`、`tests/core/ReputationStateTests.cpp`（保护清零断言） |
| 差分/基准 | `tests/core/RandomTests.cpp`（`[differential]` 用例）、`tests/bench/SLSFBench.cpp`、`tests/bench/xmake.lua`、`tools/run-native-tests.ps1` |

AVX2 内核拆到独立 TU 的原因：MSVC 的 `/arch:AVX2` 对整份 TU 生效，若与标量回退同 TU，编译器可对回退路径做 AVX2 自动向量化，在没有 AVX2 的机器上启动即崩。拆分后基线 TU 不含 AVX2 指令，`KernelSupportsAvx2()` 运行时探测（CPUID leaf 7 + XGETBV）决定是否进入 AVX2 TU。

## 2. 已实现行为

- **传播**：每目标对全部入边取 `coefficient × max(L,R)[src]` 的最大值，再与 `cutoff` 比较（不是逐边过门槛）；小于阈值整槽为 0。保留来源快照，单批只推进一跳，不级联。
- **接替**：每批重算入边，`RelaxRemoteRows` 以剩余入边为下界；来源消失后按剩余入边回落。固定用例 25/20→25，来源撤销后 20。
- **批次顺序**：到期 `scheduledChanges`（时间升序稳定排序）→ 本地衰减（含时钟回拨重锚）→ 冻结快照传播 → R 松弛 → `RecomputeAccount`/`RepublishAccount`。`Advance` 不调用 `MakePublishedState`。
- **同一区间一次**：`settledAt` 只按整周期推进，且 `min(now, old + periods/perDay)`；空闲重复调用早退。纯回拨只重锚并早退，不重放传播。
- **命令队列**：`AdvanceInputs&` 为一次性队列，到期命令成功后从 vector 头部删除；同一入参重复调用不会二次应用。失败走 staging 副本，状态与队列均不变。
- **保护**：`protectionUntilDay[32]` 为绝对游戏日；gain 与 pulse 续期，手动增减不续期。衰减按 tick 窗口：deadline 覆盖 `≤deadline` 的整周期，跨批欠账只保护落入窗口的 tick。依据旧 `DecayPauseTimer` 每半小时减 1（`SLSF_Reloaded_FameManager.psc:125-227`），长跳不会整段免衰减。
- **增减**：gain 抽 `[1, tierCap]`×倍率，截断后 ×iteration，`<1→1`，前值取公开 `max(L,R)`，总和封顶 150；夜间减半为 `ReduceFameAtNight && 夜间 && 前戏`（AND，迁移契约），`ApplyGainFame` 从配置解析三个夜间键与当前小时。手动传播 `min(150, trunc(target + source×percent/100))`，source<10 拒绝、percent 夹 10..100；手动衰减夹 0..150、`Min>=Max` 取 Min、`0,0` 拒绝。
- **自定义地区衰减**：`customRegions` 命中时用旧固定档 `{5,4,3,2,1}`，VeryHigh 固定取 -1（乘倍率后取整）不抽样；默认地区用配置档。

## 3. 默认图与参数

- `BuildLegacyDefaultGraph`：21 节点、39 条无向链接、78 条有向边，节点顺序与链接逐项对齐 `docs/research/default-reputation-topology.proposed.json`；测试用独立副本断言集合相等、无重复、无自环。绑定要求稳定键唯一，链接端点缺失报错，`defaultRetention=0` 允许（全禁用）。
- `LegacyDefaultParameters()`：`decayPerDay=2.0`（12h/次）、`relaxationPerDay=1.0`、`spreadThreshold=30`、`spreadRetention=0.6`、`maxFame=150`。`spreadThreshold=30` 来自 MCM `MinimumFameToSpread=30`；`decayPerDay=2` 来自 `DecayTimeNeeded=24` 个半小时批；`relaxation=1.0` 与 `retention=0.6` 是未标定的工程默认值（研究探针只比较了 0.2–0.5 的性能区间，不是推荐值），需 A07/配置组标定。`Parameters.spreadRetention` 当前不参与计算，边权以图为唯一来源。

## 4. 验证环境

Windows 10 x64、MSVC 19.44（ReleaseWithDebInfo）、Clang 20（ASan）、Intel i5-13600KF（AVX2=yes）；游戏版本 Skyrim SE 1.6.1170 / SKSE 2.2.6（`README-Skyrim1170.md`），**未进游戏实机加载**，ABI/行为证据来自旧 PSC、`contracts/*.json` 与本机 probe。

## 5. 命令、退出码与结果

每条日志首行为实际命令，末行为 `EXIT:`。

| 命令 | 退出码 | 结果 |
| --- | --- | --- |
| `xmake build SLSFCore` / `SLSFCoreAsan` / `SLSFCoreTests` | 0 | 构建成功 |
| `powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/run-native-tests.ps1 -Mode core -Seed 1170` | 0 | 119 用例 / 119,204 断言 |
| 同上 `-Mode asan` | 0 | 119 用例 / 119,204 断言；测试分片无 ASan 报告（日志中的 heap-buffer-overflow 是 `SLSFAsanProbe oob` 的预期负例，`AsanProbe.cpp:15`） |
| 同上 `-Mode differential` | 0 | 6 用例 / 17,148 断言 |
| 同上 `-Mode boundary` | 0 | 4 用例 / 22 断言 |
| 同上 `-Mode fuzz` | 0 | `StateCodecFuzz -runs=100000 -seed=1170` |
| 同上 `-Mode bench` | 0 | 见下表 |
| 同上 `-Mode all` | 0 | 以上全通过 |

日志：`build/test-results/03-{core,asan,differential,boundary,fuzz,bench,all}-transcript.log`；分片与 JUnit 在 `build/test-results/{core-asan,core-shards,differential,boundary}.xml`。注意 `core-parallel.log` 等旧聚合文件是早期轮次，不作为证据。

基准（`bench.txt`，单次运行；三次运行 AVX2 加速比 3.75–4.13×）：

| 规模 | scalar | AVX2 | 加速 |
| --- | --- | --- | --- |
| 21 账号 / 63 边 | 64.8 ns/行 | 16.6 ns/行 | 3.90× |
| 64 / 192 | 67.2 | 17.9 | 3.75× |
| 128 / 384 | 78.8 | 20.3 | 3.87× |

结算+发布：21 账号 15.3 µs/批（2000 批），64 账号 34.7 µs/批（800 批）。两端计时都包含每次调用的图校验（O(rows)，AVX2 内部还会再验一次），因此加速比偏保守。

## 6. 固定验收用例

| 要求 | 测试 |
| --- | --- |
| 同类 25/20→25；来源降低→20 且不越过剩余入边 | `DiffusionKernelTests` 25/20；`SettlementTests` 25→20→20 |
| 本批传入 10 保留 | `SettlementTests` "settles the batch before propagating" |
| 夜间+前戏减半；配置驱动 | `GainRulesTests` AND/窗口端点；`SettlementTests` 仅配置+hour 的 gain |
| 来源 100、目标 20、30%→50 | `GainRulesTests` / `SettlementTests` 手动传播（含同地点 100→130） |
| 一跳不级联 | `SettlementTests` 0→2→3 链；pulse 链 |
| 回拨不重放 | `SettlementTests` 链式回拨；锚点==now |
| 同一区间一次 | `SettlementTests` 同 now 两次；`RandomTests` 队列不清空幂等 |
| 保护窗口 | 0.75 gain→1.0 全保护、2.0 衰减；长跳只保护窗口内 tick |
| pulse 后时钟/等值续期 | `SettlementTests` pulse→Advance 39.5；2.5→2.7 等值续期 |

## 7. 随机对照与失败输入（seed 1170）

差分 6 个 case（gain、decay roll、manual spread、manual decay、Advance、边界）用 xorshift 生成图/状态/命令，每 case 种子 `1170 + case×7919`（手动衰减再 +31）；Advance 为生产 scalar、生产 AVX2、`diffref::AdvanceReference` 三方逐槽比较。参考算法独立建 CSR，不复用生产 offsets；tier 映射独立；随机流共享以便逐项对照（不是独立 RNG 的统计检验）。

本轮修复过的失败输入（均已复跑通过）：

1. `accounts=3` 但 `accountKeys=4` 调 `ApplySpreadPulse` → 旧实现越界写；现返回 `InvalidState`（ASan 用例）。
2. `scheduled.at=NaN` → 旧实现排序违反 strict weak ordering；现 `InvalidArgument`。
3. 默认图重复 `riften–shors_stone`、`morthal–solitude` → 77 有向边；现 39 链接/78 边。
4. 目标 `remote=80, local=10` 的 gain 误用 local 档；现 `fame` 档（cap 4）。
5. 同一 `AdvanceInputs` 连续调用重复应用命令并多传一跳；现队列消费。
6. 0→2→3 链回拨重放第二跳；现纯回拨早退。
7. 0.75 gain、1.0 结算的旧测试预期与 tick 窗口不符；测试改为可观察的窗口边界。
8. 图 `retention=2.0` / 重复 accountKeys / 重复 stable 绑定 → 旧实现静默接受；现拒绝。
9. pulse 等值候选不续保护；现按“收到合格候选”续期。
10. 手动传播 source==target 旧被拒；现允许（旧事件无此限制）。

## 8. 独立审核与对抗验证

第二轮由 3 个子代理只读审核（算法/行为、内存/生命周期、报告逐项），本轮确认并处置：

| 审核项 | 处置 | 复验 |
| --- | --- | --- |
| 公开图 CSR 无校验可 OOB | 内核入口 `IsValidCompiledGraph`；AVX2 转内部 TU | malformed 图用例 |
| 强制 AVX2 无 CPUID / 回退路径被向量化 | 拆 TU + 运行时探测 | 21/64/128 基准；`avx2=yes` |
| AVX2 未清 28..31、非对齐 load | `_mm256_loadu/storeu`，显式填 25..31 | NaN/padding 用例 |
| 命令队列可重放、失败半写 | 一次性队列 + staging 提交 | 幂等/混合批用例 |
| 保护窗口“整批跳过”建议 | **未采纳**：重读 PSC，`DecayPauseTimer` 每半小时减 1，应按 tick 覆盖；长跳只保护窗口 | 长跳用例 |
| 自定义地区衰减缺失 | `customRegions` + 固定档/VHigh 不抽样 | custom 用例 |
| 夜间配置未接线 | `ApplyGainFame` 解析三键+小时 | 配置驱动用例 |
| pulse 未更新 `remoteSettledAt` | 目标锚 `max(now)` | pulse→Advance 39.5 |
| 等值候选不续保护 | received 计数续期 | 2.5→2.7 |
| 参考容差反向、未排序、无尺寸检查 | `+1e-12`、排序/NaN、尺寸校验 | 半日边界/乱序/NaN |
| `maxFame` 被动传播不封顶 | arrival/relax 统一 clamp | Set 200→pulse→Advance |
| 重复 accountKeys / stable 绑定 / 链接端点 | 编译期 `adjacent_find`、绑定唯一、端点检查 | 三个负例 |
| 夜间旧表达式为 OR | 迁移契约为 AND，保留偏差记录 | AND/端点用例 |
| `RuleGainFame` iteration NaN 可产出 0 | 非有限按 1.0，结果至少 1 | NaN 用例 |
| 报告批次顺序/发布函数/测试名/百分比范围 | 全文按实现重写；旧 10–50% 改为菜单范围，默认 30% | 本文件 |

未采纳/未修复并留档：`Parameters.decayPerDay` 与配置 `DecayTimeNeeded` 是两个来源（配置组接入时统一）；保护窗口按当前配置反推 gainTime（配置热改会追溯，低概率）；gain/手动传播把公开值写回 L，R 退去后公开值仍保留增益（旧版读写同一 Fame 值，属模型语义而非算术错误，见偏差表）。

## 9. 已知偏差（相对旧 Papyrus）

1. 夜间减半：旧 `((Hour > NightStart || Hour < NightEnd) && ReduceFameAtNight) || IsForeplay` 是 OR；NG 按迁移契约为 AND（任务书固定用例要求“同时成立”）。
2. 自动传播：旧 `SpreadFame` 每次随机加 10–50%（默认 30%，`MCM.psc`），目标写 `min(150, trunc(t+s×pct))`，且只对默认地点设保护；NG 为 retention×max 覆盖式 max，可被后续来源替换，到达不续保护。这是 2026-10-26 设计的有意替换，不是等价移植。
3. 传播门槛：NG 在“跨入边 max 后”比较阈值；旧版在来源类别值 ≥30 时进入候选池。前者是设计文档定义的内核语义。
4. 固定用例 25/20 使用 `retention=1.0` 边以得到精确 25；默认图 0.6 时结果为 15。验收句子描述的是 max 规则，不依赖 0.6。
5. `Advance` 不处理 gain；gain 由事件层（A07）在批次边界调用 `ApplyGainFame`。手动传播/衰减不改保护。
6. pulse 只续目标整行保护（与 gain 一致），旧版只对默认地点设暂停。

## 10. 下组接入

- 使用 `Advance(State&, GameTime, AdvanceInputs&)`；`scheduledChanges` 是一次性队列，调用后到期项被删除，禁止复用同一入参重放。
- `AdvanceInputs.backend` 可注入 `Scalar/Avx2/Auto` 做对照；生产用 `Auto`。
- 配置键：`GainRules.h` 的 `kGainConfigKey*`（含 `FameCategoryMultiplier` 前缀、夜间三键、decay 档）；04 组绑定 MCM 时需提供 `ReduceFameAtNight/NightStart/NightEnd` 的布尔/小时值。
- 绑定默认图用 `BuildLegacyDefaultGraph(bindings, retention)`；绑定要求 21 个稳定键唯一。04 组的地区绑定完成后应跑一次默认图端到端（当前只有单元图与随机图）。
- A07 事件层负责 gain/manual spread/manual decay 的调用与保护续期策略；同一 tick 内容变化后的传播需显式 `Advance`（或等待下一时间推进），当前没有 content-dirty 自动结算。
- 未实机加载；进游戏前需按 `README-Skyrim1170.md` 验证 1.6.1170 + SKSE 2.2.6 的 ABI。

## 11. 整链优化后的当前接口与交接

- `Advance` 按到期命令的时间稳定排序，先结算到该边界，再应用命令，最后推进到 now。迟到命令在已消费时刻应用。Set/Clear 后从命令边界累计后续衰减；成功消费到期项，失败保留原业务状态与原队列顺序。
- `PrepareSettlement(state, graph, configuration, regions)` 拥有 owner 投影后的 CSR、类型化规则和按 RegionId 排列的自定义地区标记。同 owner 自环删除，平行边取最大权重。默认图仍为 21/78。
- 当前原 `Advance` 入口自动复用内容缓存。会话持有 `AdvanceInputs.prepared` 与 `SettlementWorkspace`，绑定、连接、配置或自定义地区注册更新后替换准备结果。数值更新继续复用；同一工作空间由统一业务执行器串行使用。
- 脉冲 prepared 重载复用 CSR 与来源缓冲；raw 重载保留单次准备。B02 在公开 Spread/SpreadRoll 入口先推进到当前时间，再调用脉冲原语。
- `CompiledFameRules` 保存 25 类倍率、默认档位及夜间/保护参数；配置重载继续只读取当前类别与档位。衰减按有效类别升序处理，保留旧抽样次数和范围。
- `RelaxRemoteRows` 的 AVX2 路径连续处理类别块，补槽归零。标量、强制 AVX2、Auto 路径的特殊值和逐位对照通过。
- 本轮全量日志、红绿回归、独立 Standards/Spec 与注释审查见[归档](../implementation-03/README.md)。最终源码审核树为 `28b4160cbba5afc84ce869d3f990648510db74fa`，18 个源与测试文件发生变化；普通/ASan、边界、差分、fuzz、合同与任务书检查均通过。
- 完整生产 `Advance` 对照中，默认图连续推进由 7.242 µs 降至原入口 2.324 µs、显式准备入口 1.831 µs；八组新旧业务校验和相同。测量包含原入口内容比较，准备成本与测试条件见[探针报告](../implementation-03/advance-probe.md)。
- 04 接玩法规则；B02 接会话事务、准备结果生命周期和复合发布；B07 完成 JSON 更新及 `DecayTimeNeeded` 到 `decayPerDay` 的转换。游戏记录采集与 1.6.1170 实机加载按后续任务执行。
