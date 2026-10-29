# 05 / 06 审核诊断材料

固定源码提交：`184c964eb226f614169a6b582c3adbd9970a04af`。普通 MSVC `/O2 /MD`，链接当前 `build/windows/x64/releasedbg/SLSFCore.lib`。完整裁决见 [审核报告](../2026-10-29-05-06-adversarial-audit.md)。

| 材料 | 内容 |
|---|---|
| stable-key-probe.cpp / stable-key-probe.stdout.log | 合成 ESL form ID，调用真实 RestoreState，对比当前 mask 与 light 本地 ID 控制组。 |
| coroutine-abort-probe.cpp / results.json | Delay 恢复后抛异常，记录显式 abort 的进程退出码。 |
| capability-pending-probe.cpp / capability-pending-probe.stdout.log | loaded+pending 后的两次 Ensure，验证是否重试。 |
| capability-reentry-probe.cpp / capability-reentry-probe.stdout.log | provider 同步查询自身，父进程有 2 秒超时并终止子进程。 |
| wait-retire-probe.cpp / wait-retire-probe.stdout.log | 100/1000/5000 项到期及取消，各 5 轮；输出完成数、顺序、checksum、耗时。 |
| statecodec-fuzz.log / serialization-fuzz.log | 本轮各 100000 次、seed 1170 的 parser fuzz 原始日志。 |
| run-probes.ps1 | 在系统临时目录编译与运行五个独立探针，不修改生产源或 build。 |

从仓库根运行：

```powershell
xmake build -y SLSFCore
.\docs\evidence\review-05-06\run-probes.ps1
```

脚本接受 `-VsEnvironment` 和 `-OutputDirectory`；默认工具链路径为 `E:\BuildTools\VC\Auxiliary\Build\vcvars64.bat`，默认输出为新的系统临时目录。结果写入该目录的 results.json 和各 stdout/stderr 日志。归档的结果来自主代理复放，未归档二进制。

协程异常预期终止其独立 EXE，查询重入预期超时；父脚本正常返回仅表示采集完成。存量源码修复后，这些输出应发生相应变化。等待项测量为当前实现的合成负载，未实现替代结构。
