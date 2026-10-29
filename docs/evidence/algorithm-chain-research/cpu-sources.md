# 声望链 CPU / ISA 研究补充

日期：2026-10-28。研究范围是现有 C++23 单线程业务执行器中的数据依赖、类别位集合和数值内核。依据当前生产源码以及 Intel、AMD、MSVC 官方资料；本文给出选型和测量要求，本轮未改生产/测试代码，也未运行构建。

## 结论

执行次序建议：**先复用固定 CSR、编译热配置和减少重复搬运，再测外层早退、R 推进向量化和边描述布局；继续使用按来源整行连续 AVX2 加载。** 新指令无法消除字符串查表、重复编译或地址依赖造成的工作。

| 候选 | 当前选择 | 项目依据 |
|---|---|---|
| `std::countr_zero` + 清最低置位 | 保留 | `CategoryMask::ForEach` 已这样实现，循环次数等于集合大小，保持类别升序 |
| `std::popcount` | 可用于替换 `Count` 的 25 次检查 | 标准表达直接描述需求；检查实际 STL/编译目标生成的 fallback 或指令 |
| 强制 TZCNT / BLSR / POPCNT / PDEP | 当前无需增加专用分派 | 单个 25 位集合的普通整数操作足够；收益应包含分派和调用成本 |
| AVX2 连续 load/max/mul | 继续采用 | 一条边读取同一来源整行 25 类，类别方向已经连续 |
| AVX2 gather | 当前布局无需采用 | source 行之间离散，但每个 source 内类别连续；gather 不会让散开的行变成连续存储 |
| R 推进向量化 | 下一轮值得比较 | `RelaxRemoteRows` 当前忽略 backend，仍逐类执行分支 |
| FMA / `/fp:fast` | 当前公式保留 precise | incoming 是 max 后乘权，缺少融合乘加；改变舍入对当前门槛算法收益弱 |
| AVX512 | 保留 AVX2 发布目标 | 本机 i5-13600KF 的 Intel 规格列出 AVX2；新增叶函数需独立能力与性能验证 |
| 软件 prefetch | 等到来源加载确实等待内存时比较 | 默认图数值集小，复用高；预取也需要提前取得 source 索引 |
| non-temporal store | 当前 incoming / R 不采用 | incoming 立即被 R 推进读取，R 随后供传播、发布及查询使用 |
| 外层早退 + 内层向量 mask | 采用这一分层 | 无到期工作、空候选、受保护行可跳过整批；逐类阈值用比较/掩码 |

以上是项目推导，不是已测得的新增加速比。

## 当前代码的数据依赖与连续范围

- `E:/source/repos/SLSF Reloaded NG/include/slsf/core/NumericRow.h:17` 定义 `alignas(32) NumericRow`；32 个 double、256 字节步长，25 个业务值加 7 个实际补槽；`NumericRowBuffer` 为连续 `std::vector<NumericRow>`。
- `E:/source/repos/SLSF Reloaded NG/include/slsf/core/reputation/DiffusionGraph.h:15` 的 CSR 分成连续 `operands` 和 `inEdgeOffsets`。`Operand` 同时保存 `uint32_t source` 与 `double coefficient`；常见 Windows x64 布局下会有字段间填充，改 SoA 应先量 `sizeof` 和完整内核成本。
- `E:/source/repos/SLSF Reloaded NG/src/core/reputation/DiffusionKernelAvx2.cpp:20` 按 target 顺序遍历；每条边经 `operand.source` 定位 L/R，随后连续加载 7 组四 double。**连续的是 CSR 段、数值行及类别块，跨边 source 行依然可能跳跃。** 数字映射连续不代表图的访问顺序连续。
- 同一类别块跨入边有 max 累加依赖；7 个类别块彼此独立，CPU 可交错执行它们。继续检查 Release 汇编中的展开、寄存器分配和循环内 spill，避免单纯增加展开量。
- `E:/source/repos/SLSF Reloaded NG/src/core/reputation/DiffusionKernel.cpp:182` 的 R 推进当前逐 25 类执行，backend 参数未参与分派。`elapsed`、`retreat` 和 `cap` 已在行循环外计算。

Intel 的分析方法把缓存未命中、错误推测和执行资源瓶颈分开；链式间接寻址可能阻碍预取与流水执行。这里应分别测编译/查找/计算/发布，才能知道下一步应减少访问还是增加计算并行度。[Intel VTune Top-down Microarchitecture Analysis](https://www.intel.com/content/www/us/en/docs/vtune-profiler/cookbook/2024-1/top-down-microarchitecture-analysis-method.html)

### R 推进的候选表达

对有限的业务输入、非负 retreat，当前分支可写成下面的比较候选：

```cpp
destination = clamp(incoming, 0.0, cap);
result = max(destination, current - retreat);
```

推导：当 destination >= current 时，`current - retreat <= destination`，结果仍为 destination；其余情况正是现有退去分支。先让优化编译器处理这一平直循环，再比较手写 AVX2。保持原有 NaN/Infinity 与有符号零处理、25 类边界和补槽清零要求。当前底层函数对极端输入的行为也是候选对照的一部分。

`VMAXPD` 的 NaN/双零结果取决于第二操作数，不能直接把 `std::max`、比较选择和 vector max 任意交换。TZCNT 在零输入时返回操作数宽度，旧 BSF 在零输入时的目标值未定义；当前 `ForEach` 已先检查非零，因此可由编译器选择适合目标的 bit scan。[Intel SDM Volume 2B，MAXPD 与 TZCNT](https://cdrdv2-public.intel.com/835752/253667-sdm-vol-2b.pdf)

## 位集合与能力边界

`E:/source/repos/SLSF Reloaded NG/include/slsf/core/CategoryMask.h:62` 的 `Count()` 仍逐位检查；`E:/source/repos/SLSF Reloaded NG/include/slsf/core/CategoryMask.h:73` 的 `ForEach` 已为置位枚举。`std::popcount(bits_)` 可直接表达数量；`std::countr_zero` 表达最低置位位置。标准接口保证语义，不等于保证特定机器指令。[MSVC `<bit>` functions](https://learn.microsoft.com/en-us/cpp/standard-library/bit-functions?view=msvc-170)

| 指令族 | 独立 CPU 条件 | OS 状态条件 |
|---|---|---|
| POPCNT | CPUID(1).ECX[23] | 通用寄存器，无 YMM 状态要求 |
| BMI1：TZCNT、BLSR 等 | CPUID(7,0).EBX[3] | 通用寄存器，无 YMM 状态要求 |
| BMI2：PDEP/PEXT 等 | CPUID(7,0).EBX[8] | 通用寄存器，无 YMM 状态要求 |
| FMA | CPUID(1).ECX[12] | AVX 的 OSXSAVE/XMM/YMM 条件 |
| AVX2 | CPUID(7,0).EBX[5]，以及 AVX / OSXSAVE | XCR0[2:1] = 11b |
| AVX512F | CPUID(7,0).EBX[16]；使用 VL/DQ/BW 等另查各自位 | XCR0[7:5] = 111b，同时 XCR0[2:1] = 11b，即 `(XCR0 & 0xE6) == 0xE6` |

各 CPU feature 位见 [MSVC `__cpuid` / `__cpuidex` 示例](https://learn.microsoft.com/en-us/cpp/intrinsics/cpuid-cpuidex?view=msvc-170)；AVX/AVX512 的 OS 状态检查见 [Intel SDM Volume 1，AVX/AVX512 Detection](https://cdrdv2-public.intel.com/843827/253665-sdm-vol-1-dec-24.pdf)。**AVX2 的单个位不蕴含 BMI1、BMI2、POPCNT 或 FMA 的独立位。**

当前 `E:/source/repos/SLSF Reloaded NG/src/core/reputation/DiffusionKernel.cpp:20` 只选择 AVX2 路径，检查 OSXSAVE、AVX、XCR0 0x6 和 AVX2。若加入新的强制 intrinsic，分派应覆盖其实际能力。通常让标准位函数在基线代码中生成合适实现即可，不需要为了一个 25 位 Count 建立新的全局 CPU 等级。

还需核对编译目标：MSVC `/arch:AVX2` 允许 FMA，并可能生成某些 BMI 指令。即使源码没有显式 BMI/FMA intrinsic，最终对象仍应与运行时能力检查对应。现有 precise 选项限制浮点收缩，但不限制整数指令选择。[MSVC `/arch` x64](https://learn.microsoft.com/en-us/cpp/build/reference/arch-x64?view=msvc-170)

## 各 ISA 候选的适用性

### gather 与跨边并行

gather 从由向量索引给出的非连续地址收集元素。[Intel 对 gather 的官方说明](https://www.intel.com/content/www/us/en/developer/articles/technical/software-security-guidance/technical-documentation/gather-data-sampling.html) 与 [MSVC gather intrinsic 列表](https://learn.microsoft.com/en-us/cpp/intrinsics/x86-intrinsics-list?view=msvc-170)确认该用途。

本项目若改成“一个向量放多个来源的同类别”，会额外需要来源索引向量、L/R 两次 gather、边权向量和横向 max；之后再处理其余 24 类。现有“一个向量放同来源的四类别”直接 load，已经匹配 AoS 行布局。只有显著稀疏类别、足够高入度且跨边并行获益时，gather 才值得单独探针比较。不能仅因 source 索引离散就认为 gather 会更快。

### FMA、快速数学与时间系数

incoming 当前是 `max(L,R) * coefficient` 再取最大，没有可融合的加法。R 推进的 `relaxationPerDay * elapsed` 已整批算一次。因此优先移出循环/复用参数，FMA 不是这两个内核的主要机会。

FMA 只有最终一次舍入，与先乘后加可能不同；VS2022 的 `/fp:precise` 默认不收缩。保留 precise，若未来某个真实热点存在乘加公式，单独比较 FMA 的阈值两侧、整数公开结果和完整批次成本。[MSVC `/fp` behavior](https://learn.microsoft.com/en-us/cpp/build/reference/fp-specify-floating-point-behavior?view=msvc-170)

### AVX512 与本机

Intel 官方 i5-13600KF 产品页列出 6 个 P-core、8 个 E-core、20 线程及 AVX2；它适合继续验证当前 AVX2 基线。AVX512 候选需要另一台具备对应 CPU/OS 能力的机器，不能从当前机器推算加速比。[Intel i5-13600KF specifications](https://www.intel.com/content/www/us/en/products/sku/230494/intel-core-i513600kf-processor-24m-cache-up-to-5-10-ghz/specifications.html)

AVX512 对 25 double 可减少向量块数并提供原生 lane mask；收益还取决于循环、内存、频率与实际核心。当前更有价值的是先消除每次编译 CSR 等固定开销。保留现有 AVX2 发布选择。

### prefetch、non-temporal 与分支

AMD 的软件优化资料将手动预取用于存在 cache miss 的链式数据，并提醒过多预取会挤出其他线程的缓存；其硬件预取器可处理顺序/步长模式。[AMD Ryzen Software Optimization，22–26 页](https://gpuopen.com/download/GDC-2023-AMD-Ryzen-Processor-Software-Optimization.pdf)

项目推导：CSR 边数组适合顺序读取；source 行地址需先加载 `operand.source`，若大图测出来源 miss，再比较提前若干条边预取 L/R 行。记录预取距离、总执行时间与 miss 变化，不能只看计数器下降。incoming 马上被读取，R 反复复用，普通缓存写更匹配当前数据寿命。non-temporal store 适合另行验证的大块一次性输出，它也引入写组合/顺序约束。[Intel SDM Volume 2B，MOVNTPD](https://cdrdv2-public.intel.com/835752/253667-sdm-vol-2b.pdf)

Intel 说明，CMOV/SETCC 用数据依赖替换控制依赖，可能执行两路工作并约束乱序引擎。因此保留能跳过整批/整行且容易预测的分支；逐类 max、阈值和夹取适合向量比较。**早退以业务条件为单位**：无到期工作先返回、空类别先跳过、地区保护先跳整行。L 为零仍可能有 R 退去或其他来源输入，不能用它代表整条传播链无事可做。[Intel Optimization Manual Documentation Changes，3.4.1.1](https://cdrdv2-public.intel.com/814201/355308-Software-Optimization-Manual-048-Changes-Doc.pdf)

## 下一轮可执行验证

1. 分开测完整 Advance、incoming 内核、R 推进、发布；先记录 CSR 复用前后端到端成本，避免局部指令收益被准备开销淹没。
2. R 推进比较当前实现、平直普通 C++、独立 AVX2 叶函数；使用 `/O2 /fp:precise`、相同输入和输出检查。查看 `/Qvec-report:2` 以及实际汇编，确认自动向量化、循环内 spill 和尾部处理。[MSVC `/Qvec-report`](https://learn.microsoft.com/en-us/cpp/build/reference/qvec-report-auto-vectorizer-reporting-level?view=msvc-170)
3. 位集合覆盖 0/1/2/12/25 个置位、固定和随机分布；区分只求数量、只判非空、枚举类别。只判非空直接 `bits != 0`，不先求 popcount。
4. 图覆盖默认 21/78、128、512、1024 地区，入度 0/1/4/高入度，source 相邻/随机，空/稀疏/密集数值。测热数据和扩大工作集的情况；CPU、核心类型、编译器、选项、seed1170、重复数与中位数一起记录。
5. 13600KF 的 P-core 与 E-core 分开测，避免线程迁移把不同内核结果混成一个中位数；业务执行器继续单线程。硬件计数器能用时，分别看 branch miss、缓存等待、前端/后端瓶颈；先以总耗时决定采用结果。
6. 数值检查覆盖 cutoff 上下相邻值、0/负零、极大有限值、原函数实际支持的特殊值、25 业务格、7 补槽与公开整数值。随机抽样次数和顺序由原业务入口保持，SIMD 只替换已确定的数据计算。

本轮交付到此：源码读取与主来源核实完成；候选收益由下一轮生产入口探针给出。
