# 05 组独立审核：调度、同步事务与会话生命周期

日期：2026-10-29。任务：B01、B02。依据实际实现、独立入口探针和现有测试逐项核查交付记录。四名 `gpt-6-luna / xhigh` 子代理分别完成 Standards/内存、Spec、算法及 fresh 对抗；主代理复放探针并汇总裁决。

固定比较：初始化提交 `5222a3c31841426bb26c3635a37638abb8f707e5` → 工作区树 `4c72b3da398994ae3e1c0d4567545ee7c1713696`，包含未跟踪源码。范围包括 Scheduler、ClockService、Dispatcher、公开快照查询及对应接口、测试、构建输入。审核仅新增报告和诊断材料，保留生产源码、原测试、任务书、HEAD 与真实 Git index。

## 结论

现有测试通过；额外探针复现了同步写入时间顺序、同刻命令顺序、旧回调跨会话及远期取消回收四项问题。Attach 另有锁内准备偏差，WakeSource 实机验收仍为部分交付。

### Standards

最终保留两项规范偏差：旧定时批次缺少会话复核、Attach 在事务锁内准备，对应下表 F3/F5；前者可使旧会话工作影响新会话。初审提出的单项公开查询线性扫描归入 O3 性能建议，当前返回值正确。完整 12 项 Fowler 检查未形成额外违规或必须重构的 smell。详见 [Standards 与内存报告](review-05/standards-memory.md)及 [fresh 裁决](review-05/adversarial.md)。

### Spec

确认六项 P2：F1–F5 为当前代码/回收设计偏差，F6 为本组验收缺口；未发现范围扩张。条件风险与优化建议分别列出。详见 [独立 Spec](review-05/spec.md)、[算法报告](review-05/algorithms.md)及 [fresh 逐项对抗](review-05/adversarial.md)。

| 编号 | 裁决 | 实际机制与证据 | 对应要求 |
|---|---|---|---|
| F1 | P2：Execute 未先结算当前时间 | `Execute` 直接 ApplyExplicit；无到期工作的一次 `WakeTick(day 1)` 后 Set(20)，再结算 day 1 得 **18**，先结算再 Set 得 **20** | B02 校验→结算→修改→发布；当前钟源采样 |
| F2 | P2：同刻命令反转 | `lower_bound` 在相等时间的旧项前插入；之后 stable_sort 保留反序。依次 Set(10)、Set(20) 最终 **10**，应为 **20** | 同一业务时间保持入队顺序 |
| F3 | P2：已提取回调跨会话 | AcquireDue 先取出整批；首回调 Detach/Attach 后，下一旧 raw timer 仍在新 session 执行。旧 dirty 回调也可提前消费新会话同目标请求 | 会话换代使旧工作失效，消费时核对代次 |
| F4 | P2：远期失效堆项缺少回收触发 | Cancel/NewSession 使任务失效，但压缩计数只在死项到期弹出时增长。1000 次远期取消后 live=0、heap=1000、discarded=0；到期限前一直保留 | 连续取消下处理容器回收 |
| F5 | P2：Attach 锁内准备 | 持 dispatcher mutex 时分配 workspace 并调用 PrepareSettlement，包含 CSR/规则/自定义地区准备 | 图及候选准备在事务锁外，提交时验证版本 |
| F6 | P2：B01 实机验收部分完成 | 当前 probe 只有静态签名和 `unverified:menu-open-wake,load-screen-wake,no-input-idle-wake`；尚无运行中的 WakeSource 实现/节奏记录 | 1170 菜单、空闲、加载及唤醒上下文实测 |

## 逐项反证与处理方向

### F1：同步写入必须使用当前业务边界

[Dispatcher.cpp:330](<E:/source/repos/SLSF Reloaded NG/src/runtime/Dispatcher.cpp:330>) 使用 `lastAdvance_.value_or(GameTime{})` 构造 context，随后只调用 ApplyExplicit 和发布。Step 仅在有到期变更或游戏域定时器时 Advance，因此一次普通唤醒可以更新 ClockService 而保留旧结算地板。

两个独立探针覆盖不同路径：算法探针通过 `WakeTick(day 1)` 得到 **18 vs 20**；fresh 探针另加入 day 1 的排队 Set(10)，在 day 2 同步 Set(20) 后才 Advance，得到 **8 vs 20**。后者还显示较早业务消息可能覆盖较新的同步命令。[B02 计划:21](<E:/source/repos/SLSF Reloaded NG/docs/superpowers/plans/2026-10-27-runtime-and-compat-plan.md:21>)与 [design.md:97](<E:/source/repos/SLSF Reloaded NG/.scratch/native-migration/design.md:97>)要求同步事务处理当前业务时间。

修复方向：Execute 取得当前钟源，在同一事务中先按业务时间消费到期命令并结算，再应用本次命令、更新 revision 和发布。结算失败时返回对应结果，命令保持未提交。需要提取锁内结算步骤，避免持锁递归调用公开 AdvanceTime。

### F2：插入位置决定同刻顺序

[Dispatcher.cpp:371](<E:/source/repos/SLSF Reloaded NG/src/runtime/Dispatcher.cpp:371>) 的 lower_bound 已经把后入队命令放在前面；Settlement 的稳定排序无法恢复原入队序。Scheduler 自己的 `(deadline, sequence)` 堆序正确，错误位于另一条 ScheduleChange 队列。[optimization-plan.md:91](<E:/source/repos/SLSF Reloaded NG/.scratch/native-migration/optimization-plan.md:91>)明确规定同刻保序。

修复方向：用 upper_bound 插在现有相等项之后，或显式保存 enqueue sequence。当前有序 vector 可继续保留。

### F3：wait 专项保护没有覆盖普通任务与 dirty 任务

[Dispatcher.cpp:535](<E:/source/repos/SLSF Reloaded NG/src/runtime/Dispatcher.cpp:535>) 对 AcquireDue 返回的 bodies 直接逐个调用。NewSession 能使堆/槽中的旧任务失效，已移到局部 vector 的任务仍可执行。现有 weak WaitEntry 测试覆盖了等待恢复，普通 Schedule 没有同样的消费检查。

fresh 还复现：旧 Active 批次首回调重新 Attach 并注册同 RegionId 的 dirty 请求；随后旧 dirty body 调用 FireDirty，提前清掉新请求并调用 sink，新请求的定时器仍留在堆中。输出为 `sink_calls=1 pending_dirty=0 active_timers=1`。这是当前内建 dirty 路径的反例。[05 任务书:23](<E:/source/repos/SLSF Reloaded NG/docs/superpowers/plans/2026-10-27-task-books/05-scheduler-and-dispatcher.md:23>)要求换代失效。

修复方向：待执行任务携带取得它时的 session generation/依赖 epoch，每次调用前复核。保留统一串行执行器及既有域序；dirty 请求同时核对自身所属会话。

### F4：待清理死项与累计丢弃次数应分别计数

[Scheduler.cpp:104](<E:/source/repos/SLSF Reloaded NG/src/core/time/Scheduler.cpp:104>) Cancel 立即释放槽体，但没有登记仍留在堆中的失效项；[Scheduler.cpp:221](<E:/source/repos/SLSF Reloaded NG/src/core/time/Scheduler.cpp:221>) NewSession 也依赖尚未增长的 discarded 触发压缩。探针对重复 Cancel 和重复 NewSession 均复现 1000 条元数据积存。[optimization-plan.md:103](<E:/source/repos/SLSF Reloaded NG/.scratch/native-migration/optimization-plan.md:103>)要求处理取消密集场景。

这些条目保存期限/槽/代次，任务闭包已释放。问题是失效元数据及后续扫描成本增长，当前证据没有闭包泄漏。修复方向：单独维护 pending tombstones，取消/换代时登记、弹出时扣除、压缩后重置；以死项数量和占比触发重建，保留公开累计 DiscardedCount 的含义和稳定 sequence。

### F5/F6：准备和验收边界

[Dispatcher.cpp:230](<E:/source/repos/SLSF Reloaded NG/src/runtime/Dispatcher.cpp:230>) 锁内调用 PrepareSettlement 与 [design.md:103](<E:/source/repos/SLSF Reloaded NG/.scratch/native-migration/design.md:103>)的锁外准备要求不同。先用自有输入构建候选，再在提交锁内核对 epoch/revision 并替换；需要由 B03 明确 RuntimeBindings 借用对象的寿命，避免仅把当前调用机械移到锁外。

[TargetProbe.cpp:134](<E:/source/repos/SLSF Reloaded NG/tests/probe/TargetProbe.cpp:134>)静态检查通过，游戏菜单/加载/空闲节奏仍待实测。B01 应标部分完成；实机项按 [运行计划:15](<E:/source/repos/SLSF Reloaded NG/docs/superpowers/plans/2026-10-27-runtime-and-compat-plan.md:15>)记录入口、线程上下文、菜单状态及三钟增量。B03/B06 后续接线缺失没有额外计作本组缺陷。

## 结果保持一致的处理改进

以下建议保留当前正常输入的返回值、顺序和异常行为；与 F1–F4 的错误修复分别验收。

| 编号 | 代码与成本 | 推荐过程 | 现有测量 |
|---|---|---|---|
| O1 | Step 每次 `events_.erase(begin())`，整批 O(E²) 搬移 | deque 逐个 pop_front，或带正确生命周期处理的 vector head cursor | 同 payload 的 5000 事件容器排空：28.6748 ms → cursor 25.3 µs / deque 23.7 µs，checksum 相同 |
| O2 | Step 用完整 DueCount 询问是否存在到期任务，逐次 O(H) | Scheduler 增加 HasDue；正常未来根节点直接返回，遇到过期死根先清理并继续检查 | 128/1024/8192 live future 项的真实 DueCount：52.2 ns / 750.6 ns / 2.5866 µs 每次 |
| O3 | PublishedState 的地区查询 std::find，当前生产发布为 `regionIds[i]=RegionId(i)` | 先验证 identity 下标并直取；重排 DTO 保留原 find fallback | fresh 重排 DTO 查询 region 2 正确返回 22；本轮未测该 fastpath |

O1 数字是局部容器测量，包含相同 callback payload，未包含生产 Step 的锁、钟源或 epoch 检查。采用时应保留入队顺序、回调中新 Post 当轮可见、Detach 清队列、异常后余项留存及逐项 epoch 校验。单次把整批交换到局部容器会改变这些行为。

O2 的 11.8 ns root-key 对照只有同类 mutex 和小根堆键，没有生产 slot/session/generation 校验，是无死项路径的比较下限。实际 HasDue 需要维护死根回收语义；仅在确有 live due 项时推动 Advance，保持无工作时的 revision 行为。精确计数接口继续保留。

O3 对应 [ReputationState.cpp:497](<E:/source/repos/SLSF Reloaded NG/src/core/reputation/ReputationState.cpp:497>)及 [Query:525](<E:/source/repos/SLSF Reloaded NG/src/core/reputation/ReputationState.cpp:525>)。[PublishedState](<E:/source/repos/SLSF Reloaded NG/include/slsf/core/Snapshots.h:108>)允许 regionIds 描述各块身份，直接删除 find 并假定所有 DTO 都为 identity 会读错合法重排数据。

## 内存、堆栈和线程

- 当前实现使用 vector 堆、槽表、free list、std::function、shared/weak_ptr 和协程帧。范围内没有自建 pool/arena、pmr 或自定义协程 allocator。旧快照由读者 shared_ptr 保留，属于明确的共享所有权。
- SpawnFlow 把 callable 保存在堆上，由 promise 的 keepBody 管理；WaitEntry 采用共享所有权、timer 采用 weak_ptr。取消/Detach 在锁内翻转 live，锁外恢复。当前 ASan 和独立探针覆盖这些已执行路径。
- F3 是同线程重入引起的会话逻辑错误，ASan 无报错与该错误同时成立。单业务执行器继续适用；本轮没有建议增加业务线程池。
- **C1 条件风险**：RetireSlotLocked/NewSession 在 scheduler mutex 内销毁任意 TaskBody captures。fresh 用 capture 析构调用 PendingCount 的合法 C++ RAII 对象复现异常终止，子进程退出 **3221226505 / 0xC0000409**，停在 `capture_destructor=enter`。初审的“死锁”措辞以本机实测校正为重入无法正常完成；普通内建 wait/dirty captures 没有该行为。可将释放的 closures 移到锁外析构，并同时检查外层 dispatcher lock 的重入边界。
- **C2 条件风险**：ClockService 在 mutex 内调用注入的 WallNowFn。fresh 的自定义 supplier 调用同一服务的 Real()，同样在输出 enter 后以 `0xC0000409` 退出。默认 steady_clock 和现有 fake 均不重入；扩展 supplier 应明确调用约定，或在锁外采样并处理串行化顺序。
- Suspend 在 round 开始时阻止消费；回调中途 Suspend 后当前轮后续任务和事件仍执行。fresh 记录了这一行为，当前规范缺少“中途立即停止”条款，列为接口语义说明，未新增缺陷。
- 本轮未发现已复现的越界、UAF、双重释放或 pool/arena 寿命问题。独立 Clang ASan+UBSan session-batch 复放无诊断；未进行 TSan、LSan、栈峰值或最坏调用深度测量。线程结论沿用公开合同：同一 Flow 的 Start/await/destroy 与它的回调串行执行。

## 交付记录核对

原 [05 交付记录](task-groups/05-scheduler-and-dispatcher.md)仅作为待核查主张：

| 主张 | 实际裁决 |
|---|---|
| 取消/换代时任务失效 | 堆内任务及 wait 特例有保护；已提取 raw/dirty 批次存在 F3 |
| 死项压缩已实现 | 到期弹出的路径存在；取消密集的远期回收存在 F4 |
| Execute 同步发布 | 发布在返回前完成，但结算步骤缺失，见 F1 |
| 同期限稳定 | Scheduler 堆序通过；ScheduleChange 等时插入存在 F2 |
| 事件抛异常只影响该事件 | 异常会离开当前 Step，后续事件留队，下次 Step 再消费；不会自动继续当前轮 |
| 仅 WakeSource 为待完成项 | F1–F5 表明还需修正代码；WakeSource 实机本身确实 pending |
| 1170 环境写 SKSE2.6.x | 记录笔误；项目选定目标为 SKSE **2.2.6** |

原报告所称 ASan/异常处理工具链限制，本轮没有重建其历史最小复现。异常控制流采用本轮 Release 探针作为证据。

## 本轮验证与复放

| 检查 | 结果 |
|---|---|
| 普通 / ASan 核心 | 各 210 cases，128222 assertions，通过；JUnit 300 个展开实例 |
| 普通 / ASan 边界 | 各 4 cases，22 assertions，通过 |
| 差分 | 8 cases，17161 assertions，通过 |
| fuzz | 100000 次，seed 1170，通过 |
| ASan 正/负探针 | 正例通过，负例按预期 heap-buffer-overflow |
| 插件 / EngineProbe | 构建通过；STATIC_PASS，运行时唤醒和游戏查询 pending |
| 合同与任务书 | 11/11 坏例拒绝；11 组、25 任务、37 CPP 唯一归属、11 桌面提示、101 链接通过 |
| 归档与源码 | 6 份 Markdown、44 个本地链接、17 处行号引用及围栏通过；19 件归档与最终产物一致；固定源码树、HEAD 和新增材料空白检查通过 |

独立入口探针和完整日志位于 [归档索引](review-05/README.md)。`ARCHIVE_REPLAY=PASS` 表示归档源码可以编译，并复现报告记录的当前行为；其中错误行为仍然存在。固定源码树及 HEAD 由独立 index 复核，源码、原测试、真实 index 未修改。
