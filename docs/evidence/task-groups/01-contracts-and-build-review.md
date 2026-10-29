# 01 组独立验收与修正

日期：2026-10-27。项目：SLSF Reloaded NG 0.1；目标 SkyrimSE.exe 1.6.1170。

本页记录 2026-10-27 的初次验收：01 组的接口清单、共享类型和测试/构建基础通过修正后验收。02 后续实现与修复见[当前修复记录](../2026-10-28-01-02-supplement-review.md)。

## 兼容边界

兼容下游常用功能、公开函数和事件的使用方式。已批准的旧 BUG 修正、传播、时间和存储按新方案验收。124 个历史函数中，86 个公开入口遵循兼容要求，其余内部职责由新设计实现。

例如旧 PEX 的手动传播先整除百分比，导致 10–99% 无增长。合同现规定先用浮点百分比计算，最后截断：来源 100、目标 20、比例 30，结果为 50。这里只修正了规则合同，01 组没有实现传播函数。

## 独立检查及发现

检查原 PSC、PEX、ESP、当前 C++ 类型、校验器、测试入口及实际编译/链接命令。

| 发现 | 复现及处理 |
|---|---|
| 签名校验漏检 | 在临时副本将 GetFameValue 参数由 String 改为 Int，原校验仍成功。新增独立签名快照，校验返回值、参数类型/次序/默认值、Global/Native 原声明及实例 VMAD 绑定；新增 5 个损坏样例。 |
| 合法索引 0 被判无效 | 新增 `[ids]` 用例先失败。LocationId、RegionId、ApiLocationIndex 改用类型最大值表示无效；TaskId/RequestId 仍以 0 表示无效。 |
| 测试入口漏掉新增用例 | 新 `[ids]` 用例失败时，原 runner 因硬编码标签仍报告通过。现每个目标运行全部测试，console 与 JUnit 均不筛标签。 |
| 独立头文件未被正常构建收集 | core/boundary 的 `*.cpp` 改为 `**.cpp`，覆盖 headers 子目录；独立编译及无游戏依赖检查通过。 |
| 非类别标志字符串错误 | `WhoreEvent` 改为公开查询使用的 `Whore Event`，先复现断言失败再修正；事件名保持原名称。 |
| 合同中的技术描述错误 | 修正 double AVX2 为每向量 4 lane、32 槽为 8 向量；明确 FormID 的 master 索引与 local ID 区别；修正各入口数值边界与来源。 |
| 持久化 DTO 混入运行标记 | 从 PersistentState 移除 epoch、revision，避免后续 codec 将进程内代次写入存档。 |
| 无用占位和注释 | 删除未调用的 BoundaryAnchor 占位与重复叙述注释；保留未知事实、单位、资产顺序、快照所有权等约束说明。 |

独立来源核查：剥除注释解析 17 份 PSC 的 124 个函数声明，使用 Orvid/Champollion v1.3.2 反编译的对应 17 份 PEX 复核类型及声明标志。默认值取自 PSC，因为 PEX 不保存默认值。共记录 17 PSC + 18 PEX 的 35 个文件 SHA256。

另直接遍历 ESP 记录与 VMAD，核对 58 个记录身份、320 项属性（包含 alias 属性）。没有发现与当前资产清单的内容差异。独立声明快照保存在 [api-signatures.json](../../../tests/fixtures/contracts/api-signatures.json)；临时审计脚本、反编译产物与完整构建日志在被忽略的 `build/review01/`，不进入插件。

## 最终验证

以下命令在 `E:/source/repos/SLSF Reloaded NG` 运行；PowerShell 使用进程级 Bypass，没有修改系统执行策略。

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/verify-contracts.ps1 -NegativeFixtures -EspPath '../SLSF Reloaded/SLSF Reloaded.esp'
powershell -NoProfile -ExecutionPolicy Bypass -File tools/run-native-tests.ps1 -Suite all
xmake build -r -v SLSFCoreTests
xmake build -r -v SLSFReloadedNG
powershell -NoProfile -ExecutionPolicy Bypass -File build-skyrim1170.ps1
python tools/verify-task-books.py
git diff --check
```

| 检查 | 实际结果 |
|---|---|
| 资产与合同校验 | 退出 0；124 函数（86 公开）、52 输入、7 回应、25 类别、58 记录、23 规则；11/11 损坏样例准确拒绝。 |
| 普通 core | 9 用例 / 56 断言通过；seed 1170；console、JUnit 均成功。 |
| 普通 boundary | 4 用例 / 22 断言通过；console、JUnit 均成功。 |
| ASan core / boundary | 分别 9/56、4/22，通过；console、JUnit 均成功。 |
| ASan 正负探针 | 8 个 int 的数组写索引 7 输出 `probe=42 index=7` 并退出 0；索引 8 非零退出，报告 `AddressSanitizer: heap-buffer-overflow`，位置 `tests/asan/AsanProbe.cpp:15`。这是有意触发的探针结果。 |
| differential / fuzz / bench | 初次验收时等待业务入口，标为 SKIP；`-Suite all` 退出 2、报告 PARTIAL。 |
| 构建隔离 | 强制重编译并检查命令：core 无 RE/SKSE/PCH/Catch2；测试单独链接 Catch2；插件链接核心库，未重复编译核心，也未链接测试。 |
| 插件骨架 | 构建脚本与强制重编译均退出 0；生成 SLSFReloadedNG.dll。staging 中该插件只有 DLL/PDB，没有测试程序；未部署到游戏。 |
| 任务书 | 退出 0；11 组、25 任务各一次、37 CPP 归属唯一、96 本地链接有效。 |
| 空白检查 | tracked diff 及本次变更的未跟踪文本文件分别检查，无空白错误。 |

工具环境：Xmake 3.0.9+master.0a2255cc2、MSVC 14.44.35207、LLVM 20.1.0、Catch2 3.8.1。详细 JUnit 输出位于 `build/test-results/{core,boundary,core-asan,boundary-asan}.xml`。外层 PowerShell 调用需检查 `$LASTEXITCODE`；若再套一层命令，可用 `exit $LASTEXITCODE` 透传 2，避免把 PARTIAL 误认成 1。

## 初次验收后的接入安排

- core 的类型、接口清单及 BuildInfo 交给后续数值、传播和存档任务使用。
- boundary 通过 fake 验证端口值对象和所有权，真实调用在对应适配任务中验证。
- VM 适配任务为 VmRequest 补充实例目标，并测试 Quest 实例路由。
- 对应运行时和打包组在 SkyrimSE.exe 1.6.1170 加载 DLL，运行下游 PEX 兼容探针并记录结果。

执行指南和相关计划已同步以上兼容边界；此后修复正常功能即可，不以旧异常结果作为必须保留的行为。
