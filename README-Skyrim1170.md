# SLSF Reloaded NG 0.1 — SkyrimSE.exe 1.6.1170 本地开发配置

## 项目与源码参考

- 主项目：`E:\source\repos\SLSF Reloaded NG`。
- 项目/插件显示名称：`SLSF Reloaded NG`；版本：`0.1`（构建元数据为 `0.1.0`）。
- 构建目标和 DLL 基名：`SLSFReloadedNG`。
- 插件入口：`src/main.cpp`；公共头：`src/pch.h`；构建入口：`xmake.lua`。
- 编译依赖：模板固定提交的 `lib/commonlibsse-ng` 子模块，以及 Xmake 管理的 DirectXMath、DirectXTK、spdlog。
- `E:\source\repos\skse64` 仅作为独立源码参考，不加入插件工程，也不与 CommonLibSSE-NG 混用头文件或静态库。
- SKSE 源码已切换至 `v2.2.6` 标签（detached HEAD），提交 `9398d04592a7eb9d754f2997701116df1022f1b4`，与 1.6.1170 对应。
- 模板提交为 `117325dfe8ef4351b6a973e55692ea522dfbf026`；CommonLib 子模块提交为 `15ad90b31b2ca09cf477c7e1a43ff5ff05af120f`。
- 项目名和版本已写入 `xmake.lua`；作者信息留空，待填写实际作者。

## 构建

需要 Xmake >= 3.0.0、支持 C++23 的 MSVC、Windows SDK。当前模板使用 Xmake，无需为它另建 CMake/vcpkg 工程。

在主项目目录运行：

```powershell
pwsh -NoProfile -File .\build-skyrim1170.ps1 -ConfigureOnly
pwsh -NoProfile -File .\build-skyrim1170.ps1
```

也可以在当前 PowerShell 会话执行 `& .\build-skyrim1170.ps1`。脚本的配置命令为：

```powershell
xmake f -y -p windows -a x64 -m releasedbg --toolchain=msvc --skyrim_se=n --skyrim_ae=y --skyrim_vr=n
```

1.6.1170 属于 CommonLib 的 AE（1.6.x）目标，即使游戏程序名仍是 SkyrimSE.exe。此配置启用 AE，关闭 1.5.x SE 和 VR；它不表示已验证其他 AE 版本兼容，也不会自动把插件限制为仅可在 1.6.1170 加载。

构建脚本临时把模板的自动安装路径指向本项目 `build/staging`，结束后恢复原环境变量。无需游戏路径即可编译，生成文件不会直接写入游戏目录。

`xmake.lua` 将三个库依赖设为 `system = false`，避免本机 Xmake 在缓存未命中时探测缺失的 vcpkg。由于 GitHub 常规下载连接失败，DirectXTK 24.2.0 和 spdlog v1.16.0 的官方源码包已放在 `build/downloads`，并与 Xmake 包定义的 SHA256 核对一致；脚本会自动使用该目录，不修改全局 Xmake 设置。

可选的嵌套 `extern/openvr` 子模块下载遇到网络连接失败，暂未初始化。本配置关闭 VR，因此 AE 构建不需要它；以后启用 VR 时先运行 `git submodule update --init --recursive`。

- DLL/PDB：`build/windows/x64/releasedbg/`。
- DLL：`build/windows/x64/releasedbg/SLSFReloadedNG.dll`。
- 本地暂存 Mod：`build/staging/SLSFReloadedNG/SKSE/Plugins/`。
- 可选 Visual Studio 工程：配置成功后运行 `xmake project -k vsxmake`。
- 可选 clangd 索引：配置成功后运行 `xmake project -k compile_commands`。

## 游戏运行前提

以 **Steam SkyrimSE.exe 1.6.1170** 为本次目标：

1. 使用匹配此运行时的 **SKSE64 2.2.6**，不要仅依据官网“最新版”选择。本地参考源码已固定至对应标签。
2. 安装 Address Library for SKSE Plugins 的 AE / 1.6.x 数据，确认游戏 `Data/SKSE/Plugins/versionlib-1-6-1170-0.bin` 存在。
3. 核实实际 SkyrimSE.exe 的文件版本和商店来源，再通过 `skse64_loader.exe` 启动。此处尚未确认本机游戏安装位置，也没有进行游戏内加载验证。
4. 通过 Mod 管理器部署暂存目录中的 Mod，或在确认目标路径后复制 DLL/PDB 到游戏 `Data/SKSE/Plugins`。

参考：

- [模板说明](https://github.com/libxse/commonlibsse-ng-template)
- [SKSE 官网与历史版本入口](https://skse.silverlock.org/)
- [SKSE 更新记录（2.2.6 支持 1.6.1170）](https://github.com/ianpatt/skse64/blob/master/skse64_whatsnew.txt)
- [Address Library](https://www.nexusmods.com/skyrimspecialedition/mods/32444)

上游许可证文件保持原样。

## 本机验证记录（2026-10-26）

- Xmake `3.0.9+master.0a2255cc2`；Visual Studio Build Tools 2022；MSVC `14.44.35207`。
- `build-skyrim1170.ps1 -ConfigureOnly` 退出码 0；完整 `build-skyrim1170.ps1` 退出码 0，输出 `build ok`。
- 已生成 x64 插件 DLL/PDB，并暂存到上述 Mod 目录。
- 已检查 `SKSEPlugin_Load` 和 `SKSEPlugin_Version` 导出；暂存 DLL 与构建 DLL 的 SHA256 相同。
- `git diff --check` 通过；SKSE 参考仓库工作树干净。
- 尚未部署到游戏，也未进行 1.6.1170 游戏进程内加载测试。
- 项目更名为 SLSF Reloaded NG 后已重新构建成功；`SLSFReloadedNG.dll` 的 ProductName 为 `SLSF Reloaded NG`，ProductVersion/FileVersion 为 `0.1.0.0`。
