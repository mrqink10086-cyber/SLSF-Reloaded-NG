# 原脚本的时间、轮询与同步状态语义审计

日期：2026-10-26。对象：`E:/source/repos/SLSF Reloaded/Scripts/Source` 的 17 份 PSC 中全部时间注册、延迟等待和相关业务调用链；另对照本地 DbSkseFunctions 的计时器实现。

本文记录原源码行为及计时参考。当前要求见[项目规格](../../.scratch/native-migration/spec.md)，实现进度见对应 evidence。旧 MCM 缺少源码，其额外初始化与菜单单位换算仍需从其他资产核查。

调度的现行结构见[接口生命周期设计](../superpowers/specs/2026-10-26-integration-state-lifecycle-design.md)：进程能力复用，本档动态状态和等待任务按会话管理。

## 1. 时间单位与入口清单

本地 `E:/source/repos/skse64/scripts/vanilla/Utility.psc:8-38` 明确：`GetCurrentGameTime()` 返回游戏经过天数，`Wait()` 使用秒且菜单模式不计时，`WaitMenuMode()` 使用秒且菜单模式计时，`WaitGameTime()` 的参数才是游戏小时。

`Form.psc:33-56` 区分秒单位的 Update 和游戏小时单位的 UpdateGameTime。下表的秒数按对应现实时间或菜单计时累计，事件在游戏/VM 可投递阶段执行。

| 活动入口 | 节拍或延迟 | 要表达的含义 | 源码 |
|---|---|---|---|
| MasterClock 初始化 | `Wait(2)`，随后 `RegisterForUpdateGameTime(0.25)` | 短暂启动延迟，然后以约 15 个游戏分钟为唤醒粒度 | MasterClock:27-31 |
| MasterClock 结算 | 两个 15 分钟单位合为一个半小时单位 | 驱动衰减、传播、目击扫描、衰减保护期、Skooma 减少与兼容输出 | MasterClock:33-126 |
| DynamicAnonymity | 每 1 个游戏小时 | 检查当前地点停留是否已使匿名资格失效，并刷新综合匿名状态 | DynamicAnonymity:35-64 |
| CommentManager | 初次 10 秒；匿名时下次 10 秒；其他情况下下次 5 秒 | 更新评论许可与裸体评论条件，供对话系统读取 | CommentManager:13-47 |
| 动画分析 | 收到开始/前戏结束事件后 `Wait(10)` | 给玩家切换/取消动画和框架初始化留时间，然后判断声望 | SexAnimationAnalyzer:25-120 |
| 可见性刷新 | 完成计算后 `Wait(1)` 再清除忙标记 | 约一秒内抑制重复装备刷新；等待并不发生在计算之前 | VisibilityManager:163-203 |
| 声望增加提示 | 提示后 `WaitMenuMode(30)` | 提示限流，菜单中也经过这段时间 | FameManager:2623-2628 |
| 导入地点 | 每次新注册自定义地点后 `WaitMenuMode(3)` | 注册间隔/节流；从现有代码看没有等待明确完成事件 | Data_Importer:53-68 |
| 旧 SLSF 写入异常提示 | 特定错误分支 `Wait(0.5)` | 两个警告框之间的流程延迟，正常同步路径不等待 | LegacyOverwrite:170-175 |
| 卸载提示 | 停止 Quest 后 `Wait(15)` | 延迟显示“保存并退出”消息 | Uninstall:11-30 |

有效源码中只有三个周期性服务：MasterClock、DynamicAnonymity、CommentManager。共五处 Update 注册调用，其中 CommentManager 自己有三条注册分支。共七处活动的 Utility.Wait/WaitMenuMode 调用；没有活动的 WaitGameTime 调用。

`While` 主要执行地点/类别遍历和随机重试；等待通过明确的 latent 调用发生。Logger 的相关调试循环位于注释代码中。

## 2. 主时钟：唤醒粒度与结算粒度不同

源码实际计算：

```text
Q = 0.010417 游戏天，约 15 分钟
n = int((当前游戏天数 - LastTimeChecked) / Q)
HalfHourSync += n
m = int(HalfHourSync / 2)

若 m > 0：
    HalfHourChecks(m)
    HalfHourSync -= 2 * m

LastTimeChecked += Q * n
```

`LastTimeChecked` 按已消费的单位推进，保留不足一个单位的余量；`HalfHourSync` 保留不足半小时的余量。每次唤醒按实际经过时间求批次数，边界按小数常量和截断结果处理。

当游戏时间倒退时，只将 `LastTimeChecked` 改为当前值并返回；现有代码不同时清空其他累计字段。`Startup()` 本身也没有将 `LastTimeChecked` 显式设为当前时间；是否有旧 MCM 的额外初始化，现有 PSC 无法证明。

`HourSync`、`DaySync` 和整点处理路径属于未启用代码。FameManager 的 `DecayCountdown=24`、`SpreadCountdown=48` 只有赋值，递减与触发逻辑位于块注释中；有效计时由上述 Ticker 路径驱动。

## 3. 一个半小时批次具体做什么

`HalfHourChecks(m)` 严格按下列调用顺序执行：

```text
DecayHandler(m)
    累积 DecayTicker，求应衰减次数 d
    调用一次 DecayFame(d)

SpreadHandler(m)
    累积 SpreadTicker
    每到一个传播周期，单独调用一次 SpreadFameRoll()

PeriodicFameUpdate(m)
    UpdateFame(m)
    Skooma = max(0, Skooma - m)
    按配置生成旧 SLSF 兼容输出
```

`DecayTimeNeeded`、`SpreadTimeNeeded` 以半小时批次数比较，字段值 4 表示四个半小时单位。旧 MCM 的显示和写入换算待补齐源码或资产后核对。

### 3.1 衰减：一次抽样乘以欠账次数

`DecayFame(d)` 遍历可衰减且有名望的地点，对每个正值声望按当前档位抽一次衰减量，再乘以 `d`。默认地点使用配置的分档最大衰减值；自定义地点使用源码中写定的另一套分档值，最后将负数归零。

**一次 `DecayFame(3)` 与三次 `DecayFame(1)` 结果可能不同**：旧实现抽样一次再乘次数，逐周期调用则会多次抽样，还可能跨越声望档位。迁移默认保留旧抽样含义。

### 3.2 传播：有记忆的多次随机试验

`SpreadFameRoll()` 对有可传播声望的地点逐个处理：失败增加下一次传播概率，成功降低下一次传播概率，概率夹在 0–100。成功时再选择目标、类别和传播比例，并修改目标名望。

所以传播周期无法仅用一个总乘子替代。前一次结果会改变后一次概率和名望；遍历期间某个后续地点也可能受到前面传播的影响。

### 3.3 保护期：近期获得声望，暂不衰减

`GainFame()` 将该地点的 `CanDecay` 设为 false，并将 `DecayPauseTimer` 重置为 `int(Config.DecayTimeNeeded)`。这是地点级保护，覆盖该地点各声望类别。

`UpdateFame(m)` 每次扣掉 `m` 个半小时单位，归零后允许衰减。**扣保护期发生在本批次衰减和传播之后。** 若一次跳过很长时间，旧代码会先按调用时的保护状态处理整批衰减，然后才扣保护期；这与严格按每个截止时间交错执行不同。

传播结尾也意图给目标地点设置保护期，但代码固定写入默认地点数组，而目标来自合并的候选表；应作为索引/状态绑定问题单独修正，不作为新的业务规则照搬。

### 3.4 目击扫描：试图补偿错过的检查

`UpdateFame(m)` 将 `GainIterationMultiplier=m`，玩家不匿名时施放 NPC 扫描法术，然后立即将共享倍率重置为 1。NPC 的 OnEffectStart 按地点、死亡/战斗、SexLab 活动、LOS、距离和关系概率过滤目标；一个共享成功门闩尝试让整次扫描最多成功一次。

旧注释意图以一次放大的增量补偿错过的检查。实际实现立即重置共享倍率，法术回调异步到达且扫描共用状态，存在读到重置值或交叉影响的路径。NG 为实际目击请求保存独立身份和参数，跳时只推进传播、衰减与匿名记忆。

### 3.5 缓存维护和类别清理

`UpdateFame()` 周期检查可选框架和外部启用状态，清除已失去全部来源的四类声望，再调用 `Data.FameCheck()` 重算有声望和可传播类别。NG 在来源、类别设置或声望变更时更新这些汇总。

### 3.6 一个跳时例子

欠 12 个半小时批次时，原代码执行一次带乘子的衰减、所欠次数的逐次传播、一次扫描请求，随后保护期和 Skooma 各减 12。配置、保护期、声望档位及传播概率均按该调用顺序生效；NG 按已选时间规则处理各自的推进和边界。

## 4. 动态匿名：停留暴露与离开后被遗忘

该系统同时使用地点事件和每游戏小时的检查：

- 离开声望区域：记录离开时间，按此次停留时间增长 RecognitionTime。
- 进入新声望区域：记录进入时间，以离开了多久判断是否恢复本地匿名资格，并更新 RecognitionTime。
- 在当前区域停留：每游戏小时检查一次；从入场起达到 `0.125` 游戏天，也就是三游戏小时，取消本地匿名资格。因此实际取消存在检查粒度带来的延迟。
- 最终匿名状态：结合本地匿名资格、该地点全部类别是否低于名望阈值，以及 Thane 等禁止匿名规则。

进入、离开记账以声望地区变化为触发条件。同一地区内切换子地点时继续本次停留计时；原生实现通过地点归属判断地区变化。

停留计账区分原本认识玩家与本次匿名进入：匿名进入的前三小时不计入记忆，增长最高 365 游戏天。重新进入公式为 `R' = max(7, R - (离开时间 - R))`，即 `max(7, 2R - 离开时间)`；离开时间小于 R 时可能增大 R。项目已确定沿用该公式。

名望阈值资格由 SetFameValue/DecayDynamicAnonymityCheck 更新；每小时检查当前地点和停留时间，离开后的记忆在重新进入时按公式计算。

## 5. 现实秒数的轮询与延迟

### 评论许可

CommentManager 每轮维护 Public Whore/AND 的裸体评论条件。匿名时关闭评论许可，十秒后再检查；其他情况每五秒检查，战斗中禁用，否则按配置概率设置 `SLSF_AllowComment`，供对话条件读取。每次唤醒处理一次当前许可。

### 装备可见性

装备/卸下事件尝试在不忙时执行一次 FullUpdate，立即计算可见体液、纹身、束缚状态，随后等一秒才清除 UpdateRunning。因此忙时到来的新装备事件可能被直接跳过；这个结构表达的是合并/节流意图，现有实现没有明确的“忙期间变脏，结束后再处理一次”协议。CountingBodyTats 则标记纹身统计正在执行。

### 动画延迟分析

SexLab 开始和前戏结束事件均可启动十秒延迟分析，结束事件只把共享 SexSceneEnded 设为 true。分析器在等待前取得控制器，在等待后检查共享标志再读动画。它既没有把任务绑定到传入 threadID 的完整生命周期，也没有区分同一 ID 的不同场景代次。因此新场景将 bool 改回 false 后，旧等待任务可能无法区分自己的场景已经结束。这里真正需要的是场景身份、取消和等待后重新验证对象。

### 提示限流

FameGainNotification 立即发提示并设置 cooldown，三十秒后清除。旧 GainFame 直接调用该 latent 函数，声望和保护期先更新，兼容输出延后。NG 同步提交声望和输出，单独保存提示冷却截止时间。

## 6. 旧字段的业务含义

| 字段 | 实际角色 | 原生表示 |
|---|---|---|
| HalfHourSync、DecayTicker、SpreadTicker | 经过时间与周期余量 | 尚未消费的时间/下一次到期，不要求保留字段布局 |
| CanDecay、DecayPauseTimer | 近期声望增长的地点保护期 | 该地点什么时候重新允许衰减 |
| UpdateRunning、CountingBodyTats | 抑制刷新重入/重叠 | 有序刷新、事件合并及可靠收尾 |
| SexSceneEnded | 延迟分析的粗粒度取消 | 具体场景任务是否仍有效 |
| FameGainNotifyCooldown | UI 限流 | 下次何时允许显示提示 |
| NPCScanSucess | 单次扫描至多一个成功者的门闩 | 扫描 ID 下的一次性提交 |
| GainIterationMultiplier | 扫描补偿参数 | 归属本次请求的不可变参数 |
| 动态匿名数组中的 flags | 识别/匿名业务状态 | 按区域保存资格与记忆 |
| HourSync、DaySync、旧 Countdown | 当前 PSC 中未启用的遗留路径 | 不据此制造新的周期任务 |

## 7. 原生执行边界

一个执行器负责恢复、取消和销毁协程。各入口核对实际上下文；引擎对象访问切到合法阶段，纯数值写入按短锁事务提交，查询取得一致快照。[C++ 数据竞争规则](https://eel.is/c++draft/intro.races)说明跨线程数据需要同步，复合快照统一发布。

挂起前保存自有参数和稳定身份，恢复后核对 session/index/scene epoch，再解析所需对象。排队任务携带 ID 与 generation，帧生命周期由执行器持有。[协程恢复与销毁前提](https://eel.is/c++draft/coroutine.handle.resumption)

等待、外部调用和文件访问在核心锁外进行；RAII 处理取消、异常和提前返回。[MSVC C26810](https://learn.microsoft.com/en-us/cpp/code-quality/c26810?view=msvc-170)列出捕获 lambda 协程的引用生命周期问题。保存取得完整业务快照，加载完成绑定后再开放任务业务开关。

## 8. DbSkseFunctions 可借鉴的含义及静态风险

本地 `Papyrus/Scripts/Source/DbFormTimer.psc:4-47` 提供四种计时语义：普通 Timer 排除暂停时间，NoMenuModeTimer 排除所有菜单时间，MenuModeTimer 继续累计现实时间，GameTimer 以游戏小时计时。Form、Alias、ActiveMagicEffect 三套接收者共享同类计时服务，Timer ID 用于区分同一接收者的多个注册。

游戏计时部分采用 Calendar 总小时数和 endTime，加载后重新解析 VM handle、按当前游戏时间恢复等待。辅助线程的 UpdateLoop 定期向 SKSE 主线程队列投递 PollGameState，读取游戏状态后通知等待者。

参考实现有以下可定位的生命周期问题，接入时采用本项目的执行器和取消机制：

- GameTimeTimer::StartTimer（Timers.cpp:1072-1098）每个计时器创建 detached 线程并访问 this；cancelled/finished 及多个 empty 标记是普通 bool，读写没有统一的同步所有权。部分集合操作使用 mutex，不足以覆盖对象字段和对象生命周期。
- 等待谓词（1079-1081）仅以游戏时刻到期为唤醒条件，取消后仍会等到该时刻。新实现把取消和会话结束一起作为唤醒条件。
- EraseFinishedGameTimers（1158-1190）会删除已取消对象；其他等待线程和排队的 `[this]` 主线程任务可能仍引用它。存在释放后访问路径，需要在新实现中通过所有权与队列失效机制消除。
- GameTimeTimer 构造器（1057-1069）未初始化 startTime，查询、日志和保存却会读取它；loadTimers（1379）按剩余时间重建对象时也遗漏该值。新实现应初始化并恢复起始时刻，按同一时钟计算已运行时间。

以上结论来自源码静态分析。计时设计采用本项目的执行器和取消机制；复用 Dylbill/DbSkseFunctions 代码时保留署名及对应 LICENSE/EXCEPTIONS。

## 9. 后续已确定的行为与实施边界

- 跳时按实际游戏时间推进传播、衰减和匿名记忆。
- L 本地规则与 R 传入推进统一结算、提交，保护边界按需要分段，同一区间只消费一次。
- 目击只按实际记录计分，持续计分保留原节奏；检测刷新只更新事实。
- 匿名记忆沿用原公式，短期场景等待按会话隔离，持久业务及截止时间由本档恢复。

完整需求和公开命令映射统一见[项目规格](../../.scratch/native-migration/spec.md)，实现任务为 A06/B01/B02/B03；实际运行结果记录在 evidence。

## 源码定位

根目录：`E:/source/repos/SLSF Reloaded/Scripts/Source/`。

- `SLSF_Reloaded_MasterClock.psc:27-126`：主计时、余量和执行顺序。
- `SLSF_Reloaded_FameManager.psc:125-227`：扫描、保护期、类别维护。
- 同文件 `1284-1372`：增益、夜间倍率和保护期；`1374-1601`：衰减与传播试验；`2618-2628`：传播保护与提示冷却。
- `SLSF_Reloaded_DynamicAnonymity.psc:35-309`：小时检查、访问记账和匿名状态。
- `SLSF_Reloaded_CommentManager.psc:13-47`：评论许可。
- `SLSF_Reloaded_PlayerScript.psc:61-159`、`SLSF_Reloaded_NPCScan.psc:13-85`：事件与扫描。
- `SLSF_Reloaded_SexAnimationAnalyzer.psc:25-120`：十秒延迟分析。
- `SLSF_Reloaded_VisibilityManager.psc:163-203,329-347`：忙状态和刷新。
- `SLSF_Reloaded_DataManager.psc:144-147,1024-1411,2001-2052`：派生状态。
- `SLSF_Reloaded_Data_Importer.psc:53-68`、`SLSF_Reloaded_LegacyOverwrite.psc:170-175`、`SLSF_Reloaded_Uninstall.psc:11-30`：其他等待。
- `E:/source/repos/DbSkseFunctions/src/Timers.cpp:1047-1385`、`plugin.cpp:6292-6380`：原生计时参考及静态风险。

相关：[整体迁移评估](migration-assessment.md)、[Papyrus 审计](papyrus-source-audit.md)、[地点实现方案](../superpowers/specs/2026-10-26-location-index-design.md)。

后续讨论：[原生目击检测与时间调度优化评估](native-detection-and-scheduling-options.md)，记录删除旧中间节拍、短协程/回调分工、并行边界及当前源码中的引擎检测接口。
