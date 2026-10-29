# 05 修复证据

主报告：[05 修复与离线主线程探针](../2026-10-29-05-fixes-and-offline-probe.md)。对照基线为 `4c72b3da398994ae3e1c0d4567545ee7c1713696`，最终源快照为 `963ba4608f52c9dba83745b17ab75036e16ac57c`。

## 文件

- [all.log](all.log)：最终普通、ASan、差分、fuzz、bench 和离线 EXE 的完整输出。
- [test-summary.txt](test-summary.txt)：五个 JUnit 汇总及真实 Catch2 case 数；[dispatcher-offline-probe.txt](dispatcher-offline-probe.txt)为离线 EXE 输出。
- [plugin-build.log](plugin-build.log)：SLSFReloadedNG / SLSFEngineProbe 构建输出。
- [regressions.md](regressions.md)：修复前错误与公开入口回归对应表。
- [standards-review.md](standards-review.md)、[spec-review.md](spec-review.md)、[comments-review.md](comments-review.md)：独立复核；Standards 的两条初步建议已关闭。
- [performance-probe.cpp](performance-probe.cpp)、[performance-probe.sh](performance-probe.sh)：真实生产入口对照；[performance-baseline.log](performance-baseline.log)与[performance-current.log](performance-current.log)为首次测量输出。

## 重放

在项目根目录运行：

```text
xmake build -y SLSFDispatcherOfflineProbe
xmake run SLSFDispatcherOfflineProbe
powershell -NoProfile -ExecutionPolicy Bypass -File tools/run-native-tests.ps1 -Mode all -Seed 1170 -Runs 100000 -Jobs 20
```

性能探针使用 Git Bash、Clang 20.1.0 和本仓库的基线 tree 对象：

```bash
bash docs/evidence/fix-05/performance-probe.sh
```

脚本从归档源码构建基线和当前生产实现，结果写入 `build/diagnostics/fix-05/`，比较 checksum。离线 EXE 源码在 [DispatcherOfflineProbe.cpp](../../../tests/probe/DispatcherOfflineProbe.cpp:67)，xmake 目标依赖 SLSFCore。

本轮所有构建与运行退出码为 0。ASan 故意越界探针的非零退出是运行器检查的预期结果。
