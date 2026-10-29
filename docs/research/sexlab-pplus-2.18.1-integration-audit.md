# SexLab P+ 2.18.1：本地发布包与集成入口核查

调研日期：2026-10-26。来源：`E:/source/repos/sexlab P+` 的脚本、DLL 导出和 PDB 元数据。本文给出生产端定位，运行时验证由 C03 完成，当前需求见[项目规格](../../.scratch/native-migration/spec.md)。

采用普通原生通知、公开 VM 异步查询和自有场景快照。P+ 的参与者、角色和实际交互作为 SLSF 分类输入；进程能力与本档场景分别管理，见[生命周期设计](../superpowers/specs/2026-10-26-integration-state-lifecycle-design.md)。

## 1. 版本与证据边界

- `SKSE/Plugins/SexLabUtil.dll` 的 Windows FileVersion / ProductVersion 均为 **2.18.1.0**。这是本次本地包的二进制版本标识，不采用包内旧 `LicenseReadme` 的基础 SexLab 版本标识。
- LLVM 读取到 DLL CodeView GUID `{24BD52C6-3449-40AB-A9D0-A98E4F7A4C8C}`、Age 1；附带 `SexLabUtil.pdb` 的 GUID / Age 相同。
- 发布包提供 `Source/Scripts/*.psc`，本次检查未发现供外部插件使用的 C++ API 头文件。DLL 导出只有 `SKSEPlugin_Load`、`SKSEPlugin_Query`、`SKSEPlugin_Version`，没有 `GetAPI` 导出。
- 这份包未确认公开场景数据 C++ ABI，采用已公开的 VM 方法；PDB 用于核对二进制身份。
- 框架版本通过 `SexLabFramework.GetVersion()` → `SexLabUtil.GetVersion()` → `SKSE.GetPluginVersion("SexLabUtil")` 查询。

元数据锁定本地二进制身份；1.6.1170 加载与脚本时序纳入 C03 验证。

## 2. 三个动画通知可直接进入 C++ sink

关键证据是 `Source/Scripts/sslThreadModel.psc:2256-2273`：每次 `SendThreadEvent(name)` 调用 `SetupThreadEvent(name)`，后者同时执行两条发送路径。

| 事件阶段 | typed Papyrus 名称及载荷 | 普通 SKSE callback 名称及载荷 |
|---|---|---|
| 动画开始 | `HookAnimationStart(int thread_id, bool HasPlayer)` | `AnimationStart`；`strArg` 为线程 ID 的字符串 |
| 前戏结束 | `HookLeadInEnd(int thread_id, bool HasPlayer)` | `LeadInEnd`；`strArg` 为线程 ID 的字符串 |
| 动画结束 | `HookAnimationEnd(int thread_id, bool HasPlayer)` | `AnimationEnd`；`strArg` 为线程 ID 的字符串 |

普通发送为 `SendModEvent(HookEvent, thread_id)`：线程 ID 位于 **strArg**，numArg 使用默认值，sender 为 controller Quest。普通通道的玩家参与情况通过场景查询取得。SLSF 只订阅表中基础名称，每个场景结算一次；自定义 `_Hooks` 的后缀名称单独处理。

本地 SKSE 源码 `E:/source/repos/skse64/skse64/PapyrusForm.cpp:274-280` 证明 `Form.SendModEvent` 构造并广播 `SKSEModCallbackEvent`。因此，本版本可以在 C++ 注册 `BSTEventSink<SKSE::ModCallbackEvent>`，接收表中**没有 Hook 前缀**的普通名称；这三个通知不需要额外的 Papyrus 监听转发器。

typed `ModEvent.Send` 仍投递 Papyrus 回调注册表。SLSF 对这三个动画通知只消费普通通道，每个场景结算一次。

原生接收器复制载荷后排队，核对名称、完整线程 ID 和 sender/controller；参与者快照确认包含玩家后进入业务。VM 查询在后续异步阶段完成。

## 3. 场景数据入口与兼容含义

| SLSF 所需信息 | 本地脚本入口/含义 | 迁移要求 |
|---|---|---|
| 场景、当前阶段、参与者顺序 | `sslThreadModel.psc:14-17,48` 的 native `GetActiveScene`、`GetActiveStage`、`GetPositions` | 通过公开 Papyrus VM 方法查询 |
| 玩家、受害者/施加者、strapon、男女人数 | controller 查询及旧 SLSF 分析器的调用 | 按 P+ 的角色与分类分别采集 |
| 性别分类 | `SexlabRegistry.psc:32-34`：Male 0、Female 1、Futa 2、CrtMale 3、CrtFemale 4，并支持 overwrite | 保留 P+ 的覆盖与分类含义，再映射旧 SLSF 声望判断 |
| 动画标签 | `sslBaseAnimation.psc:57-58` 将 scene tags 与 scene annotations 合并 | 旧 `Animation.HasTag` 对应 scene tags 与 annotations 的合并结果 |
| 标签作用域 | `SexLabThread.psc:68-83` 区分 thread / scene / stage tags | 每个旧判断读取对应作用域的标签集合 |
| 主动/被动的具体交互 | `SexLabThread.psc:181-283` 的 28 项交互标记、GetCurrentInteractionFlags、IsVaginal/Anal/OralComplex 等；`sslThreadModel.psc:1673-1681` 的 native collision 查询 | 读取 P+ 的交互结果和就绪状态，缺项标记未知并暂停依赖该项的计分 |
| Submissive、Victim、Aggressor、Consent | `sslThreadModel.psc:291-333` 查询 context/ActorAlias；`SexLabThread.psc:329-337` 公开角色入口 | 分别读取角色字段，再按 SLSF 规则映射声望类别 |

`sslBaseAnimation` 是访问 native registry 的兼容代理。适配器一次查询所需字段，将字符串和类别转换为自有数字、位集合和场景快照，随后在 C++ 中分析声望。

当前路线已确定为“普通原生通知 + 公开 VM 异步事实 + C++ 业务”。适配器一次采集所需字段，将结果复制到带生命周期代次的自有快照，供本地分类使用。

旧分析器还使用伙伴种族、关系、DD 关键词，以及 Oral / Anal / Orgy / Aggressive / Creature / Bestiality 和亲密类标签。入口变化不改变这些公开声望分类的含义。参考 `E:/source/repos/SLSF Reloaded/Scripts/Source/SLSF_Reloaded_SexAnimationAnalyzer.psc`。

## 4. 生命周期、延迟与内存边界

- **开始**：`sslThreadModel.psc:1537-1556` 在 `OnAnimationSynchronized` 中首次发送 AnimationStart。框架同步完成后，SLSF 继续保留原十秒取消/换动画宽限，再检查场景身份及当前数据。
- **前戏结束**：`sslThreadModel.psc:1454-1467` 先发送 LeadInEnd，再选取并 ResetScene 到主场景。后续查询确认主场景切换完成后，才使用主场景标签；用于确认的通知和查询顺序在 C03 验证。
- **结束**：`sslThreadModel.psc:1692-1720` 在取消待执行动画、停用控制、等待 actor aliases 回到 idle 后发送 AnimationEnding / AnimationEnd。结束结算使用 SLSF 已捕获的参与者和场景快照。
- **复用与读档**：thread ID 是可复用槽位。SLSF 另存场景序号和读档 generation；异步返回、协程恢复和排队执行时核对身份，结束或读档时取消旧任务。跨任务保存 Actor handle/FormID 和自有数组，执行时重新解析对象。
- **执行线程**：普通事件由发送方同步广播。接收后引擎工作切到合法上下文，VM 查询走 VM 调度，会话状态由执行器和短锁事务管理。
- **阻塞 hook**：`SexLabThreadHook.psc` 是 Papyrus ReferenceAlias 的阻塞合同。SLSF 使用上述普通通知及异步查询。

## 5. 外观/效果的独立入口

`sslActorLibrary.psc:26-84` 的 `SexLabApplyCum` / `SexLabClearCum` 使用 typed ModEvent，经 Papyrus 注册表接收。`sslThreadModel.psc:2234` 附近的 `SexLabApplyCumFX` 单独核对发送路径。

`sslActorLibrary.psc:87-89` 的 `CountCumFx` 读取 `StorageUtil.CountObjIntValuePrefix`，前缀为 `SexLabActiveLayer` 加类型，类型为 Vaginal 0 / Anal 1 / Oral 2。框架层数查询读取这组应用记录，RaceMenu overlay 状态用于外观查询。

三个动画通知走普通 callback；效果状态按其记录和协议查询，触发计分仍按原 SLSF 需求。

## 6. 后续验证

在 1.6.1170 记录版本、加载状态、三个通知的名称、strArg、sender 与顺序。覆盖玩家/非玩家场景、前戏切换、取消/换动画、线程槽复用、结束和读档时的异步请求；按公开类别规则核对结果。实际日志和结论写入 C03 证据。
