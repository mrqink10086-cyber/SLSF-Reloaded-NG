# 03 组：在现有逻辑中落实结算链优化

2026-10-28。已直接修改现有 A05/A06 代码，并完成普通、ASan、随机差分、fuzz 和真实 `Advance` 前后对照。任务要求见 [03 任务书](../superpowers/plans/2026-10-27-task-books/03-diffusion-and-settlement.md)，研究依据见[整链研究](../research/native-chain-preparation-and-layout.md)。

## 实现结果

| 项目 | 当前行为 | 实现 |
|---|---|---|
| 按事件时间结算 | 到期命令前结算到该边界，再修改数值，最后推进到 now；同时间命令保持入队次序 | [Settlement.cpp](../../src/core/reputation/Settlement.cpp) |
| 迟到命令 | 在已消费时刻应用，后续区间使用新值；已结算区间消费一次 | [SettlementTests.cpp](../../tests/core/SettlementTests.cpp) |
| 准备结果复用 | `PrepareSettlement` 拥有稳定身份、owner 投影 CSR、类型化规则和自定义地区标记 | [Settlement.h](../../include/slsf/core/reputation/Settlement.h) |
| 原入口直接提速 | 现有 `Advance` 的 graph/configuration 入口自动复用内容缓存与工作缓冲 | [Commands.h](../../include/slsf/core/Commands.h) |
| owner 图投影 | 端点映射到最终所属地区，同 owner 自环删除，投影后平行边取最大权重 | [DiffusionGraph.cpp](../../src/core/reputation/DiffusionGraph.cpp) |
| 热规则 | 准备阶段生成 25 类倍率和档位数组；衰减按有效类别升序枚举，保持抽样顺序 | [GainRules.cpp](../../src/core/reputation/GainRules.cpp) |
| 自定义地区判定 | 准备阶段生成与 RegionId 同序的标记，结算按下标读取 | [Settlement.cpp](../../src/core/reputation/Settlement.cpp) |
| 工作缓冲与提交 | 单执行器复用 staging、L/R/incoming、elapsed 和队列空间；整批成功后提交数值与类别汇总 | [Settlement.cpp](../../src/core/reputation/Settlement.cpp) |
| AVX2 松弛 | 连续处理前 24 类，第 25 类按标量规则处理，补槽清零；基线与 AVX2 保持独立 TU | [DiffusionKernelAvx2.cpp](../../src/core/reputation/DiffusionKernelAvx2.cpp) |
| 脉冲复用 | prepared 重载复用 CSR 和数值缓冲，选择来源后执行一跳；原重载保留单次准备行为 | [Settlement.h](../../include/slsf/core/reputation/Settlement.h) |

旧配置重载继续只读取当前类别与档位，和预编译重载共享数值计算。默认图仍是 21 节点、39 对连接、78 条有向边，包含原有小镇与据点。

## 准备结果的生命周期

绑定、连接、配置或自定义地区注册更新时，调用方生成并替换准备结果；声望数值变化继续复用。准备结果拥有输入副本。`SettlementWorkspace` 由同一业务执行器串行使用，`AdvanceInputs` 的副本共享该工作空间。

原入口会比较稳定键、连接、owner、配置与自定义地区内容，更新受影响的准备数据；仅配置或自定义地区变化时复用已有 CSR。原入口保留 O(N+E+M) 内容比较，显式 prepared 入口仍核对 O(N) 稳定键。当前优化包含这些成本。

staging 保存本次数值事务需要的地区行、类别汇总和计数；ClearAll/Wipe 才复制待清理的暂缺地区记录。提供方、注册身份和地点映射沿用现有状态。失败时原业务状态与原命令队列保持原值。

## 行为回归与全量检查

先记录两条失败回归，再修改实现：[时间边界 red 日志](implementation-03/time-boundary-red.log)。单账户、固定抽样下，Set 10 @ day 0.5：

| 调用 | 修改前 | 修改后要求与结果 |
|---|---:|---:|
| Advance 到 0.5 | 9 | 10 |
| Advance 到 1.0 | 8 | 9 |

独立标量参考按事件时间分段，保持自己的衰减、行遍历和一跳计算；随机对照比较 L/R/F、保护、两个时钟及 RNG 调用。新增 Set/Clear 边界、迟到命令、缓存输入变更、owner 变更、raw/prepared 一致、来源选择和失败重试用例。

最终命令：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/run-native-tests.ps1 -Mode all -Seed 1170 -Runs 100000 -Jobs 20
```

| 检查 | 结果 |
|---|---|
| 普通核心 | 147 用例 / 119,837 断言 |
| ASan 核心 | 147 用例 / 119,837 断言 |
| 普通及 ASan 边界 | 各 4 用例 / 22 断言 |
| 随机差分 | 8 用例 / 17,161 断言 |
| StateCodec fuzz | 100,000 次，seed 1170 |
| ASan 正负探针 | 正例成功；故意越界负例按预期报告 heap-buffer-overflow |
| 合同与负例 | 合同检查通过，11/11 负例拒绝 |
| 任务书 | 11 组、25 任务、37 个 CPP 所有权、11 桌面提示及 101 本地链接通过 |

[全量日志](implementation-03/all-checks-final.log)、[JUnit 汇总](implementation-03/junit-summary.txt)、[合同日志](implementation-03/contracts.log)、[任务书日志](implementation-03/task-books.log)。JUnit 的 163 个核心节点包含 Catch2 section 展开，核心用例数取运行器列出的 147；每份 JUnit 节点唯一，failure/error 均为 0。

## 真实 Advance 性能

直接调用生产 `Advance`。基线来自固定源码树 `7a43e4e4b3a67b3ac5fa2f4d33990ac4e0101e70`；当前源码审核树为 `28b4160cbba5afc84ce869d3f990648510db74fa`。Clang 20.1、`-O2 -DNDEBUG`，只有 AVX2 TU 使用 `-mavx2`，Intel i5-13600KF，七轮中位数。单位为 µs/次。

| 图与负载 | 修改前原入口 | 当前原入口缓存 | 当前显式 prepared |
|---|---:|---:|---:|
| 默认 21/78，无工作重复调用 | 3.865 | 0.472 | 0.087 |
| 默认 21/78，连续推进 | 7.242 | 2.324 | 1.831 |
| 128/384，连续推进 | 109.662 | 13.140 | 11.002 |
| 512/1536，连续推进 | 253.172 | 54.722 | 49.092 |
| 512 自定义地区，连续推进 | 564.355 | 78.710 | 46.415 |

八组图/负载的新旧业务值、时钟、类别汇总、revision 与随机调用校验和全部一致。原入口中位数提速 3.1–18.7 倍；准备入口的一次性准备在计时外单列。128/512 是均匀三出边图，21/78 使用真实默认拓扑。

测量覆盖完整 `Advance`，含原入口内容比较与 staging。小步推进触发传播和 R 松弛，未触发衰减抽样；行为测试另行覆盖衰减与排队命令。P/E 核亲和性和频率未固定，使用热数据；这些数字是原生调用成本。完整八组与准备成本见[探针报告](implementation-03/advance-probe.md)，[源码与重放脚本](implementation-03/README.md)。

## Standards

独立 Luna/xhigh 复核：0 项已确认规范问题。范围包括最终 18 个源与测试文件、独立标量参考、串行工作空间、提交语义和 AVX2 TU 隔离。见[Standards 报告](implementation-03/standards-review.md)。

## Spec

独立 Luna/xhigh 复核：0 项缺失/部分实现、0 项范围扩张、0 项已确认错误行为。01–03 审核的 F1 时间边界、F2 热配置查询、F3 重编译 CSR 已在当前 03 逻辑处理。见[Spec 报告](implementation-03/spec-review.md)与[fresh 注释审查](implementation-03/comments-review.md)。

## 后续接入

04/B02 接入业务执行器时持有准备结果及工作空间；B07 在 JSON 配置更新后替换准备结果，并统一 `DecayTimeNeeded` 与 `Parameters.decayPerDay` 的转换。公开 Spread/SpreadRoll 仍由 B02 先结算再调用一跳原语。raw pulse 重载每次准备，反复调用使用 prepared 重载。

当前交付为纯核心实现与本机验证；游戏记录采集、SKSE 会话接线及 1.6.1170 实机加载按后续任务执行。已有工作区变更保留，未创建提交。
