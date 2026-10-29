# 03 整链实施：证据与重放

日期：2026-10-28。[实施报告](../2026-10-28-03-chain-implementation.md)记录当前实现、测试结果和后续接入。

## 自动检查与审查

| 文件 | 内容 |
|---|---|
| [all-checks-final.log](all-checks-final.log) | 最终普通/ASan 核心、边界、差分、fuzz、基准；首行实际命令，末行 EXIT 0 |
| [junit-summary.txt](junit-summary.txt) | 各份 JUnit 的展开节点、断言、唯一性和 failure/error |
| [time-boundary-red.log](time-boundary-red.log) | 修改前两条事件边界回归失败：9/10、8/9 |
| [targeted-green.log](targeted-green.log) | 中间目标检查 51 用例 / 2,686 断言通过；最终全量见上 |
| [contracts.log](contracts.log) | 合同与 11 个负例检查 |
| [task-books.log](task-books.log) | 任务所有权、任务书、桌面提示与链接检查 |
| [standards-review.md](standards-review.md) | 独立 Standards 复核 |
| [spec-review.md](spec-review.md) | 独立 Spec 复核 |
| [comments-review.md](comments-review.md) | fresh 注释审查 |
| [review-tree.txt](review-tree.txt) | 最终 18 文件源码/测试快照身份 |

在仓库根目录重放自动检查：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/run-native-tests.ps1 -Mode all -Seed 1170 -Runs 100000 -Jobs 20
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/verify-contracts.ps1 -NegativeFixtures
python tools/verify-task-books.py
```

最终源码审核树为 `28b4160cbba5afc84ce869d3f990648510db74fa`，基线为 `7a43e4e4b3a67b3ac5fa2f4d33990ac4e0101e70`。二者用独立 Git index 生成；用户的 index 和 HEAD 保持原值。

## 真实 Advance 前后对照

[探针报告](advance-probe.md)、[探针源码](advance-probe.cpp)、[Bash 重放脚本](advance-probe.sh)、[首轮日志](advance-probe.log)、[归档脚本重放日志](advance-probe-replay.log)。

```powershell
& 'C:/Program Files/Git/bin/bash.exe' --noprofile --norc 'E:/source/repos/SLSF Reloaded NG/docs/evidence/implementation-03/advance-probe.sh'
```

脚本从本仓库的基线 Git 对象导出核心源码到临时目录，独立编译基线与当前生产 `Advance`，运行八组图/负载、七轮中位数，并比较三条路径的业务校验和。Clang 20.1 可从 PATH 调用，只有 AVX2 TU 使用 `-mavx2`。新输出写入 `build/diagnostics/implement-03/replay/advance-probe.log`，本目录保留首轮记录。

归档脚本已实际重放成功，八组校验和仍相同。默认图连续推进重放为 7.229 → 2.244 µs/次，prepared 为 1.839 µs/次；完整重放数值见日志。测量条件与首轮一致，P/E 核与频率未固定。短步推进未触发衰减抽样，排队命令与衰减由行为回归及独立参考验证。
