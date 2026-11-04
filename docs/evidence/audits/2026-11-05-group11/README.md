# 第 11 组审核重放与证据

日期：2026-11-05（Asia/Shanghai）。源码提交 `b5f47eb9ec70729ca24c32e7d0e7a194fee09c18`。以下从仓库根执行，工具链为当前 Windows x64 MSVC/LLVM/xmake；探针无需游戏或 VM。

## 常规检查

PowerShell：

```powershell
& ./tools/run-native-tests.ps1 -Mode all -Seed 1170 -Jobs 8 *> build/group11-review-all.log
$LASTEXITCODE
xmake build -y SLSFReloadedNG *> build/group11-review-plugin.log
$LASTEXITCODE
& ./tools/run-native-tests.ps1 -Mode fuzz -Target JsonTransferFuzz -Runs 100000 -Seed 1170 -Jobs 8 *> build/group11-review-json-fuzz.log
$LASTEXITCODE
```

| 归档 | 观察结果 |
| --- | --- |
| [native-all.log](native-all.log) | 退出 0；核心普通/ASan 各 414/150051，边界各 4/22，差分 11/19605；StateCodecFuzz 100000 |
| [plugin-build.log](plugin-build.log) | 退出 -1；VmNatives.cpp:106 C3668、:116 C2259 |
| [json-fuzz.log](json-fuzz.log) | 退出 0；JsonTransferFuzz 100000，cov 1560 / ft 5818 |

all 的越界负向 ASan 诊断是测试工具自检；普通及 ASan 功能套件均通过。独立 DLL 构建失败是产品缺陷，不能被 all 的成功覆盖。

## 定向模拟

在 x64 MSVC Developer Command Prompt 中运行；`clang++` 也需在 PATH。输出仅写入 `build/group11-review`。

```bat
if not exist "build\group11-review" mkdir "build\group11-review"
cl /nologo /std:c++latest /EHsc /MD /Iinclude "docs\evidence\audits\2026-11-05-group11\behavior-probe.cpp" "build\windows\x64\releasedbg\SLSFCore.lib" /Fe:"build\group11-review\behavior-probe.exe" /Fo:"build\group11-review\behavior-probe.obj"
"build\group11-review\behavior-probe.exe"
clang++ -std=c++23 -fms-runtime-lib=static -fsanitize=float-cast-overflow -Iinclude "docs\evidence\audits\2026-11-05-group11\number-probe.cpp" "src\io\JsonTransfer.cpp" -o "build\group11-review\number-probe.exe"
"build\group11-review\number-probe.exe"
```

[behavior-probe.cpp](behavior-probe.cpp) 链接真实核心；通知只计数，镜像当前仅日志而不应用配置的装配；导入循环镜像菜单/native 的逐项 PublicApi 提交。它没有执行 `VmNatives.cpp` 或 SKSE 回调。实际[输出](behavior-probe.log)：

```text
config_update_success=1 notifications=2 initial_revision=1 store_revision=2 status_revision=2 published_revision=1
import_before=20 applied=1 rejected=1 import_after=12
```

[number-probe.cpp](number-probe.cpp) 直接编译真实 JsonTransfer，并启用 float-cast-overflow。实际[输出](number-probe.log) 包含 `1e+20 is outside the range of representable values of type 'long long'`。此次 sanitizer 使用恢复模式并退出 0；此项判为缺陷复现。

上述两条编译与执行命令已重放，合并输出见 [replay-check.log](replay-check.log)。数值探针使用 static CRT 匹配本机 LLVM UBSan runtime；行为探针使用 `/MD` 匹配当前 `SLSFCore.lib`。

## 输入身份

| 文件 | SHA-256 |
| --- | --- |
| src/compat/VmNatives.cpp | 8870820C9CD0577A2E9AC7CC814F7831344E5385B00BDD01035F7C67220BCCBD |
| src/io/JsonTransfer.cpp | D7DFAB1189F08FA02C3EF7EC93F9267800C173DBE63D0FD6A6589444DD09D52B |
| src/main.cpp | 50CBD357F2B657DA93CFD5A9093A6B08F9E272572556B7C6976C618FE41E37B7 |
| build/windows/x64/releasedbg/SLSFCore.lib | 4A23B2AC3B4073E533846B573E568ABC78961579C9D5803FC0017971D9C4AF6C |

本目录保存原始日志及探针源码，不保存可执行文件。不同编译器、已有 fuzz corpus 或调度可能改变诊断顺序、覆盖率和耗时；语义结果及固定源码身份用于核对。
