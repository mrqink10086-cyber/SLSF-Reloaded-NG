# 第 08 组交接报告：P+、DDNG 和其他模组状态

日期：2026-10-31
修订：2026-11-01。本文保留 10 月 30–31 日的历史验证日志；后续独立审核发现三项缺陷，现已补实现及回归，当前裁决与证据见[08 修复报告](../2026-11-01-08-fixes.md)。游戏内验收仍待执行。

## 交付文件

| 文件 | 内容 |
|---|---|
| `include/slsf/integrations/ExternalState.h` | 公共状态：可用性、门控、超时/重试策略、provider 信息、上限 |
| `src/integrations/ExternalState.cpp` | 状态名、`BlocksNewScoring`、可用性到门控/事实状态映射 |
| `src/integrations/ExternalWire.h` | 三种 wire 共用的有界行/字段解析、转义、整数/浮点校验 |
| `include/slsf/integrations/SexLabPPlus.h` | P+ 场景/角色/标签/28 种交互 DTO，`ParsePPlusSceneWire`、`ApplyPPlusToAnimation`、`SexLabPPlus` |
| `src/integrations/SexLabPPlus.cpp` | P+ wire 解析、epoch 校验、交互位合法性、按作用域取标签、异步请求/重试/超时/取消 |
| `include/slsf/integrations/DeviousDevices.h` | DDNG 设备/束缚 DTO，`ParseDdngWire`、`DeviousDevices` |
| `src/integrations/DeviousDevices.cpp` | DDNG wire 解析、18 位设备和 9 位束缚掩码校验、设备事实应用、装备变更重取 |
| `include/slsf/integrations/ExternalMods.h` | 外部模组枚举/状态、场景快照、事实投影、`ExternalMods` |
| `src/integrations/ExternalMods.cpp` | `SLSFEX1` 解析、模块/角色/Public Whore 状态、变更失效与重试 |
| `tests/core/ExternalStateTests.cpp` | 本组原有 55 例及修复新增 10 例，固定/随机输入与寿命、异常回归 |
| `include/slsf/integrations/PapyrusCalls.h`、`src/integrations/PapyrusCalls.cpp` | 新增 `CurrentEpoch()`，供适配器拒绝旧 epoch 的迟到回调 |

没有修改 `src/api`、Papyrus 公开签名或第 07 组已冻结的接口。工作区中他人的改动未回退。

## 接口与行为

### 公共状态

- `ExternalAvailability{Unknown, Available, Missing, Incompatible}`。
- `ExternalGate{Ready, Waiting, Changed, Stale, Missing, Incompatible, Failed, Closed}`。
- `ExternalTimingPolicy{timeoutMs=5000, retryIntervalMs=250, maxAttempts=20}`。
- `BlocksNewScoring(gate)` 仅在 `Ready` 返回 false；等待、变更、过期、缺失、不兼容、失败均暂停新计分。
- `GateForAvailability`、`FactStateForAvailability` 保证缺失/不兼容不会被当成“未知”。

### P+（C03）

- wire 头 `SLSFPP1`；必需 `version 1`、`availability`、`epoch`、`active`、`stage`。
- 每个 `actor` 行携带插件/本地 FormID、姓名、种族、标志、28 位交互掩码和 28 位 unknown 掩码；交互位在 ABI 之外或两掩码重叠时拒绝整帧。
- 标签带 `thread` / `scene` / `stage` 作用域。旧分类只使用 `Scene`（含 annotations），对应旧 Animation.HasTag；`Stage` / `Thread` 原始标签保留供按作用域查询。
- 帧内 epoch 必须等于调用方传入的 `Epoch`；迟到结果、旧 epoch 事件、旧 `AnimationEnd` 都不能关闭或覆盖当前场景。
- 交互掩码会合成 `Oral` / `Anal` 分类标签，作为标签缺失时的兼容回退；最终标签去重并排序。面部交互位只合成 `Facial`，不会合成 `Oral`。
- 只有存在已验证角色且场景为 `active` 时才应用 `AnimationFacts`；实际角色性别、玩家、strapon、victim/aggressor 状态均来自该帧。

### DDNG（C04）

- wire 头 `SLSFDD1`；必需 `version 1`、`availability`、`epoch`；Available 还需 `provider` 和 `revision`，没有必需 `active` 行。
- `actor` 行按 `StableFormKey` 关联请求角色，依次携带插件、本地 ID、bondage 和 devices；未知角色不会写入事实。
- `devices` 是 18 位设备掩码；`bondage` 范围为 0–0x1ff，`sTotal`（0x200）是哨兵值，达到或超过即拒绝。
- 紧身胸衣（`ChastifiedGenital` 或 `ChastifiedAnal`）合成 belt，胸甲（`ChastifiedBreasts`）合成 bra，与旧行为一致。
- 装备变更只在当前活动场景且 epoch 匹配时使快照失效；停用/旧 epoch 事件被忽略。
- 设备事实在未验证前为 `Unknown`，provider 缺失时为 `Missing`，版本不兼容时为 `Incompatible`。

### 其他模组（C07）

- wire 头 `SLSFEX1`；必需 `version 1`、`availability`、`epoch`、`player`、`publicwhore`、`provider`。
- 模块：`PublicWhore`、`FillHerUp`、`FertilityMode`、`EstrusChaurus`、`EstrusSpider`、`HentaiPregnancy`、`FameComments`、`Bimbos`。
- 每角色字段：`fertilityModeBreeder`、`hentaiPregnant`、`estrusChaurusBreeder`、`estrusSpiderBreeder`、`inflation`、`female`，另保留 FM/HP 原始 rank 供分层计分。Available 帧必须包含八项模块状态。
- `Public Whore.esp` 的“当前场景激活”是场景级状态；其余为角色级。`isSexWorker` 属于业务模型（`Data.CheckSexWorker`/`SexWorkerFlags`），不在本适配器重复维护。
- `Bimbos` 的检测对象是 `CustomComments.esp`，与旧 `SLSF_Reloaded_ModIntegration.psc` 的 `IsBimbosInstalled` 一致；`SLSFFameComments.esp` 单独映射为 `FameComments`。
- 数字解析拒绝 NaN/Inf，重复模块、重复 player、重复公开状态行、未知键均拒绝整帧。

### 请求生命周期

三个适配器统一遵守：

1. 请求时记录 epoch 和请求角色集合；同一场景重复请求复用进行中的请求，不增加 attempt；被复用的旧等待者立即收到一次 `Changed`，不会静默丢失。
2. 失败/超时按 `retryIntervalMs` 重试，最多 `maxAttempts`；超过后进入 `Failed`。
3. 每个等待者至多收到一个终止状态（`Ready`/`Stale`/`Closed`/`Failed`/`Changed`）；中间重试可能收到 `Waiting`。解析失败、VM 失败、超时、取消都不会重复完成同一等待者；旧 epoch 完成时返回 `Stale` 且不提交快照。
4. 场景结束/停用时，停止接受旧快照，后续事件不会重新激活。
5. `Reset`/析构取消在途请求，避免回调悬挂。

## 实际运行记录

工作目录：`E:/source/repos/SLSF Reloaded NG`

### 构建

命令：

```powershell
xmake build SLSFCoreTests
```

- 退出码：`0`
- 日志：`docs/evidence/task-groups/logs/2026-10-31-group08-final-core.log`
- 结果：`build ok`

### 全量单元测试（固定 seed）

命令：

```powershell
./build/windows/x64/releasedbg/SLSFCoreTests.exe --reporter compact --rng-seed 1170
```

- 退出码：`0`
- seed：`1170`
- 结果：`All tests passed (145637 assertions in 356 test cases)`
- 日志：`docs/evidence/task-groups/logs/2026-10-31-group08-final-core.log`

本组相关用例包括：

- `P+ wire keeps only classification scopes and canonicalizes tags`
- `P+ wire rejects a mismatched scene epoch`
- `P+ facts use the validated scene actors and interactions`
- `P+ adapter retries within the bounded policy and gates readiness`
- `P+ adapter reports stale results and does not score them`
- `P+ stage changes refetch instead of serving a cached scene`
- `P+ interaction flags synthesize classification scope tags`
- `P+ adapter invokes the completion once after malformed responses`
- `P+ stale request completes both waiters without submitting`
- `P+ end event for an old epoch cannot close the current scene`
- `DDNG wire applies device and bondage masks to the requested actor`
- `DDNG wire rejects the bondage sentinel bit`
- `DDNG chastity state contributes belt and bra facts`
- `DDNG equipment changes outside the active scene are ignored`
- `DDNG actor outside the snapshot leaves facts unknown`
- `external wire exposes module and per-actor state`
- `external adapter retries on timeout and gates new scoring while missing`
- `external state changes outside the active scene are ignored`
- `external mod events invalidate an in-flight request`
- `P+ replaced waiter receives changed exactly once`
- `P+ new scene start supersedes an active scene`
- `P+ facial interaction does not synthesize oral`
- `Papyrus result epoch mismatch is reported stale`
- `DDNG replaced waiter receives changed exactly once`
- `DDNG anal chastity contributes a belt fact`
- `external replaced waiter receives changed exactly once`
- `wire parsers handle deterministic mutated input`

### ASan

命令：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/run-native-tests.ps1 -Mode asan -Seed 1170 -Jobs 1
```

- 退出码：`0`
- seed：`1170`
- core：`All tests passed (145637 assertions in 356 test cases)`
- boundary：`All tests passed (22 assertions in 4 test cases)`
- 日志：`docs/evidence/task-groups/logs/2026-10-31-group08-final-asan.log`
- ASan 负向探针按预期报告 `heap-buffer-overflow`，随后正常通过。

### 解析器随机对照

`wire parsers handle deterministic mutated input` 使用固定 seed `1170`，对三种 wire 做 2000 次确定性变异（截断、替换、超长、重复键、非法数字、非法作用域、非法掩码、非有限浮点等），要求解析器不崩溃、不越界，且拒绝或一致接受。该用例包含在 356 个测试中。

### 失败输入与复跑

以下输入均被拒绝，且在同一 seed 下复跑通过：

- P+：错误 epoch、未知/重复键、交互掩码越界或重叠、重复 actor、缺失 provider、超出角色/标签上限。
- DDNG：`sTotal` 哨兵位、`bondage > Total`、未知键、重复 provider/actor、未知设备关键字。
- 外部模组：重复模块/player/publicwhore、NaN/Inf、缺失公开状态、重复 actor。
- 生命周期：超时、重试耗尽、旧 epoch 回调、场景停用后的装备/状态事件。

开发期另有两个已修复失败：

1. `P+ adapter invokes the completion once after malformed responses` 起初在一次解析失败路径上重复完成等待者；修复为只在重试耗尽时完成一次，使用同一 seed `1170` 复跑通过。
2. DD probe 首版把 `Armor` 直接传给 `FormHasKeywordString`，编译类型不匹配；改为 `worn[index] as Form` 后编译成功，最终日志见上。

未发现需要保留的未修复失败输入；以上输入已固化为测试，可直接重放。

## 历史组内审核与修复主张（2026-10-31）

以下为当时组内主张；后续独立审核确认其中 Facial 合成、失败前清除旧投影及 LeadInEnd 行为没有完整实现，已在 11 月 1 日实际修复。此处历史测试数与日志保留，当前结果见修复报告。

审核按“实际代码逻辑优先、报告只作线索”执行，覆盖算法正确性、内存/栈安全、线程/生命周期和报告逐项核验。确认并修复的问题：

1. P+ 同一场景重复请求会覆盖旧等待者且不回调；现在共享同一个在途请求，旧等待者收到一次 `Changed`。
2. P+ 在新场景 `AnimationStart` 时仍可能短暂返回旧场景快照；现在新请求会清空旧快照并重新取帧。
3. P+ 的 `aFacial` 交互位曾被错误合成为 `Oral`；现在只合成 `Facial`，不会污染口交分类。
4. DDNG 的 `ChastifiedAnal` 未参与 belt 合成；现在与 `ChastifiedGenital` 一样产生 belt 事实。
5. `PapyrusCalls` 只比较调用时的 epoch，未校验结果自带的 epoch；现在两者都必须等于当前会话 epoch，否则返回 `Stale`。
6. 旧 epoch 请求在部分分支会把适配器重新置为 active；现在旧请求只产生 `Stale` 观察结果，不修改当前场景。
7. P+ provider 返回 `availability=Available, active=0` 时仍可能保留 active；现在该帧会关闭当前场景。
8. P+ 无角色或 provider 停用时，玩家侧旧设备/事实可能残留；现在应用帧前重置玩家事实。
9. 三种 wire 的未知键、重复键、重复对象和重复模块从“部分忽略”改为整帧拒绝；DDNG/P+ 的重复检查与外部模组一致。
10. `PapyrusCalls::Call` 的交付标志改为 `std::atomic<bool>`，调用表使用 `shared_ptr` 固定节点地址，消除“提交返回前完成”导致的对象悬空。

新增回归用例覆盖：重复等待者、新场景替换、面部交互、结果 epoch、DDNG anal belt、外部模组重复等待者、LeadIn 阶段读取、未知 availability 重试和重试期间等待者交接。最终全量 356 例、ASan 356 例通过。

### 过程改进研究

在不改变结果的前提下，评估并采用了以下更稳妥做法：

- **先登记后提交**：`PapyrusCalls::Submit` 先把调用放入稳定表，再交给 transport；即使 transport 同步回调，调用对象也不会在 `Submit` 返回前被释放。
- **共享所有权**：调用节点用 `shared_ptr`，取消/完成/`Advance` 各自持有临时引用；避免 `vector` 扩容或异步回调造成悬空。
- **每调用互斥 + 原子终止标志**：终止状态只交付一次，互斥锁不覆盖用户回调。
- **适配器外部锁内只做状态变更**：完成回调和 transport 取消都在释放适配器锁后执行，避免重入死锁。
- **解析器全有界**：所有行/字段/对象/标签都有上限，解析为迭代过程，无递归、无按输入长度分配的栈结构。

未采用的替代方案：直接用 CommonLib/RE 读 P+ 运行时结构会绑定未公开 ABI；把三种 provider 合并成一次 Papyrus 批量调用会耦合可用性并放大单点失败；改用 JSON 需要引入解析依赖且不能改善本组的错误定位。当前分层（薄 PSC 传输 + C++ 规则/状态）更符合项目边界。

线程与生命周期契约：业务操作由统一执行器排序；epoch 提供器必须线程安全。修复后完成回调仅弱引用共享内部状态，允许完成期间销毁外层适配器，迟到回调不会访问已释放状态；`PapyrusCalls`、transport 与 epoch 提供器的寿命仍须覆盖未完成调用。详见 11 月 1 日修复报告。

## 真实提供方 probe

### 目标环境清单

日志：`docs/evidence/task-groups/logs/2026-10-30-group08-provider-inventory.log`

- `GAME_VERSION=1.6.1170.0`
- `SKSE_VERSION=0, 2, 2, 6`
- `SexLabUtil.dll` 2.16.0.0，SHA256 `1AD6F9226E5998BA9ACE789BF256363A8BA91EF5F4A33BAECCF08290D9C1BC9D`
- `DeviousDevices.dll` 3.0.0.1，SHA256 `0D374DC7D6E2B1181795957EDC19D6AD48DCD397AFB6579D5A7EF511E185D274`
- 已安装：`SexLab.esm`、`sr_FillHerUp.esp`、`Fertility Mode 3 Fixes and Updates.esp`
- 未安装：`SexLabPPlus.esp`、`Public Whore.esp`、`EstrusChaurus.esp`、`EstrusSpider.esp`、`HentaiPregnancy.esm`、`SLSFFameComments.esp`、`CustomComments.esp`

导出表 probe：

```powershell
cd "E:/st/steamapps/common/Skyrim Special Edition/Data/SKSE/Plugins"
llvm-objdump -p SexLabUtil.dll | grep -A6 "Export Table"
llvm-objdump -p DeviousDevices.dll | grep -A8 "Export Table"
```

- 退出码：`0`
- `SexLabUtil.dll`：`SKSEPlugin_Load`、`SKSEPlugin_Version`
- `DeviousDevices.dll`：`GetAPI`、`SKSEPlugin_Load`、`SKSEPlugin_Query`、`SKSEPlugin_Version`
- 日志：`docs/evidence/task-groups/logs/2026-10-30-group08-api-exports.log`

`GetAPI` 说明 DDNG 3.0.0.1 的 C++ API v2 入口存在；本组仍通过 Papyrus provider 取状态，避免核心库直接绑定 SKSE/CommonLib ABI。

### P+ 编译 probe

源：`E:/source/repos/sexlab P+/Source/Scripts/SexLabThread.psc`（本地 P+ 2.18.1.0 源）。

ABI 来源：`SexLabThread.psc:80-84` 的 `HasSceneTag` / `HasStageTag`，`SexLabThread.psc:238` 的 `GetCurrentInteractionFlags`，`SexLabThread.psc:332` 的 `GetSubmissive`，`SexLabThread.psc:421` 的 `IsUsingStrapon`；28 个交互的索引顺序来自同文件 `SUPPORTED_INTER_COUNT` 区块。

脚本：`docs/evidence/task-groups/group08-probes/SLSFGroup08PPlusProbe.psc`

```papyrus
Scriptname SLSFGroup08PPlusProbe

bool Function Probe(SexLabThread threadRef, Actor actorRef)
    bool[] flags = threadRef.GetCurrentInteractionFlags(actorRef)
    return flags.Length == 28 && threadRef.HasTag("Oral") && threadRef.HasSceneTag("Oral") && threadRef.HasStageTag("LeadIn")
EndFunction
```

P+ 2.18 编译：

```powershell
"E:/st/steamapps/common/Skyrim Special Edition/Papyrus Compiler/PapyrusCompiler.exe" \
  .scratch/group08-probe/SLSFGroup08PPlusProbe.psc \
  -f="E:/st/steamapps/common/Skyrim Special Edition/Data/Source/Scripts/TESV_Papyrus_Flags.flg" \
  -i="E:/source/repos/sexlab P+/Source/Scripts;.scratch/group08-probe;E:/st/steamapps/common/Skyrim Special Edition/Data/Source/Scripts" \
  -o=".scratch/group08-probe/compiled18-final"
```

- 退出码：`0`
- 结果：`0 error(s), 0 warning(s)`，`Assembly succeeded`
- 日志：`docs/evidence/task-groups/logs/2026-10-30-group08-pplus-2.18-compile.log`

同一脚本使用本机已安装 P+ 2.16 的 `SexLabThread.pex`（Champollion 反编译到 `.scratch/group08-probe`）编译：

```powershell
"E:/st/steamapps/common/Skyrim Special Edition/Papyrus Compiler/PapyrusCompiler.exe" \
  .scratch/group08-probe/SLSFGroup08PPlusProbe.psc \
  -f="E:/st/steamapps/common/Skyrim Special Edition/Data/Source/Scripts/TESV_Papyrus_Flags.flg" \
  -i=".scratch/group08-probe;E:/st/steamapps/common/Skyrim Special Edition/Data/Source/Scripts" \
  -o=".scratch/group08-probe/compiled16"
```

- 退出码：`1`
- 错误：`GetCurrentInteractionFlags is not a function or does not exist`
- 日志：`docs/evidence/task-groups/logs/2026-10-30-group08-pplus-2.16-compile.log`
- 反编译函数清单：`docs/evidence/task-groups/logs/2026-10-30-group08-pplus-2.16-functions.log`（确认无 `GetCurrentInteractionFlags`）

结论：本机安装的 P+ 2.16 不具备本组使用的逐角色交互 API，不能作为 P+ 运行时验收环境；2.18 源编译通过。已安装的 `SexLabUtil.dll` 2.16 暴露的 `GetInteractionString`/`GetInteractionTypes`/`HasInteractionType` 不足以恢复 28 位逐角色实际交互，因此代码按 2.18 契约实现；provider 在检测到缺 API 的运行时必须返回 `Incompatible`，适配器不猜测版本，也不会用错误事实计分。

### DDNG 编译 probe

源：`E:/source/repos/DeviousDevicesNG-main`，目标 `DeviousDevices.dll` 3.0.0.1，API v2。

ABI 来源：`include/API.h:92-102` 的 `BondageState` 位定义（`sHandsBound=0x1` 至 `sTotal=0x200`），`include/API.h:144` 的 `GetBondageState`，`include/Export.h:167-169` 的 `GetAPI` 导出路径。

脚本：`docs/evidence/task-groups/group08-probes/SLSFGroup08DdProbe.psc`

```papyrus
Scriptname SLSFGroup08DdProbe

bool Function Probe(Actor actorRef)
    Armor[] worn = zadNativeFunctions.GetDevices(actorRef, 0, true)
    if worn == None
        return false
    endif

    int index = 0
    while index < worn.Length
        if worn[index] != None
            if zadNativeFunctions.FormHasKeywordString(worn[index] as Form, "zad_DeviousBelt")
                return true
            endif
            if zadNativeFunctions.FormHasKeywordString(worn[index] as Form, "zad_DeviousGag")
                return true
            endif
        endif
        index += 1
    endwhile
    return false
EndFunction
```

编译：

```powershell
"E:/st/steamapps/common/Skyrim Special Edition/Papyrus Compiler/PapyrusCompiler.exe" \
  .scratch/group08-probe/SLSFGroup08DdProbe.psc \
  -f="E:/st/steamapps/common/Skyrim Special Edition/Data/Source/Scripts/TESV_Papyrus_Flags.flg" \
  -i=".scratch/group08-probe;E:/st/steamapps/common/Skyrim Special Edition/Data/Source/Scripts" \
  -o=".scratch/group08-probe/compiledDD"
```

- 退出码：`0`
- 结果：`0 error(s), 0 warning(s)`，`Assembly succeeded`
- 日志：`docs/evidence/task-groups/logs/2026-10-30-group08-ddng-probe-compile.log`

DDNG 的 `GetAPI` 确有导出，可取得 v2 虚接口。11 月 1 日已增加原生 `SLSF_Reloaded_DDProbe.CollectScene`：DataLoaded 后检查版本并固定 DLL，游戏线程调用 `GetWornDevices`、按 18 个关键词归并设备位，再调用 `GetBondageState`。不调用始终返回 false 的 `IsDevice`/`ActorHasBlockingGag` API stub。此前声称 C++ API 未导出，以及将全量采集推迟到下组，均不准确。

运行时 probe 尚未执行：本机没有可用的 P+ 2.18 运行环境，且未启动游戏；DDNG 运行时掩码需要在安装 DDNG 3.0.0.1 的存档中与 C++ 结果交叉验证。

## 报告逐项核验

按“实际代码/日志优先”复核本报告：

- 历史日志已核验：当时本组测试文件 55 个 `TEST_CASE`；全量 356 例、145637 断言、seed `1170` 与 ASan 356 例均退出 `0`；P+ 2.18 编译 probe 成功、2.16 编译失败；DDNG probe 编译成功；provider 版本、SHA256、导出表与安装清单和日志一致；`Bimbos` 检测对象确为 `CustomComments.esp`。当前新增修复用例与 366 例结果见 11 月 1 日报告。
- 已更正：报告早期版本写“52 个本组用例、353 例、145617 断言”，是加入 LeadIn/未知 availability/等待者交接回归用例前的旧数字；日志路径已改为最终 `2026-10-31-group08-final-*.log`。P+ 2.16 的 `Incompatible` 由 provider 检测后上报，适配器本身不按 DLL 版本猜测。
- 未能离线核验：`SLSFPP1`/`SLSFDD1`/`SLSFEX1` 的游戏内实际帧、P+ stage 事件时序、DDNG 运行时 18 位掩码、Public Whore/怀孕/触手模组的真实返回值。这些项目在本机缺少 P+ 2.18 运行环境，报告已标为待运行，不把编译 probe 当作运行时通过。
- 复核结论：C++ 契约、生命周期、边界解析与编译期 ABI 来源可复核；游戏内行为仍需按“未完成 / 下组所需”执行。

## 未完成 / 下组所需

1. **运行时 probe**：安装 P+ 2.18+（`SexLabPPlus.esp`、匹配的 `SexLabUtil.dll`）与 DDNG 3.0.0.1，加载存档并进入至少 2 角色的 SexLab 场景，记录 `SLSFPP1`、`SLSFDD1`、`SLSFEX1` 原始帧、stage 切换、装备变更和 Public Whore/怀孕/触手状态。
2. **运行时装配**：全量采集器已经补齐，入口为 `SLSF_Reloaded_PPlusProbe.CollectScene`、原生 `SLSF_Reloaded_DDProbe.CollectScene`、`SLSF_Reloaded_ExternalProbe.Collect`；C02 VM 宿主及 B03 执行泵仍需消费这些入口。
3. **Papyrus 事件桥**：把实际普通 `SexLabAnimationStart/LeadInEnd/AnimationEnd` 通知中的 `strArg` thread ID、DDNG 装备事件和外部模组事件交给统一执行器。`StageStart/StageChange` 只作可选通知兼容，不能作为前戏结束后必然存在的恢复事件。
4. **UI/业务接线**：`ExternalGate::BlocksNewScoring`、`FactStateForAvailability` 需被评分执行器消费；`Bimbos` 映射 `CustomComments.esp` 需在 provider 安装检测中保持。
5. **运行时 ABI 复核**：P+ 2.18 的 28 位交互位顺序、DDNG 3.0.0.1 的 18 位掩码与 `sTotal` 语义已按源码/头文件实现，仍需游戏内样本确认。

## 结论

C03、C04、C07 的 C++ 适配器、公共状态、wire 契约、生命周期和单元测试已完成，固定 seed 与 ASan 均通过。真实提供方的版本、导出/编译表面和本机安装差异已记录；由于本机 P+ 2.16 缺少 2.18 逐角色交互 API，P+ 运行时验收仍需在 2.18 环境执行，不能把编译 probe 当作运行时通过。
