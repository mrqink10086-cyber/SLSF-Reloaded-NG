# 06 组最小游戏加载记录

日期：2026-10-29。目标：Skyrim SE 1.6.1170 + SKSE 2.2.6。目的：验证插件真实入口、消息注册顺序、静态地点表构建、DataLoaded 时的可选能力完成查询与 DDNG v2 ABI。

## 环境与版本判定

- 游戏目录：`E:/st/steamapps/common/Skyrim Special Edition`。
- 目标基线：Skyrim SE 1.6.1170.0；插件元数据由 CommonLibSSE-NG 生成，运行库兼容策略为 `SKSE::VersionIndependence::AddressLibrary`。
- 宿主版本只通过 SKSE 官方接口记录：`SKSE::LoadInterface::RuntimeVersion()` 与 `SKSEVersion()`。不读取 EXE 哈希，也不以文件哈希作为加载条件。
- 当前安装的 `SkyrimSE.exe` 文件元数据为 `1.7.104.0`。本机 SKSE 2.2.6 加载器已明确拒绝该运行时，当前无法重新执行目标加载；历史 12:34 目标运行与 14:36 阻断分别记录在下方。
- 当前插件：`build/windows/x64/releasedbg/SLSFReloadedNG.dll`，790016 bytes，构建命令 `xmake build SLSFReloadedNG`，`EXIT=0`，日志 [`2026-10-29-group06-plugin-build-final.log`](logs/2026-10-29-group06-plugin-build-final.log)。历史 12:34 运行使用的是父地点投影修正前的 788992 bytes 构建；修正版尚未在目标运行时复跑。
- 隔离方式：探针脚本把 `Data/SKSE/Plugins` 下除本插件和 `DeviousDevices.dll` 外的 DLL 临时改名 `.group06-disabled`；运行后恢复并确认无残留。

## 历史目标运行（12:34，Skyrim SE 1.6.1170 + SKSE 2.2.6）

命令：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/group06-game-load-probe.ps1 `
  -MinimalPlugins -KeepPlugins DeviousDevices.dll -Label ddng -TimeoutSeconds 150
# GAME_LOAD_MARKERS=True loader_exit=True game_pids=28500 minimal=True
# EXIT=0

powershell -NoProfile -ExecutionPolicy Bypass -File tools/group06-game-load-probe.ps1 `
  -MinimalPlugins -Label minimal -TimeoutSeconds 150
# GAME_LOAD_MARKERS=True loader_exit=True game_pids=39340 minimal=True
# EXIT=0
```

DDNG 隔离日志 [`2026-10-29-group06-game-load-ddng-plugin.log`](logs/2026-10-29-group06-game-load-ddng-plugin.log)：

```text
SLSF Reloaded NG lifecycle bootstrap
lifecycle: baseline ISA check avx2=true
lifecycle: serialization and messaging listeners registered
[capability:postload] ddng state=Waiting version=0 api=0 reason=module not loaded yet
[capability:postpostload] ddng state=Waiting version=0 api=0 reason=module not loaded yet
lifecycle: static table built locations=766 regions=766 ownershipRules=81
lifecycle: startup configuration status=1 message=configuration unchanged
[capability:dataloaded] ddng state=Available version=2 api=2
```

SKSE 日志 [`2026-10-29-group06-game-load-ddng-skse64.log`](logs/2026-10-29-group06-game-load-ddng-skse64.log) 显示加载器发现 `DeviousDevices.dll` 与 `SLSFReloadedNG.dll`，依次派发消息 0/1/6/8。仅本插件运行的能力状态为 `Missing`，静态表仍为 766/766/81，见 [`2026-10-29-group06-game-load-minimal-plugin.log`](logs/2026-10-29-group06-game-load-minimal-plugin.log)。

这组历史记录验证：

1. 序列化与消息监听在 `kPostLoad` 广播前注册；
2. DDNG 在 `kPostLoad` 尚未发布 API 时不误判为 Missing，`Waiting` 在 `DataLoaded` 被完成；
3. `GetAPI()` vtable 首槽 `GetVersion()` 在目标 DLL 上返回 2，与 API v2 锁定一致；
4. `TESDataHandler` 在 DataLoaded 可用，766 个地点完成稳定键索引与归属绑定。

说明：历史二进制入口尚未加入 `host: runtime=...` 日志；该日志是在本次审查修正中加入的。当前代码若在目标运行时加载，会先输出这行版本诊断。

## 当前阻断（14:36）

命令：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/group06-game-load-probe.ps1 `
  -MinimalPlugins -KeepPlugins DeviousDevices.dll -Label blocked -TimeoutSeconds 150
# GAME_LOAD_MARKERS=False loader_exit=True game_pids= minimal=True
# EXIT=1
```

加载器日志 [`2026-10-29-group06-game-load-blocked-skse64-loader.log`](logs/2026-10-29-group06-game-load-blocked-skse64-loader.log)：

```text
productVersionStr = 1.7.104.0
product name = TESV: Skyrim
You are using a newer version of Skyrim than this version of SKSE64 supports.
Runtime: 1.7.104
SKSE64: 2.2.6
```

这不是插件自身的哈希/版本分支：SKSE 加载器在插件入口之前就拒绝了该运行时。要重跑本组的目标加载，需要恢复 Skyrim SE 1.6.1170，或安装与当前 1.7.104 匹配的 SKSE；插件的 `RuntimeVersion()`/`SKSEVersion()` 日志会记录实际值，不需要额外哈希校验。

## 限制与说明

- 历史目标运行与当前阻断都在同一台机器、同一插件目录上完成；当前 EXE 已被更新/替换，无法用同一二进制复现 1.6.1170 加载。
- 本记录未进行“新游戏/读档”交互；读档、保存、A/B 切换与失败关闭由 `SLSFCoreTests` 的生命周期用例验证。
- 历史 12:34 构建未包含评审后的 `parentLoc → parentFormId` 投影；当前修正版只完成编译，未能在目标运行时复跑，因此静态表的父子归属尚未取得新的实机日志。
- 探针脚本 `tools/group06-game-load-probe.ps1` 已修正：运行前删除旧的 SKSE/插件日志，避免上一次运行的 `static table built` 被误判为本次成功。
- 另有一次完整模组环境尝试在 150 秒内未到 DataLoaded，不能作为通过证据；隔离运行才是本组的目标运行时记录。

