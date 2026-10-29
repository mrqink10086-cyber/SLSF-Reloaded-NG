# 连续索引、CSR 复用与局部批次成本探针

日期：2026-10-28。仅在本目录构建独立诊断程序；未修改生产源码、现有测试或 Xmake 配置。

## 重放与方法

- 源码：[chain-probe.cpp](chain-probe.cpp)；构建脚本：[chain-probe.sh](chain-probe.sh)；原始结果：[chain-probe.log](chain-probe.log)。在 Git Bash 执行 `bash docs/evidence/algorithm-chain-research/chain-probe.sh`。
- Windows x64，13th Gen Intel Core i5-13600KF；Clang 20.1.0；`-std=c++23 -O2 -DNDEBUG`。只有生产 `DiffusionKernelAvx2.cpp` 使用 `-mavx2`，CPUID/调度代码仍使用基础指令集。运行时确认 AVX2 可执行。
- 每项预热 32 次，计时 7 轮，报告各轮平均耗时的中位数；单位均为 ns。源码和日志记录每轮迭代数。变化输入、跨调用可见的 checksum、局部 `noinline` 函数及编译器 fence 防止无效工作被消去。
- 图、地区身份、源状态、custom 标记表和待复用缓冲区均在计时外准备；CSR 每次编译组有意包含编译产生的临时对象生命周期。State 构造复制项有意包含目标分配与析构，另列预先构造目标的赋值复制。
- 未固定 P 核/E 核、CPU 频率或执行顺序；测量反复使用同一批图和数值，因此是预热后的热数据局部对照，没有模拟冷缓存、引擎、VM、存档、外部 MOD 或完整提交/发布路径。下列数字不能直接换算整次 `Advance` 或游戏帧率。

## CSR：保留生产计算与验证，只改变编译图是否复用

两组都直接调用生产 `ComputeIncomingRows`，每次仍清零 incoming、执行 `IsValidCompiledGraph` 的 O(N+E) 扫描，再执行 AVX2 传播。复用组没有移除验证。验证耗时列单独调用同一生产函数，不应与 Compute 列重复相加。

21 节点图直接由生产 `BuildLegacyDefaultGraph` 构造，使用真实 39 对双向连接（78 条有向边），入度为 1–8。128 和 512 节点的合成图，每个来源连接 `(source+1)%N`、`(source+N/4+1)%N`、`(source+N/2+1)%N`，每节点入度和出度均为 3。所有边 retention=0.6，cutoff=30；输入覆盖 25 类，填充槽保持零。

| 节点 / 边 | 每次 Compile + Compute | 复用 CSR + Compute | 单独验证 | 局部比值 |
|---|---:|---:|---:|---:|
| 21 / 78（真实默认拓扑） | 4339.22 | 284.74 | 35.30 | 15.24× |
| 128 / 384（均匀合成） | 40079.92 | 2130.44 | 147.80 | 18.81× |
| 512 / 1536（均匀合成） | 229549.60 | 8673.53 | 533.93 | 26.47× |

每个规模对 17 组变化输入逐元素比较全部 32 槽，fresh/cached 和 scalar/AVX2 均 bitwise 相同。结果支持在地区绑定和图配置不变期间复用编译后的 CSR；实际接入仍需以地区索引版本和图参数版本失效缓存。本探针没有实现生产缓存、重绑定失效、配置更新或错误提交协议，也没有测完整 `Advance`。

生产依据：[CompileDiffusionGraph](../../../src/core/reputation/DiffusionGraph.cpp#L105) 每次按稳定身份查找、排序并分配 CSR；[ComputeIncomingRows](../../../src/core/reputation/DiffusionKernel.cpp#L144) 保留清零及校验；[Advance](../../../src/core/reputation/Settlement.cpp#L444) 当前在无工作判断之前编译图。

## 自定义地区：同一批成员判定的隔离成本

原方式复现生产 `std::any_of(customRegions)` 对稳定身份的比较；连续表方式在计时外生成 `vector<uint8_t> customByRegion`。两组按同一轮转顺序查询每个地区一次，保持同样的顺序敏感 checksum；每种规模、比例对所有起点检查答案一致。计时不包含表构建、State 复制、衰减或传播。

| 地区数 | 自定义地区数 | 逐地区 any_of，整批 | 连续标记表，整批 | 局部比值 |
|---|---:|---:|---:|---:|
| 21 | 0 | 9.30 | 25.80 | 0.36× |
| 21 | 1 | 85.10 | 25.80 | 3.30× |
| 21 | 5 | 291.40 | 25.80 | 11.29× |
| 21 | 21 | 745.90 | 25.80 | 28.91× |
| 128 | 0 | 64.00 | 142.80 | 0.45× |
| 128 | 1 | 431.00 | 145.50 | 2.96× |
| 128 | 32 | 10685.80 | 142.70 | 74.88× |
| 128 | 128 | 22918.50 | 142.80 | 160.49× |
| 512 | 0 | 274.00 | 563.00 | 0.49× |
| 512 | 1 | 1694.00 | 595.00 | 2.85× |
| 512 | 128 | 153603.00 | 562.50 | 273.07× |
| 512 | 512 | 347675.00 | 573.50 | 606.23× |

空 custom 集合已经很便宜，应在外层复用“全部默认地区”的路径；有自定义地区时，绑定期生成的连续标记表消除了每地区的稳定身份线性搜索。这里所有 plugin 字符串相同且长于常见短字符串存储，custom 身份取前 C 个地区；不同命中分布和字符串形态会改变绝对数字。规模趋势是 O(N·C) 身份比较与 O(N) 字节读取的区别，不能把该局部比值写成完整结算加速。

生产依据：[IsCustomAccount](../../../src/core/reputation/Settlement.cpp#L410)，以及[结算循环](../../../src/core/reputation/Settlement.cpp#L518)中每地区调用该函数。

## 布局与复制：连续行不等于整条路径都连续访问

本机构建得到：

| 类型 / 字段 | 大小或偏移（字节） | 对齐（字节） |
|---|---:|---:|
| NumericRow | 256 | 32 |
| AccountState | 1120 | 32 |
| CSR Operand | 16 | 8 |
| AccountState.local | 0 | — |
| AccountState.remote | 256 | — |
| AccountState.fame | 512 | — |
| AccountState.flags | 768 | — |
| AccountState.protectionUntilDay | 800 | — |
| Operand.source | 0 | — |
| Operand.coefficient | 8 | — |

`accounts` 是连续的 AccountState 数组；每地区内部 L/R/F 各为连续 256 字节行，但相邻地区 L 的间距是 1120 字节。L/R 快照各是连续 NumericRow 数组；CSR offsets 与 Operand 各在自己的连续 vector 中，读取来源时仍按 `source` 间接访问数值行。稳定身份 vector 的元素连续，长字符串字符数据单独分配；因此“表元素连续”不代表沿图访问时顺序读取或整个对象图共用一块存储。

复制源使用 N 个地区、N 个身份/地点映射、floor(N/4) 个自定义地区，provider 和暂缺地区为空。身份 plugin 为 `algorithm-chain-research-plugin.esp`；自定义名为 `custom-region-with-long-name`。源状态、赋值目标及 L/R 目标缓冲区在计时外构造，每轮改变一个本地值。L/R 全槽复制结果与源逐元素相同。

| 地区数 | AccountState 数组字节数 | 两份 L/R 快照字节数 | 完整 State 构造复制 + 析构 | 复用 State 赋值 | 复用缓冲区提取 L/R |
|---|---:|---:|---:|---:|---:|
| 21 | 23520 | 10752 | 1198.86 | 392.84 | 91.38 |
| 128 | 143360 | 65536 | 7869.84 | 2622.74 | 1230.06 |
| 512 | 573440 | 262144 | 29824.75 | 9970.55 | 4621.05 |

三项载荷和用途不同。完整 State 提供原实现需要的候选事务副本，L/R 快照只冻结传播输入；不能用 L/R 提取成本直接替代事务副本并声称等价。结果说明冷身份和全部业务字段的重复深拷贝值得在完整路径中定位，缓冲区复用也应测实际生命周期；本轮没有改 State 所有权或失败回滚方案。

生产依据：[AccountState/State](../../../include/slsf/core/reputation/ReputationState.h#L27)、[NumericRow](../../../include/slsf/core/NumericRow.h#L17)、[Operand](../../../include/slsf/core/reputation/DiffusionGraph.h#L15)、[候选 State 复制](../../../src/core/reputation/Settlement.cpp#L478)和[L/R 临时缓冲区](../../../src/core/reputation/Settlement.cpp#L514)。

## 类别稀疏枚举：512 个 mask 的局部对照

两组分别用 25 项逐类分支扫描和生产 `CategoryMask::ForEach` 置位枚举；每个 mask 的选中类别相同并按升序处理，使用顺序敏感 checksum 验证相同结果。输入值在轮间变化；不是 AVX2 数值行比较，也不包含随机数或配置读取。

| 每 mask 有效类别数 | 扫描 25 类 | 枚举置位 | 局部比值 |
|---|---:|---:|---:|
| 0 | 2437.80 | 103.30 | 23.60× |
| 1 | 3012.90 | 292.50 | 10.30× |
| 6 | 3907.90 | 1770.40 | 2.21× |
| 25 | 7476.80 | 7452.80 | 1.00× |

空集合可在外层早退，稀疏候选适合升序置位枚举，稠密数值传播仍应与现有连续 AVX2 分开衡量。本轮不能确定混合阈值，也不能把稀疏枚举用于改写原有随机抽样顺序。生产 `CategoryMask::ForEach` 已使用 `std::countr_zero` 和 `remaining &= remaining - 1`，见[实现](../../../include/slsf/core/CategoryMask.h#L72)。

## 仍未测量的候选

在全部本地衰减完成后先计算一份冻结的 `F=max(L,R)`，再让每边只读 F 一行，可能减少高出度来源的重复 L/R 读取及 max。本轮没有测量这一候选。它需要保持同阶段冻结、一跳传播、零值/NaN/max 语义与业务时间边界；不能直接复用衰减前的 `account.fame`。当前已有方案的布局和缓存修改应分别测量完整结算后再决定。

最终结果：`chain-probe=PASS`，checksum=`2242174067907451490`。

归档后从本目录源码重新编译执行，结果见[复跑日志](chain-probe-replay.log)。21/128/512 节点组分别为 4240.24→295.02、39329.26→2183.50、230365.60→15318.67 ns；逐位检查和最终 checksum 相同。512 节点复用组波动明显，本文表格保留首轮测量，完整路径的性能阈值应使用固定核心后的数据。
