# 本机C++测试工具实测

日期：2026-10-27。目的：确认任务书采用的工具在这台电脑上能编译并运行。

## 工具

| 项目 | 本机结果 |
|---|---|
| Xmake | 3.0.9+master.0a2255cc2，`C:/Program Files/xmake/xmake.exe` |
| clang-cl / clang++ | LLVM 20.1.0，x86_64-pc-windows-msvc |
| VS Build Tools | `E:/BuildTools`，用VsDevCmd配置x64开发环境 |
| Python | `C:/Users/15219/AppData/Local/Programs/Python/Python313/python.exe` |
| LLVM ASan DLL | `C:/Program Files/LLVM/lib/clang/20/lib/windows/clang_rt.asan_dynamic-x86_64.dll` |

## 已执行结果

| 探针 | 实际结果 |
|---|---|
| clang-cl ASan编译 | 返回0，选项 `/std:c++20 /fsanitize=address /Zi /Od /MD`，链接 `/INCREMENTAL:NO` |
| 8字节数组写索引7 | 返回0，输出 `probe=42` |
| 同一程序故意写索引8 | 返回1，ASan报告 `heap-buffer-overflow`，定位probe源第8行 |
| clang++ fuzz编译 | 返回0，选项 `-std=c++20 -g -O1 -fsanitize=fuzzer,address` |
| libFuzzer运行 | 返回0，seed1170，`#1000 DONE`，完成1000次 |

首次直接运行探针返回0xC0000135（缺少DLL）。确认LLVM ASan库实际位置后，仅给测试子进程PATH添加其目录，正常探针随后通过，越界探针按预期被抓到。01组测试脚本需要做同样配置。

探针代码和程序位于当前工作区 `build/planning-probes/`，属于临时构建输出。01组负责将可重复的工具探针纳入正式测试工程。

## 复现命令

在项目根编译；VsDevCmd负责开发环境，路径中的空格保持原引号：

```powershell
cmd.exe /d /s /c 'call "E:\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul && clang-cl /nologo /std:c++20 /fsanitize=address /Zi /Od /MD build\planning-probes\asan_probe.cpp /Fobuild\planning-probes\asan_probe.obj /Febuild\planning-probes\asan_probe.exe /link /INCREMENTAL:NO'
cmd.exe /d /s /c 'call "E:\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul && clang++ -std=c++20 -g -O1 -fsanitize=fuzzer,address build\planning-probes\fuzzer_probe.cpp -o build\planning-probes\fuzzer_probe.exe'
```

在临时PowerShell会话里运行，PATH修改仅留在这个会话：

```powershell
$env:PATH = 'C:\Program Files\LLVM\lib\clang\20\lib\windows;' + $env:PATH
.\build\planning-probes\asan_probe.exe
.\build\planning-probes\asan_probe.exe overflow
.\build\planning-probes\fuzzer_probe.exe -runs=1000 -seed=1170
```

中间的overflow命令预期返回非0并报告越界，其余两个预期返回0。

这些结果验证测试工具能用。SLSF业务CPP、Catch2测试target和解码器fuzz target由后续任务组实现，再用这些工具验证。
