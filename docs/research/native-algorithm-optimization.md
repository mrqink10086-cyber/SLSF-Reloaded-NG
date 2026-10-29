# 时间、地点与声望算法的原生优化研究

日期：2026-10-28。依据[项目规格](../../.scratch/native-migration/spec.md)和[代码库设计](../../.scratch/native-migration/design.md)，研究时间、地点、状态发布和传播的执行效率。结果包括当前源码分析、官方硬件/编译器依据和本机可复跑探针。

已采纳的选择、01/02 补做及后续验收见[优化实施方案](../../.scratch/native-migration/optimization-plan.md)。本文保留研究依据和探针结果，生产实施状态由项目路线维护。

本文的“当前源码”记录首轮研究时的版本；地点二分、owner 记忆化、地区数据和 AVX2 等后续交付见[整链后续研究](native-chain-preparation-and-layout.md)。该文继续核对 CSR 复用、小镇与自定义地区、连续布局及本机新探针。

## 结论与实施次序

整体还有优化空间。优先减少重复工作和热路径搬运，然后实现连续数值内核；当前默认规模很小，单轮向量计算已低于一微秒，运行时的唤醒、事实查询和发布方式更值得一起关注。

| 次序 | 建议 | 依据与预期作用 | 对应任务 |
|---|---|---|---|
| 1 | 实际地点先映射 RegionId，业务按地区声望分配数值行 | 让多个室内/子地点共享一行；避免将整个 BGSLocation 森林变成数值工作集 | A04–A06 |
| 2 | 地点绑定预编译 owner、alias 和稳定键索引 | 去掉反复父链、字符串规范化及全表查询；稳定键用于准备阶段，数字 ID 用于运行期 | A04 |
| 3 | 三种时钟分别使用连续期限堆；dirty/刷新请求合并 | 无到期工作时只看堆顶，按到期对象推进；减少轮询、回调和跨 VM 请求 | A06/B01/C02–C07 |
| 4 | 可变状态、公开查询快照、完整保存 DTO 分开 | 单项写入和查询保持数字寻址；公开发布只复制消费方需要的数据 | A03/A06/B02/B05 |
| 5 | 入边 CSR + 按类别连续的 AVX2 内核 | 来源地址虽离散，每条边内的类别连续；多个独立累加器提供指令级并行 | A05/A06 |
| 6 | 按地区声望数据保存类别位集合，分别表示数值资格、计分条件和类别设置 | 25 类放入一个 uint32_t；用集合运算、空集跳过和稀疏枚举替换布尔数组扫描 | A03/A05–A07 |

前四项首先改善访问次数、数据量或唤醒次数；类别位集合筛选需要处理的工作，向量内核加速其中的连续计算。它们都放在既定模块内部，公开协议和玩法继续按项目规格实现。

## 当前源码：地点与地区声望数据

### 已有 O(1) 查询继续使用

[LocationIndex.cpp](../../src/core/location/LocationIndex.cpp) 第 131–144 行用 DFS 进入/退出区间判断祖先；[RegionRegistry.cpp](../../src/core/location/RegionRegistry.cpp) 第 246–251 行通过 `locationOwners[LocationId]` 直接解析地区声望数据。这两条运行期查询已经是常数次寻址，优化重点在构建这些表和到达这些表之前的转换。

当前稳定键查询仍线性扫描：`FindLocation` 位于前者第 147–154 行，`FindRegion` 位于后者第 237–243 行。两份数组在构建时已经按稳定键排序，可以使用 `lower_bound`；二分的比较次数为对数级，前提是保持同一种排序/比较关系。[C++ 工作草案：lower_bound](https://eel.is/c++draft/lower.bound)

这类查询主要服务加载、注册和重新绑定；高频的“玩家当前地点属于哪片声望区域”应尽早转换到数字 ID，再直接查 owner。小型区域表可继续连续扫描；几千条地点记录才是本轮二分收益最明显的输入。

### 把重复父链改成一次父先子后计算

`BindRegions` 第 182–198 行为每个区域重新追溯 owner 链，第 202–231 行为每个地点重新走到根，并在沿途执行稳定键 map 查询。后者近似为 `O(N × 平均深度 × 区域查询成本)`。

建议在 WorldModel 准备阶段完成以下步骤：

1. 把区域 owner 关系编译为数字父索引，用迭代遍历和已完成结果得到 canonical owner；保留现有环和引用校验。
2. 给每个地点预先写入候选 `(canonical RegionId, priority)`，字符串查询在这一步结束。
3. 按父先于子的森林顺序计算 `best[node] = min_by_priority(candidate[node], best[parent])`，然后写入 `locationOwners`。

现有规则选择**全局优先级最高的祖先候选**，向下传递 priority 和最终所属地区。森林遍历与优先级归并的数字部分可降为 `O(N)`，稳定身份绑定另计成本。现有 entry/exit 在进入、退出时都递增；实现时显式保存 DFS preorder，或从父子数组生成该顺序，保持现有 LocationId 和存档身份。

运行期仍读相同的 owner 表。图或区域编辑准备新表，提交后统一切换；沿用已验证森林的路径，并保留现有构建入口对缺失父数组等输入的处理约定。本地探针只验证数字归并内核，完整 BindRegions 迁移还需复用现有归属用例。

### alias 和构建期分配

`ResolveLegacyLocation` 第 254–268 行每次规范化全部已存 alias，匹配后又扫描 customOrder。绑定时直接生成“规范化 alias → ApiLocationIndex”的紧凑表，请求只规范化一次即可；原有重复 alias 校验仍在绑定阶段完成。

`BuildLocationIndex` 第 28–48 行先建 map、排序、再重建 map，第 53 行为每个地点分配子 vector。后续可合并重复身份处理，并用子节点 offsets/indices 表组织 DFS。它们属于加载期成本，排在运行期地区声望数据、查询和发布之后处理。

### 数值工作集按 RegionId，而非每个引擎地点分配

[当前寻址函数](../../include/slsf/core/reputation/ReputationState.h)仍接收 LocationId；代码库设计已经要求先解析为 RegionId。按每行 32 个 double 计算：

| 地区数量 | L + R | 再加一份 F | 含义 |
|---|---:|---:|---|
| 21 | 10.5 KiB | 15.75 KiB | 默认拓扑规模 |
| 128 | 64 KiB | 96 KiB | 较多注册区域 |
| 1024 | 512 KiB | 768 KiB | 扩展规模比较 |

这些数值只包含三份数值数组。地点森林、字符串、映射、flags 和外部事实另计。默认拓扑有 21 个节点、78 条有向边，见[提议拓扑](default-reputation-topology.proposed.json)；其中边权仍为待配置的 null，探针使用独立合成权重。

## 时间：按到期对象工作，把相同工作合并

Scheduler、完整 Settlement 和实际 WakeSource 尚待实现；下列是实施建议。现有时序依据见[时间与轮询审计](time-and-polling-semantics.md)，运行时入口仍需 B01 在目标游戏验证。

### 期限结构与连续执行

游戏时间、排除菜单的活动实时时间、包含菜单的实时时间各有自己的比较域和期限堆。堆项保留紧凑的 deadline、任务槽索引和取消代次；协程帧、字符串及回调对象留在堆外。堆使用预留容量的连续 vector，唤醒时读一次各钟源，取出本轮已到期工作，再顺序执行。

`priority_queue` 的堆顶读取为常数成本，插入/移除为对数成本，底层可使用 vector；这是少量到期任务替代全表扫描的基础。[MSVC priority_queue](https://learn.microsoft.com/en-us/cpp/standard-library/priority-queue-class?view=msvc-170)

小量固定逻辑可以共享一个到期项，例如全局结算或一次 dirty 刷新；有独立取消需求的场景等待才持有各自任务槽。同一目标的连续装备/外观变化只更新 dirty 位与所需版本，既定刷新时点统一请求事实。外部请求按参与者和 provider 批量取回自有 DTO，再交回 Session 顺序消费。

本地期限探针仍模拟每毫秒检查一次堆顶。真正的 WakeSource 若能只在有到期工作时调度，还会减少这些检查；收益取决于运行时入口，报告暂不赋予它游戏帧时间数字。

### 业务时间可以合并，抽样规则保持原意

声望结算拥有一个已消费时刻，按实际时间差和已选保护边界处理变化。保护状态适合存地区级截止时间；匿名记忆适合存离开/进入时间，在实际查询或业务使用时计算。它们都能减少“每个地点每分钟扣一个计数器”的工作。

原 `DecayFame(d)` 是每个符合条件的格抽一次随机量，再乘批次数 d；改成循环 d 次会改变随机次数和声望分档。这一点已由原源码审计确认。时间优化使用批量系数、预解析的 25 类参数和有序业务边界，保留各入口的抽样时机及范围。默认与自定义地区的旧衰减档位分别处理；地点保护在外层跳过整行。跳时推进已有声望、保护和记忆，不补造目击事件。

到期工作与活跃/dirty 工作是两类索引：到期堆回答“现在该执行什么”，区域位集合回答“这次结算需检查哪些地区声望数据”。出现来源下降、边修改或较强来源消失时，受影响目标要重新比较**全部入边**；R 尚未退去的地区即使 L 为零也可能需要推进。当前小图可以直接整图扫一遍，待实际成本需要时再加入增量传播队列。

## 发布：先减少复制的数据

[ReputationState.cpp](../../src/core/reputation/ReputationState.cpp) 第 133–144 行的 `MakePublishedState` 先复制 L/R/F、保护和 flags，又清空并重算整份 F。若每个单项 Set 都调用它，O(1) 的数值写入会附带一次 O(地区数量 × 32) 的复制/重算。

数据按用途保存：可变模型保存完整本档状态；公开查询快照提供 F、flags、地点归属和所需诊断；Capture 取得完整保存记录。单项查询按地区编号读取，批量查询共享同一不可变版本。同批到期工作在批次末发布，同步公开写入在返回前完成提交。

dirty 行减少 F 重算；平面不可变快照仍需复制整份 vector。默认 21 个地区采用紧凑的公开查询快照，分块共享或分页复制在大规模发布成为热点时评估。探针的查询快照只复制 F 和 flags；保存游戏则捕获 L/R、保护等完整状态。

## 地点 × 类别：用位集合表达条件与派生状态

### 原脚本实际保存了哪些集合

原版确实有地点 × 类别布尔矩阵，但其中的 `SpreadFlags` 表示数值资格。可见的 17 个 PSC 和 ESP/VMAD 绑定未显示通用的“每地点每类别允许增长、衰减、传播”权限矩阵；MCM 的 PSC 缺失，现有证据来自业务调用及绑定，见[源码审计](papyrus-source-audit.md)。优化应沿用各条件自己的作用范围。

| 原结构 | 真实含义与入口 | 合适的原生表示 |
|---|---|---|
| 各地点 `SpreadFlags[25]` | `SetFameValue` 按 `value >= MinimumFameToSpread` 写入；`CheckSpreadableFame` 对整行查找 True | 每个地区一个 `legacySpreadableMask`，非空即存在候选 |
| `HasFameAtDefaultLocation/CustomLocation` | 任一类别值大于 0 的缓存 | `positiveMask != 0` |
| `DefaultLocationCanDecay/CustomLocationCanDecay` | 增长后的地点级保护，计时结束才允许整地衰减 | 地点截止时间；外层跳过整行 |
| `FameForbiddenByFriend/Lover[25]` | 关系 × 类别禁令，自动目击/动画入口各按自己的规则消费 | 分别保存两份类别 mask |
| 各类 `Flags[provider]` | 每个模组启用的类别；跨模组取 OR | 每个模组一份类别 mask，加每类启用模组数和汇总 mask |
| `PossibleFameGain` / `PossibleCategoryList` | 当前事件或来源实际满足条件的候选类别 | 事件临时 mask；需要旧抽样顺序时生成紧凑 CategoryId 列表 |

依据：[旧 DataManager](../../../SLSF%20Reloaded/Scripts/Source/SLSF_Reloaded_DataManager.psc) 第 206–215、550–558、1706–1713、1793–1828、2001–2050 行；[旧 FameManager](../../../SLSF%20Reloaded/Scripts/Source/SLSF_Reloaded_FameManager.psc) 第 169–194、772–787、1343–1362、1389–1398、1613–1625 行。`CanGain…(location)` 还会读取该地点的当前值/上限，所以候选集合有动态部分。

公开 Set、按类别 Gain、ManualSpread 和手动衰减分别检查自己的条件。ManualSpread 独立于关系禁令，手动衰减独立于自动衰减保护；各入口分别使用对应的类别集合。依据：[旧事件监听](../../../SLSF%20Reloaded/Scripts/Source/SLSF_Reloaded_ModEventListener.psc) 第 208–234、478–530、719–762 行。02 组已保存显式 flags 及其类别汇总；自动计分模块按上述入口规则读取它们。

### 一个 uint32_t 表示一个地区的 25 类

固定类别集合用 `uint32_t`。原来的 32 字节行压缩为 4 字节；默认 21 个地区的一种布尔矩阵从 672 字节降到 84 字节。L/R 继续使用连续 double，位集合保存筛选条件和状态。

```cpp
constexpr std::uint32_t kValid = (std::uint32_t{1} << 25) - 1;
const bool any = (candidates & kValid) != 0;
const bool all = (candidates & required) == required;
const auto changed = oldMask ^ newMask;
for (auto bits = candidates & kValid; bits; bits &= bits - 1) {
    const auto category = std::countr_zero(bits);
    // 按该入口原有规则处理此类别。
}
```

`popcount` 得到候选数量，`countr_zero` 找到下一置位类别；循环每次清除最低置位，工作量与实际候选数相关。[MSVC `<bit>` 函数](https://learn.microsoft.com/en-us/cpp/standard-library/bit-functions?view=msvc-170)

同一类别内部固定一个 bit 身份，公开文档索引、ESP 顺序和持久字段经过既有映射转换；高七位保持 0。涉及取反时与 `kValid` 相交，数值补槽继续按 32 槽分配。类别集合只需一个整数；数量可变的 dirty 地区集合可用连续 `vector<uint64_t>`，按同样方法逐字枚举。

实现上，配置/类别设置更新时编译长期条件；当前事件只构造一次动态候选集合。readiness 仍附在具体事实：纹身未知只暂停依赖纹身的候选，已有有效外部类别设置继续按它自己的规则使用。NotReady/Stale 不清持久类别设置。关系背叛与类别计分的随机调用保留原入口的条件和先后顺序。

### 派生 mask 随数值写入更新

旧版反复 `.Find(True)` 的工作可以变成一次整数非零比较。Set/Clear/批量结算更新同一地区声望数据的派生位，再统一发布。阈值改变时重建对应集合；这类偶发重建适合顺序扫描全部行。

撤销一个模组的类别时，`activeCounts` 按变化位减少启用模组数，计数归零才清汇总位。暂时未知保留计数和类别设置，四类清理由业务检查全部条件后执行。

新自动传播检查“来源值乘边权后达到门槛”。每条边的 AVX2 比较可用 `_mm256_movemask_pd` 收集四个类别位，再与该入口的静态规则集合相交。源下降、边关闭或较强来源消失时，从全部入边重算目标。L 为零而 R 仍在退去的地区继续推进。

### 稀疏枚举与连续 SIMD 配合

掩码为 0 时跳过整批。类别稀疏时，`countr_zero` 枚举减少数值加载和计算；类别密集时，连续 AVX2 处理整行。AVX2 的 double mask 使用每 lane 的向量位模式：探针通过共享的 16 项四-lane 表，将每四个类别位展开为一组 256 位 mask，再用 `blendv` 保留未参与类别。表共 512 字节，由所有地区共用。[MSVC x64 intrinsics](https://learn.microsoft.com/en-us/cpp/intrinsics/x64-amd64-intrinsics-list?view=msvc-170)

这些是整数集合操作与 SIMD 数值计算的配合。bit scan、BMI/BMI2、POPCNT 和 AVX2 有各自的指令能力；优先使用标准位函数和当前编译目标，不为取一个置位类别强行引入 PDEP 或新的 CPU 基线。

旧自动目击计分先建立合格候选，再随机选类；重复抽到已经选过的类别会消耗尝试次数，最多 50 次，可能不足额。用 `selectedMask` 替换字符串重复查找能保留这一行为；从剩余集合抽一个就清位会变成无放回抽样，改变随机次数及不足额结果。这里保留原规则和候选顺序。已选重写的自动传播按新图算法执行，无需复制旧随机选类流程。依据：[旧 FameManager](../../../SLSF%20Reloaded/Scripts/Source/SLSF_Reloaded_FameManager.psc) 第 1226–1280、2522–2561 行。

### 本机补充探针

仍使用本报告的 MSVC/AVX2/precise 环境、逻辑处理器 0、seed 1170、预热后七次中位数。组合探针比较四组逐类别字节条件和四个预打包 mask 的相交；数值探针对选择类别加同一份确定增量，并保留未选类别及全部补槽。逐行组合结果及完整数值输出都相等。

| 工作 | 21 个地区，一轮 μs | 解释 |
|---|---:|---|
| 四条件组合：逐字节 → 预打包相交 | 0.236 → 0.009 | 打包在输入准备阶段完成；未包含事实查询/打包成本 |
| 每个地区 2 类：逐字节 → 置位枚举 → AVX2 | 0.206 → 0.082 → 0.116 | 默认小规模稀疏时，置位枚举更快 |
| 每个地区 12 类：同上 | 0.191 → 0.168 → 0.131 | 连续数值计算开始占优 |
| 每个地区 25 类：同上 | 0.216 → 0.285 → 0.130 | 全部类别逐位置位扫描反而更慢 |

1024 个地区时，2/12/25 类的 AVX2 均快于置位枚举。稀疏枚举与连续计算的选择同时取决于行数、复制成本和计算内容，按实际业务批次测量后确定。本探针测量类别集合和确定数值更新的耗时；完整结算与游戏内指标见后续集成测量。

复跑：`powershell -NoProfile -ExecutionPolicy Bypass -File tools/research/run-native-algorithm-probe.ps1 -Probe permission-mask`。[源码](../../tools/research/permission-mask-probe.cpp)、[原始结果](../evidence/2026-10-28-permission-mask-probe.csv)。生产实现保持原状；建议把该布局随 A03/A05–A07 内部实现一起落地。

## 传播：连续类别、入边归并与指令级并行

当前 `DiffusionGraph` 只有稳定键边定义，CSR 和自动传播内核尚待 A05 实现。候选执行方式为：

```text
for each target region:
    clear independent category-block accumulators
    for each incoming edge in CSR[target]:
        source = max(L[sourceId], R[sourceId])
        candidate = source * retention
        accumulator = max(accumulator, candidate >= cutoff ? candidate : 0)
    write target incoming row once
```

每条边内按类别连续加载；target 的多个向量累加器让 CPU 交错执行独立类别块。来源值来自同一冻结阶段，输出写入独立缓冲；计算 incoming 与推进 R 分开，保留已选的一跳和实际时间语义。

本轮 MSVC 汇编把七个有效类别块展开为独立 YMM 累加器，内层包含 `VMAXPD/VMULPD/VCMPPD/VANDPD`，没有累加器的循环内栈 spill；每个目标外侧仍有累加器数组初始化和栈读写。进一步改写是否值得，以完整内核成本比较。跨边来源地址仍有间接寻址，连续的是每条来源行的类别和边描述。

预计算 F 会增加一次全行扫描和 F 写回；本轮度数为 4 的合成图没有显示收益。先直接读 L/R。若 F 已因发布而按同一冻结阶段更新，或实际入度更高，再比较复用它；避免维护两份没有共同版本依据的“缓存 F”。

## 本机探针结果

环境：Intel Core i5-13600KF；MSVC 19.44.35229；`/O2 /std:c++latest /arch:AVX2 /fp:precise`。固定逻辑处理器 0，seed 1170；每项先预热，再取 7 次中位数。输入和结果检查在计时外完成，计时内用校验和消费输出。构建同时生成 `/FAs` 汇编。

| 比较 | 数据 | 基线 → 候选 | 解释 |
|---|---|---|---|
| 实际 FindLocation → 二分 | 256 条地点，4096 次约半命中请求 | 612.0 → 77.3 ns/次，7.9× | 同一已排序快照、同一查询及命中结果 |
| 实际 FindLocation → 二分 | 4096 条地点，相同请求数 | 9332.8 → 115.4 ns/次，80.9× | 字符串/整数复合键，收益主要对应绑定侧 |
| 每节点走父链 → 父先子后归并 | 4096 节点，四叉树 | 13.112 → 2.532 μs/次，5.18× | 数字 priority 归并内核，未计稳定身份绑定 |
| 同上 | 4096 节点链 | 13449 → 5.975 μs/次 | 病态深链上界；游戏父树不按这种分布推算 |
| 全表期限扫描 → vector 堆 | 32 项，10000 次模拟唤醒 | 157.3 → 9.0 μs | 相同到期总结果，包含本次容器初始化 |
| 同上 | 1024 项，10000 次模拟唤醒 | 2208.2 → 136.9 μs | 只测期限管理，未包含真实 WakeSource/协程 |
| 实际完整发布 → 缓存 F+flags 公开查询快照 | 21 个地区 | 1.607 → 0.087 μs/次 | 相同 F/flags，消费载荷不同；包含分配/释放 |
| 同上 | 512 / 4096 个地区 | 85.465 → 4.073 / 810.432 → 239.525 μs | 大规模复制放大成本 |
| 普通优化 C++ → 手写 AVX2 | 21 个地区，每目标 4 入边 | 1.282 → 0.337 μs/轮，3.81× | 同一合成图，25 个有效类别；逐格结果相等 |
| 同上 | 128 / 1024 个地区 | 8.469 → 2.297 / 77.357 → 21.215 μs | 3.69× / 3.65× |
| 直接 L/R → 额外预计算 F | 21 / 128 / 1024 个地区 | 0.337 → 0.340 / 2.297 → 2.579 / 21.215 → 22.112 μs | 本组数据中持平或变慢 |

扩散探针使用有限的 0–150 输入、合成 retention 0.2–0.5、cutoff 5，每个目标四条入边。正常优化 C++ 作为性能基线，另保留关闭自动向量化的标量版本解释 SIMD 成本。期限探针的回调彼此独立，校验到期结果总和；地点归属和数值探针逐项比较完整输出。测量指标为热数据微基准耗时，游戏帧时间、cache miss 和 branch miss 待集成测量。

复跑：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/research/run-native-algorithm-probe.ps1
```

[探针源码](../../tools/research/native-algorithm-probe.cpp)、[运行脚本](../../tools/research/run-native-algorithm-probe.ps1)、[本次原始结果](../evidence/2026-10-28-native-algorithm-probe.csv)。构建日志与汇编在 `build/research/native-algorithm/`。本轮新增研究工具和资料，生产算法保持原状。

## 硬件与编译器依据

### 先缩短访问路径，再考虑更多指令

Intel 的数据布局建议强调对齐和紧凑组织，把一起使用的数据放在相邻空间。本项目将字符串、持久身份和诊断留在准备/发布阶段，数值内核遍历稠密 RegionId、连续数值行和数字边，以缩小工作集并缩短寻址路径。[Intel Optimization Reference Manual，第 3.6.5、3.6.10 节](https://cdrdv2-public.intel.com/821612/248966-Optimization-Reference-Manual-V1-050.pdf)

**本项目的推导：**按共同访问的数据安排连续空间。一条入边使用同一来源的 25 类，适合读取整行；地点查找只使用键和索引，适合独立紧凑索引。额外预计算数组的读写和更新成本，与直接读取 L/R 一起比较。

### 交错执行独立类别块

**本项目的推导：**`owner = parent[owner]` 每步地址依赖上步结果。构建阶段通过迭代记忆化生成最终归属，运行时直接索引。传播的各类别相互独立，可维护多个向量 max 累加器，交错加载来源、乘边权和比较，同时保留每类的入边顺序。通过汇编中的寄存器 spill 和代码体积确定展开量。[Intel Optimization Reference Manual，第 3.5.2、3.5.3、3.6.2–3.6.3 节](https://cdrdv2-public.intel.com/821612/248966-Optimization-Reference-Manual-V1-050.pdf)

业务继续使用单一执行器，在一个线程内交错执行独立类别块。

### 保留能够跳过整批工作的分支

Intel 说明，CMOV/SETCC 将控制依赖换成数据依赖，并可能增加路径计算量。容易预测且能跳过大量工作的分支适合保留。[Intel Optimization Reference Manual，第 3.4.1.1 节](https://cdrdv2-public.intel.com/821612/248966-Optimization-Reference-Manual-V1-050.pdf)

**本项目的推导：**在外层检查会话就绪、到期工作、地区保护和活跃类别，条件满足后再进入结算。传播内层的逐类门槛与 max 使用向量比较和掩码。“跳过零来源行”和“固定遍历整行”按实际稀疏度比较耗时。随机抽样保持玩法规定的调用次数、时点和顺序。

### 25 类补到 32 槽：对齐与尾槽是数据约定

256 位向量一次容纳 4 个 double。25 个有效值可用 7 次向量处理覆盖到第 28 槽，或按既有 32 槽约定处理 8 块；补槽必须实际属于分配对象，并在初始化/恢复后保持约定值。`_mm256_load_pd` 的源地址要求 32 字节对齐；`loadu` 解除该地址要求，不允许越过对象末尾。指令行为见 [Intel Intrinsics Guide](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html) 和 [Intel SDM Volume 2B，MOVAPD](https://cdrdv2-public.intel.com/835752/253667-sdm-vol-2b.pdf)。

**本项目的推导：**实际行类型使用 `alignas(32)` 或 `alignas(64)`，配合 256 字节步长保证行首对齐。32 槽以额外 7 个 double 换取规则步长和合法向量尾部；公开遍历、序列化和类别掩码使用 25 个真实类别。

### 数值指令使用相同的输入和舍入规则

`VMAXPD` 在 NaN 或两者均为零时按第二源操作数规则取值。本项目在输入边界验证有限值和合法范围，在同一值域核对标量与向量结果，有效类别和尾槽使用一致约定。[Intel SDM Volume 2B，MAXPD](https://cdrdv2-public.intel.com/835752/253667-sdm-vol-2b.pdf)

MSVC 的 `/arch:AVX2` 允许使用 256 位指令，也允许 FMA；但是否收缩表达式由浮点选项控制。FMA 只做一次舍入，可能与分开的乘法和加法不同；VS 2022 起 `/fp:precise` 默认不进行这种收缩。对门槛判断、整数公开值转换和标量对照，应先保留 `/fp:precise`；若比较 FMA 或 `/fp:fast`，另列误差与边界结果。[MSVC `/arch` (x64)](https://learn.microsoft.com/en-us/cpp/build/reference/arch-x64?view=msvc-170)，[MSVC `/fp`](https://learn.microsoft.com/en-us/cpp/build/reference/fp-specify-floating-point-behavior?view=msvc-170)

`_mm256_exp_pd` 由向量数学库实现，AVX2 的硬件运算由多条指令组成。现有公式若需要 `exp`，先将同一时间间隔和参数共有的系数移到类别循环外；该函数成为热点且误差要求明确后，再比较近似多项式、查表或数学库。[Intel Compiler Classic Guide，`_mm_exp_pd, _mm256_exp_pd`，SVML](https://www.intel.com/content/dam/develop/external/us/en/documents/cpp_compiler_classic.pdf)

### 按来源行连续加载，热点确认后评估预取

AVX2 gather 的用途是从非连续地址收集元素。对于本项目，边之间的来源索引可能离散，但一条边使用来源的连续类别行；先取来源行指针，再连续加载类别块，可避免为每个类别都做 gather。其优势是本项目访问模式推导，实际仍需比较。[MSVC x86 intrinsics list：gather 与 load](https://learn.microsoft.com/en-us/cpp/intrinsics/x86-intrinsics-list?view=msvc-170)

AMD 的优化资料将软件预取定位到遭遇缓存未命中的链式访问，并提醒过多预取可能挤出其他线程工作集，策略也随微架构不同。对当前小图和反复访问的数值行，不先加入手写预取或 non-temporal store；若后续计数器显示来源行加载确实等待内存，再比较预取距离和总耗时。[AMD Ryzen Processor Software Optimization，2023，第 22 页](https://gpuopen.com/download/GDC-2023-AMD-Ryzen-Processor-Software-Optimization.pdf)

### 编译结果和测量应回答具体问题

`/Qvec-report:2` 报告循环向量化结果和阻碍原因。性能比较使用 Release 和相同浮点选项，对照普通 C++ 与手写 intrinsics，再检查汇编中的加载、向量运算、分支、库调用及循环内 spill。[MSVC `/Qvec-report`](https://learn.microsoft.com/en-us/cpp/build/reference/qvec-report-auto-vectorizer-reporting-level?view=msvc-170)

**本项目的测量建议：**覆盖真实小规模、较大注册量、空/稀疏/密集行和规则边界。先测端到端查找、结算和发布，再测单个内核；记录机器、编译器、选项、规模、重复方式和相同输出检查。性能收益对照正常优化的 C++ 基线；关闭自动向量化的版本用于解释 SIMD 并行度。
