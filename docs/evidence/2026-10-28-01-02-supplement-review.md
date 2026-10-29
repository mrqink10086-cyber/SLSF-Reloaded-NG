# 01/02 审核与修复记录

日期：2026-10-28。

审核提出的六项问题均已修复。地区暂缺时保存完整声望，地点归属沿用注册顺序，零类别模组注册可以保存；单项写入直接按 RegionId 更新目标地区，模组启用类别使用 CategoryMask。

需求来自[项目规格](../../.scratch/native-migration/spec.md)、[代码库设计](../../.scratch/native-migration/design.md)、[优化方案](../../.scratch/native-migration/optimization-plan.md)、[配置设计](../../.scratch/native-migration/configuration-design.md)、[A 任务说明](../superpowers/plans/2026-10-27-foundation-and-core-plan.md)和 [02 任务书](../superpowers/plans/2026-10-27-task-books/02-state-location-and-codec.md)。用语见[术语表](../../GLOSSARY.md)。

## 审核基线

原审核比较初始化提交 `5222a3c31841426bb26c3635a37638abb8f707e5` 与工作区快照 `84017e61963b7ca7492fe67c2adcd87f67ce167b`，包含未跟踪文件。三个 Luna/xhigh 子代理分别审核规范、需求和算法，主代理重放反例。原结论为 Standards 0 项、Spec 1 项 P1 和 5 项 P2。

本轮修复的源码快照为 `6b22845bcec29a63eefd1e46867f4934893d84c8`。复核使用：

```text
git diff 84017e61963b7ca7492fe67c2adcd87f67ce167b 6b22845bcec29a63eefd1e46867f4934893d84c8 -- include/slsf/core src/core tests/core docs/evidence/review-01-02/algorithm-single-write-probe.cpp
```

## Standards

修复后独立复核为 0 项规范问题、0 项代码异味。注释清理由用户本轮及前轮要求授权，审查删除 16 个多余注释块。

## Spec

修复后独立复核确认 S1–S6 均已落实，0 项遗漏或行为错误。原六项问题及回归结果如下。

## 六项修复

| 项目与要求 | 原问题 | 修复与回归 |
|---|---|---|
| S1 / P1，A04/A08：暂缺地区的完整声望随存档保留 | Restore 只留下地区身份，下一次 Capture 丢掉声望、标记、保护和时间 | `State.unresolvedAccounts` 保存完整记录。Capture 写回已绑定和暂缺地区，Restore/重新绑定按稳定身份恢复；测试覆盖声望、flags、保护、结算时刻、匿名和辅助字段 |
| S2 / P2，A04：归属保留原注册优先级 | 声明地区并入其他地区后，只使用最终所属地区的排名，可能错误继承父地点 | 候选取 `min(priority[声明地区], priority[最终所属地区])`，结果仍指向最终所属地区；增加嵌套地区与中间排名父地点的反例 |
| S3 / P2，A03/A08：模组可先注册再选择类别 | 注册成功但 changed 为 false；编码器返回 `provider contribution has no categories` | 新注册返回 changed；v2 编码、解码和恢复支持零类别注册；测试覆盖保存、恢复、后续启用类别、重复注册及注销 |
| S4 / P2，A03：单项写入只重算目标地区 | 每次 Set 又扫描全部地区，成本随地区数量增长 | 目标地区重算一次，三组类别计数只处理变化位；最后一个地区移除该类时才清汇总位。批量清理、恢复和绑定走全量重建 |
| S5 / P2，A02/A03：数值命令接收 RegionId | `ExplicitCommand.target` 使用 LocationId，数值模块再解析地点 | target 改为 RegionId；调用方先通过地点映射取得地区。回归覆盖有声望地区、无实际地点记录的直接写入 |
| S6 / P2，A03：模组启用类别使用紧凑位集合 | 运行期每个模组保存 32 个字节并逐类遍历 | `ProviderContribution.categories` 改为 CategoryMask；添加/移除位更新 activeCounts，计数查询直接读取；NotReady/Stale 保留注册和类别 |

实现位于 [ReputationState.cpp](../../src/core/reputation/ReputationState.cpp)、[RegionRegistry.cpp](../../src/core/location/RegionRegistry.cpp)、[ProviderRegistry.cpp](../../src/core/reputation/ProviderRegistry.cpp)、[StateCodec.cpp](../../src/core/persistence/StateCodec.cpp)及对应头文件。回归位于 [ReputationStateTests.cpp](../../tests/core/ReputationStateTests.cpp)、[RegionRegistryTests.cpp](../../tests/core/RegionRegistryTests.cpp)、[ProviderRegistryTests.cpp](../../tests/core/ProviderRegistryTests.cpp)、[StateCodecTests.cpp](../../tests/core/StateCodecTests.cpp)；[SharedTypesTests.cpp](../../tests/core/SharedTypesTests.cpp)检查命令 ID 和类别集合类型。

## 原反例重放

- **暂缺地区**：R1 的 `L=42、R=9、flag=1、protection=123.5、settledAt=77.25` 经“地区缺席→保存→恢复地区”后，结果由 `restored=0` 改为 `restored=1`，上述字段恢复原值。
- **注册顺序**：父 R3、子 R2，R1 包含 R2，`customOrder=[R2,R3,R1]`。生产与独立参考现在均返回 R1（RegionId 0）；修复前生产返回 R3（RegionId 2）。
- **零类别注册**：255 个注册全部 Capture、Encode 成功；第 256 个仍按容量规则返回 Invalid。
- **单项写入**：原探针在 128/4096/16384/65535 个地区分别约 1.87/76.50/394.59/4024.11 μs/次；修复后同批输入约 0.030/0.017/0.023/0.020 μs/次。增加到各 100 万次写入后，分别为 0.017778/0.017867/0.017586/0.017464 μs/次。测量包含真实 ApplyExplicit 和类别汇总，初始化在计时外；小批次结果受计时分辨率影响，长批次用于确认成本随地区数量保持平稳。

这些数据是本机 clang++ `-O2` 对纯核心入口的测量。游戏帧、外部查询和快照发布成本由后续集成测量。

四个探针保存在本报告旁的 `review-01-02/`。历史输出使用 `*-replay.log`，本轮输出使用 `fix-*.log`。

## 检查结果

| 检查 | 修复后结果 |
|---|---|
| 普通核心 | 75 cases / 100987 assertions，通过 |
| ASan 核心 | 75 / 100987，通过；20 分片本次 5.175 秒 |
| 普通 / ASan 边界 | 各 4 / 22，通过 |
| JUnit | 核心 75、边界 4 个唯一 case，failures/errors 为 0 |
| ASan 自检 | 正向 probe=42 index=7；负向报告预期的 heap-buffer-overflow |
| StateCodec fuzz | seed 1170，100000 runs，通过 |
| 任务书和桌面提示 | 11 组、25 任务、37 个 CPP 责任归属、11 份提示及 100 个链接，通过 |
| 文档 | 52 份 Markdown、294 个本地链接及代码围栏检查，通过 |

命令：

```powershell
tools/run-native-tests.ps1 -Mode core -Seed 1170
tools/run-native-tests.ps1 -Mode boundary -Seed 1170
tools/run-native-tests.ps1 -Mode asan -Seed 1170
tools/run-native-tests.ps1 -Mode fuzz -Seed 1170 -Runs 100000
python tools/verify-task-books.py
```

日志：[核心](review-01-02/fix-core.log)、[边界](review-01-02/fix-boundary.log)、[ASan](review-01-02/fix-asan.log)、[fuzz](review-01-02/fix-fuzz.log)、[暂缺地区](review-01-02/fix-unresolved-state.log)、[归属](review-01-02/fix-owner-priority.log)、[零类别注册](review-01-02/fix-empty-provider.log)、[写入](review-01-02/fix-single-write.log)、[长批次写入](review-01-02/fix-single-write-long.log)。

## 注释与文档

独立注释审查删除 16 个历史叙述、自解释过程及命名空间闭合注释块，保留公开位值、遍历顺序、数据所有权、保存版本和外部状态语义。项目规格、设计、任务书、研究、交付报告和桌面提示统一使用“声望地区、地区声望数据、模组启用的类别、类别汇总、公开查询快照”，把要求写成操作、条件和结果。

## 后续接入

03/04 组继续传播、结算和玩法规则。B02 把内置来源与外部模组启用情况一起用于四类清理，并发布同版本的地点映射和声望；B05/B07 接保存回调、跨档 JSON 和菜单。游戏业务继续使用统一执行器，测试使用独立进程分片。

## 探针重建

在项目根使用 clang++ `-std=c++23 -O2 -Iinclude -I.`，分别编译以下源码，输出到 `build/diagnostics/review-01-02/`：

| 探针 | 同时编译的实现 |
|---|---|
| [algorithm-unresolved-state-probe.cpp](review-01-02/algorithm-unresolved-state-probe.cpp)、[algorithm-empty-provider-probe.cpp](review-01-02/algorithm-empty-provider-probe.cpp) | `src/core/reputation/ReputationState.cpp`、`src/core/reputation/ProviderRegistry.cpp`、`src/core/persistence/StateCodec.cpp` |
| [algorithm-owner-priority-probe.cpp](review-01-02/algorithm-owner-priority-probe.cpp) | `src/core/location/LocationIndex.cpp`、`src/core/location/RegionRegistry.cpp`、`tests/reference/LocationReference.cpp` |
| [algorithm-single-write-probe.cpp](review-01-02/algorithm-single-write-probe.cpp) | `src/core/reputation/ReputationState.cpp`、`src/core/reputation/ProviderRegistry.cpp` |

写入探针的可选第一个参数是写入次数；传入 `1000000` 重放长批次。
