# 04 组审核复放材料

日期：2026-10-29。主报告：[04 组独立审核与逐项对抗验证](../2026-10-29-04-independent-review.md)。

审核对象固定为 `2b3912e81d62b4a26dbe43b20b92e18f5ce522cf`，相对初始化提交 `5222a3c31841426bb26c3635a37638abb8f707e5` 审查，另以 03 组树 `28b4160cbba5afc84ce869d3f990648510db74fa` 识别增量。生产源码、原测试、任务书、真实 Git index 与 HEAD 保持原状。

## 独立报告

- [Standards、内存、堆栈与线程](standards-memory.md)：规范违规 0；可选重复逻辑 1；未发现已证实的内存、栈或线程缺陷。
- [Spec 初审](spec.md)：原始规格判断，与最终裁决对照阅读。
- [算法初审及修正](algorithms.md)：A1 已撤回；A2 为 readiness 精度观察。
- [fresh 逐项对抗报告](adversarial.md)：核对原脚本完整分支，记录误报撤回、严重度调整和安全结论的覆盖范围。

最终裁决以主报告为准：B04 实机验收为 P2 缺口；DD 状态保留与 AND Unknown 精度各为 P3。原始 Spec 报告保留其初审判断，便于追踪对抗前后的变化。

## 归档源码复放

在 Git Bash 中运行：

```bash
cd 'E:/source/repos/SLSF Reloaded NG'
bash docs/evidence/review-04/replay-probes.sh
```

[统一脚本](replay-probes.sh)直接编译本目录的五份探针源码，并链接当前工作区的生产实现。需要 PATH 中的 Clang C++23 编译器。二进制和新日志写入 `build/diagnostics/review-04/archive-replay/`；本目录的首轮日志保留。

| 探针源码 | 检查内容 | 首轮结果 |
| --- | --- | --- |
| [memory-layout.cpp](memory-layout.cpp) | 固定对象的 sizeof/alignof；不测栈峰值 | 编译、执行成功 |
| [algorithm-gameplay-probe.cpp](algorithm-gameplay-probe.cpp) | A1 的修正 control 与 A2 三种 AND 安装事实 | [日志](algorithm-gameplay-probe.log) |
| [algorithm-performance-probe.cpp](algorithm-performance-probe.cpp) | 候选选择分配量及运行时 mask 查询成本 | [日志](algorithm-performance-probe.log) |
| [adversarial-heavy-bondage.cpp](adversarial-heavy-bondage.cpp) | 独立确认 heavy 分支与旧脚本一致 | [日志](adversarial-heavy-bondage.log) |
| [adversarial-body-tattoo-unknown.cpp](adversarial-body-tattoo-unknown.cpp) | 独立执行 AND=true/false/unknown | [日志](adversarial-body-tattoo-unknown.log) |

[根代理从归档源码复放的完整日志](root-replay.log)记录编译器、命令、输出和退出码。性能数字属于单机局部测量，复放耗时随调度变化；判断语义与构建结果时使用输出值和退出码。

同时保留各代理最初使用的同名 `.sh`，用于核查原执行方式；这些脚本引用诊断目录。直接从本目录源码复放时使用上面的统一脚本。

## 全套检查与固定范围

- [existing-checks.log](existing-checks.log)：普通与 ASan 核心各 178 cases / 120451 assertions；普通与 ASan 边界各 4 / 22；差分 8 / 17161；fuzz 100000 次，seed 1170。ASan 的故意失败探针按预期报告 heap-buffer-overflow。
- [target-probe.log](target-probe.log)：EngineProbe 与插件构建通过，`STATIC_PASS`；真实游戏查询仍为 pending。
- [junit-summary.txt](junit-summary.txt)：核心 XML 的 268 个展开节点与 Catch2 的 178 个 cases 分开统计，无重复节点、failure 或 error。
- [contracts.log](contracts.log)、[task-books.log](task-books.log)：合同负例 11/11 拒绝；11 组、25 任务、37 CPP 唯一归属及 101 链接通过。
- [baselines.txt](baselines.txt)、[review-tree.txt](review-tree.txt)：固定基线与审核树。
- [source-unchanged.log](source-unchanged.log)：审核收尾核对固定源码树。

全套检查的命令保存在 [run-checks.sh](run-checks.sh) 和 [verify-results.sh](verify-results.sh)。这些脚本调用现有项目测试工具；统一探针脚本无需运行 xmake。
