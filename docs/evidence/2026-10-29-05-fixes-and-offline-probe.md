# 05 修复与离线主线程探针

2026-10-29。已完成 [05 独立审核](2026-10-29-05-independent-review.md)中的修复和三项优化。按用户要求，B01 的唤醒与等待验收使用离线 EXE，直接驱动真实 Dispatcher。

## 修复结果

| 项目 | 当前行为 | 回归证据 |
|---|---|---|
| F1 同步写入前结算 | Execute 读取当前游戏钟，先处理到期变更并结算到当前时间，再应用命令和发布；结算失败时返回错误 | 当前日 Set20 保持 20；已有排队命令先处理后再写入 |
| F2 同刻顺序 | ScheduleChange 在相同时间的已有命令之后插入 | 同刻 Set10 → Set20，最终 20 |
| F3 跨会话回调 | Step 在批次和每项执行前核对生命周期代次及 scheduler session；dirty 回调携带原代次 | Detach 后以相同 Epoch 重新 Attach，旧 timer 被丢弃，新 dirty 请求保持待处理 |
| F4 取消回收 | Cancel 累计失效堆项并按比例压缩；NewSession 清空旧堆 | 取消 1000 个远期任务后堆项至多 16；换会话后旧堆为 0 |
| F5 锁外准备 | Attach 在锁外建立准备数据和工作缓冲，提交时重新核对生命周期代次 | 独立代码审阅通过 |
| F6 唤醒与协程等待 | 离线主线程队列调用真实 WakeTick；Delay 登记期限后挂起 | 3600 帧心跳、菜单、取消、换会话与 Stop 全部通过 |
| C1 capture 析构重入 | Cancel、NewSession 及 Dispatcher 清理将任务体移到锁外释放；预留退休列表容量后再移动任务体 | capture 析构可调用 PendingCount / Attached |
| C2 时钟供应器重入 | supplier 在 ClockService 和 Dispatcher 锁外调用；旧 supplier 在锁外释放，提交时核对来源代次 | supplier 可读取 Real、替换来源；旧 capture 析构可读取时钟 |

固定回归在 [DispatcherTests.cpp](../../tests/core/DispatcherTests.cpp:782)、[SchedulerTests.cpp](../../tests/core/SchedulerTests.cpp:354)和 [ClockServiceTests.cpp](../../tests/core/ClockServiceTests.cpp:38)，统一标签为 `[fix05]`。

## 三项优化及生产入口测量

- O1：事件改用 deque，逐项 pop_front。保留 FIFO、回调中新 Post 当轮可见、Detach 清队列、异常后余项留队，以及逐项会话校验。
- O2：Step 使用 HasDue；先清理无效堆根，再检查最早有效期限。DueCount 继续提供完整计数，两者共用槽位、取消代次和 session 的有效性判断。
- O3：公开 Query 先验证 RegionId 与行身份相符后直接下标访问；重排的公开 DTO 继续使用查找回退。

性能探针直接调用生产 Step、Scheduler 和 Query。基线为修复前固定树 `4c72b3da398994ae3e1c0d4567545ee7c1713696`，Clang 20.1.0、C++23、`-O2 -DNDEBUG`，仅 AVX2 数值文件加 `-mavx2`，CPU 为 i5-13600KF。每组取 7 轮中位数，构造及入队在计时外。

| 入口与负载 | 修复前 | 修复后 |
|---|---:|---:|
| Step，100 个事件 | 19.4 μs | 3.6 μs |
| Step，1000 个事件 | 1.2154 ms | 34.2 μs |
| Step，5000 个事件 | 26.4213 ms | 170.9 μs |
| 到期布尔查询，128 个未来任务 | 52.76 ns | 12.22 ns |
| 到期布尔查询，8192 个未来任务 | 2414.58 ns | 13.42 ns |
| Query，128 行末行 | 59.8 ns | 34.0 ns |
| Query，65535 行末行 | 12306.3 ns | 33.0 ns |

两组 checksum 均为 `9110483571492202422`。这是热数据局部负载，未固定 P/E 核及频率；它衡量这些生产入口的成本。完整规模、源码和重放脚本见[归档](fix-05/README.md)。

## 离线 EXE

[DispatcherOfflineProbe.cpp](../../tests/probe/DispatcherOfflineProbe.cpp:67)使用模拟 AddTask 队列、可控墙钟和真实 Dispatcher。所有 wake、心跳及业务恢复均在同一 owner 线程执行；监控线程只在 owner 超过 10 秒未返回时报告失败。

模拟共 3600 帧、60 秒，前 20 秒处于菜单。Real 的 10 秒等待在菜单期间完成，Active 和 Game 在菜单关闭后完成；1000000 秒长等待持续挂起。随后验证 CancelWait、Detach/Attach 中止旧等待和旧批次，以及 Stop 丢弃已投递 wake。

最终输出：

```text
heartbeat=3600 frames=3600 simulated_seconds=60 max_pump_us=5.5 long_wait_seconds=1000000 completions=3 stale_callbacks=0
OFFLINE_WAKE_PROBE=PASS
```

`max_pump_us` 为本次运行观测值。该 EXE 验证本项目协程与执行器交还主线程；B03 负责将实际 SKSE AddTask 与生命周期接入这条路径。

在项目根目录可直接运行：

```text
xmake build -y SLSFDispatcherOfflineProbe
xmake run SLSFDispatcherOfflineProbe
```

`tools/run-native-tests.ps1 -Mode all` 已包含该探针，并输出 `build/test-results/dispatcher-offline-probe.txt`。

## 最终验证

最终修复后运行 `tools/run-native-tests.ps1 -Mode all -Seed 1170 -Runs 100000 -Jobs 20`，再构建 SLSFReloadedNG 与 SLSFEngineProbe，全部退出码为 0。

| 检查 | 结果 |
|---|---|
| 普通核心 / ASan 核心 | 各 219 cases、129271 assertions |
| 普通边界 / ASan 边界 | 各 4 cases、22 assertions |
| 独立差分 | 8 cases、17161 assertions |
| StateCodecFuzz | 100000 次，seed 1170 |
| ASan 正向 / 故意越界负向探针 | 正向通过，负向检出预期越界 |
| 离线主线程 EXE | PASS |
| 插件及 EngineProbe 构建 | 通过 |

核心 JUnit 的 311 个展开节点对应 219 个 Catch2 cases；普通与 ASan 覆盖相同用例且 failures/errors 为 0。

独立 Standards 和 Spec 复核已完成；两条初步建议经增量修正后关闭，最终均为 0 项未关闭发现。fresh 注释审查保留 API 的顺序、所有权和锁约束，删除实现复述。报告及原始日志见[归档目录](fix-05/README.md)。

最终源快照为 `963ba4608f52c9dba83745b17ab75036e16ac57c`；HEAD 仍为 `5222a3c31841426bb26c3635a37638abb8f707e5`。本轮使用独立 index 固定比较范围，保留共享工作区和真实 index。

收尾检查通过：11 份 Markdown 的 73 个本地链接、8 个源码行锚点及围栏有效，11 份归档产物与最终输出一致；任务书检查通过 11 组、25 个任务、37 个 CPP 唯一归属及 103 个本地链接。最终源码与复核快照相同，独立 index 的 whitespace 检查通过。
