# 06 组：生命周期、SKSE 保存与依赖缓存

日期：2026-10-29。任务：B03、B05、C01。工作区：`E:/source/repos/SLSF Reloaded NG`（共享工作区，未提交）。

## 结论

本组五个 CPP 及配套头文件、测试、构建项已完成。核心、边界、ASan、两个 codec 模糊目标和插件构建均通过；目标运行时加载有一条 12:34 的历史通过记录，当前机器因 `SkyrimSE.exe` 为 1.7.104.0、本机 SKSE 2.2.6 加载器拒绝该运行时，无法复跑 1.6.1170 目标加载。该阻断发生在插件入口之前，不是插件版本分支。

版本策略已按要求改为：不校验游戏 EXE/安装文件哈希；由 CommonLibSSE-NG 生成 AddressLibrary 兼容声明，入口通过 SKSE 的 `RuntimeVersion()`/`SKSEVersion()` 记录宿主版本。

## 交付文件

| 领域 | 文件 |
|---|---|
| 统一入口 | `src/main.cpp`；`include/slsf/runtime/Lifecycle.h`、`src/runtime/Lifecycle.cpp` |
| 异步消息 | `include/slsf/engine/EventBridge.h`、`src/engine/EventBridge.cpp` |
| 保存封装 | `include/slsf/persistence/Serialization.h`、`src/persistence/Serialization.cpp` |
| 能力缓存 | `include/slsf/integrations/CapabilityRegistry.h`、`src/integrations/CapabilityRegistry.cpp` |
| 调度扩展 | `include/slsf/runtime/Dispatcher.h`、`src/runtime/Dispatcher.cpp`（新增 `Capture`） |
| 测试 | `tests/core/LifecycleTests.cpp`、`tests/core/SerializationTests.cpp`、`tests/core/CapabilityRegistryTests.cpp`、`tests/fuzz/SerializationFuzz.cpp` |
| 构建/探针 | `xmake.lua`、`tools/run-native-tests.ps1`、`tools/group06-game-load-probe.ps1` |
| 证据 | `docs/evidence/task-groups/06-dependency-lock.md`、`06-game-load-record.md`、`logs/` |

## 实现要点

- **入口与生命周期**：`src/main.cpp` 在 `SKSE::Init` 后记录 `host: runtime=... skse=... commonlib=...`，不比较或拒绝版本；由 SKSE 加载器与 AddressLibrary 声明负责兼容。`kPostLoad`/`kPostPostLoad`/`kDataLoaded` 依次 `EnsureAll()` 能力查询；序列化/消息监听在 `kPostLoad` 一次性广播前注册。
- **业务开关**：`Lifecycle` 维护 `LifecyclePhase{Detached, Preparing, Active, Failed}`（`include/slsf/runtime/Lifecycle.h:27-32`；订正：原报告笔下的 "Uninitialized/Menu" 为业务开关语义的描述性措辞，实际枚举如左，Menu 语义由 `ReturnToMenu`/`BusinessOpen()` 承载）。`ReturnToMenu`/`Prepare*` 立即关闭 `Dispatcher`；只有 `Activate` 在完整绑定后才打开。失败 `RejectPreparation` 保持关闭；缺失本插件记录也走失败关闭并报错，不再合成空状态。
- **保存**：`Lifecycle::Save` 在事务锁内 Capture，锁外 `EncodeState`，再交给 SKSE 回调写记录；v1/v2 codec、校验和、长度边界、截断/未知版本均返回带原因的错误。
- **A/B 恢复**：静态地点/API 表为进程级；每档 `RegionSnapshot` 与会话归属重新绑定。区域按 `StableRegionKey` 匹配，缺失地区保留完整 `LocationPersistentState` 进入 unresolved 集；Provider 注册随档恢复，OR 合并类别。
- **配置**：`Begin` 每次读当前 `Configuration` 快照；同一内容复用 revision，非法/IO 错误保留上一有效值并记入结果消息。`MakeRuleConfiguration` 把 typed JSON 字段投影到旧规则键（全局倍率、25 类倍率、夜间控制、`DecayTimeNeeded`），不足的 JSON 解析由第 10 组接入。
- **异步提交**：`EventBridge::Enqueue` 以当前 `Epoch` 盖章；`Flush` 只处理启动批次，逐条 epoch 校验后提交，未通过即丢弃。队列上限 4096，溢出计数，避免无界增长。
- **能力缓存**：Unqueried/Waiting/Available/Missing/Incompatible；首个请求冻结 required min/max/API/struct/function 契约。provider 查询在锁外执行，重复并发请求通过条件变量共享同一结果，异常转为 Incompatible；迟到能力在等待后完成。
- **DDNG ABI**：`GetAPI()` 返回对象，首虚函数 `GetVersion()`；历史目标运行返回 `version=2 api=2`。不缓存裸指针，不做哈希校验。

## 运行记录

工作目录均为仓库根目录。`run-native-tests.ps1` 会构建并执行对应目标。

| 命令 | 退出码 | 结果 | 日志 |
|---|---:|---|---|
| `powershell -NoProfile -ExecutionPolicy Bypass -File tools/run-native-tests.ps1 -Mode core -Seed 1170 -Jobs 1` | 0 | 238 cases / 132308 assertions | `logs/2026-10-29-group06-core-console.log` |
| `... -Mode boundary -Seed 1170 -Jobs 1` | 0 | 4 cases / 22 assertions | `logs/2026-10-29-group06-boundary-console.log` |
| `... -Mode asan -Seed 1170 -Jobs 1` | 0 | core 238/132308；boundary 4/22；`SLSFAsanProbe` 正向 `probe=42 index=7`，负向 OOB 产生预期 ASan 报告 | `logs/2026-10-29-group06-asan-console.log` |
| `powershell -NoProfile -ExecutionPolicy Bypass -File tools/run-native-tests.ps1 -Mode fuzz -Target StateCodecFuzz -FuzzIterations 100000 -Seed 1170` | 0 | `Done 100000 runs` | `logs/2026-10-29-group06-statecodec-fuzz.log` |
| `powershell -NoProfile -ExecutionPolicy Bypass -File tools/run-native-tests.ps1 -Mode fuzz -Target SerializationFuzz -FuzzIterations 100000 -Seed 1170` | 0 | `Done 100000 runs` | `logs/2026-10-29-group06-serialization-fuzz.log` |
| `xmake build SLSFReloadedNG` | 0 | `build/windows/x64/releasedbg/SLSFReloadedNG.dll`，790016 bytes（评审后父地点投影修正版） | `logs/2026-10-29-group06-plugin-build-final.log` |

测试统计：新增 Lifecycle 15 例（订正：原报告 9 例为早期交付统计，未含后续补测与审计修复新增的 oversize 拒绝用例）、Serialization 5 例、Capability 10 例（原统计 5 例，审计修复新增 bounded 等待与 Waiting 重探 2 例）。随机种子统一为 `1170`；确定性 malformed/truncation 用例另用固定字节序列，失败输入为预期拒绝的截断、校验和破坏、未知版本与非法计数，均无非预期崩溃。

当前目标运行时复跑（14:36）：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/group06-game-load-probe.ps1 `
  -MinimalPlugins -KeepPlugins DeviousDevices.dll -Label blocked -TimeoutSeconds 150
# GAME_LOAD_MARKERS=False loader_exit=True game_pids= minimal=True
# EXIT=1
```

`skse64_loader.log` 原文：`You are using a newer version of Skyrim than this version of SKSE64 supports.`、`Runtime: 1.7.104`、`SKSE64: 2.2.6`。12:34 在 1.6.1170 目标运行时的历史通过记录、DDNG 能力完成与静态表 766/766/81 见[最小游戏加载记录](06-game-load-record.md)。本记录明确区分历史通过与当前环境阻断，不把阻断写成通过。

## ABI 与来源

- CommonLibSSE-NG 6.0.0（commit `15ad90b31b2ca09cf477c7e1a43ff5ff05af120f`）：`SKSEPluginInfo` 由 `lib/commonlibsse-ng/res/commonlibsse-ng-plugin.cpp.in` 生成，兼容策略 `SKSE::VersionIndependence::AddressLibrary`；`LoadInterface::RuntimeVersion()`/`SKSEVersion()` 见 `lib/commonlibsse-ng/include/SKSE/Interfaces.h:32-33`。
- SKSE64 2.2.6、Skyrim SE 目标 1.6.1170；`skse_version.h:18-22,57` 定义 `PACKED_SKSE_VERSION` 为 8/8/12/4 位，`Interfaces.h:32-33` 暴露 `RuntimeVersion()`/`SKSEVersion()`；运行时匹配由 SKSE 加载器负责，插件只记录接口返回值，不做哈希校验。
- DDNG 3.0.0.1 API v2：`E:/source/repos/DeviousDevicesNG-main/include/API.h`，`DD_APIVERSION=2U`，`virtual size_t GetVersion() const` 为 `API.h:108`；另有 12:34 目标 DLL 的 `GetProcAddress`/vtable probe 结果。（订正：`API.h:31` 宏定义与 `:108` 虚函数为**并列双定义**，二者一致（均表示 API v2），probe 兼用两者，非仅引其一。）
- 详细许可、线程/所有权、当前宿主阻断见[依赖锁定表](06-dependency-lock.md)。未使用游戏文件哈希。

## 验收对照

| 要求 | 结果 |
|---|---|
| ISA 引导后进入 AVX2 路径 | 通过（核心用例 + 插件启动日志） |
| 读档/新游戏/失败加载业务开关 | 通过（9 个 Lifecycle 用例；失败保持关闭） |
| A→B→A 恢复名声/注册/地区 | 通过（含缺失地区完整记录与重新绑定） |
| 稳定身份匹配、静态表复用 | 通过（核心 `BuildLocationIndex`/`BindRegions` 用例；历史 766/766/81 构建）。评审后补充 `parentLoc → parentFormId` 投影，修复子地点不能继承父地区的问题；该修正版因当前宿主被 SKSE 拒绝，未能再次进游戏复跑。 |
| epoch 后提交异步消息 | 通过（过期丢弃、批次上限、单批消费） |
| 迟到能力完成、重复查询合并 | 通过（Capability 5 例；DDNG `Waiting→Available`） |
| 新会话读取当前跨档配置 | 通过（typed 投影 + revision/保留上次有效值） |
| SKSE 保存记录与 codec | 通过（v1/v2、截断、校验和、100k 模糊） |
| 目标运行时加载 | 历史 12:34 通过；当前 1.7.104 被 SKSE 拒绝，环境阻断 |
| 真实交互存/读档 | 未执行，需恢复目标运行时 |

## 下组所需

1. 恢复 Skyrim SE 1.6.1170 或安装匹配 1.7.104 的 SKSE，重跑 `tools/group06-game-load-probe.ps1`，确认新入口的 `host:` 日志。
2. 第 10 组提供实际 `Data/SKSE/Plugins/SLSFReloadedNG.json` 解析器并注入 `IConfigurationSource`；当前 `BootstrapConfigurationSource` 仅为可控默认值。
3. 游戏循环组接入统一执行器的生产泵（`EventBridge::Flush` 与 `Dispatcher::Step`）；本组已验证 epoch/批次语义，未接每帧调度。
4. 真实新游戏/读档/存盘与 DDNG 设备业务联调；本组只完成 ABI probe 和生命周期/缓存边界。

## 审查修正

- 删除游戏 EXE 哈希锁定和报告中的哈希比较；改为 SKSE 运行时接口与 CommonLib 版本声明。
- 修复 `CapabilityRegistry` 在 provider 查询期间持锁导致的重入死锁；改为串行查询 + 条件变量广播。
- 缺失本插件保存记录由“空状态激活”改为失败关闭并报错。
- `MakeRuleConfiguration` 补齐 typed 配置到规则键的投影，避免新会话读取配置后规则仍为空。
- `EventBridge` 增加 4096 上限和单批消费，避免处理函数持续入队造成无界内存/单次 Flush 饥饿。
- 探针脚本删除旧日志后再启动；测试脚本的 fuzz 调用改为 `Start-Process` 临时捕获，修复 PowerShell 5.1 将 stderr 当终止错误及路径空格问题。

## 独立审计修复记录（2026-10-30）

经独立审核与对抗验证后，本组代码层修复全部落地并通过全量 core 测试（296 用例 / 136330 断言 0 失败）、DLL 目标重新构建通过：

- **06-S1（P2，C ABI 边界异常逃逸）**：`src/main.cpp:314-318` 新增 `GuardedHostCallback(where, body)`（try/catch(...) + 日志），SKSE 序列化/messaging 回调（`OnSerializationSave/Load/Revert`、`OnMessage`）全部经其包裹——宿主回调内抛出的异常不再穿透 C 边界。
- **06-S2（P2，无界队列内存）**：`EventBridge::kMaxMessageBytes`（topic+payload 合计，默认 64 KiB，`include/slsf/engine/EventBridge.h:39`）在入队前拒绝超帧消息，独立计数 `rejectedOversize`（先于 closed/epoch 判定，归因清晰）；`Counter` 新字段不破坏既有统计断言。
- **06-S3（P2，可被一个挂死的 provider 卡死）**：`CapabilityWaitPolicy::joinWait`（缺省 10 s，可 `SetWaitPolicy` 覆盖）限定搭车等待上限，超时返回记录当前快照而非无限等待；显式重查仍可等候完成。
- **06-S4（P2，一次性消息漏触发即长期 Waiting）**：`EnsureAll` 增加"deadline 驱动的重探"——`waitingTimeout`（缺省 60 s，0 = 每轮都重探，测试确定性模式）内未决的 Waiting 记录被下一轮重探；`maxWaitingProbes`（缺省 8）界定重探上限，防卫永久 pending 的 provider 被无界重问。`Ensure` 显式查询语义不变。
- **06-S5（跨组，实机待验证）**：`SetUniqueID` 的 ID 唯一性属 DDNG 联调项，需于游戏环境验证，离线测试已钉死调用形状。
- 上述修复的验证：`xmake build SLSFCoreTests` + 全量运行（fix 后 296/296 通过）；clang-cl ASan 版本（`SLSFCoreTestsAsan`）同样 296/296 通过且无 sanitizer 违规（这是对 07-S1 UAF 修复、EventBridge 字节上限与等待策略最直接的证据）；`xmake build SLSFReloadedNG` 构建通过（含 main.cpp/GuardedHostCallback）。（订正并见"测试统计"段。）

