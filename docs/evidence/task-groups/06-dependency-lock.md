# 06 组依赖锁定表

日期：2026-10-29。范围：B03/B05/C01 实际使用的编译依赖、目标运行时与可选提供方。

兼容性按版本声明和 SKSE 的运行时接口判定，不使用游戏可执行文件或已安装 DLL 的哈希做版本校验。CommonLibSSE-NG 生成的 `SKSEPluginInfo` 声明运行库兼容策略；插件入口只通过 `SKSE::LoadInterface::RuntimeVersion()` 与 `SKSEVersion()` 记录宿主实际版本。

## 锁定项

| 依赖 | 版本/标识 | 声明来源 | 许可 | ABI、布局与线程/所有权 |
|---|---|---|---|---|
| CommonLibSSE-NG | 6.0.0，源码 commit `15ad90b31b2ca09cf477c7e1a43ff5ff05af120f` | `lib/commonlibsse-ng`；`res/commonlibsse-ng-plugin.cpp.in` 生成 `SKSEPluginInfo`，`RuntimeCompatibility = SKSE::VersionIndependence::AddressLibrary` | MIT | Windows x64；`SKSE_PLUGIN_LOAD`、消息、序列化、任务接口的编译期 ABI。消息/保存回调在 SKSE 主线程调用；本组不在回调内长期保留库对象指针。 |
| SKSE64 | 2.2.6（文件版本 `0.2.2.6`） | 安装目录 `skse64_loader.exe`、`skse64_1_6_1170.dll`；`E:/source/repos/skse64/skse64_common/skse_version.h:18-22,57` 定义 8/8/12/4 位打包版本；`LoadInterface::RuntimeVersion()`/`SKSEVersion()` 是运行时版本接口 | 外部运行时，未随仓库分发 | `SerializationInterface::SetUniqueID('SLSF')`、`WriteRecord/GetNextRecordInfo/ReadRecordData`；回调同步执行，记录编码在 Capture 锁外完成。 |
| Skyrim SE | 目标基线 1.6.1170；兼容性由 SKSE 加载器判定 | 项目规格与 SKSE 加载器；插件不读取 EXE 哈希，也不复制游戏版本表 | 商业游戏，未分发 | x64；本次目标运行时在 12:34 的隔离运行中到达 `kDataLoaded`。当前安装的 EXE 元数据为 1.7.104.0，SKSE 2.2.6 加载器已拒绝该组合，详见[最小游戏加载记录](06-game-load-record.md)。 |
| Devious Devices NG | 3.0.0.1，API v2 | 安装 `Data/SKSE/Plugins/DeviousDevices.dll`；公开接口 `E:/source/repos/DeviousDevicesNG-main/include/API.h`，`DD_APIVERSION=2U`，首虚函数 `virtual size_t GetVersion() const`（`API.h:108`） | 外部模组，未分发 | 进程级单例由 DDNG 在 `kDataLoaded` 创建；`GetAPI()` 返回裸指针。本组只同步读取 vtable 槽 0 的 `GetVersion()`，不缓存函数指针或对象；同一能力的重复查询由条件变量合并，provider 查询期间不持有注册表锁。完整设备调用留给 C04。 |
| Catch2 | 3.8.1 | 根 `xmake.lua:6` `add_requires("catch2 3.8.1")` | BSL-1.0 | 测试进程内，Catch2 main/lib 组件。 |
| LLVM / clang-cl | 20.1.0 | `C:/Program Files/LLVM/bin/clang-cl.exe`；ASan 运行库目录 `C:/Program Files/LLVM/lib/clang/20/lib/windows` | Apache-2.0 WITH LLVM-exception | ASan 目标使用 clang-cl；Windows 不支持 LeakSanitizer，释放路径由行为断言与 ASan 内存错误检查覆盖。 |
| MSVC Build Tools | 17.14.37710.0，工具集 14.44.35207 | `E:/BuildTools/VC/Tools/MSVC/14.44.35207/bin/Hostx64/x64/cl.exe` | Microsoft EULA | 生产插件与普通测试；`/std:c++23`、`/fp:precise`；AVX2 仅限 `src/core/numeric/**` 与 `src/core/reputation/DiffusionKernelAvx2.cpp`。 |
| Xmake | 3.0.9 (`v3.0.9+master.0a2255cc2`) | 本地 `xmake --version` | Apache-2.0 | 构建与测试编排。 |

## 锁定结论

- `src/main.cpp` 在 `SKSE::Init` 后记录 `host: runtime=... skse=... commonlib=...`：运行时版本来自 `LoadInterface::RuntimeVersion()`，SKSE 版本来自 `SKSEVersion()`，CommonLib 版本来自编译期 `COMMONLIB_VERSION`。这是诊断证据，不是哈希白名单。
- 插件元数据由 `commonlibsse-ng.plugin` 规则生成，声明 `RuntimeCompatibility = SKSE::VersionIndependence::AddressLibrary`；宿主是否可加载由 SKSE 加载器按自身版本表判断。
- 12:34 的隔离运行在目标运行时到达 `DataLoaded`：`locations=766 regions=766 ownershipRules=81`（父地点投影修正前的历史构建），DDNG 从 `Waiting` 完成为 `Available version=2 api=2`，日志见 [`2026-10-29-group06-game-load-ddng-plugin.log`](logs/2026-10-29-group06-game-load-ddng-plugin.log)。
- 当前安装的 `SkyrimSE.exe` 文件版本为 `1.7.104.0`；同一 SKSE 2.2.6 加载器在 14:36 明确输出 `You are using a newer version of Skyrim than this version of SKSE64 supports`/`Runtime: 1.7.104`，因此本机当前不能复跑目标加载。证据见 [`2026-10-29-group06-game-load-blocked-skse64-loader.log`](logs/2026-10-29-group06-game-load-blocked-skse64-loader.log)。需要恢复 1.6.1170 或安装与该运行时匹配的 SKSE 后才能重跑。
- 本组没有新增外部源码副本；CommonLibSSE-NG 保持既有 submodule/license。DDNG 只按公开接口声明做最小 ABI probe，未链接或分发其二进制。
