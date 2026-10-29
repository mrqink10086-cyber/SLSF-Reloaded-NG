# 05 / 06 组独立审核与逐项对抗验证

日期：2026-10-29。固定提交：`184c964eb226f614169a6b582c3adbd9970a04af`。比较起点：`5222a3c31841426bb26c3635a37638abb8f707e5`。范围：B01/B02/B03/B05/C01 的实际实现、调用方、依赖接口、测试及交付报告。

三名 `gpt-6-luna / xhigh` 子代理分别负责 Standards/内存与线程、Spec/生命周期、算法与等结果优化。第一轮独立读代码，第二轮逐项反证原报告；主代理核对生产入口，并重新构建、执行现有测试及复放独立探针。生产源码、既有测试和原报告保持原样，本轮新增本报告与诊断材料。

## Standards

发现三项有明确触发条件的 API 风险；当前生产 DDNG probe 和 main 调用路径没有触发这些条件。未新增可举证的强制规范违规或必须重构的 Fowler smell。

### S1 / P2 条件风险：业务协程异常终止宿主

[Dispatcher.h:114](<E:/source/repos/SLSF Reloaded NG/include/slsf/runtime/Dispatcher.h:114>) 明确执行 `void unhandled_exception() noexcept { std::abort(); }`。在 `SpawnFlow` 的 coroutine body 中抛出异常，无论发生于初次执行还是 `Delay` 恢复后，均由 promise 直接终止进程。[Dispatcher.cpp:542](<E:/source/repos/SLSF Reloaded NG/src/runtime/Dispatcher.cpp:542>) 所述的“throwing flow”隔离与 `:562` 的 timer-body catch 覆盖不到这条路径。

独立 EXE 在 `co_await Delay(0, Active)` 后抛出 `runtime_error`，子代理和主代理均复现退出 `-1073740791`（`0xC0000409`）。该退出码在本探针对应显式 `abort` 路径。现有 timer-body 异常测试证明普通 callback 的隔离；协程 body 具有不同的异常边界。

处理方向：明确 Flow 的失败策略；若业务要求隔离异常，在 promise 中保存/报告失败并正常结束帧，再验证同批后续任务仍执行、帧与 keepBody 各销毁一次。当前 `main.cpp` 没有 `SpawnFlow` 的生产调用点，故按条件风险记录。

### S2 / P2 条件风险：已加载但仍 pending 的能力被永久缓存为不兼容

[CapabilityRegistry.h:42](<E:/source/repos/SLSF Reloaded NG/include/slsf/integrations/CapabilityRegistry.h:42>) 规定 pending 结果进入 Waiting 并重试。[CapabilityRegistry.cpp:76](<E:/source/repos/SLSF Reloaded NG/src/integrations/CapabilityRegistry.cpp:76>) 却只在 `!probe.loaded` 时考虑 pending；`loaded=true, pending=true` 且 API 尚未就绪会进入 Incompatible，随后被 `:123` 的终态分支永久缓存。

探针第一次返回“模块已加载、API 待定”，若第二次查询则返回可用 API；实际输出为 `first=Incompatible second=Incompatible calls=1`。子代理发现后，主代理独立编译复放一致。当前 [main.cpp:143](<E:/source/repos/SLSF Reloaded NG/src/main.cpp:143>) 在 DataLoaded 前 API 为空时设 `loaded=false, pending=true`，避开了此组合。

处理方向：统一 pending 与 loaded 的优先级，或在公共契约中明确并验证二者合法组合。继续保留真正不兼容结果的终态缓存语义。

### S3 / P2 条件风险：能力查询重入或依赖成环会永久等待

[CapabilityRegistry.cpp:128](<E:/source/repos/SLSF Reloaded NG/src/integrations/CapabilityRegistry.cpp:128>) 对正在查询的 entry 无条件等待；外层仅在 provider 返回后的 `:157` 清除 querying。若 provider 同步调用 `Ensure(A)` 查询自身，或 A→B→A，内层等待外层，外层等待 provider，形成闭环。锁外调用 provider 解决了普通 sibling 查询的 mutex 重入问题，但没有解决查询依赖环。

主代理的独立进程输出 `provider entered; synchronously reentering the same capability` 后，2 秒内不返回，由父进程终止；源码中的互相等待给出了无限等待机制。当前 DDNG query 不调用注册表。处理方向可为查询重入/依赖环检测，或明确禁止同步循环依赖；仍需保留合法 sibling 查询和并发重复请求共享结果。

## Spec

确认两项生产路径缺陷，均为 P2。另有两项原报告已披露的集成待办：生产执行泵和真实 JSON source。

### L1 / P2：游戏加载失败后仍会激活已准备会话

[src/main.cpp:437](<E:/source/repos/SLSF Reloaded NG/src/main.cpp:437>) 对所有 `kPostLoadGame` 无条件调用 `Activate("post-load")`，未读取加载结果。

这条触发序列得到宿主源码支持：

1. SKSE 派发 PreLoadGame，插件关闭旧会话。
2. SKSE 调插件 serialization load callback；有效 co-save 可成功 `PrepareLoad` 并留下 pending。
3. 游戏加载返回 false。
4. SKSE 派发 `PostLoadGame(false)`；当前入口仍将 pending Attach，并把 phase 设为 Active。

宿主依据为 [PluginAPI.h:204](<E:/source/repos/skse64/skse64/PluginAPI.h:204>)、[PreLoad 派发:49](<E:/source/repos/skse64/skse64/Hooks_SaveLoad.cpp:49>)、[PostLoad 派发:51](<E:/source/repos/skse64/skse64/Hooks_SaveLoad.cpp:51>)、[Papyrus 加载回调:323](<E:/source/repos/skse64/skse64/Hooks_Papyrus.cpp:323>) 和 [插件 load 调用:483](<E:/source/repos/skse64/skse64/Serialization.cpp:483>)；本地 SKSE 源提交 `9398d04592a7eb9d754f2997701116df1022f1b4` 的主题为 `address update for 1.6.1170`。激活逻辑在 [Lifecycle.cpp:285](<E:/source/repos/SLSF Reloaded NG/src/runtime/Lifecycle.cpp:285>) Attach，并在 [Lifecycle.cpp:292](<E:/source/repos/SLSF Reloaded NG/src/runtime/Lifecycle.cpp:292>) 置 Active。

[LifecycleTests.cpp:320](<E:/source/repos/SLSF Reloaded NG/tests/core/LifecycleTests.cpp:320>) 的“failed load”测试输入是坏 schema、重复键和主动 RejectPreparation；没有覆盖“Prepare 成功、宿主最终失败”的路径。因此现有全绿结果与本缺陷同时成立。

修复方向：在 PostLoadGame 读取宿主成功标志，失败时清除 pending 并保持业务关闭。SKSE 当前实现以 `(void*)result` 传递结果，处理时按该消息约定检查值，不能将其当 bool 指针解引用。补测应包含 Prepare 成功→PostLoad(false)→下一次成功加载。当前证据为插件与宿主源码控制流，真实游戏失败读档尚未执行。

### L2 / P2：ESL/light plugin 的“稳定”地区键仍含加载索引

[src/main.cpp:195](<E:/source/repos/SLSF Reloaded NG/src/main.cpp:195>) 用 `GetFormID() & 0x00FFFFFFu` 生成 `localFormId`。对 `0xFE......` 的 light form，低 24 位仍含 small-file load index。同一插件和同一本地 form 在 light index 改变后会得到不同 key。

[ReputationState.cpp:438](<E:/source/repos/SLSF Reloaded NG/src/core/reputation/ReputationState.cpp:438>) 按完整 StableRegionKey 恢复；不匹配的旧记录进入 unresolved，当前地区账户保持初始值。项目要求跨加载顺序按稳定身份恢复，见 [runtime plan:47](<E:/source/repos/SLSF Reloaded NG/docs/superpowers/plans/2026-10-27-runtime-and-compat-plan.md:47>)。

主代理探针链接当前真实 `RestoreState`，对同一 `audit.esl` 的 `0xFE012ABC` / `0xFE037ABC` 复现：

```text
canonical=0 oldKey=012ABC newKey=037ABC restored=0 unresolved=1 currentLocal=0.0
canonical=1 oldKey=000ABC newKey=000ABC restored=1 unresolved=0 currentLocal=37.5
```

第二行是手工提取 light form 12 位本地 ID 的控制组。生产修复应使用依赖提供的 [TESForm::GetLocalFormID()](<E:/source/repos/SLSF Reloaded NG/lib/commonlibsse-ng/include/RE/T/TESForm.h:305>)，其同时剥除普通 compileIndex 和 smallFileCompileIndex；不要对所有插件统一换成 12 位 mask。还需确定旧错误 key 的迁移策略。探针使用合成 FormID 并调用真实 RestoreState，未调用运行时 TESForm API；游戏内 ESL 重排待验。

Spec 轴计数为 2 项 P2；Standards 轴计数为 3 项条件风险、0 项新增强制规范违规。Standards 最严重后果是协程异常终止宿主；Spec 的两项分别影响失败加载门控和跨 ESL 索引恢复。

## 05 原报告逐项裁决

对照 [05 交付记录](task-groups/05-scheduler-and-dispatcher.md)、[历史独立审核](2026-10-29-05-independent-review.md) 与 [修复/离线探针记录](2026-10-29-05-fixes-and-offline-probe.md)。历史审核对应旧树 `4c72b3da398994ae3e1c0d4567545ee7c1713696`，不能直接当作当前缺陷列表。

| 主张/历史编号 | 当前裁决与代码依据 |
|---|---|
| 三域、同期限稳定 | 三个独立堆，deadline+sequence；Scheduler 现有差分/边界用例通过。 |
| F1 Execute 先结算再写入 | 已修复；Dispatcher.cpp:341-358 取当前游戏钟，AdvanceLocked 成功后才 ApplyExplicit。 |
| F2 同刻 ScheduleChange FIFO | 已修复；Dispatcher.cpp:377-384 的 upper_bound 插在相等旧项后。 |
| F3 提取后的 timer/dirty 跨会话 | 同线程换代路径已修复；Dispatcher.cpp:544-568 每项复核 generation/session，:491-504 的 dirty 校验原 generation。 |
| F4 远期取消元数据积存 | 已修复；Scheduler.cpp:120-124、:171-173 在取消/提取时压缩，NewSession 清旧堆。 |
| F5 Attach 锁内准备 | 已修复；Dispatcher.cpp:217-255 在首个锁区间外构建候选与 workspace，提交时复核 generation。 |
| F6 WakeSource | 本轮离线 EXE 再次 PASS；05 已采用离线验收。真实 SKSE AddTask/菜单/空闲驱动仍是 B03 集成待办。 |
| C1 capture 析构重入 | 锁外释放已实现；Scheduler/Dispatcher 用局部退休容器承接闭包。现有回归通过。 |
| C2 墙钟 supplier 重入 | 锁外采样/释放，sourceGeneration 防旧源提交；现有回归通过。 |
| Flow/WaitEntry 释放一次 | keepBody 保持 callable 寿命，shared/weak WaitEntry 与 live 翻转防重复恢复；代码及普通/ASan 既有用例支持执行过的路径。 |
| Game Delay 单位、菜单三钟 | 秒到游戏日 /86400；菜单切换按即时墙钟结算；ClockService 用例及离线探针通过。 |
| 非有限/倒退时间失败不提交 | AdvanceTime 先验证，成功后更新地板；核心 Advance 在 workspace 中准备，成功才擦除 changes。 |
| epoch、dirty、同步发布 | 单 executor 合同下有消费检查、1 秒 Active 合并和返回前发布；不推导任意并发生命周期修改安全。 |
| 事件异常行为 | 当前报告描述准确：事件已出队，异常离开本轮 Step，余项留队；契约没有要求当轮继续，不新增缺陷。 |
| O1 deque 事件队列 | 已实现，保留 FIFO、同轮新 Post 可见及异常后余项。历史性能数字本轮未重新对照旧树。 |
| O2 HasDue 堆顶判断 | 已实现，清理无效根后判断最早活项；精确 DueCount 仍保留。 |
| O3 Query identity 快路径 | 已实现；ReputationState.cpp:533-540 先验行身份，非 identity DTO 回退 find。 |
| 历史 clang-cl ASan 异常限制 | 原最小复现本轮未重建；S1 已用 MSVC 独立 EXE 复现，退出由显式 abort 决定。 |
| 219 cases / 129271 assertions | 是 05 阶段记录；当前 HEAD 本轮普通/ASan 均为 238 / 132308。 |

附带复核的历史 D1 也已修复：Settlement.cpp:618-623 排序 workspace 副本，:695-697 仅成功后擦除原 changes，失败保持原队列顺序。

## 06 原报告逐项裁决

对照 [06 交付记录](task-groups/06-lifecycle-save-and-capabilities.md) 验收表的每一行：

| 要求/行号 | 当前裁决 |
|---|---|
| ISA / :72 | baseline TU 有 CPUID、OSXSAVE/AVX、XGETBV YMM、AVX2 检查，数值入口可回退 scalar；当前构建/核心测试通过。历史启动日志不能替代当前 HEAD 的目标进程加载。 |
| 失败加载业务开关 / :73 | **不通过完整生产验收**：L1 表明 PostLoadGame(false) 被忽略；原测试只覆盖准备/解码失败。 |
| A→B→A / :74 | 内存 PersistentState 的 A/B 隔离与 unresolved 重绑有测试；尚无 SKSE serialization 回调端到端存/读档证据。 |
| 稳定身份/静态表 / :75 | 核心索引/绑定与 parentLoc 投影存在；L2 表明 ESL 加载索引变化仍失败。“通过”需缩小到合成 key 的核心层。 |
| epoch 异步提交 / :76 | EventBridge Enqueue/Flush 的盖章、二次校验、4096 上限和启动批次预算有代码及测试；生产 src 没有 Flush 调用。 |
| 迟到能力/重复合并 / :77 | 普通 sibling 与并发重复请求共享结果成立；当前 DDNG Waiting→Available 有历史日志。公共 API 仍有 S2/S3 两个条件风险。 |
| 当前跨档配置 / :78 | Lifecycle 每次 Begin 调 Reload 并取当前 typed snapshot；BootstrapConfigurationSource 仅默认值/空 connections，真实 JSON 重读属第 10 组待办。 |
| SKSE 记录与 codec / :79 | Capture→锁外 EncodeSaveRecord→WriteRecord；缺失、重复、短读、坏 envelope/schema 拒绝路径存在；两个 fuzz 各 100k 通过。实际游戏读写仍待验。 |
| 目标运行时加载 / :80 | 历史日志确有 766/766/81 和 DDNG version=2/api=2；阻断日志确有 runtime 1.7.104 / SKSE 2.2.6。当前 HEAD 本轮完成编译，未执行目标进程。 |
| 真实交互存/读档 / :81 | 原报告标“未执行”准确；仍未执行。 |

其余实现和文字核对：

- `:27` 的实际 phase 是 Detached/Preparing/Active/Failed；`:28` 所写 `Lifecycle::Save` 并不存在，生产路径是 `Lifecycle::Capture → EncodeSaveRecord → WriteRecord`。锁内 Capture、锁外编码这一核心语义正确。SKSE owner ID 是 `SLSF`，record type 是 `SLSV`。
- `:29` 的进程级地点表和每会话状态分离成立；跨存档核心恢复按 stable key 匹配，生产 key 生成错误见 L2。
- `:32` 的“provider 查询锁外执行”成立；`:93` 若被理解为所有重入死锁都已修复则过宽，S3 给出了反例。
- `:33` DDNG 最小 probe 只同步调用 GetVersion；生产 requirement 的 minimumStructSize/requiredFunctionCount 均为 0，完整函数表与设备业务仍未验收。
- `:86-88` 已明确披露 JSON、生产泵、真实存读档和 DDNG 设备业务待接入。生产 src 没有 `Dispatcher::Step/WakeTick` 或 `EventBridge::Flush` 驱动，main 的 EventBridge handler 目前只 Post 空业务回调；这些属于可定位的集成缺口。
- 依赖锁定表的“消息/保存回调在主线程”不宜作为全局线程保证：归档日志中 postload 与 dataloaded 的线程 ID 不同。线程结论应限定到宿主阶段顺序和单 executor 合同；该日志本身不足以判定存在数据竞争。
- `.scratch/native-migration/spec.md:97-99` 将 B03/B05 记为已实现，`:132` 又称最小入口/游戏接入待完成；应按上表区分模块完成与端到端待验。

已调查但未计入确定缺陷的候选：

| 候选 | 对抗裁决 |
|---|---|
| 跳过未知 SKSE record 时没有主动 ReadRecordData | 宿主 Serialization.cpp:232-263 的 GetNextRecordInfo 会先跳过上一 record 剩余数据；这条使用方式合法。 |
| 所有地点都注册成 key，子地点无法继承父地区 | RegionRegistry 会先把所有权链归并到规范 owner，再选择地区；不能仅从全部 keys 入表推导该缺陷。 |
| ActivatePrepared 二次调用没有 pending，置 Failed 但旧 dispatcher 仍 attached | 代码行为存在；常规宿主每次加载只发送一次 PostLoadGame，下次 PreLoad 会 Detach。保留为公共 API/异常消息边界，不计普通生产加载缺陷。 |
| Lifecycle 把 spreadRetention=0 转成 0.6 | Lifecycle.cpp:158-161 确有归一化；边 retention=0 关闭边的规范明确，但全局参数 0 对 custom connection 的含义尚未明确。当前默认 source 为 0.6 且无 connections，暂列配置契约待定。 |

## 内存、堆栈和线程结论

本轮在所执行路径中未发现 ASan 报告的越界/UAF、重复协程帧销毁或 stale callback 执行业务。取消与换代的任务体锁外释放，等待项由共享所有权和锁内 live 翻转保证单赢家，锁外恢复。保存 DTO 在事务锁内捕获，编码和 SKSE IO 在锁外；解码有记录长度上限、读前剩余字节与共同预算检查。LocationIndex 与地区森林使用显式 vector stack 遍历。

线程安全按公开合同评估：Step/WakeTick/AdvanceTime 由单 executor 驱动，同一 Flow 的启动/等待/销毁与恢复串行。Lifecycle owned assets 及其变更是主线程状态；EventBridge 的跨线程 Enqueue 会读取 BusinessOpen、CurrentEpoch、IsEpochCurrent，这些读取由 atomic phase 与 dispatcher mutex 同步。当前检查未扩展成任意线程调用所有 Lifecycle/Flow 方法的保证。S3 是已证实的同步等待闭环。

验证方法为普通测试、ASan、parser fuzz、源码检查和独立进程探针；TSan、LSan、栈峰值/最坏调用深度和真实游戏交互未执行。

## 结果不变时的处理改进

### 有实测依据的候选：等待项批量退休

[Dispatcher.cpp:147](<E:/source/repos/SLSF Reloaded NG/src/runtime/Dispatcher.cpp:147>) 的 RetireWait 每次对 waits_ 执行 erase_if。W 项连续退休的扫描量为 W+(W-1)+…，呈 O(W²)。子代理链接当前库测量，主代理独立编译复放；各规模 5 轮，创建等待项在计时外。下表为主代理复放中位数：

| 等待项数 | 同时到期 Step | 逐项 CancelWait |
|---:|---:|---:|
| 100 | 28 μs | 12 μs |
| 1000 | 415 μs | 292 μs |
| 5000 | 6918 μs | 6573 μs |

所有轮次 completed 等于输入数、order_ok=1；同规模 checksum 一致。到期校验值分别是 `0x7d6bcd620df29609`、`0x5ba76aa473ab768d`、`0x9b964c68f95aed2d`。取消校验值分别是 `0xc9f171bb55b541c5`、`0xd7377f145c60eaa5`、`0x36596c7e9749a5`。从 1000 增至 5000 项，耗时约增长 16.7 / 22.5 倍。测量包含恢复、scheduler 回收等真实入口成本；子代理首轮 5000 项到期中位数为 6776 μs。

若实际同时等待数经常达到数千，优先对比支持 O(1) 擦除的保序结构；必须保持 Detach 的恢复次序、单次恢复、锁外 resume、取消 timer 和 frame/keepBody 寿命，并同时测节点分配与内存成本。当前尚无真实等待数分布，也未实现候选结构，因此这是一项有负载依据的优化研究结论，收益幅度待候选实现对比。

### 其余候选及等价条件

| 过程 | 候选 | 结果保持条件 / 当前判断 |
|---|---|---|
| Advance 对已排序 changes 再 stable_sort | 检查有序后走线性路径；任意未排序 core 输入保留原排序 | 同刻 FIFO、非法输入错误顺序、失败不改原队列、发布时机必须一致；未测收益。 |
| AcquireDue 每域新建 vector | 测量后评估显式复用 scratch | 保持 deadline/sequence、Game→Real→Active、逐回调 session 检查与重入隔离；未测收益。 |
| Save 先建 payload 再复制 envelope | 直接写最终缓冲并回填 header/checksum | v1/v2、字节序、字段顺序、checksum 与错误优先级逐字节相同；Capture/编码锁边界保持；未测收益。 |
| Restore 的 map/set 查找 | 在已证明排序的输入上评估 merge-walk | unresolved 顺序、重复键错误顺序、provider 语义、失败前不提交保持；未测收益。 |
| Capability deque 线性查找 | 先测真实能力数量 | 当前只有少量能力，暂没有引入哈希索引的性能证据。 |
| Scheduler slots 保留峰值容量 | 先测峰值与会话后常态内存 | 属现有分配复用取舍；TimerToken 不含 session，直接清槽并重置 generation 会造成旧 token 别名风险。 |

已落地 O1/O2/O3 通过源码语义核对；历史微基准数字保留为历史测量。本轮新增性能结论仅为等待项退休的测量。

## 本轮验证与复放材料

| 检查 | 本轮结果 |
|---|---|
| 重新构建 Core/Boundary/ASan/OfflineProbe/插件 | `xmake build -y -j8 SLSFCoreTests SLSFBoundaryTests SLSFCoreTestsAsan SLSFBoundaryTestsAsan SLSFDispatcherOfflineProbe SLSFReloadedNG`，退出 0。 |
| 普通核心 / ASan 核心 | 各 238 cases、132308 assertions，seed 1170，退出 0。 |
| 普通边界 / ASan 边界 | 各 4 cases、22 assertions，退出 0。 |
| 离线执行器 | heartbeat=3600、frames=3600、simulated_seconds=60、max_pump_us=4.2、long_wait_seconds=1000000、completions=3、stale_callbacks=0，PASS。 |
| StateCodecFuzz / SerializationFuzz | 各 100000 runs，seed 1170，退出 0；原始日志已归档。 |
| ESL key / RestoreState | 当前 mask 0 恢复、1 unresolved；控制组 1 恢复、值 37.5。 |
| 协程异常 | MSVC 子进程退出 -1073740791，主代理重复复现。 |
| pending 能力 | Incompatible / Incompatible / calls=1，重复复现。 |
| 查询重入 | 进入 provider 后等待不返回；2 秒后父进程终止。 |
| 等待项规模测量 | 30 条记录，完成数、顺序与 checksum 全部符合探针预期。 |

源代码、运行脚本和原始输出见 [review-05-06/README.md](review-05-06/README.md)。其中 abort 和 timeout 用于复现缺陷；脚本正常退出表示完成采集，各子进程结果见 results.json。
