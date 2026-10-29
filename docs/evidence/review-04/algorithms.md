# 04 组算法对抗审核

## 范围与方法

- 审核对象固定在 tree `2b3912e81d62b4a26dbe43b20b92e18f5ce522cf`；对照基线 `5222a3c31841426bb26c3635a37638abb8f707e5`、03 组 `28b4160cbba5afc84ce869d3f990648510db74fa`，使用 `review.diff` 和 `changes-since-03.diff`。本轮只新增 `algorithm-*` 探针/日志和本报告。
- 主要行为 oracle 是旧 Papyrus 脚本，规则与 readiness 约束见 `.scratch/native-migration/spec.md:50-52`、`.scratch/native-migration/optimization-plan.md:95-99`。04 组计划要求直接逐类候选一致、保留顺序与 RNG 调用，并且只暂停依赖未知事实的候选。
- 两个独立 C++ 探针通过 Clang 直接编译被测 gameplay CPP；没有运行 xmake，也没有改生产代码、测试或文档。探针输入只使用公开 DTO 中的 `Fact`/枚举合法值。

## 已撤回候选 A1 — heavy bondage 的 unhideable 结果与旧行为一致

这项候选反例已由完整 Papyrus 控制流复核并撤回。旧脚本在 `CheckBondage` 的 heavy 分支前将 `IsWearingUnhideable=False`（`:803`）；unhideable 扫描位于 `ElseIf lockable` 内（`:809,871-875`），随后 `Else` 与 `EndIf` 在 `:885,889` 闭合该分支。heavy 分支会跳过扫描并保留 false。新入口 `VisibilityRules::EvaluateBondage` 在 heavy 分支返回 false，符合旧行为（`src/core/gameplay/Visibility.cpp:518-533`）。

探针保留该输入作为旧行为控制：`deviousHeavyBondage=true`、`devious.hood=true` 实测为 `Ready:false`，与旧脚本一致。该字段没有 RNG 影响；在 `src/**`、`include/**` 中也未发现生产端消费者。

## 发现 A2 — body Chest tattoo 对 AND 安装状态过度暂停

**严重度：低优先级 readiness 精度观察。**

- `BodyTattooVisible` 在分支分析前检查 SLS 与 AND 安装状态，任意 Unknown 都直接返回 NotReady：`src/core/gameplay/Visibility.cpp:223-227`。
- 对合法 DTO：SLS 已安装；body tattoo 的槽位 0 为已应用的 `Chest`；Chest rank 已知为 1；身体/上背/下背装备均已知不存在；AND 安装状态设为 Unknown。将唯一未知事实分别代入两个可能分支，实际公开调用得到 AND=true 的分支 A `Ready:true`，以及 AND=false 的分支 C `Ready:true`；Unknown 输入却提前返回 `NotReady:false`。这是该具体输入下的 readiness 精度差异；04 组计划要求只暂停依赖未知事实的候选（`optimization-plan.md:97-99`）。
- 输入域依据：`VisibilityFacts` 将 mod 安装、AND rank、装备事实分别公开为独立 `Fact` 字段，没有要求 AND 安装 Unknown 时所有 ranks 必须也是 Unknown；本探针只组合 DTO 所声明的 Known/Unknown 与 Chest 枚举，不使用损坏值或非法枚举。
- 无 RNG 影响。当前确认的是纯入口的 readiness/结果可用性偏差；B06/业务层尚未接线，不能称为已观察到的玩家结果。
- 修正建议：可考虑在该已知条件下保留结果相同的 `Ready:true`。本发现不要求增加通用符号求值/分支推理框架。

## 可重放的反例

`build/diagnostics/review-04/algorithm-gameplay-probe.sh` 通过实际调用 `VisibilityRules::EvaluateBondage`（已撤回 A1 的旧行为对照）与 `VisibilityRules::BodyTattooVisible`（A2），并在未知 AND 输入外分别实际运行 AND=true/false 分支。PowerShell 中执行：

```powershell
& 'C:/Program Files/Git/bin/bash.exe' --noprofile --norc 'build/diagnostics/review-04/algorithm-gameplay-probe.sh'
```

退出码 `0`。`algorithm-gameplay-probe.log` 记录：

```text
heavy_harness_wearingUnhideable=Ready:0
heavy_harness_visiblyBound=Ready:1
legacy_expected_heavy_harness_wearingUnhideable=Ready:0
heavy_harness_matches_legacy=true
chest_tattoo_unknown_AND=NotReady:0
branch_A_AND_true=Ready:1
branch_C_AND_false=Ready:1
probe_result=REPRODUCED
EXIT_CODE=0
```

## 其他审核面

以下项目按旧脚本顺序和输入事实路径检查，未找到新的可复现差异：

- 匿名区域进入/离开、首次 now=0、记忆与小时推进；没有发现公式、阈值或时间回退上的新反例。
- 可见性其余 body tattoo 子类、face/hand/foot tattoo、cum/性别门控、bondage 其余分支；A2 是已确认的 readiness 精度观察。注意 DD 运行时事实在报告中属于记录的近似，不能外推为已获批准的玩法变更。
- NPC scanner 的过滤先后、skip/continue、每个存活候选的 RNG、单次扫描至多一次成功，以及同身份列表续跑；没有发现新顺序或调用次数偏差。
- Fame 候选枚举顺序、betray modifier 跨类别保留、50 次抽取与 selectedMask 去重；没有发现新差异。
- Animation gate 顺序、10 秒宽限、multi-actor kiss 抽骰、类别顺序/race 映射/重复添加；没有发现新差异。
- Comments 裸体机会的前置顺序、匿名/战斗调度、整数频率边界及正常路径单次 1–100 抽骰；没有发现新差异。

B04 仍没有运行中的目标游戏，因此关系 rank 映射与引擎检测/LOS 等运行时表现不在本次纯算法结果中背书。

## 性能与分配观察

规范要求记录热查询、结算和 dirty 刷新的成本（`spec.md:128`）；本探针仅量了 `FameGainRoller` 与完整 25 位 mask 的热查找，不能代表全插件、游戏帧预算或任意 NPC 批次。

- `algorithm-performance-probe.cpp` 以 `-O2 -DNDEBUG` 编译，固定 RNG 返回区间上界；压力批次为 10,000 次 `FameGainRoller::Run`，每次 25 类均 eligible，selector 选中全部类别。结果 250,000 个选择、`operator new/new[]` 计 180,000 次（18 次/调用）、总耗时 5,557 μs，约 1.80M 调用/秒。分配数包含返回容器/动态存储的分配，不计底层 malloc 实作细节。
- 同一探针以 noinline 查询函数实际执行 1,000,000 次完整 mask 连续扫描（25M `CategoryMask::Test`）；命中 25,000,000，耗时 21,746,000 ns，约 1.15G 次测试/秒。mask 为 `0x1ffffff`。
- 这些是本机单次微基准的成本记录，不构成性能缺陷或 P1 证据。复杂度检查显示候选枚举受 25 个固定类别约束，selector 抽样封顶 50 次，均有明确固定上限。

重放命令：

```powershell
& 'C:/Program Files/Git/bin/bash.exe' --noprofile --norc 'build/diagnostics/review-04/algorithm-performance-probe.sh'
```

退出码 `0`；完整数值及构建命令见 `algorithm-performance-probe.log`。编译器、构建 flags 与本机时钟不同会改变耗时数字。
