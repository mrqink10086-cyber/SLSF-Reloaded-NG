# SLSFR NG 的多参数 ModEvent 接收

日期：2026-11-05。本次核对源码，没有修改运行实现、构建包或外部参考源码，未运行游戏。

## 结论

原版 SLSF 的相关事件确实是 typed 多参数事件。当前 SLSFR NG 已通过同名薄 PSC 接收原参数，再调用 C++ native，因此不需要仅因为 SLACS2 NG 的研究记录而重新设计接收方式。

“保留 typed 参数”和“让事件直接执行 native 实例方法”是两个不同决定。后者在 [SLACS2 NG 研究](<E:/source/repos/SLACS2 NG/docs/research/pplus-native-events.md:74>)中仍是待 P0 验证的候选，不能作为当前桥接实现有误的依据。

## SKSE 的两条路径

- `Form.SendModEvent` 构造固定的 eventName/strArg/numArg/sender 事件并调用 dispatcher：[本地发送端](<E:/source/repos/skse64/skse64/PapyrusForm.cpp:274>)。
- `ModEvent.Create/Push/Send` 则按注册表向接收 handle 的 callbackName 执行 QueueEvent，复制原始变长参数；Send 没有调用上述 dispatcher：[参数复制与投递](<E:/source/repos/skse64/skse64/PapyrusModEvent.cpp:80>)、[Send](<E:/source/repos/skse64/skse64/PapyrusModEvent.cpp:255>)。公开上游源码：[PapyrusModEvent.cpp](https://github.com/ianpatt/skse64/blob/master/skse64/PapyrusModEvent.cpp)、[PapyrusForm.cpp](https://github.com/ianpatt/skse64/blob/master/skse64/PapyrusForm.cpp)。

因此，单独添加普通 `SKSE::ModCallbackEvent` sink 不能代替 typed 接收链；业务四参数事件也不能因参数数量相同，就套用简单事件的固定四字段签名。

## 当前接收链

### P+ 场景通知

P+ 发送端按顺序 PushInt(thread_id)、PushBool(HasPlayer)，再发送 Hook 事件；随后另发一个简单事件：[sslThreadModel.psc](<E:/source/repos/sexlab P+/Source/Scripts/sslThreadModel.psc:2265>)。

原版 SLSF 注册 HookAnimationStart、HookLeadInEnd、HookAnimationEnd，回调签名均为 `(Int threadID, Bool hasPlayer)`：[注册](<E:/source/repos/SLSF Reloaded NG/build/review01/decompiled/SLSF_Reloaded_PlayerScript.psc:35>)、[原始回调](<E:/source/repos/SLSF Reloaded NG/build/review01/decompiled/SLSF_Reloaded_PlayerScript.psc:72>)。

当前同名 PlayerScript 仍注册 typed Hook，并在 OnPlayerLoadGame 重新订阅；回调保留 `(Int ThreadID, Bool HasPlayer)`，先筛选 HasPlayer，再调用 SceneEvent(topic, ThreadID)：[订阅](<E:/source/repos/SLSF Reloaded NG/src/compat/papyrus/SLSF_Reloaded_PlayerScript.psc:22>)、[回调](<E:/source/repos/SLSF Reloaded NG/src/compat/papyrus/SLSF_Reloaded_PlayerScript.psc:47>)。Bool 已用于筛选，未作为待处理业务数据丢失。

SceneEvent native 在保存 epoch 后排入主线程任务，再调用 Gameplay：[native 绑定](<E:/source/repos/SLSF Reloaded NG/src/compat/VmNatives.cpp:951>)、[Invoke](<E:/source/repos/SLSF Reloaded NG/src/boundary/Gameplay.cpp:641>)、[场景消费](<E:/source/repos/SLSF Reloaded NG/src/boundary/Gameplay.cpp:844>)。

### SLSFR 对外事件

当前 ModEventListener 在 OnInit 注册外部事件。例如 SLSF_Reloaded_SendFameGain 对应 OnExternalFameGain，其签名是 `(String Category, String EventLocation, Int MinFame, Int MaxFame)`，四个参数逐个传给 SLSFR_EventSink：[订阅](<E:/source/repos/SLSF Reloaded NG/src/compat/papyrus/SLSF_Reloaded_ModEventListener.psc:12>)、[完整转发](<E:/source/repos/SLSF Reloaded NG/src/compat/papyrus/SLSF_Reloaded_ModEventListener.psc:74>)。

此链路与研究记录中的“PSC 接收 typed 参数，再调用 native”属于同一种接入方式，符合保留同名脚本薄转发、业务由 C++ 执行的项目要求。

## 验证边界

以上证明源码中的事件协议和转发路径。没有执行真实 VM 事件投递测试，也没有证明向已有 VM 类型动态绑定 native 方法就能直接成为事件接收器；后者仍需独立验证对象绑定、方法查找、参数顺序和投递次数。
