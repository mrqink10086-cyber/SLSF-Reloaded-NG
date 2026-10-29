# 原生目击检测与时间调度优化评估

日期：2026-10-26。本文记录本地检测入口及 CPU 优化候选。当前需求与实施状态见[项目规格](../../.scratch/native-migration/spec.md)；引擎检测行为由 B04 在 1.6.1170 验证。

已选实现为固定图持续传播、单执行器和 AVX2，详见[传播结构](../superpowers/specs/2026-10-26-weighted-reputation-diffusion-design.md)。下文随机传播优化和后台计算是早期评估记录。

## 已表达的方向

- 不再保留 MasterClock 的两秒启动延迟和“两个十五分钟累计为半小时”的中间层。
- 根据 SKSE 生命周期消息与实际就绪状态初始化/恢复服务。
- 对仍有业务必要的短时等待，用短协程表达顺序流程；保留相应的游戏时间、现实时间、菜单计时语义。
- 装备刷新一类防抖/节流使用回调与事件合并，不照搬一秒 latent 等待。
- 替换施放 NPC 扫描法术并等待 Papyrus 回调的检测链路，优先利用引擎的检测数据和原生查询。
- 执行器负责对象与协程生命周期，跨线程读写通过约定的同步入口完成。

## 1. 随机性与数据访问可以分开优化

旧脚本主要使用数组和字符串查找。优化先处理重复查找、重复构建和随机重试：

| 旧模式 | 建议的新方式 |
|---|---|
| 用字符串决定地点，再逐个数组搜索 | API 边界转数字身份，内部直接访问连续记录 |
| 按城市/领地展开大量 if 分支重建传播候选 | 基于地点注册表/归属规则缓存候选集合，变更时失效 |
| 抽到传播源自身就反复重抽 | 在候选中排除源，直接均匀选有效目标；候选为空直接结束 |
| 为避免重复类别反复抽样并最多尝试 50 次 | 对有效类别做无放回抽样，例如部分 Fisher–Yates；数量不超过有效类别数 |
| 对没有名望、无法传播的地点仍做全量业务检查 | 维护有效地点/类别集合，按变化更新 |
| 每个写入重复生成大量兼容输出 | 在明确的事务边界合并脏数据输出，并保留对外读取一致性 |

这组无放回抽样方案是早期比较项。当前自动传播采用固定图；其他沿用旧抽样的入口保留候选顺序、最多 50 次尝试和不足额结果。旧多轮传播逐次更新概率与数值。

## 2. 早期并行方案评估

第一版采用单一业务执行器。以旧版 21 个默认地区、最多 21 个自定义地区和 25 类为起点，先优化候选及内存布局，再测量实际热点。

| 工作 | 建议 |
|---|---|
| 获取角色、检查加载/战斗状态、调用检测/LOS、写 RE 对象 | 由允许访问相应引擎数据的执行阶段完成，通常安排在游戏主线程；不放进通用并行循环 |
| 修改名望、传播概率、保护截止时间、提交目击结果 | 明确顺序和事务边界，避免多个线程并发修改同一状态 |
| 纯数值的大批量离线补算、纯数据索引构建 | 有足够规模且输入可冻结时，可以评估后台执行 |
| 5/10 秒等等待 | 挂起任务，不为每个等待创建线程，也不使用忙等 |

早期后台方案要求主线程准备普通值快照，任务生成结果后按会话、索引代次和输入版本提交，冲突时重算；随机源按稳定任务身份分配。当前业务采用下述单执行器方案。

旧原地更新存在遍历依赖。当前模型使用单执行器读取冻结快照、统一提交；同一批的中转从下一步发生。

## 3. 生命周期、业务周期和短等待

本地 `E:/source/repos/skse64/skse64/PluginAPI.h:195-211` 已明确：

- DataLoaded：表单数据加载完成，准备静态记录和注册服务；存档与玩家数据另按会话就绪条件开放。
- PostLoadGame：一次加载尝试完成，须检查成功标志。
- NewGame：新游戏已创建，但消息注释明确仍在游戏加载完成之前。

因此用实际生命周期/就绪条件替代固定两秒等待。进入可用会话后，衰减、传播、匿名状态等按自己的业务截止时间登记，不再依赖十五分钟计数器。

加载消息处理会话切换，统一时钟入口检查下一到期时间，并处理睡眠、等待和快速旅行引起的跳时。队列为空时跳过业务扫描，有任务时只推进已到期项。后续调度任务负责接入并验证主循环。

短协程按顺序表达等待流程，执行器负责唤醒、取消、异常和帧生命周期。外部操作通过完成事件或完成结果恢复，玩法宽限期则按规定时钟等待。

装备事件标记 dirty，最多排一个刷新回调，在允许访问游戏对象的阶段统一处理。处理期间再次变化时保留 dirty 并补一轮。需要延迟的回调由统一调度器管理。

## 4. 已查明的原生检测入口

下表来自本地 CommonLibSSE-NG，列出 B04 的 probe 入口及源码定位。

| 接口/结构 | 可以提供什么 | 本地位置 |
|---|---|---|
| ProcessLists::ForEachHighActor / highActorHandles | 高处理级角色候选，避免枚举整个世界 | include/RE/P/ProcessLists.h:42-48,72；src/RE/P/ProcessLists.cpp:45-53 |
| Actor::RequestDetectionLevel(target, priority) | 有方向的观察者到目标的检测查询 | include/RE/A/Actor.h:672；src/RE/A/Actor.cpp:1259-1264 |
| Actor::HasLineOfSight(ref, arg2) | 原生 LOS 查询 | include/RE/A/Actor.h:608；src/RE/A/Actor.cpp:829-834 |
| ProcessLists::RequestHighestDetectionLevelAgainstActor | 返回聚合检测级别和 LOS 数量；逐个观察者使用下方缓存查询 | include/RE/P/ProcessLists.h:48 |
| HighProcessData::knowledgeArray / knowledgeLock | AI 角色知识缓存及容器锁 | include/RE/H/HighProcessData.h:252-253 |
| ActorKnowledge::owner / target / detectionState | 区分观察方向，并关联检测状态 | include/RE/A/ActorKnowledge.h:28-35 |
| DetectionState::level / hasLOS | 缓存的检测级别与 LOS 字段 | include/RE/D/DetectionState.h:17-18 |

`DetectionListener` 的方法仍标为 Unk_02/03/04，当前可用路线是取得具体候选后查询检测/LOS。

缓存缺项或尚未更新时返回 unknown，暂停依赖该事实的新计分。probe 核对读取阶段、锁协议和检测/LOS 的实际返回语义。

## 5. Devious Curses 中已有的对应实现

`E:/source/repos/devious-curses-main/src/Utils.cpp:238-278` 的 GetWatchingActors()：

1. 遍历 ProcessLists::aliveActorList，解析角色 handle。
2. 按加载、死亡、年龄、队友、生物、距离和角度等条件筛选。
3. 以观察者调用 `RequestDetectionLevel(target, kCritical)`。
4. 调用 `HasLineOfSight(target, ...)`，以 detection > 0 且 LOS 为该函数的目击条件。

该实现先取得角色候选，再按观察者到玩家的方向查询检测和视线，可替代扫描法术。

SLSF 使用自己的筛选规则、查询优先级及 NPCNeedsLOS 距离例外；旧扫描法术的有效范围结合 ESP 配置确认。

## 6. 目击更新频率和声望增长频率分开

推荐让原生检测生成当前观察者/目击上下文，替代 NPCScan 法术及共享成功 bool；相关的场景、装备、地点等事件触发刷新。有持续目击业务时可按预算刷新候选或检测结果，没有此需求时不做无条件全量扫描。

计分方向本轮已经确定，检测接口与覆盖面仍须实施验证：

- 检测刷新与计分分开：装备、视线和 P+ 阶段变化更新事实，声望按原增长节奏或等价预算计分，每个计分机会只抽样和提交一次。
- 潜行眼睛是汇总状态，无法给出具体观察者的关系、距离和 SexLab 状态；SLSF 仍需要观察者身份。
- 分别记录听觉检测、AI 检测级别、视线和行为条件，按各计分入口组合判断目击。
- 用户已接受跳时只推进传播、衰减、匿名记忆，不用醒来时的 NPC 名单补造过去目击。增益只处理实际记录的有效目击；无目击时段不积攒可在醒来时一次性兑换的多轮增长额度。
- B04 验证视线和检测变化的通知覆盖；有稳定通知时按事件刷新，其余变化按低成本周期刷新。

## 7. 候选构建与 CPU 指令级优化评估

本节保留优化候选。引擎 Location 数量 N、声望区域数量 R、25 个类别分别编号；当前 L/R 数值行使用 32 个 double 槽。下文 int32 类别掩码示例针对资格筛选。

### 7.1 把游戏查询集中到构建边界

- 在允许访问引擎的阶段，取得记录身份、父记录身份和规则字段，转换为连续普通值；SIMD 批处理使用这些已整理的数据。
- 建立父子森林、DFS 区间和归属缓存，避免每个候选都重新走父链。父子拓扑本身只有线性数量的边，无需构造 N×N 关系；规则解析成本仍需单独计入。
- 静态传播规则生成地区或规则组掩码，在加载、地区注册或策略修改时更新。
- 维护每个地区的动态类别资格掩码。单值变化更新对应位，全局阈值变化时批量重建。资格按实际入口条件计算，零声望目标也可接收传闻。
- 完整初建资格仅需处理 R×25 个内部数值。按旧脚本 42 个区域计算是 1,050 个值；全部游戏 Location 的归属构建是另一项工作。

### 7.2 单机器字集合与抽样

25 个类别的资格可放入 uint32_t；按旧脚本最多 42 个声望区域，目标集合可放入 uint64_t。未来超过 64 个区域使用多机器字集合，不将 64 变成新的业务上限。

```cpp
targets = ruleTargets[source] & registeredTargets & ~sourceBit;
categories = thresholdEligible[source] & enabledCategories;
```

这里只表达集合关系；完整业务还需处理已有的概率、保护条件和配置。目标选择先检查非空，再从有效位中均匀选择。类别每选一个便清掉相应位，实现无放回抽样。

- std::popcount / std::countr_zero 表达计数和位定位；编译器是否生成 POPCNT / TZCNT 等指令取决于目标与代码生成。零集合必须在抽样前处理。
- BMI2 的 PDEP 可以把 `1ull << rank` 沉积到有效掩码的第 rank 个置位位置，再用位定位取得编号，避免逐位寻找该排名；前提是 rank 小于有效位数。按最新指令集决定直接纳入这一实现，不另维护候选数组/普通位循环回退；仍需记录目标 CPU 的执行成本，不假定所有 CPU 上同速。
- 单个 32/64 位集合的 AND/OR/清位直接使用整数运算；跨多个机器字的大集合才是 SIMD 按位批处理的候选。

### 7.3 适合 SIMD 的批量数值内核

| 内核 | 可评估的方式 | 约束 |
|---|---|---|
| 批量重建声望类别资格 | AVX2 一次比较 8 个 int32_t，再提取比较位生成类别掩码 | >= 通过 > 与 == 合并，完整处理 Int32 边界 |
| 大量地点的区间/属性筛选 | 对连续 tin、归属 ID、属性列做 SIMD 比较 | 树构建和指针解析单独处理；单点查询使用已有 O(1) 索引 |
| 大型候选位集合求交/差 | AVX2 按 256 位执行 AND/OR/ANDNOT 等 | 尾部、分配范围及掩码编号必须正确 |
| 目击候选的距离粗筛 | 主线程取得位置等普通值后，对连续坐标批量算距离平方 | LOS/检测查询在允许访问引擎对象的阶段执行 |

int32 资格比较可处理 3 组完整向量，再处理剩余一项；若使用 4 组向量，缓冲区应分配 32 个元素并屏蔽高 7 位。当前 double 声望行已分配 32 槽。单值变化直接更新对应资格位。

### 7.4 调度、兼容性和验证

- 先使用连续、少分支的纯数值循环，再对实测热点比较 intrinsics；通过汇编确认生成的向量指令。
- 按用户最新决定，仅维护 AVX2 实现，并允许 POPCNT、BMI1/BMI2；不另写低指令集算法或运行时多版本分派。编译配置与所用 intrinsics 明确表达这些要求。入口可检查 CPU 特性及 OSXSAVE/XGETBV 的 XMM/YMM 状态支持；不满足时报告不支持并退出加载，不进入回退算法。若全局使用 /arch:AVX2，检测入口及检测前的初始化必须避免提前执行所需扩展指令。
- 用连续存储、预分配和预解析缩短访问路径。父指针依赖链通过预先生成归属结果消除；地址可提前获知且内存加载成为热点时，再测量手动预取。
- 纯数据构建可以在足够规模时评估后台批处理；引擎读取与业务提交保持受控。只读索引按完整快照发布，业务写入由明确所有者执行，避免为了 SIMD 为每个字段增加原子读写或让线程争抢同一掩码。
- 通过已知输入、边界样例和测试侧参考计算校验结果。测量启动构建、日常更新和查询耗时，覆盖不同规模及冷/热缓存；需要时用硬件计数器区分分支预测、内存延迟和执行资源瓶颈。

依据：[MSVC x64 intrinsics](https://learn.microsoft.com/en-us/cpp/intrinsics/x64-amd64-intrinsics-list?view=msvc-170)、[Intel PDEP/BMI2 示例](https://www.intel.com/content/www/us/en/developer/articles/technical/resetting-the-lowest-n-set-bits.html)、[MSVC /arch](https://learn.microsoft.com/en-us/cpp/build/reference/arch-x64?view=msvc-170)、[Intel AVX 状态支持要求](https://cdrdv2-public.intel.com/814201/355308-Software-Optimization-Manual-048-Changes-Doc.pdf)、[Intel Top-down 分析](https://www.intel.com/content/www/us/en/docs/vtune-profiler/cookbook/2024-1/top-down-microarchitecture-analysis-method.html)。

### 7.5 已确定的指令集与映射生命周期

目标范围为 Ryzen 5000、第 12 代 Intel Core 及后续同类处理器，数值实现使用 AVX2，POPCNT、BMI1 的 TZCNT/清位及 BMI2 PDEP 可按热点采用。启动检查分别验证实际使用的 CPU 特性和操作系统向量状态支持。

| 特性 | Ryzen 5000 | 第 12 代 Intel Core | 用途 |
|---|---|---|---|
| AVX2 | 支持 | 支持，含 P/E 核 | 连续数值、批量位集合 |
| 标量 POPCNT | 支持 | 支持 | 有效位计数；不要求 AVX-512 的向量 popcount |
| TZCNT（BMI1）；BSF/BSR | 支持 | 支持 | 位定位；BSF/BSR 的零输入边界须另处理 |
| PDEP（BMI2） | 支持 | 支持，含 P/E 核 | 按随机排名选择有效位 |

PDEP/BMI2 在 Intel Haswell（第 4 代 Core）、AMD Excavator（bdver4）及第一代 Ryzen Zen 起可用。Zen 3 的 PDEP 使用原生 ALU，Zen/Zen+/Zen 2 较慢。Ryzen 5000 移动系列中的 5300U、5500U、5700U 使用 Zen 2；本项目采用同一实现，按实际微架构比较执行成本。

地点记录的静态 `FormID ↔ DenseID`、数字父子拓扑与基础字段在每次游戏启动、记录可用时构建；同进程切换存档复用。保存状态、存档自定义区域及动态记录单独绑定和失效处理。内部查询与声望计算只访问数字记录；确需访问游戏对象时才通过反向表解析，单纯修改本插件声望不需要解析回游戏对象。详细生命周期见 [地点设计](../superpowers/specs/2026-10-26-location-index-design.md)。

硬件依据：[GCC 架构特性表](https://gcc.gnu.org/onlinedocs/gcc/x86-Options.html)、[AMD 提交的 bdver4 AVX2/BMI2 支持](https://gcc.gnu.org/legacy-ml/gcc-patches/2013-11/msg01237.html)、[Intel BMI2 引入说明](https://www.intel.com/content/www/us/en/support/articles/000102715/processors/intel-xeon-processors.html)、[LLVM Zen 2 调度模型](https://github.com/llvm/llvm-project/blob/main/llvm/lib/Target/X86/X86ScheduleZnver2.td)、[LLVM Zen 3 调度模型](https://github.com/llvm/llvm-project/blob/main/llvm/lib/Target/X86/X86ScheduleZnver3.td)、[Lenovo Ryzen 5000 移动架构表](https://www.lenovo.com/gb/en/faqs/amd/amd-consumer-laptops/)。LLVM 模型用于交叉核查实现特征，不视为本插件的实测结果。

## 当前结论

数字 ID、连续布局、位集合、缓存和 AVX2 可按实际热点采用。当前执行边界、计分与刷新节奏、跳时和传播行为统一见项目规格；实现时保留上述接口定位，记录目标运行时 probe 与性能结果。
