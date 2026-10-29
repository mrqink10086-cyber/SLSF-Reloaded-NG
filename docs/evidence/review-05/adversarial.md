# 05 组 B01/B02：独立对抗复核

## 范围与方法

复核目标是 `5222a3c31841426bb26c3635a37638abb8f707e5` → `4c72b3da398994ae3e1c0d4567545ee7c1713696`，范围为 `Scheduler`、`ClockService`、`Dispatcher`、公开快照查询及其验收/设计文档。本文结论按生产实现、项目规范和下列独立复放得出；不把 `spec.md`、`standards-memory.md` 或 `algorithms.md` 当作一手证据。

独立 Release 探针由 `adversarial-build.sh` 直接编译 13 个生产 TU，使用 Clang 20.1.0、`x86_64-pc-windows-msvc`、C++23、`-O2 -DNDEBUG`，AVX2 仅用于 `DiffusionKernelAvx2.cpp`；未调用 xmake 或共享测试目标。源文本与审核树提取副本一致（差异仅行尾）。命令和完整输出见本文末尾的 replay 小节。两个锁重入子进程在输出 `enter` 后以 `3221226505` / `0xC0000409` 退出；没有把这写成超时或普通运行时 hang。

## 逐项裁决

| 主张 | 裁决 | 结论摘要 |
|---|---|---|
| WakeSource 实机时序已通过 | **条件接受：验收未完成** | 现有证据明确为 `STATIC_PASS`；菜单、无输入空闲、加载时序仍未实机观测。 |
| Execute 缺少当前时间采样及命令前结算 | **接受，语义缺陷** | 可观察到公开调用次序造成不同结果；仅设置旧 `RuleContext.clock` 并非根因，因为 `ApplyExplicit` 当前忽略 context。 |
| 同刻 ScheduleChange 逆序 | **接受，语义缺陷** | `lower_bound(<)` 把新命令放到旧命令前，稳定排序保留该逆序。 |
| Attach 持锁准备 settlement 图 | **接受，架构/锁时长偏差** | 持锁调用 `PrepareSettlement` 已证；没有测出实际卡顿或竞争影响。 |
| Detach 后旧 due batch 仍执行 | **接受，生命周期缺陷** | 已取出的通用 raw timer 无 epoch/session 复核；其后旧 dirty timer 还可取走新会话同 RegionId 的刷新请求。 |
| 远期 Cancel/NewSession tombstone 无法及时压缩 | **接受，条件性资源增长** | heap 项和 slot/body 生命周期分离；远期 dead heap refs 可一直保留到 deadline 到达。 |
| TaskBody 捕获析构或 WallNowFn 重入会锁死 | **条件接受：API 合同缺口** | 探针在重入后进程 fail-fast；正常回调路径不触发。合同未明确禁止重入析构/时钟 supplier，故不按无条件缺陷计。 |
| Published Query 的线性 region 查找是错误 | **撤回为正确性 finding；保留低优先优化建议** | 公共 DTO 定义的是 ID 到 block 的映射，不声明 ID 必须等于行号。探针中的重排 DTO 正确返回对应块。 |
| DueCount O(n)、events 前删 O(E²) | **算法事实；性能建议，不是验收失败** | DueCount 的 O(n) 是头文件明确合同；事件逐项顺序符合验收。未在本复核中测完整 Dispatcher 工作负载。 |
| Suspend 在 timer 回调中生效后，本轮仍运行余下 timer/event | **无确定 finding：语义边界未定** | 只有进入 Step 时已挂起的 round 明确只走时钟；规范未说 Suspend 必须中断已开始的 round。 |
| Event body 抛异常 | **无规范违反证据；补充交付报告精度** | 异常传播并结束当前 Step；抛出项已出队，尚未消费的后续事件留到下次 Step。 |

### 1. Execute 先结算再执行命令

规范锚点：`.scratch/native-migration/design.md:63-68,79,95-105` 明确 Execute 使用当前游戏时间并“先处理待结算时间，再提交命令”；`docs/superpowers/plans/2026-10-27-runtime-and-compat-plan.md:19-27` 要求命令事务包含结算、修改和 revision 发布。

实现锚点：`src/runtime/Dispatcher.cpp:330-346` 只用 `lastAdvance_.value_or(GameTime{})` 构造 context，调用 `ApplyExplicit` 后发布；没有采样当前 ClockService 或 Advance 待结算时间。`src/core/reputation/ReputationState.cpp:162-165` 当前忽略该 context，Set 在 `:207-215` 写 local 值但不更新已结算锚点。因此“context 时间值错误”目前不是直接影响；缺少命令前结算才是可观测问题。

最小复放：无 due timer/change 时 `WakeTick(day 1)` → `Execute(Set 20)` → `AdvanceTime(day 1)` 得 `final=18`；先 `AdvanceTime(day 1)` 再 Execute 相同命令得 `final=20`。Release 输出在 `adversarial-release.log:3-5`。这是 Dispatcher 公共入口可复现的顺序差异，不依赖直接操作私有字段。保持 `Execute` 的 commit-before-return 并不能补足时间语义。

### 2. 同刻 scheduled change 顺序

规范锚点：`.scratch/native-migration/optimization-plan.md:91` 要求同一业务时间保持入队顺序；`docs/superpowers/plans/2026-10-27-task-books/05-scheduler-and-dispatcher.md:22` 要求稳定顺序。

实现锚点：`src/runtime/Dispatcher.cpp:369-375` 的 `lower_bound` 比较 `existing.at < when`，故新同刻项插在既有同刻项之前；`src/core/reputation/Settlement.cpp:618-623,664-681` 稳定排序后顺序不再改变，并依次应用显式命令。

最小复放：同一地区同刻先 Set(10) 后 Set(20)，Advance 后读到 10（FIFO 应为 20），见 `adversarial-release.log:6`。接受为确定的稳定顺序缺陷。

### 3. Attach 的锁范围

规范锚点：`.scratch/native-migration/design.md:101-105` 要求准备工作放在短事务锁外，提交候选时检查其基于的 revision/epoch。

实现锚点：`src/runtime/Dispatcher.cpp:230-249` 在 `Attach` 开始持有 `mutex_`，期间分配 `SettlementWorkspace` 并调用 `PrepareSettlement`；`src/core/reputation/Settlement.cpp:205-246` 可构造并复制图、规则和自定义地区数据。故这是确定的锁范围/架构偏差。Attach 期间其他取同一 mutex 的调用可能等待；本审计没有提供图规模基准、等待时长或玩家可见卡顿证据。

### 4. 已提取 due batch 越过会话边界

规范锚点：05 task book `:22-25` 要求取消/换代使对应任务失效并由执行器检查 epoch；Dispatcher 头文件 `include/slsf/runtime/Dispatcher.h:199-202` 说明 Detach 清工作并滚动 scheduler session generation。

实现锚点：`src/core/time/Scheduler.cpp:126-172` 在 `AcquireDue` 返回前取出本次所有 due body，并逐一 retire/recycle slot；`src/runtime/Dispatcher.cpp:532-550` 随后无 session/epoch 校验地调用返回向量。Detach 的 `NewSession` 无法再访问已经从 slot 移出的 body（`Dispatcher.cpp:263-299`）。

最小复放：同一批两个零延时 Real raw timer，A 调用 Detach 后立即 Attach session 2，B 检查 epoch。输出 `detach_batch first=1 stale_callback_in_new_session=1 attached=1`，见 `adversarial-release.log:7`。这不是跨线程竞态。已有 `tests/core/DispatcherTests.cpp:738-779` 只验证 Flow 路径：B 的 wait callback 进入 `FireWait`/`RetireWait` 时会检查 `WaitEntry::live`（`Dispatcher.cpp:158-195`），所以 flow B abort；这个保护不覆盖普通 Schedule body。

同路径的脏刷新影响也已复放：旧 session 中先排普通 Active timer，再排目标 42 的 dirty timer；普通 timer Detach、Attach 新 session 并重新 `RequestDirtyRefresh(42)`。旧 due dirty body 随后调用 `FireDirty(42)`，该函数只按目标从当前 `dirty_` 提取，没有检查 timer/session token（`Dispatcher.cpp:460-495`）。输出 `sink_calls=1 pending_dirty=0 active_timers=1`（`adversarial-release.log:10`）：新请求在窗口到期前被旧回调消费，自己的新 timer 仍在，但之后找不到 dirty 项。此为比“任意旧 callback 仍运行”更窄、可复现的会话污染反例。

Suspend 单独测得：回调内调用 Suspend 后，本轮后续 due timer 和 event 仍执行；Step 开始前已挂起则 timer/event 留存。输出见 `adversarial-release.log:8-9`。`Dispatcher.h:206-209,257-262` 描述挂起 round 的门控，但没定义 round 中途挂起是否立刻截断当前 round，故暂不升格为 finding。

### 5. 远期 dead heap entry 保留

实现锚点：Cancel 只 retire slot（`src/core/time/Scheduler.cpp:104-124`）；dead 项只在 deadline 到期后被 `AcquireDue` pop 并增加 `discarded`（`:134-150`）；NewSession 只在已有 `discarded >= 16` 时压缩（`:221-242`）。公开头文件明确允许死项延迟至 pop/compaction（`include/slsf/core/time/Scheduler.h:8-10,98-102`），但优化要求明确要求处理连续取消时的容器回收（`optimization-plan.md:101-107`）。

最小复放：在 deadline=1000 安排并 Cancel 1000 个 Game timer，`live=0 heap=1000 discarded=0`；`AcquireDue(999)` 后不变，`AcquireDue(1000)` 才清空 heap、计入 1000 dead refs。1000 次 Schedule→NewSession 也留下 `heap=1000 discarded=0`。见 `adversarial-release.log:13-16`。TaskBody 已释放、slots 可复用，不是 closure 泄漏；但若高频取消/会话换代的任务期限一直遥远，dead heap refs 可随次数增长并长期占容量。确认存在回收盲点，实际生产规模与增长速度未测。

### 6. 锁内用户对象析构 / 注入时钟回调

`Scheduler::RetireSlotLocked` 在 mutex 持有期间执行 `released = nullptr`（`src/core/time/Scheduler.cpp:39-49`）；Cancel 和 NewSession 从锁内调用它/等价析构（`:104-123,221-243`）。Release 复放用 `TaskBody` 捕获一个 RAII 对象，其析构调用 `PendingCount()`：打印 `capture_destructor=enter` 后进程以 `0xC0000409` 退出，没有打印 returned（`adversarial-release.log:18-21`）。这证明此复放下的重入析构不安全；不是普通 TaskBody 调度或取消的回调重入。

`ClockService::Tick` 和 `SetMenuOpen` 在持有 `ClockService::mutex_` 时调用 `wallNow_()`（`src/runtime/ClockService.cpp:30-49,65-89`）。注入 supplier 调用 `Real()` 后同样输出 enter 并以 `0xC0000409` 退出（`:22-25`）。默认 steady-clock supplier以及已检查的普通 fake supplier不重入。

合同边界：`Scheduler.h:12-17` 明确允许已派发的 task body 在锁外再次 Schedule/Cancel；没有明确说用户 capture 的析构也可重入，也没有禁止这种析构。`ClockService.h:32-60` 只描述单调秒数与注入，没有 supplier 不可重入限制。因此这两项应列为**条件/API 合同缺口**：若公共 callable 及其析构允许调用所属对象方法，则需要锁外释放或显式禁止重入；当前证据不支持将其报告成普通使用必现死锁。探针退出码是 fail-fast，报告未据此声称另一个平台会挂起。

### 7. Query 线性映射的裁决

`MakePublishedState` 当前以 `RegionId(i)` 构建身份顺序（`src/core/reputation/ReputationState.cpp:497-518`），Query 用 `std::find` 找 region 对应 block（`:525-538`）。`include/slsf/core/Snapshots.h:99-118` 只声明每个 `regionIds` 项对应一个 account-major block；DTO 字段是公开 vector，未规定 `regionIds[i] == RegionId(i)`。因此仅用 region.value 作为 block 下标会误读有效的重排快照。

复放 DTO：`regionIds={7,2}`，location 映射至 RegionId 2，两个 block 中分别写 71 和 22；`Query` 返回 22，见 `adversarial-release.log:17`。故将 S4 当作错误或直接下标修复均撤回。若要优化当前生产 identity 快照，可先验证 `region.value < regionIds.size() && regionIds[region.value] == region` 走 O(1)，失败时保留现有 `find` 回退；这保持现有映射语义。属于低优先性能建议。

### 8. DueCount、事件容器与异常语义

`Scheduler::DueCount` 的公共头文件合同就是 O(n)（`include/slsf/core/time/Scheduler.h:90-92`），实现遍历 heap 并验证 slot/generation（`Scheduler.cpp:174-191`）；`Step` 每轮把它当 boolean 做 Game-domain 有无到期任务判断（`Dispatcher.cpp:518-530`）。故线性复杂度是确定的，但不是合同违反。另一个独立算法复核的无墓碑 heap 微基准报告 128/1024/8192 entries 分别 52.2 ns/750.6 ns/2.5866 µs，并明确 root-only model 11.8 ns 缺少 slot/session 校验（`algorithms.md:57-66`）；不能把这个简化模型当作生产加速倍数。可另增专门 `HasDue` 路径，只在 workload 证明必要时优化。

事件循环每消费一项执行 `events_.erase(events_.begin())`（`Dispatcher.cpp:552-578`），故排空 E 项有 `1+...+(E-1)=O(E²)` 次元素搬移。算法复核的同 payload 微基准在 5000 项测得前删 vector 28.6748 ms，head cursor 25.3 µs、deque 23.7 µs（`algorithms.md:43-55`）；这是容器微基准，不是完整 Step 时延。05 规范只要求事件逐个消费，现实现 FIFO/epoch 校验与同轮 Post 可见性，不构成正确性失败；作为有规模证据后再处理的性能项。

事件异常另有边界：事件先从队首移动并 erase，再执行 body；事件体不在 catch 中（`Dispatcher.cpp:552-578`）。复放第一个 Post 抛出 int 17，Step 将异常传出，第二事件该轮尚未执行但仍 pending，下次 Step 才消费，输出见 `adversarial-release.log:11-12`。被抛出的第一项不会重试。05 task book 要求逐个消费，没有要求吞异常或同轮继续；交付记录 `05-scheduler-and-dispatcher.md:17` 的“只影响该事件（队列保留）”仅在“后续项仍排队”意义上成立，不能解读为本轮仍继续消费。

## 其余交付与基线核对

- Scheduler 的三域独立 deadline heap、按 `(deadline, sequence)` 稳定 dispatch、slot generation、取消立即释放 body、迟到时回收 heap 项、无效 domain/deadline 拒绝均与实现相符；确认本复核中的远期 tombstone 回收盲点除外。`DueCount` 的慢路径是显式公开 O(n) 合同。
- `Execute` 对本次有效显式命令仍在返回前 ApplyExplicit 并发布；缺陷是当前待结算游戏时间未在其前消费。`AdvanceTime` 先验证后推进且失败时保留 due change 的报告主张与实现路径相符。
- dirty 同 RegionId 合并、普通 detach 时清理当前 map、Flow wait 取消/live 防重放、跨域秒/游戏日换算以及时钟菜单归属有对应实现。上文已说明旧 due dirty callback 可越过 detach 清理并影响新 map。
- event epoch 检查在每个回调前执行；异常不捕获，但后续队列项保留。timer body 有独立 catch，当前 timer 批余项继续执行。
- 目标 WakeSource 仍是唯一已明确未完成的实机验收：`target-probe.log:1-18` 写明 `probe_result=STATIC_PASS`，并将 menu、load、no-input wake timing 标为 `unverified`；task book 要求记录实际目标时序（`:17-25`）。交付报告 `docs/evidence/task-groups/05-scheduler-and-dispatcher.md:78-100` 已披露此限制，没有将其伪报为实机通过。
- `baseline-validation.log:17-22` 记录 core/core-asan 各 210 Catch cases、300 XML instances、128222 assertions；boundary/boundary-asan 各 4 cases、22 assertions；differential 8 cases、17161 assertions，failures/errors 均为 0。交付报告 `:33-44` 用 210 test cases 和 assertion 数描述，与之相符。该基线由主审核记录，本复核没有重跑共享 xmake/测试。
- 未发现 B01/B02 范围外的明确 scope creep；“先前对抗发现已全部修复”的历史记录不能代替当前树复核，本报告所列残余问题仍成立。

## Replay

从仓库根目录在 PowerShell 运行：

```powershell
& 'C:/Program Files/Git/bin/bash.exe' --noprofile --norc 'build/diagnostics/review-05/adversarial-build.sh' > 'build/diagnostics/review-05/adversarial-release.log' 2>&1
```

独立源码和脚本：`adversarial-probe.cpp`、`adversarial-scheduler-reentry.cpp`、`adversarial-clock-reentry.cpp`、`adversarial-build.sh`；Release 输出为 `adversarial-release.log`，编译对象和可执行文件位于 `adversarial-build/`。脚本对子进程使用 Python `subprocess.run(..., timeout=3)`，所以重入 fail-fast 与 timeout 可区分。
