# SLSF Reloaded NG 0.1.0 game-test package

Target: Skyrim Special Edition 1.6.1170, SKSE 2.2.6, Windows x64.

Install the `Data` directory with a mod manager, or run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Install.ps1 -DataDirectory 'E:\Games\Skyrim Special Edition\Data'
```

The installer verifies every file against `manifest.json` before writing. Existing
`SKSE/Plugins/SLSFReloadedNG.json` is preserved. On first installation the shipped
JSON supplies defaults. When using a mod manager, retain your existing JSON as
the winning file. Exports and other user files are not part of the package.

Enable `SLSF Reloaded.esp`. Let this package replace the same-name SLSF scripts;
remove the previous NG DLL installation before selecting this version in the mod
manager. Use the SKSE Menu Framework menu for settings and JSON operations. The
same-name MCM script preserves attached properties and does not register a menu.

本模组的 SKSE 菜单直接使用简体中文。打开菜单框架的字体设置，启用
Chinese 并选择 `Futura Condensed (CN)`；也可在启动游戏前编辑
`Data/SKSE/Plugins/SKSEMenuFramework.ini`：

```ini
[Fonts]
PrimaryFont = Futura Condensed (CN)
EnableChinese = true
```

中文字体由游戏的中文字体资源提供。如果显示方框，请检查上述选项和中文游戏字体。
本包不覆盖菜单框架的全局配置。

The package contains the frozen ESP/interface assets, one NG DLL, 25 SLSF PEX and
their sources, defaults, licenses and a hash manifest. It contains no replacement
SexLab, Public Whore, SLACS or FHU PEX. Compiler interface declarations are build
inputs only; see the source repository's `tools/papyrus-imports/README.md`.

Read `DEPENDENCIES.md` before testing. Automated core, sanitizer, fuzz and
controlled boundary checks are recorded in the source repository's delivery
report. In-game integration verification is the remaining user-run acceptance
step. Test loading/saving, location transitions, scene start/lead-in/stages/end,
optional-provider changes, JSON reload and compatibility ModEvent replies.

Rebuild with the pinned CommonLib submodule and configured MSVC/xmake environment:

```powershell
xmake build -y SLSFReloadedNG SLSFConfigProbe
powershell -NoProfile -ExecutionPolicy Bypass -File tools/compile-papyrus.ps1 -Clean
powershell -NoProfile -ExecutionPolicy Bypass -File tools/build-staging.ps1
```

`manifest.json` records source and CommonLib revisions, runtime, compiler mode,
Papyrus compilation inputs and SHA-256 per payload file. `SHA256SUMS.txt` mirrors
the payload hashes; the manifests exclude themselves. Paths and byte hashes
identify the exact delivered payload; a fresh build can have different PE/PEX
timestamps and is not promised to be byte reproducible.
