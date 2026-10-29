# 整链算法研究证据

日期：2026-10-28。主报告为[地点、配置、时间与传播整链优化](../../research/native-chain-preparation-and-layout.md)。本轮新增研究文档和独立探针；生产源码、现有测试及 Xmake 配置保持本轮研究开始时的内容。

| 材料 | 内容 |
|---|---|
| [places.md](places.md) | 默认 21 地区、小镇、自定义子空间与独立城市、owner 与图绑定、运行期接线情况；附原始脚本和 C++ 行号 |
| [cpu-sources.md](cpu-sources.md) | Intel、AMD、MSVC 官方资料；分支、位集合、AVX2、其他 ISA 与编译目标 |
| [chain-probe.md](chain-probe.md) | CSR 复用、custom 成员查询、实际布局、复制和类别枚举的测量方法与结果 |
| [chain-probe.cpp](chain-probe.cpp) | 调用生产入口的独立探针 |
| [chain-probe.sh](chain-probe.sh) | 从归档源码编译到 build 诊断目录，只有 AVX2 内核单元使用 `-mavx2` |
| [chain-probe.log](chain-probe.log) | 本次原始结果，checksum 为 2242174067907451490，最终 PASS |
| [chain-probe-replay.log](chain-probe-replay.log) | 使用归档源码和脚本重新编译执行的结果，checksum 相同，最终 PASS |

在仓库根目录的 Git Bash 执行：

```bash
bash docs/evidence/algorithm-chain-research/chain-probe.sh
```

脚本使用本机 `C:/Program Files/LLVM/bin/clang++.exe`，需要可用的 Windows C++ 工具链；输出写入 `build/diagnostics/algorithm-chain-research/`。归档日志保留本次结果，新执行日志写入 build。该探针直接链接当前生产源码，因此后续代码变化后的数字属于新的测量。

本次使用 i5-13600KF、Clang 20.1、优化构建、7 轮中位数，未固定 P/E 核或频率。数据为预热后的局部阶段，完整 `Advance`、引擎入口和提交/发布由集成基准测量。完整 State 与 L/R 快照承担不同工作，复制数字按各自载荷解读。

归档复跑的 21/128/512 节点组分别为 4.240→0.295、39.329→2.184、230.366→15.319 μs；对应局部比值 14.37×、18.01×、15.04×。512 节点复用组与首轮 8.674 μs 有明显波动，后续采用性能阈值前应固定执行核心并测完整路径。
