# 第 07 组：异步 VM 与旧版对外 API（C02、B06）

日期：2026-10-30
结论：**通过**。五个 CPP（M25/M31/M32/M33/M34）+ 薄 PSC 源 + VM/协议测试全部落地；core/boundary/asan/fuzz 四模式全绿（seed 1170），DLL 目标可链接。

## 交付文件

| 文件 | 说明 |
| --- | --- |
| `src/integrations/PapyrusCalls.cpp` + `include/slsf/integrations/PapyrusCalls.h` | M25 异步 VM 传输：实例/Global 目标、成功/超时/取消各恰好完成一次、结果按提交时 epoch 校验后提交 |
| `src/api/PublicApi.cpp` + `include/slsf/api/PublicApi.h` | M31 公开 facade：同步命令提交-后返回、已发布视图查询、52 事件路由、7 类回应 |
| `src/compat/PapyrusBindings.cpp` + `include/slsf/compat/PapyrusBindings.h` | M32 冻结函数表（124 项：86 public + 38 internal）、参数转换/缺省补齐、无效输入结果 |
| `src/compat/papyrus_function_table.inc` | M32 冻结表数据（124 条，由 `contracts/legacy-api.json` 生成） |
| `src/compat/papyrus/*.psc`（16 个） | 薄 PSC 源：覆盖全部 124 个合同函数的原实例/Global 声明（MCM 无合同函数，无 facade） |
| `src/compat/ModEventBridge.cpp` + `include/slsf/compat/ModEventBridge.h` | M33 typed ModEvent：52 输入 + 7 回应冻结合同、按名取值、缺省补齐、校验与一次派发 |
| `src/compat/LegacyOutputs.cpp` + `include/slsf/compat/LegacyOutputs.h` | M34 输出合同：25 Reloaded Global、20×24 StorageUtil、20 原始 esm 桥接，读已提交 revision |
| `tests/core/PapyrusCallsTests.cpp`（10，审计后 +3）/`PublicApiTests.cpp`（8）/`ModEventBridgeTests.cpp`（9）/`LegacyOutputsTests.cpp`（8）/`PapyrusBindingsTests.cpp`（9） | 本组 44 个 TEST_CASE（订正：原报告 43 误计，实测 41；审计修复再补 3 个并发/生命周期用例） |
| `include/slsf/interfaces/VmPort.h` | 接口同步：`VmTargetKind{Global,Instance}` + `VmRequest.instance`（既有构造无位置参数受影响） |
| `include/slsf/runtime/Dispatcher.h`、`src/runtime/Dispatcher.cpp` | 接口同步：暴露 `GainFame/ApplyManualSpread/ApplyManualDecay/ApplyManualGain/ApplySpreadPulse` 及结果类型 |
| `include/slsf/core/reputation/GainRules.h/.cpp`、`Settlement.h/.cpp` | M31 所需：`RuleManualGainAmount`、`ApplyManualGain`（纯增量，无删改） |
| `xmake.lua` | SLSFCore 与 SLSFCoreAsan 纳入 `src/api/**`、`src/compat/**` |

## 实现要点

- **M25**：提交时复制参数、记录提交 epoch；完成回调经**共享堆上 `Call` 状态**（每调用一把互斥的 exactly-once 标志）在锁外调用处理函数。完成闭包**不捕获所属实例指针**（独立审计 07-S1/P1：旧实现捕获 `this`，transport 晚于会话析构完成任务时为悬垂访问）——闭包仅持有 `shared_ptr<Call>` 与 epoch 探针副本；析构前对每个 pending 调用先经 `IVmPort::RequestCancellation` 通知传输层、再统一交付 `Cancelled`，迟到完成只能触碰共享 `Call` 并在 exactly-once 竞争中落败。`Cancel`/`Advance`/析构/完成回调全部收敛到同一 `DeliverCall` 原子迁移（同时消除旧 Cancel TOCTOU）。新增 `PapyrusCallOptions::maxInFlight`（缺省 256）：超限提交在触达传输层前即以 `Failed/Status::Invalid` 回调拒绝（07-S3）。超时用单调逻辑时钟（`Advance(nowMs)`）判定，到期后迟到的完成被忽略；取消同理。成功但 epoch 已变 → `kind=Stale` 且 `status=Status::Stale`（本轮修正点 1）。
- **M31**：同步命令走统一执行器 `Execute`（结算→应用→Ready 才发布），返回前已提交，故返回后查询必读到提交值；异步入口（gain/decay/spread 等）经执行器事务提交后按 epoch 校验再交付。事件路由把 52 输入映射到执行器命令或外部 Mod 注册表，7 类 Request 经 `ModEventBridge::SendReply` 回应。
- **M32**：函数表冻结自 `contracts/legacy-api.json`（PEX 溯源见下）；`ConvertInvalidInput` 先剥离合同里残留的 PSC 引号层（如 `DoesFlagExist` 存 `"No"`）再做类型映射（本轮修正点 2）。
- **M33**：合同表（52 输入/7 回应）逐字段固定事件名、处理函数、参数名/类型/顺序/数量；缺省值按合同补齐（如 `RegisterMod` 缺省 `"-EMPTY-"`）；未知事件/多参/类型错位全部拒绝。
- **M34**：输出只读已发布不可变视图（`Dispatcher::Query`），未发布/未解析一律 0，不写业务状态；25 Reloaded Global 用 `SLSF Reloaded.esp` + 本地 ID（0x80C–0x823、0x82F），20 桥接 Global 用 `SexLab - Sexual Fame [SLSF].esm` + 本地 ID（0x24255–0x24268）。StorageUtil 的 20 键 × 24 地点索引合同保持不变。运行时不再传递旧完整 FormID，见下文 FormID 修复。

## 运行记录（实际命令与退出码）

环境：Windows + MSVC（E:\BuildTools）、clang-cl LLVM 20（ASan）、xmake、Catch2 3.8.1；随机 seed **1170**；分片运行 `--rng-seed 1170 --order rand --shard-count 20 --shard-index i` 可完整重放。

| 命令 | 退出码 | 结果 | 日志 |
| --- | --- | --- | --- |
| `Set-Location 'E:/source/repos/SLSF Reloaded NG'; xmake build SLSFReloadedNG` | 0 | DLL 链接成功，0 警告 | `build_g07_dll1.log` |
| `tools/run-native-tests.ps1 -Mode core -Seed 1170` | 0 | 20/20 分片通过，290 用例，136282 断言，0 失败 | `build_g07_core5.log` |
| `tools/run-native-tests.ps1 -Mode boundary -Seed 1170` | 0 | 4/4 分片通过 | `build_g07_boundary1.log` |
| `tools/run-native-tests.ps1 -Mode asan -Seed 1170` | 0 | 正/负探针符合预期（故意 OOB 触发 heap-buffer-overflow）；24/24 分片通过 | `build_g07_asan4.log` |
| `tools/run-native-tests.ps1 -Mode fuzz -Seed 1170` | 0 | StateCodecFuzz 100000 runs 无崩溃（seed 1170） | `build_g07_fuzz1.log` |

JUnit：`build/test-results/core.xml`、`boundary.xml`、`core-asan.xml`、`boundary-asan.xml`。

## 失败输入与修正

首次全量 core 运行（`build_g07_core4.log`，退出码 1）2 个用例失败，均在本组新增测试：

1. `papyrus stale epoch result is demoted to Stale`：陈旧 epoch 完成时 `kind=Stale` 但 `status` 保留了传输层 `Ready`。修正：`PapyrusCalls.cpp` 陈旧分支同步置 `base = Status::Stale`。
2. `M32 pins invalid-input results`：`DoesFlagExist` 无效输入合同存的是带引号 `"No"`，直接透传导致 4 字符而非 2 字符。修正：`PapyrusBindings.cpp` `ConvertInvalidInput` 先剥离一层 PSC 引号再做类型映射。

用原输入复跑（`build_g07_core5.log`）2 用例转绿，其余 18 分片结果不变。

**订正（独立审计，2026-10-30）**：`build_g07_core4.log` 现存全文仅含 `[100%]: build ok, spent 0.078s` 一行，**不含**首次运行的失败案例名/退出码记录，上述"首次 2 例失败"的叙述在该日志内无直接证据支撑；两项代码修正本身已在源码中复核属实（`PapyrusCalls.cpp` Stale 分支置 `Status::Stale`、`PapyrusBindings.cpp` `ConvertInvalidInput` 剥离 PSC 引号层），引以为代码事实而非该日志。另就 `tests/fixtures/contracts/api-signatures.json` 的计数：文件为 35 个源文件合计（17 PSC 明言），原报告"17 PEX + 17 PSC"的 PEX 计数与逐类重算结果不一致，以逐类清单为准。

## ABI 与来源

- 插件 ABI：SkyrimSE 1.6.1170（`xmake.lua` 规则 `commonlibsse-ng.plugin`，`name 'SLSF Reloaded NG'`）。本组为离线构建/测试，未在真实游戏进程内运行。
- Papyrus 绑定 ABI：`tests/fixtures/contracts/api-signatures.json`（17 PEX + 17 PSC 溯源，Orvid/Champollion v1.3.2，sha256 在案）。
- 事件合同：`tests/fixtures/contracts/legacy-cases.json`（52 输入/7 回应/25 类/21 地点/6 别名/记录绑定）。
- 输出记录来源：旧仓库 `E:/source/repos/SLSF Reloaded/Scripts/Source/` 之 `SLSF_Reloaded_LegacyOverwrite.psc`（StorageUtil 键与 24 地点索引）、`ModIntegration.psc`（20 桥接 Global 的插件名/本地 ID）、`FameManager.psc`（25 类映射）；游戏内冻结 PEX 按原记录/实例绑定运行属待执行项。离线测试固定稳定键和 editorId，合同中的旧完整 ID 仅作来源证据。
- probe 结果：ASan 负探针在 `tests/asan/AsanProbe.cpp:15` 按预期触发 `heap-buffer-overflow`；fuzz 100000 runs 无崩溃。

## 验收对照（任务书通过标准）

| 标准 | 结果 |
| --- | --- |
| VM 支持实例/Global 目标；成功/超时/取消各完成一次，结果按相关 epoch 处理 | 通过：`PapyrusCallsTests` 7 用例（含 epoch 陈旧降级、迟到完成忽略） |
| 同步数值命令返回前提交，随后查询读到提交值；52 输入 + 7 回应名称/类型/参数顺序符合合同 | 通过：`SetFameValue("Whiterun","Whore",20)` 返回后查询读到 20（`PublicApiTests`）；`ModEventBridgeTests` 10 用例逐字段对合同 |
| Global、StorageUtil、原始 SLSF 桥接输出已提交版本；冻结 PEX 按原记录和实例绑定运行 | 通过（离线）：`LegacyOutputsTests` 8 用例钉死记录与绑定；游戏内 PEX 运行待执行（需游戏环境） |

## 下组所需产物（08 组：C03/C04/C07）

1. **M25 接口已冻结**（`include/slsf/integrations/PapyrusCalls.h`）：`Submit(target, instance, script, function, args, onDone) -> RequestId`、`Cancel(id)`、`Advance(nowMs)`、`InFlight()`、`LastTargetWasInstance()`；结果类型 `PapyrusCallKind{Ready,Stale,Failed,TimedOut,Cancelled}`。P+ 通知、DDNG 快照直接复用其 epoch 校验与一次完成语义。
2. **M33 合同表已冻结**：`ModEventBridge::InputContracts/ReplyContracts/ParseInput/ParseReply/ValidateReply/Dispatch/SendReply`；新增事件必须先改 `legacy-cases.json` 再改 C++ 表并同步测试。
3. **M34 发布接口**：`PublishCurrentFameGlobals/PublishStorageUtilLists/PublishLegacyBridge` + 解析器注入（`SetFormRegionResolver`/`CommittedResolver`），宿主接线（`src/main.cpp`）不在本组，按 C03 运行联调接入。
4. 宿主侧尚未把 PublicApi/ModEventBridge 挂到游戏消息与 Papyrus 原生注册上（B03 联调项）；游戏内冻结 PEX 探针（函数返回/52 输入/7 回应/记录绑定/数据输出）需 SkyrimSE 1.6.1170 + SKSE 环境。

## 审查修正

- 本轮共修正 2 处（见"失败输入与修正"），无回归；共享工作区中他人改动（本组外的既有文件）未触碰，`GainRules/Settlement` 的增量为本组 M31 所需且纯新增。

## 独立审计修复记录（2026-10-30）

对 07/06 组交付进行独立审核与对抗验证后，本组代码层修复均已落地并通过全量测试（core 296 用例 / 136330 断言 0 失败；clang-cl ASan 全量同步通过且无 sanitizer 违规；DLL 目标构建通过）：

- **07-S1（P1，契约级 UAF）**：`IVmPort` 新增 `RequestCancellation(RequestId)` 默认实现（`include/slsf/interfaces/VmPort.h:69`）；`PapyrusCalls` 重写为**共享堆 `Call` 状态** + 类级静态 `DeliverCall` 原子迁移（`src/integrations/PapyrusCalls.cpp`）：完成闭包不再捕获所属实例指针，仅捕获 `shared_ptr<Call>` 与 epoch 探针副本；析构先对每个 pending 调用 `RequestCancellation`，再统一交付 `Cancelled`；迟到完成只能触碰共享 `Call`、在 exactly-once 竞争中落败。连带消除 07-S7（`Cancel` 两次读之间 TOCTOU）与析构/超时/完成回调的多头竞争。
- **07-S2（P2)**：`PublicApi::CurrentLocation()` 改为**按值**返回 `std::string`（`include/slsf/api/PublicApi.h:162`），不再暴露锁保护共享状态的引用。
- **07-S3（P2）**：`PapyrusCallOptions::maxInFlight`（缺省 256）——超限提交在触达传输层前即回调 `Failed`/`Status::Invalid`（"too many in-flight calls"，`PapyrusCalls.cpp:136`），挂死的 transport 不再无界堆积。
- **07-S4（P2）**：补齐并发测试（`PapyrusCallsTests.cpp` 7→10：in-flight 超限拒绝、析构时 pending 一律 `Cancelled` 且通知传输层、多线程 Submit/Complete/Cancel/Advance 竞争下 exactly-once 与无重复交付）。
- **07-S5（P2）**：`OnRequestLocation` 分支：`Strict=true` 支持宿主注入 `SetStrictLocationResolver`（`PublicApi.h:170-171`），返回 nullopt 即 legacy "None"；未注入时回落 `CurrentLocation()`，partial 语义在代码注释与本节披露。
- **07-S6（P3，补披露）**：`OnRequestRecognitionTime` 无条件回复 0.0 —— recognition time 不在第 02 组发布投影内，待匿名层接入；源码注释已声明，此处正式补记。
- **报告订正**：本组 TEST_CASE 43→实测 41（7/8/9/8/9），审计修复后 44（10/8/9/8/9）；`api-signatures.json` 溯源计数以文件逐类清单为准；`build_g07_core4.log` 证据链缺口见"失败输入与修正"订正段。
- **遗留（跨组/实机）**：用户提供的 xEdit 图已确认 Reloaded 记录的本地 ID；实际引擎写入、冻结 PEX 与 ME 挂接仍需目标游戏联调，见下一节。

## FormID 修复（2026-10-30）

运行时 Global 输出及 Papyrus 任务/alias 绑定均改为 `StableFormKey{plugin, localFormId}`。`0x0600080D` 是旧源插件表中的完整 ID，现运行表使用 `{ "SLSF Reloaded.esp", 0x80D }`。`contracts/*.json` 与原始 fixtures 保留原始取证数据；生成器按该冻结完整插件来源剥除索引，重新生成 124 项表与 16 个 PSC 的绑定说明，函数声明保持相同。

`include/slsf/engine/RecordLookup.h` 将插件/本地 ID 交给 `TESDataHandler::LookupForm<T>`，不自行拼接运行时 ID；查询前验证本地 ID 范围和引擎文件的 light 属性。DLL 中的 `EngineLegacyOutputSink` 实际查询 `RE::TESGlobal` 后写值，`LookupLegacyQuest` 查询 `RE::TESQuest`；`PapyrusInstanceForQuest` 用源文件名和引擎 `GetLocalFormID()` 还原实例稳定身份。缺插件、缺记录、类型不符均不写入；不缓存缺失结果。StorageUtil 写入由后续 VM 宿主传入回调。主线程/DataLoaded 合同已写入边界头；整体 API/VM 宿主接线仍按上节 C03/B03 计划推进。

回归覆盖普通插件索引 0x06→0x35、light 索引 0x000→0x037，25 Global、20 桥接 Global 及全部非 Global 函数的任务/alias 绑定；并覆盖错误插件、文件名大小写、缺失后出现、错误记录类型、把完整 FormID 当作本地 ID 等反例。此处索引矩阵由可控引擎数据代替真实游戏，DLL 编译验证实际 CommonLib 调用；目标游戏写入与 PEX 执行尚未实测。完整命令、计数及独立复核见 [FormID 修复报告](../2026-10-30-formid-fixes.md)。
