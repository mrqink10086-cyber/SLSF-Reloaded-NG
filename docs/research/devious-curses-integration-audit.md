# Devious Curses NG 与 SexLab / Devious Devices 交互源码审计

调研日期：2026-10-26。本文以 `E:/source/repos/devious-curses-main` 说明事件和设备适配，保留消费端源码定位。SLSF 当前范围及任务统一见[项目规格](../../.scratch/native-migration/spec.md)。

后续生产端核查已确认 DDNG API v2，以及 P+ 2.18.1.0 的三个普通动画通知。详见[设备/外观 API](racemenu-ddng-native-api-audit.md)和[P+ 发布包](sexlab-pplus-2.18.1-integration-audit.md)；目标安装版的 probe 在 C01/C03/C04 完成。

## 结论摘要

- Devious Curses 的 C++ sink 处理 `DCurses_*`；SexLab/DD typed 事件由 `DCurses_MCM` 订阅并转到 native。这里展示 UI、订阅、业务三个独立职责。
- SKSE typed `ModEvent.Send` 投递 Papyrus 注册表；`Form.SendModEvent` 广播 `SKSEModCallbackEvent`。SLSF 的 typed 公开协议保留 PSC 传输，P+ 的三个动画通知走已核实的普通通道。
- DCurses 查询 `DeviousDevices.dll!GetAPI` 并要求版本严格等于 2，失败路径报告需要 DDNG 4.0+。此数据接口与 DD 高潮消息是不同入口。
- 本仓库 event sink 没有单独的 1.6.1170 门槛；SLSF 的实际加载、载荷及顺序仍在目标环境验证。

## 事件路径

### 1. SKSE 原生 ModCallbackEvent sink

`ModEventSink` 继承 `RE::BSTEventSink<SKSE::ModCallbackEvent>`，`RegisterEvent()` 将其挂到 `SKSE::GetModCallbackEventSource()`。`plugin.cpp` 在 SKSE `kDataLoaded` 时调用 `RegisterEventSinks()`。接收函数读取 `eventName`、`strArg`、`numArg`，转小写/去空格后只进入 `dcurses_` 命名空间分支；这些分支处理自家 `doevent`、`setlewdmark`、`equipoppressivedevice` 和移除设备等逻辑。当前接收函数没有 SexLab 或 DD 专用分支，也没有使用 `sender` 字段。

相关证据：

- `E:/source/repos/devious-curses-main/include/TESEvents.h:81-86`：sink 类型和虚函数签名。
- `E:/source/repos/devious-curses-main/src/TESEvents.cpp:22-35,350-408,409-544,546-549`：注册、参数解析、处理分支。
- `E:/source/repos/devious-curses-main/plugin.cpp:244-268`：SKSE 消息监听及 `kDataLoaded` 注册 sink。

最小接收接口（来源：`include/TESEvents.h:81-86`、`src/TESEvents.cpp:546-549`）：

```cpp
class ModEventSink : public RE::BSTEventSink<SKSE::ModCallbackEvent> { /* ProcessEvent(...) */ };
SKSE::GetModCallbackEventSource()->AddEventSink(&eventSink);
```

本仓库有两种容易混淆的“ModEvent”发送方法：`CustomModEvent::SendModEvent` 直接创建 `SKSE::ModCallbackEvent` 并调用事件源 `SendEvent`；而 `CustomModEvent(eventName).PushString/PushInt/PushBool/PushForm(...).Send()` 则通过 Papyrus VM 依次调用 `ModEvent.Create`、`Push*`、`Send`。`SendCurseModEvent` 用的是后一种 typed Papyrus 事件。

本地 SKSE 源码中，`E:/source/repos/skse64/skse64/PapyrusModEvent.cpp:255-268` 的 typed `ModEvent.Send(handle)` 调用 `g_modCallbackRegs.ForEach(...)` 给 Papyrus 注册对象排队；`E:/source/repos/skse64/skse64/PapyrusForm.cpp:274-280` 的 `Form.SendModEvent` 构造 `SKSEModCallbackEvent` 并广播。适配器按各框架实际发送的通道订阅，生产端发送路径见后续框架审计。

### 2. SexLab 动画事件与 Papyrus native 回调

`CODEGEN.py` 生成的 `DCurses_MCM.psc` 在 `RegisterModEvents()` 中订阅 `HookAnimationStart`、`HookAnimationEnd`；HasPlayer 为真时，按 thread id 查询 controller/positions，再调用 `DCursesLib.SexStarted(positions)` 或 `SexEnded(positions)`。`ScriptingManager::MCMRegisterModEvents()` 在 kNewGame/kPostLoadGame 派发这项注册。SLSF 采用 P+ 已确认的普通通知，场景事实异步查询。

随后 C++ 的 `PapyrusFunctionsSex` 将 `SexStarted`、`SexEnded` 注册为 `DCursesLib` native。`P_SexEnded` 对第一个非玩家 actor 调用 consequences、纹身和 oppressive device 业务，再执行玩家相关逻辑；`P_SexStarted` 对玩家命令的召唤 actor 延长效果持续时间。完整链路为 Papyrus 事件处理器调用 DCurses 自有的 C++ native。

相关证据：

- `E:/source/repos/devious-curses-main/CODEGEN.py:572-604,617`：事件注册、参数签名、SexLab controller 查询和 native 调用；该脚本会生成 MCM Papyrus 文件。
- `E:/source/repos/devious-curses-main/src/Scripting.cpp:337-341`：向 MCM quest 派发 `RegisterModEvents`。
- `E:/source/repos/devious-curses-main/plugin.cpp:281-315,367-374`：新游戏/读档后注册事件，并注册 Papyrus native 表。
- `E:/source/repos/devious-curses-main/src/Sex.cpp:594-639`：SexLab 事件对应的 C++ 业务及 native 注册。

SexLab 的**开始场景调用**经 Papyrus VM 派发到 `DCursesLib.StartSex`；该函数的 PSC 实现待补齐后核对 SE / P+ 分支。已收录的 `StartMasturbationImpl()` 调用 SexLab Papyrus `GetVersion`：版本 `<=16601` 使用 `QuickStart`，更高版本使用 `StartScene`。这个分支负责手淫启动路径。`Settings.h:442` 将 futa tag 筛选限定为 SexLab SE。

`P_GetAnimationFilterTagsP()` 按 `-Oral,-Anal,-Vaginal,...` 格式生成限制标签，由 `StartScene` 分支通过 Papyrus tags 参数传入。证据：`E:/source/repos/devious-curses-main/src/Sex.cpp:467-502,504-520`。

DCurses 自有的 `SexLabPPFunctions.h` 提供分类辅助函数：`GetSex` 先查 `SexLab.esm` 的 gender faction rank，缺项时使用 actor base sex；futa 判断读取 TNG keyword / SOS faction。证据：`E:/source/repos/devious-curses-main/include/SexLabPPFunctions.h:45-105`。上述版本分支位于 `E:/source/repos/devious-curses-main/src/Sex.cpp:504-520`，一般场景启动派发位于 `E:/source/repos/devious-curses-main/src/Scripting.cpp:343-349`。

### 3. Devious Devices 事件与 DDNG 原生 API

DCurses 的 MCM 脚本订阅 `DeviceActorOrgasmExp`、`DeviceActorOrgasmEx`，转换 actor 并筛玩家后调用 `DCursesLib.DDPlayerOrgasm()`，native 转到 `OppMadnessPlayerOrgasm()`。这是该模组的 Papyrus typed 消息链；SLSF 只接其需求已有的触发。

DCurses 通过 DDNG 的独立 DLL 接口读取设备数据。`include/apis/DDNG_API.h` 声明 `DeviousDevicesAPI` 和 `DD_APIVERSION 2U`；`LoadAPI()` 使用 `GetModuleHandle(L"DeviousDevices.dll")`、`GetProcAddress("GetAPI")`，并检查 `GetVersion() == DD_APIVERSION`。`plugin.cpp` 在 `kNewGame` / `kPostLoadGame` 加载，失败时报告“version 4.0+”并 `report_and_fail`。成功后 `Devices.cpp` 遍历 `GetDatabase()`，用 `GetDeviceRender()`、`GetPropertyForm/Int/Float()` 生成设备清单。表达式和 hider 使用同一接口表；设备锁定/解锁经 Papyrus `zadLibs.LockDevice/UnlockDevice`。后续生产端审计已核对 API v2，C01 再验证目标安装 DLL。

最小 DLL API 要点（来源：`include/apis/DDNG_API.h:31,152-180`）：

```cpp
GetProcAddress(dllHandle, "GetAPI");
api->GetVersion() == DD_APIVERSION;  // DD_APIVERSION == 2
```

相关证据：

- `E:/source/repos/devious-curses-main/CODEGEN.py:572-579,599-604`：DD 事件名称和 Papyrus 参数。
- `E:/source/repos/devious-curses-main/src/Sex.cpp:627-639`：DD player orgasm native 的业务逻辑和绑定。
- `E:/source/repos/devious-curses-main/include/apis/DDNG_API.h:31,104-146,149-181`：本地 ABI 声明、接口表、API 版本和 DLL 查找。
- `E:/source/repos/devious-curses-main/plugin.cpp:281-315`：加载时点、严格失败路径和设备列表重算。
- `E:/source/repos/devious-curses-main/src/Devices.cpp:493-552`：API 的实际数据读取调用。
- `E:/source/repos/devious-curses-main/src/Scripting.cpp:424-451`：设备锁/解锁走外部 `zadLibs` Papyrus 方法。

## 头文件与依赖边界

`SKSE::ModCallbackEvent`、`SKSE::GetModCallbackEventSource()`、`RE::BSTEventSink` 和 Papyrus VM 类型由 `PCH.h` 的 `SKSE/SKSE.h`、`RE/Skyrim.h` 提供。构建配置使用 `CommonLibSSE`，vcpkg 依赖为 `commonlibsse-ng`；`ModEvents.h` 在这些类型之上封装发送操作。

`include/apis/DDNG_API.h` 是 DCurses 收录的消费端声明，实际函数和 `GetAPI` 导出由 `DeviousDevices.dll` 提供。`DDNG_FRAMEWORK` 分支引用生产端的 `DeviceReader.h`；DCurses CMake 使用消费端分支。`SexLabPPFunctions.h` 则由 DCurses 自己实现。

证据：`E:/source/repos/devious-curses-main/PCH.h:3-4`、`CMakeLists.txt:46-55`、`vcpkg.json:1-5`、`include/apis/DDNG_API.h:1-5,152-180`。

## SLSF 采用方式

原生 sink 解析 `eventName/strArg/numArg/sender`，消费 P+ 的普通 AnimationStart/LeadInEnd/AnimationEnd；typed SLSF 公开事件经薄 PSC 传输。参与者和场景数据走公开 VM 异步查询，设备事实走已核实的 DDNG v2 查询。

菜单、事件适配和业务分别接统一状态服务。1.6.1170 安装版的 DLL、协议及实际顺序由对应集成任务记录；本页保留的 DCurses 行为只用作源码参考。
