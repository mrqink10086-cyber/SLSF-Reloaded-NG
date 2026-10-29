# 01–03 审核证据

结论及最终优先级见[统一报告](../2026-10-28-01-03-independent-review.md)。本目录保存五份独立报告、实际生产入口的探针源码、构建脚本和最终执行日志，不保存二进制。

| 文件 | 内容 |
|---|---|
| [standards-memory.md](standards-memory.md) | 规范、12 类 Fowler 基线、所有权、借用、边界、栈及池/arena 清点 |
| [spec.md](spec.md) | 任务要求、实现和 01–03 交付主张核对 |
| [algorithms.md](algorithms.md) | 时间边界反例、固定图编译及自定义地区查找成本、排除的候选问题 |
| [adversarial.md](adversarial.md) | 对规格、内存、交付矩阵和既有六项修复逐项反证 |
| [algorithm-counter.md](algorithm-counter.md) | 对算法报告逐项反证、优先级及影响范围收窄 |
| [existing-checks.log](existing-checks.log) | 本轮完整 all-mode 检查，含核心、边界、ASan、fuzz、差分及基准 |
| [junit-summary.txt](junit-summary.txt) | 展开节点唯一性、断言数、failure/error 汇总 |
| [contracts.log](contracts.log)、[task-books.log](task-books.log) | 合同反例和任务书校验 |
| [memory-probe.cpp](memory-probe.cpp)、[memory-probe.log](memory-probe.log) | 借用输入销毁后读取、精确缓冲 SIMD、全部截断前缀及 ASan+UBSan 输出 |
| [algorithm-public-probes.cpp](algorithm-public-probes.cpp)、[algorithm-cost-probes.cpp](algorithm-cost-probes.cpp)、[algorithm-probes.log](algorithm-probes.log) | 有限时刻反例、重绑、特殊值和实际入口成本的最终输入/输出 |
| [adversarial-probe.log](adversarial-probe.log) | 重编译当前归档的单项写入探针，四种地区规模各 100 万次写入 |

## 从 Git Bash 复放探针

项目使用本机 Clang 20、MSVC 标准库与 Windows x64 工具链。脚本按本项目路径构建，AVX2 只用于对应内核翻译单元。先将探针输入放回脚本的构建目录：

```bash
cd '/e/source/repos/SLSF Reloaded NG'
mkdir -p build/diagnostics/review-01-03
cp docs/evidence/review-01-03/{memory-probe.cpp,algorithm-public-probes.cpp,algorithm-cost-probes.cpp} build/diagnostics/review-01-03/
bash docs/evidence/review-01-03/memory-probe.sh > build/diagnostics/review-01-03/memory-replay.log 2>&1
bash docs/evidence/review-01-03/algorithm-build-run.sh > build/diagnostics/review-01-03/algorithm-replay.log 2>&1
bash docs/evidence/review-01-03/adversarial-run.sh
```

内存脚本见 [memory-probe.sh](memory-probe.sh)，算法脚本见 [algorithm-build-run.sh](algorithm-build-run.sh)。[adversarial-run.sh](adversarial-run.sh) 直接编译 01/02 的[归档单项写入源码](../review-01-02/algorithm-single-write-probe.cpp)，并记录源码 SHA-256。

算法探针退出 0 表示观察结果已重现；`queued_fame=9` 对 `settle_then_apply_fame=10` 是保留的错误证据。成本数据是本机定向测量，初始化在计时外；自定义地区比较包含整个 Advance 的状态复制成本。历史报告中的优先级和条件性结论以统一报告的对抗后处置为准。
