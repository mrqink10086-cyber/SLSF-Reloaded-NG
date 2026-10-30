# Global 与 Papyrus 实例 FormID 修复

日期：2026-10-30。修复范围是第 07 组现有输出、任务/alias 绑定和生成数据中的固定完整 FormID。普通测试、ASan、DLL 构建、冻结合同校验及两轴独立复核通过。实际游戏输出和 PEX 执行仍待目标环境验收。

## 身份与查询

| 入口 | 修复后的身份 |
|---|---|
| 25 个当前声望 Global | `SLSF Reloaded.esp` + 本地 ID，例如 Whore 为 `0x80D` |
| 20 个旧版桥接 Global | `SexLab - Sexual Fame [SLSF].esm` + `0x24255`–`0x24268` |
| Papyrus 任务实例 | `StableFormKey{plugin, localFormId}` |
| Papyrus alias 实例 | 所属任务的稳定键 + 原 alias 索引 |

`0x0600080D` 是冻结旧源的完整 ID。输出表现在保存插件名和本地 ID，当前加载索引交给引擎解析。桥接插件名和本地 ID 来自旧仓库 `Scripts/Source/SLSF_Reloaded_ModIntegration.psc:270` 的 `GetFormFromFile` 调用。

[`RecordLookup.h`](../../include/slsf/engine/RecordLookup.h) 调用 `TESDataHandler::LookupForm<T>(localFormId, plugin)`；CommonLib 负责普通/light 插件的当前索引和记录类型校验。调用前拒绝空身份、超范围本地 ID，以及 light 插件中大于 `0xFFF` 的 ID。缺插件、缺记录、类型不符返回空，不缓存缺失结果。

[`LegacyRecords.cpp`](../../src/boundary/LegacyRecords.cpp) 提供实际 `RE::TESGlobal` 写入与 `RE::TESQuest` 查询。实例反查从引擎任务对象取得源文件名和 `GetLocalFormID()`。边界要求在 DataLoaded 后的游戏线程调用；没有新增运行时 ESP/ESL 文件解析器或手工加载索引拼接。StorageUtil 使用宿主提供的 VM 写入回调。

生成器从冻结的 `legacy-records.json` 关联任务记录，验证 `QUST` 和旧源完整插件索引后，仅在离线生成阶段提取本地 ID。已重生成 124 项函数表及 16 份 PSC；函数声明保持相同，实例绑定说明改为插件名和本地 ID。`contracts/` 与 `tests/fixtures/contracts/` 保持原取证数据，重复生成得到相同输出。

## 验证

工作目录为本仓库根目录，seed 为 `1170`。

| 命令/检查 | 结果 | 记录 |
|---|---|---|
| `tools/run-native-tests.ps1 -Mode all -Seed 1170 -FuzzIterations 100000 -Jobs 8` | 退出 0；普通/ASan 核心各 300 用例、139303 断言，边界各 4 用例、22 断言；差分、基准、StateCodecFuzz 100000 次和离线调度通过 | [native-all.log](formid-fix-20261030/native-all.log) |
| `xmake build -y -j8 SLSFCoreTests SLSFCoreTestsAsan SLSFReloadedNG` | 退出 0，实际 CommonLib 调用编入 DLL | [final-build.log](formid-fix-20261030/final-build.log) |
| 普通/ASan 核心 EXE 的 `[formid-fix] --rng-seed 1170` | 各 4 用例、2973 断言通过 | [普通](formid-fix-20261030/formid-regressions.log)、[ASan](formid-fix-20261030/formid-regressions-asan.log) |
| 最后整理后的普通/ASan `LegacyOutputs*,Publish*` | 各 10 用例、1758 断言通过 | [普通](formid-fix-20261030/final-output-checks.log)、[ASan](formid-fix-20261030/final-output-checks-asan.log) |
| `tools/verify-contracts.ps1 -NegativeFixtures` | 124 函数/86 public、52 输入、7 回应、25 类、58 记录、23 规则；11/11 错误 fixtures 被拒绝 | [contracts.log](formid-fix-20261030/contracts.log) |
| `python tools/gen-papyrus-bindings.py` 与输出比较 | 124 项/16 PSC，再生成字节相同 | [generator.log](formid-fix-20261030/generator.log) |

ASan 的故意越界探针按预期报错，这是检查工具生效的负对照；核心和边界用例通过。全量运行后的改动仅为测试循环引用、变量改名和注释整理，相关测试已重建并复跑。

[`RecordLookupTests.cpp`](../../tests/core/RecordLookupTests.cpp) 用可控引擎数据覆盖普通插件索引 `0x06`/`0x35`、light 索引 `0x000`/`0x037`，检查全部 25 Global、20 桥接 Global 及全部非 Global 函数的任务/alias 绑定。另覆盖错误插件、文件名大小写、缺失后出现、错误类型、临时记录，以及把完整/带 light 索引的 ID 当作本地 ID。

该矩阵验证提交给引擎的插件/本地 ID 参数和边界行为；真实加载索引解析由 CommonLib 执行，尚未在游戏中重放。DLL 构建不能替代目标游戏写入及 PEX 验收。

## 独立复核与工作区

Standards 与 Spec 两个独立只读代理分别复核修复前快照到修复后的差异，均未发现确证缺陷或需求遗漏。Standards 建议的两句实现说明已删除，测试常量已改名为 `kExpectedGlobalLocalIds`；保留引擎时序/所有权和冻结实例合同注释。

本次在既有、未提交的第 07 组工作上修复；保留其他在途改动。用户随后要求全部提交，第 07 组实现、审核修正、FormID 修复及现存验证记录一并纳入本次提交。API/VM 的整体宿主接线继续按 B03/C03 联调计划推进，见 [07 组报告](task-groups/07-vm-and-public-api.md)。本次没有把离线模块的通过扩展为游戏验收完成。
