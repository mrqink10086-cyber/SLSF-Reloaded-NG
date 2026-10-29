# 配置、菜单事件与跨存档 JSON

更新：2026-10-28。依据[项目规格](spec.md)，本文供 A02/A08、B03/B05/B07、C01/C08 接入。配置由 Configuration 模块统一读取、校验和发布；核心及菜单消费同一份有类型配置。

## 数据归属

用户配置文件为 `Data/SKSE/Plugins/SLSFReloadedNG.json`。它在新游戏、读档和退出重进之间共用，当前文件内容决定设置。

| 数据 | 保存位置 | 使用方式 |
|---|---|---|
| 功能开关、类别倍率/门槛、关系规则、时间及传播参数 | 配置 JSON | 读取后编译为数组、类别 mask 和有类型参数 |
| 用户连接覆盖、菜单区域选择及显示偏好 | 同一配置 JSON | 用稳定区域身份保存；按当前存档的区域绑定 |
| L/R、保护、已结算时刻、匿名/记忆、模组注册和类别设置、模组注册区域 | SKSE 原生存档 | 各存档独立恢复 |
| owner/alias、CSR、数值资格 mask、dirty 集合、任务堆 | 运行期派生数据 | 从本档业务和当前配置重建 |
| 旧公开 JSON、NG 状态转移文件 | 各自的导入/导出文件 | 保留各自 schema 和操作含义 |

配置 JSON 使用独立的 `schemaVersion`。原公开文件协议、存档 codec 版本与它分别演进。用户连接引用当前档不存在的区域时保留稳定 key，显示 unresolved；区域可用后重新绑定。提供者注册形成的区域与业务关系仍属于本档。

## Configuration 模块的接口

- `Reload(reason)`：请求从配置文件重读，返回完成结果及配置 revision。
- `Update(edit)`：把菜单完成的一次编辑应用到当前配置，校验后保存，再走同一重读流程。
- `Snapshot()`：取得不可变的配置、revision 及最近一次读写结果，供菜单和其他读者使用。

文件操作、JSON 解析、默认值合并、校验及编译都集中在模块内。生产文件 adapter 与内存文件 adapter 覆盖真实读写和可控错误；业务核心接收自有配置 DTO。内部通知是 C++ 有类型消息 `ConfigurationChanged(revision, compiledConfig)`，由现有执行器分发。

模块调用方共享这份结果。热循环读取参数数组和 mask；文件读取及字符串解析发生在配置更新入口。

## 读取与提交时点

| 入口 | 工作 |
|---|---|
| 插件配置初始化 | 读取文件，建立首份配置；缺失时使用并生成默认文件 |
| 新游戏、每次游戏读档/重载 | 重读 JSON，以当前配置绑定待激活的本档状态 |
| 菜单打开、配置页重新进入或显式刷新 | 发起一次读取，完成后刷新编辑模型 |
| 控件修改完成/应用 | 提交一次编辑，保存 JSON 后重读，发布变化 |
| 菜单关闭 | 提交尚未完成的脏编辑，再请求一次读取；同轮重复通知合并 |
| 逐帧渲染 | 读取配置及业务快照，展示已应用 revision 和读写结果 |

滑块拖动期间只更新编辑草稿，在编辑完成时提交；多字段应用按一批处理。菜单关闭包含框架主菜单关闭和 SLSF 自有窗口的关闭入口。框架重复 close 或同轮多项修改共用待处理请求。

保存使用同目录临时文件替换正式文件，之后通过 Reload 读取实际落盘内容。相同有效内容沿用配置 revision，省去各模块重复编译和重建。文件缺失有明确默认；无效 JSON 或写入失败保留最近一次有效配置，并向菜单报告具体错误。首次启动尚无有效配置时使用默认值。

## 如何让各模块同时生效

Configuration 持有进程级配置 revision。Session 收到通知后，按自己的区域/索引准备图绑定及类别汇总；外部查询和文件操作在事务锁外完成。已有会话先用旧配置结算至切换时刻，再在一次提交内替换参数、关系 mask、阈值资格集合、图及其配置 revision。需要重新采集的事实置 dirty，由 Observations 在正常刷新入口处理。

没有活动存档时仍可读取并保存配置；下一次激活使用最新快照。配置通知属于进程，Session 内准备好的绑定结果带本会话 epoch/index，换档后按新世界重新绑定。读者取得的业务快照注明已应用的配置 revision，菜单据此显示应用结果。

读档顺序为：关闭旧会话 → 解码本档业务 → 重读配置 JSON → 绑定稳定身份与当前连接覆盖 → 重建派生数据 → 激活。Configuration 应以当前 JSON 提供设置和用户连接偏好，StateCodec 应恢复本档业务，并按 codec 版本规则处理 v1 settings/用户 graphEdits 字段。

## 菜单框架的已核实接入点

本地来源为 `E:/source/repos/SKSE-Menu-Framework-3-master`，源码自报框架 **3.8**、API **1**。公共 consumer header 是 [resources/SKSEMenuFramework.h](<../../../SKSE-Menu-Framework-3-master/resources/SKSEMenuFramework.h>)，最终使用的 header hash、来源版本和目标 DLL 由 C01/C08 锁定并验证。

| 源码事实 | 接入方式 |
|---|---|
| `AddEvent(callback, priority)`；事件为 open/close/before-render/after-render | 注册一次框架监听，open/close 转成配置请求 |
| `WindowManager::Open/Close` 同步发出全局菜单通知 | SLSF 控件另发编辑完成通知；自有窗口关闭也进入同一配置入口 |
| `Hooks.cpp` 的 Render 每帧发出 before/after-render，菜单关闭时也可发生 | 渲染回调使用缓存；配置刷新由上表的业务入口触发 |
| `DispatchEvent` 在调用它的上下文直接执行 callback | callback 复制请求并投递；由既有执行器处理文件和业务提交 |
| 当前 `AddEventListener` 接受 priority 但没有保存它 | 配置顺序由自身队列管理 |

注册/注销的真实导出为 `RegisterEventPriority` / `UnregisterEvent`，回调为 `void (__stdcall*)(EventType)`。先核实已加载模块和所需导出，再持有成功注册的 token 并注销。公共 header 的 `IsInstalled()` 只检查文件存在，实际可用性按加载模块和导出确认。

依据：[事件实现](<../../../SKSE-Menu-Framework-3-master/src/Event.cpp>)、[开关入口](<../../../SKSE-Menu-Framework-3-master/src/WindowManager.cpp>)、[渲染入口](<../../../SKSE-Menu-Framework-3-master/src/Hooks.cpp>)、[版本与导出](<../../../SKSE-Menu-Framework-3-master/src/SKSEMenuFramework.cpp>)。以上是本地源码结论，游戏中的加载、回调上下文和菜单交互由 C08 probe 验证。

## 任务与完成标准

| 任务/组 | 交付 |
|---|---|
| A02 / 01 | 配置 revision、纯配置 DTO 和更新结果的共享约定 |
| A08 / 02 | 本档 Capture 与跨档配置分开；现有 v1 settings/用户 graph edit 字段的版本处理明确 |
| B03/B05 / 06 | 配置初始化/重读接线；恢复本档业务，采用当前 JSON 激活新会话 |
| B07/C08 / 10 | JSON、更新通知、编译配置和菜单编辑完整流程；锁定并 probe 框架公开入口 |
| C09/C10 / 11 | 发布默认配置，更新安装保留用户文件；验证跨存档行为 |

交付要求：菜单修改后各消费者应应用同一配置 revision；外改 JSON 后打开/刷新 UI 或读档应读取文件当前值。A→B、同档重载和退出重进应共用设置，各档声望及注册应独立恢复。同轮关闭请求应合并，逐帧渲染应读取缓存。无效文件及写入失败应显示具体错误并保留有效配置；阈值、关系 mask 和图应随同一次配置提交更新。旧公开 JSON 应按原合同完成往返。
