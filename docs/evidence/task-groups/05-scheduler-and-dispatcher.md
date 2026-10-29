# 05 组交付记录：时钟、定时协程与同步事务

任务：B01、B02。日期：2026-10-29。分支状态：工作区共享，未提交他人改动。

## 概要

当前 B01/B02 及 05 审核修复均已完成，最新测试、性能对照和离线 EXE 见[05 修复记录](../2026-10-29-05-fixes-and-offline-probe.md)。下方初次交付命令与旧计数保留为阶段记录。

- B01 假时钟三域期限调度器 + 可取消定时协程：`include/slsf/core/time/Scheduler.h` + `src/core/time/Scheduler.cpp`
  - 三域（Game=游戏日 / Real=含菜单现实秒 / Active=菜单外现实秒）独立的 vector 小根堆，键 `(deadline, sequence)`，同期限按入队序稳定执行。
  - 堆项为 `HeapEntry{deadline, sequence, slot, generation}`；重对象只放槽表的 `TaskBody`（协程帧由 `Flow`/`waits_` 管理，不进调度器）。
  - 槽 LIFO 复用 + 取消代次；取消/会话换代时任务立即失效，任务体移到锁外释放。
  - 取消时累计死项，`discarded>16 && discarded*2>heap` 时压缩重建；NewSession 清空旧堆，`discardedTotal` 记录累计清理数。
  - 入口校验：非法 domain / 非有限 deadline 拒绝，返回空 `TimerToken`。
- B02 统一执行器 + 同步事务：`include/slsf/runtime/Dispatcher.h` + `src/runtime/Dispatcher.cpp`
  - `Attach(epoch, RuntimeBindings, State)`：锁外 PrepareSettlement 与工作缓冲，提交时核对生命周期代次，重置三钟和调度器并发布新会话快照；`Detach` 清游戏态并中止全部挂起延时（每个等待流程恢复一次，`Delay()` 返回 false）。
  - `Execute(command)`：短锁内校验（Stale=未挂接 / NotReady=挂起）→ 结算到当前游戏钟 → `ApplyExplicit` → 发布变更再返回。ScheduleChange 的同刻命令保持入队顺序。
  - `AdvanceTime(GameTime)`：先校验（非有限、回拨 = `InvalidArgument`），仅成功后提交单调地板并发布；失败不移动地板、不丢 `changes_`。
  - 引擎事件 `Post(epoch, body)` 入 deque，`Step()` 逐个消费并校验 epoch 及生命周期代次。事件体抛异常会结束本轮 Step，余项留待下一轮；回调中新 Post 在同轮可见。
  - 脏合并 `RequestDirtyRefresh(region)`：同目标 Active 域 1.0s 窗口到期定时器，重复目标合并计数（dirtyMerged）；Detach 与未 Attach 时不留幻影。
  - 定时协程 `SpawnFlow`/`Delay(秒, domain)`：body 托管到协程帧（`keepBody`），等待项用 `shared_ptr` 存活到回调结束，取消同时撤销 timer；frame/等待资源各释放一次。
  - 三域定时与 `Schedule`/`Delay` 的秒值统一换算：Game 域按 1 天=86400 秒折算，其他域直接相加。
- 三钟：`include/slsf/runtime/ClockService.h` + `src/runtime/ClockService.cpp`：steady 墙钟注入点（`WallNowFn`）、菜单门控（Active 仅在非菜单时累计）、游戏时钟单调、非有限输入拒绝并重锚。
  - 菜单切换时用切换瞬间的墙钟样本结清区间，暂停/恢复的归属不依赖 Tick 调用相位。
  - 时钟供应器在锁外采样，提交时核对来源代次；替换的旧供应器在锁外释放。
- 02 组交接补全：`src/core/reputation/ReputationState.cpp:525` 自由函数 `slsf::Query(const PublishedState&, const QueryRequest&)`：Fame/Flag 均校验类别（越界 `InvalidArgument`），Location 未发布 `NotFound`，投影行截断 `Internal`，RecognitionTime 不在发布投影内返回 `NotFound`。
- 构建：根 `xmake.lua` — SLSFCore/SLSFCoreAsan 增补 `src/runtime/**.cpp` 并对含中文注释的目标加 `/utf-8`。

## 初次交付测试与复现命令

命令均以 `E:/source/repos/SLSF Reloaded NG` 为根；退出码 0 表示全部通过。

```
xmake build -j8 SLSFCoreTests SLSFBoundaryTests SLSFCoreTestsAsan SLSFBoundaryTestsAsan SLSFEngineProbe

./build/windows/x64/releasedbg/SLSFCoreTests.exe --rng-seed 1170
# All tests passed (128222 assertions in 210 test cases)；EXIT=0
# 20 个 Catch2 seed 复跑（1170 1171 2 3 7 19 2024 4242 12345 555 9001 31337 117 8888 2025 424242 77 1234 999 10007）全部 EXIT=0

$env:PATH = "C:\Program Files\LLVM\lib\clang\20\lib\windows;" + $env:PATH   # 同一 shell 会话
./build/asan/SLSFCoreTestsAsan.exe --rng-seed 1170
# All tests passed (128222 assertions in 210 test cases)；EXIT=0
# 说明：Windows clang-cl ASan 不支持 LeakSanitizer（detect_leaks=1 直接报错），
#       “释放一次”由行为断言与代码路径验证，而非 LSan。

./build/windows/x64/releasedbg/SLSFBoundaryTests.exe      # 22 assertions, 4 cases；EXIT=0
./build/asan/SLSFBoundaryTestsAsan.exe                     # EXIT=0

./build/windows/x64/releasedbg/SLSFEngineProbe.exe > build/probe-log-05.txt
# probe_result=STATIC_PASS；EXIT=0（probe_wake_* 见下）
```

本组用例（新增于 tests/core）：

- `SchedulerTests.cpp`（8 例）：三域独立推进、取消幂等与 stale token、会话换代失效、堆压缩回收（DiscardedCount 累计语义）、同期限稳定序、8 seed 差分随机对照 `kSeeds={1170,1171,2,3,7,19,4242,12345}`（对照 `tests/reference/ReferenceScheduler.h`，逐操作比较到期集合/顺序/Cancel 结果，并周期性核对 PendingCount）、非有限与非法 domain 拒绝、会话换代+槽复用的 token 隔离。
- `DispatcherTests.cpp`（18 例）：Attach/Detach 快照、同步提交即发布、定时业务恰一次、稳定域序、Flow/Delay 取消释放一次、会话中止（各恢复一次）、epoch 事件校验、脏合并、挂起门控、Execute/Query 并发交错 revision 单调、move assignment 单次析构、CancelWait 撤销 timer、弱引用释放、timer body 抛异常隔离、非有限时间不移动地板、Detach 清 changes/dirty、重挂接发布干净快照、Detach 与 timer 批并发不双恢复。
- `ClockServiceTests.cpp`（6 例）：首 Tick 锚定、Active 菜单门控、游戏钟回拨拒绝、无墙钟样本时菜单状态保持、菜单切换区间归属、非有限墙钟丢弃并重锚。

随机 seed 记录：调度差分固定 `kSeeds`（含 1170）；Catch2 运行期 seed 用 1170 并按 20 seed 复跑。无新增模糊测试面（本组无解析器改动，沿用既有 fuzz 目标未动）。

## 对抗审核与修复记录

三路独立子代理（只读+runtime harness）审核后按发现逐项修复，全部复跑：

- A（调度/时钟算法，`.scratch/05-review/AUDIT-REPORT.md`）：P2-1 菜单边界归属依赖 Tick 相位 → `SetMenuOpen` 用切换瞬间墙钟结清；P2-2 差分缺确定性回归 → 新增 `scheduler session change and slot reuse keep tokens isolated` 并加周期性 PendingCount 断言；P3 NaN/非法 domain、DiscardedCount 文档与累计语义、压缩计数、Game 域秒/日换算全部修复。
- B（Dispatcher 生命周期/线程，`.scratch/05-review/B02-Dispatcher-review.md`）：4 个高危缺陷全部修复：
  1. `Flow::operator=` 显式析构后作用域结束再析构 → 协程帧双重 `destroy()`。改为交换语义，自赋值保护，只释放被替换帧一次。
  2. `CancelWait`/`~Flow` 删除 `WaitEntry` 但不取消已武装 timer → 回调 UAF。改为 `shared_ptr<WaitEntry>` + 取消时 `Scheduler::Cancel`，回调持共享所有权并在锁内复核存活。
  3. `Detach` 与 `Step` 并发释放 `waits_` → UAF。等待表改为共享所有权，`FireWait`/`Detach` 均在锁内翻转 `live`，恢复只在锁外执行且至多一次。
  4. `AdvanceTime` 先提交 `lastAdvance_` 后校验 → inf 永久锁死、NaN 破坏单调。改为先校验后提交，且仅在核心 Advance 成功后发布。
  其余：dirty 幻影、跨会话 changes、Attach 不发布、`~Dispatcher` 悬挂 Flow、timer 抛异常丢同批、`Attached()` 无锁、WakeTick NaN、非有限 Delay、Step 吞错、测试缺口均修复并补回归。
- C（报告逐项核对，`.scratch/05-review/report-check/05-verification.md`）：本文件已按其 10 条清单修正（LSan 表述、假 WakeSource 声明、多实例声明、CoroJ 产物、Query 语义、Scheduler 内部描述、`Snapshot()` 大小写、用例列举、控制台命令措辞、PATH 写法）；`Coro*.exe/pdb` 构建产物已删除。

审核 harness（`.scratch/05-review/harness/main.cpp`，24 个场景）复跑结果：22 个在 ASan 下通过；`event-throw` 与 `keepbody` 因 **clang-cl 20 ASan 与 MSVC C++ 异常处理的已知不兼容**（10 行最小复现：`throw std::runtime_error` 经 `std::function` 调用后 `catch` 收到空异常对象；同代码 Release 正常）改在 Release 下验证通过。该限制与生产代码无关，报告中所有“异常路径”结论均以 Release 验证为准。

## 本组修复的回归（记录教训）

1. **Detach 恢复 SEGV（MSVC /O2 专属，ASan 不触发）**：SpawnFlow 把 flow body lambda 按值拷入协程帧，语句末 lambda 析构，帧内 `this` 载荷悬垂，恢复路径读取已覆写栈槽 → C0000005。修复：`Flow::promise_type::keepBody` 以裸指针持有堆上 lambda，promise 析构负责释放（每帧一次）。
2. **调度器取消语义差分（seed 1171, op64）**：`NewSession()` 换代回收槽体时漏置 `slot.armed = false`，freeSlots 出现重复项；后续 `Schedule` 弹到重复项会覆写仍在堆里的活任务（代次静默失效），与参考模型在同一 token 的 Cancel 结果上分叉（ref=true/prod=false）。修复即补 `slot.armed=false`；复现源码 `tests/scratch/CoroJ` 已删除，构建产物已清理，修复前二进制不可重放。

## WakeSource 说明（B01）

本轮按用户要求采用离线 EXE 验收。`SLSFDispatcherOfflineProbe` 使用真实 Dispatcher、模拟 AddTask 队列和可控时钟，已验证 3600 帧心跳、1000000 秒挂起等待、菜单三钟、取消、切会话和停止唤醒，输出 `OFFLINE_WAKE_PROBE=PASS`。

端口仍为 `IWakeSource{Start(WakeCallback)→bool, Stop()}`，回调携带 `GameTime`。边界 FakeWakeSource 覆盖 start/stop；EngineProbe 保留 UI、AddTask、1170 的静态 ABI 检查。实际 SKSE 适配器由 B03 接入。

最新普通/ASan 核心均为 219 cases、129271 assertions；边界均为 4/22，差分为 8/17161，fuzz 100000 次 seed 1170。完整命令、原始日志和独立复核见[修复记录](../2026-10-29-05-fixes-and-offline-probe.md)。

## 验收对照

- 同期限稳定序与到期集合差分：8 seed 对照 ReferenceScheduler 全绿；20 个 Catch2 seed 全绿。
- 取消/换代失效 + epoch 校验 + 释放各一次：行为断言 + ASan 内存错误检查（Windows 无 LSan）。
- 脏合并（同目标 1.0s 合并）；业务事件逐个消费（epoch 失配丢弃计数）。
- 同步命令返回前提交并发布：`snapshot->revision == result.revision` 断言；Attach 后发布新会话快照。
- WakeSource：离线主线程队列与真实 Dispatcher 通过长等待、心跳、菜单、取消、换代和 Stop 验收。

## 下组接入事项

- 06 组（B03 会话/生命周期）：`Dispatcher::Attach/Detach/Suspend/Resume/CurrentEpoch/Snapshot()` 已齐；线程模型为“唤醒回调整入 Step，查询走 atomic 快照”，会话组装勿再引入第二个调度器。
- 07 组（Papyrus）：`Execute()` 返回 `CommandResult{status,revision,message}`；事务处理范围仅 `Execute/ScheduleChange/RequestDirtyRefresh`；Query 每次走 published 快照（`Snapshot()` 返回 `shared_ptr<const PublishedState>`）。
- B03 接一次性 SKSE AddTask 投递及生命周期，并复用本组已验证的调度、等待和会话失效路径。
- 构建注意：ASan 测试 exe 落在 `build/asan/`，运行前需把 `C:\Program Files\LLVM\lib\clang\20\lib\windows` 加入 PATH（clang_rt.asan_dynamic-x86_64.dll）；VS 工具集 targets 输出在 `build/windows/x64/<mode>/`。
