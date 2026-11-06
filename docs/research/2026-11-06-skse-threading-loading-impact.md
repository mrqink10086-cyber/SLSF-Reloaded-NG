# SKSE 线程与加载笔记对 SLSFR NG 的影响

日期：2026-11-06。核对版本：`c97cea566487c57dbae48830755b70d387a5e65c`。本次为源码影响评估，没有修改运行代码、构建或安装包，也没有运行游戏。

后续已落实两项优化，见[实现与验证记录](../evidence/2026-11-06-loading-owner-delivery.md)。下文保留评估时的源码状态。

## 结论

[复用笔记](<E:/source/repos/SKSE-Threading-Loading-Notes.md:5>)对当前项目有影响，主要是生命周期与菜单查询的执行归属。当前日常业务已经通过 SKSE task 执行，但加载、重置、激活仍同步发生在 SKSE 回调栈中；不能从加载阶段名称推导它们与 task 使用同一线程。

这是一处源码缺少执行归属保证的问题。没有采集 SLSFR NG 的实际线程记录，不能据此声称已复现竞态、崩溃或笔记中 ValhallaCombat 的具体交错。

## 一手依据

- SKSE 消息直接调用 listener，没有向指定线程投递：[本地 PluginManager.cpp](<E:/source/repos/skse64/skse64/PluginManager.cpp:831>)、[官方实现](https://github.com/ianpatt/skse64/blob/master/skse64/PluginManager.cpp)。
- AddTask 在锁内 push，ProcessTasks 在锁内 front/pop、锁外 Run。只能保证按实际入队顺序取出，执行线程来自任务泵调用者；不能扩展成任意 hook 同线程或并发取出后的完成顺序保证：[本地 Hooks_Threads.cpp](<E:/source/repos/skse64/skse64/Hooks_Threads.cpp:10>)、[官方实现](https://github.com/ianpatt/skse64/blob/master/skse64/Hooks_Threads.cpp)。
- PreLoad、PostLoad 在加载锁存续期间同步发送；PostLoad 的 data 是 `(void*)result`：[本地 Hooks_SaveLoad.cpp](<E:/source/repos/skse64/skse64/Hooks_SaveLoad.cpp:46>)、[官方实现](https://github.com/ianpatt/skse64/blob/master/skse64/Hooks_SaveLoad.cpp)。
- 序列化 hook 直接调用插件回调，没有 AddTask：[本地 hook](<E:/source/repos/skse64/skse64/Hooks_Papyrus.cpp:306>)、[官方 hook](https://github.com/ianpatt/skse64/blob/master/skse64/Hooks_Papyrus.cpp)。读写记录使用回调期间的共享文件及 chunk 状态：[本地 Serialization.cpp](<E:/source/repos/skse64/skse64/Serialization.cpp:266>)、[官方实现](https://github.com/ianpatt/skse64/blob/master/skse64/Serialization.cpp)。

以上本地行号对应核对时工作副本；官方 master 链接会随上游变化。

## 影响一：生命周期没有与业务任务统一执行

当前 [OnMessage](<E:/source/repos/SLSF Reloaded NG/src/main.cpp:709>) 直接调用 OnMessageBody。PreLoad 同步 ReturnToMenu，PostLoad 同步 Activate，NewGame 同步 PrepareNewGame 与 Activate：[消息分支](<E:/source/repos/SLSF Reloaded NG/src/main.cpp:657>)。序列化 load/revert 也直接重置、准备生命周期：[load](<E:/source/repos/SLSF Reloaded NG/src/main.cpp:358>)、[revert](<E:/source/repos/SLSF Reloaded NG/src/main.cpp:446>)。

与此不同，[周期业务泵](<E:/source/repos/SLSF Reloaded NG/src/main.cpp:487>)、[配置热应用](<E:/source/repos/SLSF Reloaded NG/src/main.cpp:730>)和 [VM 完成回调](<E:/source/repos/SLSF Reloaded NG/src/boundary/EngineVm.cpp:52>)投递到 SKSE task。

Lifecycle 明确把 owned assets 定义为主线程状态，但 active_/pending_/message_ 没有整体同步：[字段](<E:/source/repos/SLSF Reloaded NG/include/slsf/runtime/Lifecycle.h:133>)。ReturnToMenu 会销毁 active_/pending_；ApplyCurrentConfiguration 会读取并替换 active_：[重置](<E:/source/repos/SLSF Reloaded NG/src/runtime/Lifecycle.cpp:339>)、[配置切换](<E:/source/repos/SLSF Reloaded NG/src/runtime/Lifecycle.cpp:128>)。如果加载回调与 task 泵确实并发，这些操作缺少互斥或同一执行器保证。Dispatcher 自身的 mutex 不能覆盖 Lifecycle 这些字段的整个操作。

另外，[Dispatcher::Detach](<E:/source/repos/SLSF Reloaded NG/src/runtime/Dispatcher.cpp:301>) 解锁后直接 `frame.resume()`，取消续体会在调用 Detach 的线程运行。因此同步重置还可能把原本由任务泵驱动的续体带入加载回调线程。

建议把生命周期控制转换汇入同一有序入口，并在选定执行器内绑定和检查 owner；会话内任务继续校验 epoch。生命周期命令需要自己的加载次序标识，不能因为前一个 reset 改变 epoch 就丢弃合法的新会话启动。当前 SLSFR 并没有笔记中那个“owner 立即执行、其他线程排队”的 lifecycle helper，不能直接声称其顺序回归已经在 SLSFR 失败。

序列化不能整体移出回调：GetNextRecordInfo/ReadRecordData/WriteRecord 必须在有效回调期间完成。应先读取、校验为插件拥有的数据，再把准备和激活操作交给执行器；保存仍需在回调内写出一致快照。也不能等待 task 来完成读取，因为任务泵可能尚未推进，回调还可能位于引擎加载锁内。

## 影响二：菜单状态不是已发布快照

[Menu::Frame](<E:/source/repos/SLSF Reloaded NG/src/ui/Menu.cpp:456>) 在 BeforeRender 回调中处理本地菜单队列。该队列没有切换到 SKSE task：框架 [DispatchEvent](<E:/source/repos/SKSE-Menu-Framework-3-master/src/Event.cpp:21>) 直接调用 listener，BeforeRender 来自 [Render](<E:/source/repos/SKSE-Menu-Framework-3-master/src/Hooks.cpp:262>) 和 [RenderUIHook](<E:/source/repos/SKSE-Menu-Framework-3-master/src/Hooks.cpp:361>)。

[菜单 status port](<E:/source/repos/SLSF Reloaded NG/src/main.cpp:536>) 直接调用 [Lifecycle::Status](<E:/source/repos/SLSF Reloaded NG/src/runtime/Lifecycle.cpp:361>)。Status 会读取 active_/pending_/message_ 等字段，未通过不可变快照或整体锁保护。仅 phase_ 为 atomic 不足以保护其余字段。如果渲染 hook 与生命周期/task 执行线程不同，这条读取存在竞争条件。

建议在生命周期执行器发布不可变 LifecycleStatus，让菜单只读该快照。菜单仍保留 SKSE 界面；与恢复 MCM 无关。配置存储本身有锁、PublicApi 的核心命令也有自己的同步，不能把所有菜单操作一概判为数据竞争。菜单内同步扫描目录、读取 JSON 则另有阻塞渲染的可能，可在后续处理命令执行归属时一起核对。

## 已符合、无需因笔记重写的部分

| 项目 | 当前证据与判断 |
| --- | --- |
| 地点拓扑的加载阶段 | [BuildStaticWorld](<E:/source/repos/SLSF Reloaded NG/src/main.cpp:156>) 只从 [DataLoaded 分支](<E:/source/repos/SLSF Reloaded NG/src/main.cpp:679>)启动，没有在插件 Load 期间枚举 Location。阶段正确；DataLoaded 本身仍不能证明 owner 线程身份。 |
| PostLoadGame 成功值 | [main.cpp](<E:/source/repos/SLSF Reloaded NG/src/main.cpp:664>) 使用 `message->data != nullptr`，符合发送端协议。 |
| 日常引擎查询 | worker 只投递任务，Gameplay::Tick 在任务内采样玩家、日历、地点和暂停状态：[泵](<E:/source/repos/SLSF Reloaded NG/src/main.cpp:487>)、[Tick](<E:/source/repos/SLSF Reloaded NG/src/boundary/Gameplay.cpp:859>)。 |
| VM 请求与返回 | [EngineVm::Submit](<E:/source/repos/SLSF Reloaded NG/src/boundary/EngineVm.cpp:64>) 排队后解析目标；[Completion](<E:/source/repos/SLSF Reloaded NG/src/boundary/EngineVm.cpp:40>) 复制结果并再次排队、校验 epoch。 |
| typed 事件参数寿命 | [SceneEvent native](<E:/source/repos/SLSF Reloaded NG/src/compat/VmNatives.cpp:951>) 捕获字符串、整数与 epoch，没有跨帧保留 SKSE Message 指针。 |
| epoch 查询 | [Lifecycle::CurrentEpoch/IsEpochCurrent](<E:/source/repos/SLSF Reloaded NG/src/runtime/Lifecycle.cpp:355>) 通过 Dispatcher 受锁查询。这里没有重新发现早前已经撤回的 epoch_ 无锁读取问题。 |
| 暂停状态 | [Tick](<E:/source/repos/SLSF Reloaded NG/src/boundary/Gameplay.cpp:872>) 使用 GameIsPaused，没有把 GetMenu 非空误认为菜单打开。 |

## 最小验证与范围

建议下一次处理上述执行归属时覆盖：

1. 记录消息接收、序列化回调、生命周期入队及执行、业务泵、菜单回调的实际线程 ID、PID、epoch 和加载次序；区分 received/enqueued/started/completed。
2. 用 fake executor 覆盖 worker reset 后 owner begin、PreLoad/revert/load/PostLoad 的组合，验证结束旧会话后仍能启动合法的新会话。
3. 覆盖加载成功、失败、无有效 co-save、NewGame、返回主菜单，以及旧 VM 完成迟到。SKSE 打不开 co-save 时可能没有 load callback，不能把该回调当作必达事件：[Serialization.cpp](<E:/source/repos/skse64/skse64/Serialization.cpp:426>)。
4. 验证重置后的续体、菜单状态发布和配置切换都遵守同一 owner 或明确同步合同；保留业务旧 epoch 丢弃测试。

本次没有重跑 host/ASan 测试；已有纯核心测试不能证明 main.cpp 与真实消息、序列化和渲染线程的组合行为。ValhallaCombat 笔记中的线程 ID、测试数量和实机结果没有转记为 SLSFR NG 的验证结果。
