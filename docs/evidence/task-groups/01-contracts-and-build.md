# 第01组：公开接口清单与测试工程（A01/A02）

- 日期：2026-10-27
- 工作目录：`E:/source/repos/SLSF Reloaded NG`
- 状态：实现、自动测试和插件构建已完成；游戏内验证未运行。
- 共享工作区说明：`README.md` 的既有修改未触碰；本组修改 `xmake.lua`、`.gitignore` 并新增下列文件。

2026-10-28 衔接：A02 类别集合、地区声望数据寻址约定、对齐布局、配置 DTO 及组合头文件检查已交付，见本文第 9 节；范围与交接要求见[01 任务书](../../superpowers/plans/2026-10-27-task-books/01-contracts-and-build.md)。原交付结果保留。

## 1. 改动文件

### 合同清单

- `contracts/legacy-api.json`
- `contracts/legacy-events.json`
- `contracts/legacy-records.json`
- `contracts/legacy-rules.json`
- `tests/fixtures/contracts/legacy-cases.json`
- `tests/fixtures/contracts/negative/delete-one-input-event.json`
- `tests/fixtures/contracts/negative/swap-event-parameters.json`
- `tests/fixtures/contracts/negative/duplicate-category.json`
- `tests/fixtures/contracts/negative/invalid-record-identity.json`
- `tests/fixtures/contracts/negative/invalid-api-result.json`
- `tests/fixtures/contracts/negative/managers-vmad-reversal.json`

### 共享头文件与 core 骨架

- `include/slsf/core/Types.h`
- `include/slsf/core/Contracts.h`
- `include/slsf/core/Snapshots.h`
- `include/slsf/core/Commands.h`
- `include/slsf/core/persistence/PersistentState.h`
- `include/slsf/interfaces/EnginePort.h`
- `include/slsf/interfaces/VmPort.h`
- `include/slsf/interfaces/ProviderPort.h`
- `include/slsf/interfaces/WakeSource.h`
- `src/core/BuildInfo.cpp`
- `src/boundary/BoundaryPlaceholder.cpp`

### 测试与工具

- `tests/core/ContractsTests.cpp`
- `tests/core/xmake.lua`
- `tests/core/headers/{Types,Contracts,Snapshots,Commands,PersistentState}Header.cpp`
- `tests/boundary/PortTests.cpp`
- `tests/boundary/xmake.lua`
- `tests/boundary/headers/{EnginePort,VmPort,ProviderPort,WakeSource}Header.cpp`
- `tests/asan/AsanProbe.cpp`
- `tools/verify-contracts.ps1`
- `tools/run-native-tests.ps1`
- `xmake.lua`（增加 `SLSFCore`、两个测试目标、ASan 目标和插件/core 源目录隔离）
- `.gitignore`（忽略仓库根目录编译中间物，避免污染共享工作区）
- `docs/evidence/legacy-contracts.md`
- `docs/evidence/dependencies.lock.json`

`README.md` 保持他人原有修改，未纳入本组。

## 2. A01 合同清单结果

事实来源为旧仓库 `E:/source/repos/SLSF Reloaded` 的 17 个 PSC、18 个 PEX 和
`SLSF Reloaded.esp`（23775 字节，SHA-256
`8ebd5ab79740e85a2a8d553e47391bdfdecd82d278e30bcfcd239c28fb544812`）。
ESP 按 TES4/GRUP/record/subrecord 和 VMAD v5 实际解码，未猜测 FormID 或 VMAD。
Main QUST 的 alias 区也已解码：alias 0 = `PlayerAlias`，绑定
`SLSF_Reloaded_SexAnimationAnalyzer`（14 个属性）和 `SLSF_Reloaded_PlayerScript`
（17 个属性）；因此 4 个 ReferenceAlias 函数带 `questFormId=0x06000808`、
`aliasIndex=0`。校验器锁定源 ESP SHA-256、Managers 的 9 个 VMAD 脚本顺序和
Main alias 详情。

| 项目 | 数量 | 结果 |
|---|---:|---|
| 有效 Papyrus Function | 124 | 86 public、38 internal |
| 输入 ModEvent | 52 | 注册顺序 1–52 |
| 回应 ModEvent | 7 | Push 顺序 1–7 |
| Fame 类别 | 25 | ESP VMAD 顺序与 PSC 注释顺序分别保存 |
| 默认地点 | 21 | 9 major + 12 minor |
| 地点别名 | 6 | Eastmarch、Haafingar、Hjaalmarch、the Reach、the Pale、the Rift |
| ESP 记录 | 58 | QUST 4、MGEF 1、SPEL 1、KYWD 10、GLOB 42 |
| 公开记录 | 57 | 1 个内部 SPEL 不计入公开身份 |
| 规则 | 23 | 当前分类：preserve 14、approved-change 6、internal-only 3 |

公开函数的判定规则是旧版可供外部脚本、控制台或 MCM 调用，或公开事件处理器
依赖的查询/状态入口；内部启动、定时推进、数组维护和计算辅助函数标为 internal。
MCM QUST `0x06000809` 保留为旧菜单识别身份；新菜单使用 SKSE 菜单框架。

58 个记录的 TES4 记录头 flags 实际全为 `0x00000000`，因为该字段位于记录头偏移 8，
本插件的 master/compressed/deleted 等 header flag 均为零。四个 QUST 的
Start Game Enabled/Run Once 等玩法 flags 位于 `DNAM`，实际值均为 `0x00000111`
（`kEnabled | kStartsEnabled | kRunOnce`），在 `legacy-records.json` 中单独记为
`questFlags`。

类别实际 VMAD 顺序以 `Whore` 开始，`Slut` 第二；`Tattoo`/`Cum Dump` 的 ESP
拼写为 `tattoo`/`cum dump`，查找按大小写不敏感处理。`WhoreEvent` 是废弃查询别名，
`Sex Worker` 作为独立 flag 保存，`FameType` 保持 25 项。

`tests/fixtures/contracts/legacy-cases.json` 是固定 oracle，保存事件签名、类别顺序、
地点、记录身份、QUST 的 `questFlags` 和 Main alias。它把从旧源码/ESP 提取的结果
固定下来；校验器另外锁定源 ESP SHA-256、负例集合和失败原因。完整逐函数 PSC
复解析属于 A01 的来源审计，不在校验器内重复执行。

## 3. A02 共享头与端口

- 强类型 ID：`LocationId`、`RegionId`、`ApiLocationIndex`、`TaskId`；`RequestId` 与
  `Revision` 也是不同的强类型。
- `Epoch{session,index,scene}`、`Status{Ready,NotReady,Missing,Incompatible,Invalid,Stale}`。
- `GameTime` 区分游戏日与 `RealSeconds`/`ActiveSeconds`。
- `Fact` 分别表达未知和已知 false；`SceneSnapshot`、`PublishedState` 都是可复制的值对象。
- `IVmPort::Submit(VmRequest,VmCompletion)->RequestId`，完成结果携带原 epoch。
- `IEnginePort::Post(EngineRequest)->RequestId`。
- `IProviderPort` 使用自有 `ProviderState`/`ProviderFlag`/`ProviderTransition`，不暴露第三方容器。
- `IWakeSource::Start(WakeCallback)/Stop()`；`IRandom::UniformInt(int32_t,int32_t)->int32_t`。

每个头文件都有独立包含的锚点翻译单元（`tests/*/headers/*Header.cpp`），测试目标只
包含 `include/` 和 Catch2，不包含 RE、SKSE 或插件 PCH。

## 4. 实际命令与退出码

| 命令 | 退出码 | 实际结果 |
|---|---:|---|
| `powershell -NoProfile -ExecutionPolicy Bypass -File tools/verify-contracts.ps1 -NegativeFixtures` | 0 | 正例与 6 个负例全部通过 |
| `xmake build SLSFCoreTests` | 0 | 构建成功 |
| `xmake run SLSFCoreTests "[contracts]" --rng-seed 1170` | 0 | `48 assertions in 8 test cases` |
| `xmake build SLSFBoundaryTests` | 0 | 构建成功 |
| `xmake run SLSFBoundaryTests "[ports]" --rng-seed 1170` | 0 | `22 assertions in 4 test cases` |
| `xmake build -y SLSFCoreTestsAsan SLSFBoundaryTestsAsan SLSFAsanProbe` | 0 | clang-cl ASan 构建成功 |
| `powershell -NoProfile -ExecutionPolicy Bypass -File tools/run-native-tests.ps1 -Mode asan` | 0 | 正例、越界探针和两组 ASan 测试通过 |
| `powershell -NoProfile -ExecutionPolicy Bypass -File build-skyrim1170.ps1` | 0 | `SLSFReloadedNG.dll` 生成成功；staging 由 `xmake install -y SLSFReloadedNG` 同步 |
| `powershell -NoProfile -ExecutionPolicy Bypass -File tools/run-native-tests.ps1 -Mode all` | 2 | core/boundary/asan 通过；differential/fuzz/bench 明确 SKIP，未伪装成功 |

本机 PowerShell 执行策略要求自动调用附带 `-ExecutionPolicy Bypass`，作用于本次进程。

最终合同校验输出：

```text
PASS contracts: 124 functions (86 public), 52 inputs, 7 replies, 25 categories, 58 records, 23 rules
REJECT  delete-one-input-event: CONTRACT-REJECT: events inputEvents length (expected '52', got '51')
REJECT  duplicate-category: CONTRACT-REJECT: unique category names (expected '25', got '24')
REJECT  invalid-api-result: CONTRACT-REJECT: GetFameLocation invalidInputResult (expected 'Null', got '$NoneText')
REJECT  invalid-record-identity: CONTRACT-REJECT: record FormID SLSF_Reloaded_Main
REJECT  managers-vmad-reversal: CONTRACT-REJECT: record VMAD scripts SLSF_Reloaded_Managers[0] ...
REJECT  swap-event-parameters: CONTRACT-REJECT: input signature for SLSF_Reloaded_SendFameGain ...
PASS negative fixtures: 6/6 rejected
```

## 5. 构建隔离证据

- `xmake build -v -r SLSFCore` 退出 0；编译命令为
  `cl.exe ... -Iinclude ... src/core/BuildInfo.cpp`，链接命令只有
  `link.exe -lib ... BuildInfo.cpp.obj`。没有 CommonLibSSE、SKSE、RE include 或 PCH。
- `xmake build -v -r SLSFReloadedNG` 退出 0；编译列表只有 `src/main.cpp`、
  `src/boundary/BoundaryPlaceholder.cpp` 和生成插件/PCH 文件，没有重新编译
  `src/core/BuildInfo.cpp`；链接命令包含 `SLSFCore.lib commonlibsse-ng.lib ...`。
- `xmake build -v -r SLSFCoreTests` 的编译命令只有 `-Iinclude` 和 Catch2 include；
  对 `commonlibsse`、`SKSE`、`RE/Skyrim`、`pch.h` 的匹配数为 0。
- 测试目标定义拆分到 `tests/core/xmake.lua` 和 `tests/boundary/xmake.lua`，根
  `xmake.lua` 通过 `includes(...)` 引入；ASan 产物写入独立目录 `build/asan/`。
- 插件产物：最终构建后 `build/windows/x64/releasedbg/SLSFReloadedNG.dll` 与
  staging DLL 均为 493568 字节，SHA-256
  `EC1A2A6D5137F6D9356991BD29D499C405C0EFEE70C9E092FCBD40C5C183D483`。
  构建/同步时间 `2026-10-27T11:13:09+08:00`，源 commit `5222a3c31841426bb26c3635a37638abb8f707e5`；
  该次构建 DLL 与 staging DLL 同哈希；重建时 PE 时间戳也参与哈希。
- `dumpbin /dependents` 显示 MSVCP140、VCRUNTIME140、VCRUNTIME140_1、KERNEL32、
  ole32、SHELL32、USER32、VERSION 及若干 `api-ms-win-crt-*` 系统依赖，没有
  Catch2 或测试 DLL。
- `SLSFReloadedNG` 名称、版本和 `build-skyrim1170.ps1` 入口保持不变；插件仍由
  `commonlibsse-ng.plugin` 包装。

## 6. ASan 与失败语料

- ASan 构建使用 clang-cl 20.1.0，产物在独立目录 `build/asan/`，运行时目录只加入
  测试子进程的 `PATH`：`C:/Program Files/LLVM/lib/clang/20/lib/windows`。
- 正例 `SLSFAsanProbe` 索引 7：退出 0，输出 `probe=42 index=7`。
- 越界输入 `SLSFAsanProbe oob` 索引 8：非零退出，输出
  `AddressSanitizer: heap-buffer-overflow`，栈帧指向
  `tests/asan/AsanProbe.cpp:15`。
- ASan 测试：core `48 assertions/8 cases`、boundary `22 assertions/4 cases`，均通过。
- 失败语料保留在 `tests/fixtures/contracts/negative/`（6 个）；固定随机 seed 为 1170。
  A02 没有数值算法，因此 differential/fuzz/bench 目标未建立；`run-native-tests.ps1`
  会打印目标名和原因，显式模式退出 2。ASan 越界探针即为本组实际执行的负例。
- 调试中曾出现的问题及修复：首轮类别顺序断言把 `Group` 放在索引 20，实际
  `Group` 为索引 7、`Tattoo` 为索引 20，修正后复跑通过；ASan 链接先缺少
  `__asan_*` 符号，补入 `clang_rt.asan_dynamic` 运行库后恢复；Catch2 与 clang-cl
  的 string/vector annotation 不一致，使用
  `_DISABLE_STRING_ANNOTATION/_DISABLE_VECTOR_ANNOTATION` 对齐。

## 7. 审计清理

- 以只读方式启动 5 个子代理做对抗性合同校验、C++ 代码/构建隔离、任务目标偏离、
  C++20/23 特性使用和清理审计；子代理未修改仓库。
- 清理审计发现任务书示例使用 `-Suite`，而脚本最初只接受 `-Mode`。已为
  `tools/run-native-tests.ps1` 增加 `-Suite`、`-Runs` 兼容别名和 `-Target` 参数，
  并在脚本内固定到仓库根目录执行；`-Suite core` 实测退出 0，
  `-Suite fuzz -Target StateCodecFuzz -Runs 100000` 按预期打印 SKIP 并退出 2。
- 修正 `src/boundary/BoundaryPlaceholder.cpp` 中关于 glob 的错误因果说明，以及
  `xmake.lua` 中把“允许实现 AVX2 核心”写成“已经实现”的注释；
  `include/slsf/core/Contracts.h` 的阶段说明已删减。
- 清理仓库根目录 4 个未跟踪编译探针 `slsf_*_probe*.obj`，并在 `.gitignore`
  增加根目录 `*.obj`/`*.pdb`/`*.ilk`/`*.exp`/`*.lib`/`*.exe` 规则。
- 未删除任何受版本控制的源码、合同、头文件、测试或证据文件；`README.md` 的
  既有修改未触碰。
- 目标偏离审计的两项高优先级问题已修复：`GetFameLocation` 的
  `invalidInputResult` 改为旧实现的 `Null`；Main QUST 的 alias 0 VMAD 已解码并
  写入 records/cases/api。
- 校验器已增加 ESP 校验和/大小、Managers VMAD 顺序、Main alias 详情、回复名
  唯一性/查询覆盖、rules 类别映射，以及 6 个负例的固定集合和
  `expectedFailure` 原因匹配。
- C++20/23 审计的中优先级问题已修：`SLSFCore::Fail` 改为按值返回
  `Result<T>`/`Result<void>`，并补 `Fail-ConstructsValueAndVoidResult` 测试；
  `IsCompleteFameOrder()` 使用 `if constexpr` 消除 C4127。
- 任务书列出的 `tests/core/xmake.lua`、`tests/boundary/xmake.lua` 已补；ASan
  产物写入独立 `build/asan/`；MCM 记录保留为“仅识别旧菜单”的例外并在报告说明。
- 校验器使用固定 JSON 逐函数核对；完整来源审计见 `docs/evidence/legacy-contracts.md`。
  修改合同时同步固定样例。

## 8. 初次交付时的后续安排（2026-10-27）

- 游戏内 Papyrus 事件、地点和声望行为由后续运行时组验证。
- 目标为新存档；外部 ABI 和引擎调用在各适配任务中接入并验证。
- 业务使用统一 C++ 执行器，新菜单由 SKSE 菜单框架组实现。
- 下一组可使用本组产物：`include/slsf/**` 公共类型与端口、`SLSFCore` 静态库、
  `contracts/legacy-*.json`、`tests/fixtures/contracts/legacy-cases.json` 和
  `tools/verify-contracts.ps1`。初次交付的 `SLSFCore` 提供 `BuildId()` 和类型基础，
  数值规则、地点图和持久化按后续任务实现；当前进度见第 11 节。
- 常规自动验收命令（实际退出码均为 0）：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/verify-contracts.ps1 -NegativeFixtures
xmake build SLSFCoreTests
xmake run SLSFCoreTests "[contracts]" --rng-seed 1170
xmake build SLSFBoundaryTests
xmake run SLSFBoundaryTests "[ports]" --rng-seed 1170
powershell -NoProfile -ExecutionPolicy Bypass -File tools/run-native-tests.ps1 -Suite asan -Seed 1170
powershell -NoProfile -ExecutionPolicy Bypass -File tools/run-native-tests.ps1 -Suite all -Seed 1170
powershell -NoProfile -ExecutionPolicy Bypass -File build-skyrim1170.ps1
```

---

## 9. 2026-10-28 A02 补充交付

- 日期：2026-10-28
- 状态：A02 共享类型、地区声望数据寻址约定、配置 DTO 与构建隔离已实现；原 A01 记录保留。
- 本节记录共享类型和构建检查；游戏内验证按运行时任务完成。

### 9.1 新增与修改

- 新增 `include/slsf/core/CategoryMask.h`：`CategoryMask` 使用 `std::uint32_t` 低 25 位；`FromBits` 拒绝高 7 位；`Set/Reset/Test` 拒绝非法类别；`~` 保持有效位；`ForEach` 按规范枚举升序。
- 修改 `include/slsf/core/Types.h`：增加 `FameCategoryIndex`、`FameCategoryFromIndex`、`FameCategoryBit`、`EspFameCategoryIndex`、`FameCategoryFromEspIndex`、`DocumentedFameCategoryIndex`、`FameCategoryFromDocumentedIndex`。内部 bit 身份固定为规范/ESP 枚举值（Whore=0 … Airhead=24）；documented 顺序只用于展示/合同转换。
- 新增 `include/slsf/core/NumericRow.h` 与 `src/core/numeric/NumericRowOps.cpp`：`NumericRow` 为 `alignas(32)` 的 32 个 double，`sizeof == 256`；`std::vector<NumericRow>` 的行地址和步长满足 256/32；`ClearRow`、`CopyRow`、`MaxSlot` 只把前 25 槽当业务值，补槽保留且默认零。
- 修改 `include/slsf/core/reputation/ReputationState.h`：新增 `ReputationSlotIndex(RegionId, FameCategory)` 作为地区声望数据寻址主入口；保留 `LocationId` 过渡重载，02/A03 迁移。
- 新增 `include/slsf/core/Configuration.h`：纯值 DTO `ConfigurationData`（开关、类别倍率/门槛、关系 mask、时间/传播、`Parameters`、稳定区域连接、菜单偏好）、`ConfigurationRevision`、`ConfigurationUpdateStatus/Result`、`ConfigurationSnapshot`；不包含 JSON 解析或文件 I/O。
- 修改 `include/slsf/interfaces/ProviderPort.h`：端口事实类型改名为 `ProviderFlagFact`、`ProviderFactState`、`ProviderFactTransition`；与 `reputation/ProviderRegistry.h` 的持久 `ProviderState`/`ProviderTransition` 明确分开，二者可同 TU 包含。
- 修改 `include/slsf/core/Commands.h`：为 `Parameters` 增加默认 `operator==`，供配置值比较。
- 修改 `tests/boundary/PortTests.cpp`：同步 `ProviderFact*` 端口类型。
- 修改 `xmake.lua`：`SLSFCore` 与 `SLSFCoreAsan` 将 `src/core/numeric/**.cpp` 以 `/arch:AVX2 /fp:precise` 单独加入；其它 core、boundary、`main.cpp`/插件目标保持基础 ISA 与 `/fp:precise`。numeric TU 只编译一次。

### 9.2 新增测试

- `tests/core/CategoryMaskTests.cpp`：类别 0/24、规范/ESP/documented 三种顺序、全部索引 round-trip、`0x01FFFFFF` 有效、`0x02000000`/`0xFFFFFFFF` 拒绝、非法类别不改 mask、升序枚举。
- `tests/core/NumericRowTests.cpp`：行尺寸/对齐/步长、默认补槽零、4/64 行容器地址、复制保留补槽、`MaxSlot` 忽略补槽、清零。
- `tests/core/ConfigurationTests.cpp`：schema 版本、默认倍率、默认 mask/参数、DTO 值语义、revision 比较、更新结果状态与消息、snapshot 所有权。
- `tests/core/SharedTypesTests.cpp`：共享头同 TU 组合；端口 fact 与核心 contribution 类型不同名；`RegionId` 地区声望数据槽位与 `LocationId` 过渡槽位算术。

### 9.3 实际检查与退出码

随机 seed 固定为 1170；日志目录为 `docs/evidence/task-groups/logs/`。

| 命令 | 退出码 | 结果 / 日志 |
|---|---:|---|
| `xmake build SLSFCoreTests` | 0 | `2026-10-28-core-build-3.log` |
| `xmake run SLSFCoreTests --rng-seed 1170` | 0 | 100842 assertions / 61 cases；`2026-10-28-core-tests.log` |
| `xmake run SLSFCoreTests "[contracts]" --rng-seed 1170` | 0 | 49 assertions / 8 cases；`2026-10-28-filtered-tests.log` |
| `xmake run SLSFCoreTests "[category]" --rng-seed 1170` | 0 | 158 / 4 |
| `xmake run SLSFCoreTests "[numeric]" --rng-seed 1170` | 0 | 214 / 3 |
| `xmake run SLSFCoreTests "[configuration]" --rng-seed 1170` | 0 | 59 / 4 |
| `xmake run SLSFCoreTests "[shared]" --rng-seed 1170` | 0 | 14 / 3 |
| `xmake build -v SLSFBoundaryTests` | 0 | 无 `/arch:AVX2`；`2026-10-28-boundary-build.log` |
| `xmake run SLSFBoundaryTests --rng-seed 1170` | 0 | 22 / 4；`2026-10-28-boundary-tests.log` |
| `xmake build -v SLSFCoreTestsAsan` | 0 | numeric TU 含 `/fsanitize=address`、`/arch:AVX2`、`/fp:precise`；`2026-10-28-asan-build.log` |
| ASan `[contracts]` | 0 | 49 / 8；`2026-10-28-asan-tags.log` |
| ASan `[category]` | 0 | 158 / 4 |
| ASan `[numeric]` | 0 | 214 / 3 |
| ASan `[configuration]` | 0 | 59 / 4 |
| ASan `[shared]` | 0 | 14 / 3 |
| ASan `[ports]`（boundary） | 0 | 22 / 4 |
| `SLSFAsanProbe`（索引 7） | 0 | `probe=42 index=7` |
| `SLSFAsanProbe oob`（索引 8） | 1 | `AddressSanitizer: heap-buffer-overflow`；`2026-10-28-asan-probe-oob.log` |
| `xmake build -v SLSFReloadedNG` | 0 | main/boundary 无 `/arch:AVX2`，有 `/fp:precise`；`2026-10-28-plugin-build.log` |
| `tools/verify-contracts.ps1 -NegativeFixtures` | 0 | 124 函数/86 public、52 输入、7 回应、25 类别、58 记录、23 规则；11/11 负例拒绝；`2026-10-28-verify-contracts-negative.log` |
| `tools/run-native-tests.ps1 -Mode fuzz -Seed 1170 -Runs 100000` | 0 | 100000 runs，cov 713，无崩溃；`2026-10-28-fuzz.log` |

补充说明：

- `xmake run SLSFCoreTests --rng-seed 1170 --durations yes` 显示既有 `region binding validates owner graph without recursion` 约 32 秒；全量 ASan core 套件在该用例上运行时间显著放大，本轮以受影响标签的 ASan 运行作为验收证据，未把未完成的全量 ASan 控制台运行计为通过。
- 官方 `-Mode asan` 会先跑 ASan 探针再跑全量 core/boundary；boundary ASan 22/4 通过、探针正负例通过。若后续需要全量 ASan 归档，建议单独放宽等待时间或以标签分批运行，不改动当前 `/Od` 配置。
- `tools/run-native-tests.ps1 -Mode all` 仍会因 differential/bench 未建立而按设计 SKIP 并退出 2；本组按模式分别执行。

### 9.4 交接点

- **02 组**：`ReputationSlotIndex(RegionId, FameCategory)` 是每个地区的声望数据主入口；`NumericRowBuffer` 提供 256 字节行、32 字节对齐；类别集合用 `CategoryMask`，mask 与数值同批提交后再发布。`LocationId` 重载只用于迁移期。
- **06 组**：生命周期/执行器可持有 `ConfigurationSnapshot` 与 `ConfigurationUpdateResult`；`ConfigurationData` 是纯 DTO，JSON/文件校验留在 adapter。`ProviderPort.h` 的事实类型与 `ProviderRegistry.h` 的类别设置类型已分离，可同时包含。
- **10 组**：菜单/消费者读取 `ConfigurationRevision` 和不可变 `shared_ptr<const ConfigurationData>`；同一有效内容应沿用 revision，失败更新保留旧 revision。
- **数值/引导隔离**：只有 `src/core/numeric/**.cpp` 使用 AVX2；`src/core` 其它 TU、`src/boundary`、`src/main.cpp` 与 boundary 测试保持在基础 ISA。正常和 ASan 的 numeric 输入路径使用同一组文件与浮点选项。

## 10. 2026-10-28 全量 ASan 与多核测试补充

第 9 节记录的长链耗时已通过 02 组 owner 迭代记忆化修复。当前测试脚本使用多进程分片，支持 `-Jobs`，一次执行同时生成控制台和 JUnit。完整普通/ASan 核心均通过 69 cases / 100918 assertions，边界均为 4 / 22；ASan 正负探针按预期完成。20 分片 ASan 核心实测 4.931 秒。

当前全量入口为 `tools/run-native-tests.ps1 -Mode asan -Seed 1170`，串行比较增加 `-Jobs 1`。第 9 节建议延长等待的记录保留为历史；当前命令、日志和前后计时见[性能修复记录](../2026-10-28-region-owner-and-parallel-tests.md)。

## 11. 2026-10-28 补充交付审核

本轮核对 A01/A02 合同、共享类型、对齐行、配置 DTO/revision 和构建隔离。02 按 A03/A04/A08 完成地区声望、地点归属、类别汇总及保存格式，并修复审核提出的六项问题。普通和 ASan 核心各通过 75 项、100987 断言，边界各通过 4 项、22 断言。具体要求、修复及日志见[01/02 审核与修复记录](../2026-10-28-01-02-supplement-review.md)。
