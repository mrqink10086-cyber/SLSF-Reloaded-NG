# 中文 MO2 测试包交付验证

日期：2026-11-05。

本次只调整说明与打包，不改变 DLL、ESP、脚本或默认配置。设置仍使用 SKSE 菜单。

## 布局与文件名

MO2 官方故障排查文档要求模组数据根目录直接包含应进入游戏 Data 的文件和目录。本包根目录直接放置 `SLSF Reloaded.esp`、`Scripts`、`Source`、`SKSE`、`Interface`，符合这一目录规则，不再套一层 `Data`。依据：[MO2 Troubleshooting](https://github.com/ModOrganizer2/modorganizer/wiki/Troubleshooting)，章节“The content of <data> does not look valid”。

压缩包名为 `SLSF声望重制NG-0.1.0-中文测试版-MO2.zip`。中文说明集中在 `Docs/SLSFReloadedNG`，包括安装说明、依赖与兼容说明、鸣谢与来源、手动安装脚本、文件清单及 SHA-256 校验表。法律文本和原始署名保留原名与原文。

ZIP 使用正斜线相对路径及 UTF-8 中文文件名。ESP、DLL、PSC、PEX 和配置文件保留原名，以维持运行时兼容。

## 验证结果

- ZIP CRC、重复条目、路径层级及中文文件名编码检查通过。
- 文件清单中的 73 项载荷及校验表 SHA-256 全部匹配；两份清单不包含自身。
- 25 对 PSC/PEX 齐全，64 个游戏文件逐字节等同上一版中文菜单测试包；DLL 同时与构建产物匹配。
- 在中文解压路径下，用 Windows PowerShell 5.1 执行手动安装：首次复制 64 项；再次安装复制 63 项并保留 1 项已有配置，自定义配置哈希保持不变。
- 包目录及目标 Data 路径带末尾斜线均通过；损坏一个 PEX 后，安装器在创建目标目录前拒绝安装。
- 手动安装脚本把默认包路径的求值移到脚本主体，避免参数默认值阶段的脚本目录不可用。
- MO2 配置保留方式已明确写入说明：使用单独的高优先级配置模组；MO2 安装过程不执行手动安装脚本。

验证通过 `build/mo2-package-smoke.py` 执行，所有安装目标均位于仓库忽略的 `build/mo2-smoke-*` 中。Windows PowerShell 子进程使用其自身模块目录，避免测试宿主的 PowerShell 7 模块路径干扰。

本次没有启动 MO2 或游戏；目录规范依据官方说明，验证覆盖 ZIP 结构、内容一致性与安装脚本。未重复 C++、Papyrus、核心或 ASan 测试。

## 重打包

执行 `powershell -NoProfile -ExecutionPolicy Bypass -File tools/build-staging.ps1`。最终源码提交后再次打包，包内 `文件清单.json` 应记录该提交及 `sourceDirty=false`。
