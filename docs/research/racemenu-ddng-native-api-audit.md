# RaceMenu、DDNG 与 SlaveTats NG 原生 API 源码核查

调研日期：2026-10-26。来源：`E:/source/repos/RaceMenu`、`E:/source/repos/DeviousDevicesNG-main`、`E:/source/repos/SlaveTatsNG-master`。本文记录源码接口，目标安装版 probe 由 C01/C04–C06 完成，当前要求见[项目规格](../../.scratch/native-migration/spec.md)。

## 结论

三者均有公开 C++ 查询接口：DDNG 提供设备，SlaveTats NG 提供已应用纹身，RaceMenu 提供外观。下文分别记录版本、对象含义及可用通知。

接口在进程内缓存，角色数据随本档刷新；SlaveTats NG 在启动时先订阅一次性广播。AND 判定仅作旧源码记录。采用路线见[接口生命周期设计](../superpowers/specs/2026-10-26-integration-state-lifecycle-design.md)。

## DDNG

### 获取方式与就绪阶段

- `include/API.h:31` 定义 `DD_APIVERSION 2U`。
- `src/Main.cpp:22-26` 导出 `GetAPI()`，返回生产端的 API 对象。
- `src/Main.cpp:76-88` 在 `kDataLoaded` 的各服务 Setup 完成后创建 API 对象。
- `include/API.h:152-180` 的获取辅助函数查找 `DeviousDevices.dll!GetAPI` 并严格比较版本等于 2；头部示例在新游戏/读档消息中调用。

SLSF 通过 GetModuleHandle 查询 SKSE 已加载的模块。生产端完成初始化后取得对象并检查版本，就绪前返回明确状态。

### 可用查询及本地实现状态

| 接口 | 实现与用途 |
|---|---|
| GetDeviceRender / GetDeviceInventory | inventory device 与 rendered armor 互转；`src/API.cpp:18-26` |
| GetPropertyForm/Int/Float/Bool/String 及数组 | 查询设备数据库属性；`include/API.h:114-123` |
| GetDevices / GetWornDevice / GetWornDevices | 枚举或按 keyword 查设备；`src/API.cpp:142-155` |
| GetHandRestrain / GetBondageState | 手部束缚或状态位集合；`src/API.cpp:112-115,157-160` |
| IsGagged / 表情、hider 接口 | 转发查询或操作到相应服务 |
| IsDevice | **此份公开包装实现固定返回 false**；`src/API.cpp:117-120` |
| ActorHasBlockingGag | **此份公开包装实现固定返回 false**；`src/API.cpp:122-125` |

接入时跳过这两个空包装。GetBondageState 调用内部实现及内部 ActorHasBlockingGag；其口塞位和 IsGagged 的含义分别核对。

`src/LibFunctions.cpp:127-158` 的 GetWornDevices 返回筛选后的 **rendered armor**；`79-124` 的 GetWornDevice 返回匹配的 **inventory device**。适配器分别保存和使用这两种设备身份。

`src/LibFunctions.cpp:171-204` 的 GetBondageState 每次枚举已穿设备并执行 keyword 字符串判断。SLSF 在装备或状态刷新时查询一次，转换为内部数字状态供声望计算使用。

### 对 SLSF 的边界

- 旧 `SLSF_Reloaded_VisibilityManager.psc:776` 的 CheckBondage 使用重束缚 keyword、装备槽、衣物遮挡、AND faction 和用户配置。SLSF 将 DDNG 设备状态与这些条件结合，计算可见、轻度和重度束缚。
- API v2 提供设备查询；设备变化、高潮及其他 DD typed-event 的接收路径分别核对生产端发送方式。
- 已选装备/加载等已验证事件标记 dirty、合并查询，业务使用边界有界刷新；运行时 probe 核对通知覆盖。
- GetDatabase 返回跨 DLL 的 const map 引用，多个接口也跨边界传递 STL 类型。使用方需匹配 MSVC/CRT/结构 ABI；不长期保存依赖内部容器迭代器或跨回调裸引用，不在工作线程遍历引擎对象。

## RaceMenu / skee

### 接口交换

- `skee64/IPluginInterface.h:24-50` 定义 IPluginInterface、IInterfaceMap 和 `InterfaceExchangeMessage`，消息类型为 `0x9E3779B9`。
- `skee64/main.cpp:555-563` 接收交换消息并回填 interfaceMap；`572-578,926-931` 给出监听注册时机。
- 插件名为 `skee`，`main.cpp:943-953` 注册 Override、Overlay、NiTransform、BodyMorph、ItemData、ActorUpdateManager、Attachment、Command、FormTag 等接口。

消费端在监听就绪后（例如 kPostPostLoad）向 skee 发送交换消息，取得 map，再通过 QueryInterface 按名称取得对象并检查各自 GetVersion。

### 与本项目相关的公开能力

| 接口 | 能力 | 本地声明 |
|---|---|---|
| IOverlayInterface v2 | 四部位 overlay 数量/格式、安装回调 | IPluginInterface.h:360-387 |
| IOverrideInterface v2 | 节点/皮肤/护甲属性及 override 查询 | IPluginInterface.h:389-465 |
| IActorUpdateManager v2 | 排队刷新、模型附件观察者、Flush 回调 | IPluginInterface.h:334-358 |
| IBodyMorphInterface v5 | morph 读取/遍历/更新及 shape 回调 | IPluginInterface.h:58-138 |
| INiTransformInterface v3 | 节点位置/旋转/缩放及遍历 | IPluginInterface.h:140-202 |

HasOverlays 回答 overlay 状态。SLSF 所需的应用记录来自 SlaveTats，身体分区、排除槽及额外声望由本插件计算。旧查询见 `VisibilityManager.psc:105-159,234`，原生替代入口见下节。

RegisterInstallCallback 在 `OverlayInterface.cpp:250-256` 的安装流程中触发；RegisterFlushCallback 在 ActorUpdateManager 的 Flush 路径调用。纹理、alpha 和分类变化的通知覆盖范围由运行时 probe 核对。

回调应只记录必要身份、标记 dirty 并安排受控刷新。安装回调在内部 callbacks 锁持有期间执行，附件通知也持有 ActorUpdateManager 的锁；避免在回调中做重计算、注销自己或阻塞等待其它服务，不跨帧保留回调中的临时节点指针。

### 版本与 ABI 限制

- 此份 RaceMenu 的 `main.cpp:719-733` 声明 Bethesda runtime 为 `RUNTIME_VERSION_1_7_99`，GOG 为 `RUNTIME_VERSION_1_6_1179_GOG`；Steam 1.6.1170 安装版 ABI 另行 probe。
- 原始公开头使用旧 SKSE 的全局 TESObjectREFR、Actor 等类型。CommonLibSSE-NG 接入需维护正确的类型声明、虚表顺序、函数签名和版本检查，不直接把生产端实现头混入项目。
- 表中列出本地源码的接口版本。目标 1.6.1170 安装版逐项验证所需接口和回调，并使用对应 ABI 声明。

## SlaveTats NG

本地 `CMakeLists.txt:7-12` 的项目版本为 0.8.2，公开 C++ 地址表版本为 1，接入时分别检查。README 将本项目描述为 SlaveTats 的 C++/SKSE 实现，使用 CommonLibSSE-NG。

### 旧查询与 C++ 接口共享实现

`include/SlaveTatsNG_Interface.h:19-60` 公开 Addresses 函数指针表；`src/SlaveTatsNG_Interface.cpp:13-40` 将其绑定到真实原生函数。

旧脚本兼容层 `papyrus/Source/Scripts/SlaveTats.psc:122-127` 调用 SlaveTatsNG 的 Papyrus native；`src/papyrus_tattoo.cpp:26-33` 将其转到 slavetats_ng 的函数。公开 C++ 表中的 get_applied_tattoo_in_slot / get_applied_tattoos_by_area 指向同一组原生函数。因此本项目可直接读取相同应用记录，不必通过 VM 调用旧查询，也不必从 RaceMenu overlay 反推 SlaveTats 的应用身份。

| 查询 | 可用方式与语义 |
|---|---|
| 单槽已应用纹身 | get_applied_tattoo_in_slot(actor, area, slot)，无匹配返回 0；非零 int 是 JContainers 对象句柄 |
| 四个部位的已应用纹身 | get_applied_tattoos_by_area(actor, body, face, hands, feet)，传入四个 JArray 句柄；一次遍历应用记录后分类追加 |
| 按模板/属性查询 | query_applied_tattoos、query_applied_tattoos_with_attribute、has_applied_tattoos_with_attribute |
| 槽数量 | 从 RaceMenu API/配置取得部位 overlay 容量；原 SLOTS 内部读取 NiOverride 缓存，初值来自 skee INI。C++ 地址表公开纹身记录查询，容量单独读取 |

头文件把低层查询标为 implementation。接入锁定已核实的布局、版本和函数指针，适配集中在一个模块。

### 消息获取与实际就绪分离

- `src/main.cpp:90-93` 在 kPostPostLoad 广播一次 `MessageType::Interface`（值为 1），data 直接指向静态 Addresses 表，消费端通过监听取得该表。
- 消费端需在发布前（例如 kPostLoad）注册 SlaveTatsNG 消息监听，避免到 kDataLoaded 才订阅而错过唯一广播。
- `src/main.cpp:95-99` 到 kDataLoaded 才初始化 JContainers 和 RaceMenu 依赖。因此收到表只保存指针，不立即查询玩家纹身；待依赖及当前存档上下文就绪后读取。
- 生产端 dataLen 为 sizeof(void*)。接收端按此协议检查消息来源、类型、非空数据和版本；表布局使用已核实的 v1 公共前缀。
- 头文件在 v1 尾部追加 compile_cache 却没有增加版本。SLSF 的纹身读取不需要这个尾部函数，不依赖它来判断新旧表长度。表与 JContainers 句柄均不写入 SLSF 的 co-save。

### 批量快照与性能

`src/tattoo.cpp:98-126` 的逐槽查询每次创建模板和结果容器并遍历应用记录。查询 S 个槽、每次 T 条记录，成本为 O(S×T)。适配器采用下面的按部位批量查询，将重复扫描合并为一次。

`src/tattoo.cpp:61-95` 的按部位查询从 `JFormDB[actor][".SlaveTats.applied"]` 一次遍历记录，分发到四个部位数组。适配器通过 JContainers 创建临时数组，读取 area/slot 等字段，转换成自有数字记录和 applied 位集合，再释放临时句柄。按每条记录的 slot 建立映射，重复槽的结果与旧单槽查询核对。

JContainers int 是带生命周期的对象句柄。适配器读取所需字段，转换为 SLSF 自有普通值，再按 JContainers 所有权协议释放临时结果。衣物遮挡、SLSF 分区、排除槽和额外声望继续在原生模块计算；纹身应用记录由 SlaveTats 保存。

按部位查询使用 fail_t：true 表示失败，false 表示没有报告错误；失败与合法空列表分别处理。高层 simple_add_tattoo/simple_remove_tattoo 的成功路径返回 true，按各自实现解释返回值。

### 变更通知和线程边界

`src/overlays.cpp:885-913` 的 SlaveTats-removed / SlaveTats-added 发送逻辑均为注释/TODO，公开地址表没有订阅回调或修订号接口。

RaceMenu 的安装/Flush 回调提供部分刷新提示。当前选择初始/读档、装备/已验证外观事件及配置变化标记 dirty，合并批量查询，并在业务使用边界有界刷新。查询暂时失败时保留已有声望和注册，暂停依赖未知事实的新计分；运行时 probe 检查第三方变更覆盖。

JContainers 和角色对象在允许访问引擎对象的阶段读取，复制为自有普通值后交给数值处理。`src/overlays.cpp:929-954` 在非主线程路径排 AddTask 后立即返回 false；协程应等待任务实际完成。延迟任务保存可重新解析的角色身份，执行时解析对象。

## 迁移决定

DDNG、RaceMenu 与 SlaveTats NG 适配器生成自有数字快照；外部回调标记 dirty，查询在合法阶段批量执行。公开分类仍按 SLSF 规则处理。实施边界和通过标准见 C04–C06，实际版本、事件覆盖和 ABI 结果记录在 evidence。
