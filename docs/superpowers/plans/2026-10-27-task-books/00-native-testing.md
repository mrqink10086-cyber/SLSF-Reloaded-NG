# 核心、边界与游戏验证

核心独立编译为控制台测试程序，使用给定地点、时钟、随机数和外部事实驱动实际业务入口。引擎、VM、PEX 和第三方 ABI 另在目标游戏验证。

## 已有工具与命令

Catch2 3.8.1 运行核心/边界用例并输出 JUnit；LLVM 20.1.0 提供 ASan 和 libFuzzer。入口为现有 `tools/run-native-tests.ps1`，结果在 `build/test-results/`。

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/run-native-tests.ps1 -Mode core -Seed 1170
powershell -NoProfile -ExecutionPolicy Bypass -File tools/run-native-tests.ps1 -Mode boundary -Seed 1170
powershell -NoProfile -ExecutionPolicy Bypass -File tools/run-native-tests.ps1 -Mode asan -Seed 1170
powershell -NoProfile -ExecutionPolicy Bypass -File tools/run-native-tests.ps1 -Mode asan -Seed 1170 -Jobs 1
powershell -NoProfile -ExecutionPolicy Bypass -File tools/run-native-tests.ps1 -Mode fuzz -Target StateCodecFuzz -Runs 100000 -Seed 1170
```

`-Suite`是`-Mode`的别名，`-Runs`是`-FuzzIterations`的别名。core、boundary、asan、differential、bench 和 StateCodecFuzz 均已有目标。all 依次运行这些检查，最后运行 `SLSFDispatcherOfflineProbe`，结果写入 `build/test-results/dispatcher-offline-probe.txt`。

core、boundary 和 asan 的 Catch2 套件默认按逻辑 CPU 数多进程分片，并发数取 `min(Jobs, 用例数)`。`-Jobs N` 指定并发数，`-Jobs 1` 用于串行比较。所有分片使用相同 seed 和随机排序，以分散同一源文件中的重用例；每个用例只执行一次，同时生成控制台结果和 JUnit。构建、测试发现和 ASan 探针在分片前执行。

汇总报告仍为 `core.xml`、`boundary.xml`、`core-asan.xml`、`boundary-asan.xml`；各分片 XML、stdout 和 stderr 位于对应的 `*-shards/` 目录。失败时按原 seed、Jobs 和 shard index 重放，直接命令例如：`build/asan/SLSFCoreTestsAsan.exe --rng-seed 1170 --order rand --shard-count 20 --shard-index 5`。多核分片用于测试执行，游戏运行时继续采用统一业务执行器。

## 选择相关检查

| 修改 | 主要验证 |
|---|---|
| 状态/类别设置 | 同步写后读、Int32、OR 撤销、未知恢复、四类清理候选 |
| 地区声望数据/集合/布局 | 多地点共享地区声望数据、独立区域、mask 与数值/类别设置对照、行对齐/补槽、同版本发布及持有快照内容 |
| 地点/绑定 | 典型归属、全局优先级、森林/深链对照、alias/customOrder、稳定身份/重排、无效引用 |
| 传播/结算 | 每跳/门槛、取强、来源接替、显式下降、保护/重复结算和跳时；标量对照 AVX2 |
| 调度/事务 | 假时钟、到期/取消/换代、写后读、快照 revision、资源生命周期 |
| 主线程唤醒/等待 | 离线 EXE 使用真实 Dispatcher，验证长等待期间的心跳、菜单、取消、切会话及 Stop |
| 编码/JSON | 捕获业务往返及集合重建、截断、计数/类型/版本、校验错误返回及当前状态保留、实际解析器 fuzz |
| 游戏边界 | WakeSource、关系/LOS、VM、外部 ABI、旧 PEX、公开数据输出、保存/换档 |

固定用例来自旧源码和项目规格。直接 Set 的 Int32 及公开整数向量精确比较；浮点比较按具体算法声明容差，门槛/整数边界/填充槽单独检查。已有随机对照保留；新增用例优先覆盖可解释的行为和故障。

使用统一 seed（默认 1170），失败记录 case、输入和 seed，便于重放。测试侧参考采用简单独立表示，生产 DLL 使用正式内核。

## ASan 与解析器 fuzz

ASan 构建同时插桩待测自有代码和测试。脚本先运行正向探针，再确认故意越界的负向探针能被 ASan 检出，随后运行核心与边界。负向探针的预期错误单独列在日志中。

ASan 子进程按脚本设置 LLVM 动态库 PATH。协程/句柄模块覆盖取消、到期交错、换档和释放；解析器 fuzz 调用真实入口，以有效状态/旧 JSON 为种子，保留 crash/timeout/OOM 输入。

## 性能与报告

优化构建对代表性输入测热点查询、结算、图重建和 dirty 刷新，比较同输入改动前后成本。计时区间保持安静并消费输出；记录 CPU、编译参数、输入规模、耗时分布及分配情况。

按[优化实施方案](../../../../.scratch/native-migration/optimization-plan.md)在实际生产入口补正确性和成本证据。研究脚本 `tools/research/run-native-algorithm-probe.ps1` 默认运行 native-algorithm，`-Probe permission-mask` 运行类别集合探针；两者的独立结果用于解释选型。

每组报告记录命令、退出码、数量、seed、日志及游戏验证状态。工具探针见[工具证据](../../../evidence/2026-10-27-native-test-tools.md)，当前修复回归见[记录](../../../evidence/2026-10-27-provider-readiness-fix.md)。

20000 节点归属长链优化及串行/多核计时见[性能修复记录](../../../evidence/2026-10-28-region-owner-and-parallel-tests.md)。

离线主线程探针可单独运行 `xmake run SLSFDispatcherOfflineProbe`；实现、复现及 05 修复结果见[离线探针记录](../../../evidence/2026-10-29-05-fixes-and-offline-probe.md)。
