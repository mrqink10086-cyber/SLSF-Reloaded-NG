# 05 审核材料与复放

主报告：[05 独立审核](../2026-10-29-05-independent-review.md)。固定工作区树 `4c72b3da398994ae3e1c0d4567545ee7c1713696`，初始化基线及审核期间 HEAD 为 `5222a3c31841426bb26c3635a37638abb8f707e5`。

## 独立审核

- [Standards、所有权、堆栈和线程](standards-memory.md)：完整 Fowler 检查及安全分析。
- [Spec](spec.md)：B01/B02 需求追溯。
- [算法与局部测量](algorithms.md)：实际入口反例、队列与到期检查成本。
- [fresh 对抗](adversarial.md)：逐项确认、收窄和撤回，直接读取源码并独立构造输入。

## 归档入口

- [algorithm-probe.cpp](algorithm-probe.cpp)：Execute/排队时间顺序、跨会话回调、取消回收，以及相同 payload 队列的局部对照。
- [adversarial-probe.cpp](adversarial-probe.cpp)：独立输入复现以上行为，另覆盖旧 dirty 回调、暂停边界、事件异常和重排快照。
- [adversarial-scheduler-reentry.cpp](adversarial-scheduler-reentry.cpp)：条件性 capture 析构重入；本机退出 `0xC0000409`，不是超时。
- [adversarial-clock-reentry.cpp](adversarial-clock-reentry.cpp)：注入时钟 supplier 读取同一 ClockService 的条件性重入。
- [memory-session-batch.cpp](memory-session-batch.cpp)：ASan+UBSan 下实际 Detach/Attach 批次入口。
- [统一复放脚本](replay-probes.sh)：从本目录读取归档探针，直接编译当前生产源码，输出只写入 `build/diagnostics/review-05/archive-replay`。
- 初审原始脚本：[algorithm-probe.sh](algorithm-probe.sh)、[adversarial-build.sh](adversarial-build.sh)。统一脚本处理 sanitizer DLL 路径、预期异常退出和明确的行为断言。

## 复放命令

在 `E:/source/repos/SLSF Reloaded NG` 执行：

```powershell
& 'C:/Program Files/Git/bin/bash.exe' --noprofile --norc docs/evidence/review-05/replay-probes.sh
```

PowerShell 仅启动 Git Bash，编译及复放逻辑均在 Bash 脚本中。需要本机 Clang 20、Python 3、MSVC C++ 标准库/链接环境。脚本为 AVX2 数值 TU 设置目标选项，本轮执行机器为 i5-13600KF。

`ARCHIVE_REPLAY=PASS` 表示当前错误和控制组均得到预期观察值，且 instrumented 路径没有 sanitizer 诊断。F1–F5 的代码修复尚未执行；修复后应把对应反例转为正确行为回归，并更新复放断言。

## 日志

- [existing-checks.log](existing-checks.log)：普通、ASan、差分、fuzz、插件和静态 probe。
- [baseline-validation.log](baseline-validation.log)：合同坏例、任务书和 JUnit case/断言数。
- [target-probe.log](target-probe.log)：静态通过及实机唤醒 pending 标记。
- [adversarial-release.log](adversarial-release.log)：fresh 原始 Release 探针。
- [instrumented-batch-replay.log](instrumented-batch-replay.log)：初审 ASan+UBSan binary 补齐 runtime PATH 后复放。
- [root-replay.log](root-replay.log)：统一脚本从归档探针重新编译后的结果。
- [source-unchanged.log](source-unchanged.log)：固定源码树与 HEAD 保持一致的校验。

微基准为热数据、局部阶段，7 次样本中位数；未固定 P/E 核或频率。事件容器比较没有执行完整 Dispatcher::Step，root-key 比较模型省略了生产 slot/generation 校验。各自语义与测量范围见算法报告。
