# 第 09 组交接报告：RaceMenu 与 SlaveTats 原生读取

日期：2026-11-01。前置：04、06 组已完成。C05、C06。

## 交付文件

| 文件 | 内容 |
| --- | --- |
| `include/slsf/integrations/RaceMenu.h` | skee 接口交换消费端头：`RaceMenuOverlayLocation`、`RaceMenuOverlayCounts`、`RaceMenuSnapshot`、`RaceMenuCapabilityRecord`、`RaceMenuOptions`、`RaceMenuExchangePort`（引擎接缝）、`RaceMenuEnsureResult`、`class RaceMenu` |
| `src/integrations/RaceMenu.cpp` | 接口交换状态机：QueryInterface/GetVersion/GetOverlayCount 镜像、能力记录、四部位 overlay 容量采集 |
| `include/slsf/integrations/SlaveTats.h` | SlaveTats 广播头：`kSlaveTatsInterfaceMessageType=1`、`AddressesTable` 前缀常量、`SlaveTatsPlacement/AreaRecords/Snapshot`、`JcString/JcTable` ABI 镜像、`SlaveTatsPort`（引擎接缝）、`JcHandle` RAII、`class SlaveTats` |
| `src/integrations/SlaveTats.cpp` | 广播一次性校验、JC 临时对象所有权、四部位应用纹身复制、重试与生命周期 |
| `tests/core/AppearanceStateTests.cpp` | 本组 19 用例（13 个名见下； jc RAII 6 SECTION 并入 1 case） |
| `tests/probe/AppearanceProbe.cpp` | 离线 probe：镜像 + fake 交换/广播；目标安装版段为 pending checklist |
| `xmake.lua`、`tools/run-native-tests.ps1` | 注册 `SLSFAppearanceProbe`（+7/+12 行，未动他人块） |

注释：新增源文件零注释；仅 `tests/probe/AppearanceProbe.cpp` 保留 6 处注释（AGENTS.md 必要例外）：头部 4 行 probe 用途与离线/在线分段说明；两行记录批量入口参数序与 legacy 顺序不镜像的外部协议约束；两行记录 vtable 主基槽位镜像来源；一处标注在线检查清单输出位置。

## 接口与行为

### C05 RaceMenu/skee 外观数据读取

- 交换协议（来源 `E:/source/repos/RaceMenu/skee64/IPluginInterface.h:24-57`、`RaceMenu/skee64/main.cpp:559/944`）：`kSkeeInterfaceExchangeMessage=0x9E3779B9`、sender `'SKEE'`、dataLen=8，data 为指向 `InterfaceExchangeMessage` 的指针（结构仅单一成员 `IInterfaceMap* interfaceMap`，共 8B，无版本字段）。引擎侧 `RaceMenuExchangePort::Exchange()` 返回 mapper 指针，核心不触 SKSE。
- 能力识别（版本、必要入口分立验证）：`QueryInterface(map,'Overlay')` → 主基指针；`IInterfaceMap` vtable slot0=QueryInterface；`IOverlayInterface` 主虚基 vtable slot1=GetVersion（要求 ≥2，实际源码返回 2）、slot11=GetOverlayCount(OverlayType=0 Normal, location)。入口指针逐个判空，缺一即 Incompatible；查询不到 'Overlay' 即 Missing。本地 8 字节指针布局静态断言。
- 存储字段：四部位容量 `RaceMenuOverlayCounts{array<u32,4>}`，位置序 Body/Hand/Feet/Face（skee `OverlayLocation` 0..3）；快照含 `availability/overlayInterfaceVersion/epoch/overlays`；能力记录 3 条（InterfaceMap/OverlayInterface/GetOverlayCount），`Capabilities(vector&)` 可审计。
- 生命周期语义与 08 组一致：未武装实例吸收首个 epoch（`epochSeen_`），旧 epoch 咨询返回 Stale 且不改快照；`MarkDirty(eventEpoch)` 命中才置 dirty/Changed；`OnSessionChanged` 清快照、Ready 重采、gate=Changed；`Cancel`→Closed、`Reset`→Waiting；attempt 上限 `ExternalTimingPolicy`（同组默认 20）后 Failed。
- 未知状态规则：gate 非 Ready 时 `BlocksNewScoring` 为真——类别设置与既有声望保留，暂停依赖该事实的新计分；就绪后按正常计分入口继续。

### C06 SlaveTats 应用纹身读取

- 一次性接口广播（来源 `E:/source/repos/SlaveTatsNG-master/src/main.cpp:90-92` kPostPostLoad Dispatch MessageType 1，dataLen=sizeof(void*)=8）：`AddressesTable` 镜像（u32 `current_version` 偏移 0，指针 i 偏移 8+8i，共 15 指针 v1 前缀，静态断言 0/8/112/120）；`current_version==1` 才接受（≠1 → Incompatible + rejections，不存表）；`get_applied_tattoo_in_slot`=slot13、`get_applied_tattoos_by_area`=slot14（C06 只消费批量入口；批量参数序 body,face,hands,feet 照源码 `SlaveTatsNG_Interface.h:51`；输出域按 `gameplay::TattooArea` Body/Face/Hands/Feet，不照抄旧 SLSF 顺序）。
- JC 对象所有权（来源 jc/jcontainers_wrapper.h、JValue/JArray/JMap 反射绑定）：`JcMirrorTable` 接入 `JcTable{domain, arrayCount, arrayObject, arrayGetObj, arrayAddObj, mapGetStr, mapGetInt, release, retain}`；临时数组用 `JcHandle`（move-only，dtor Release，Owns=handle≠0 且 table 能力集可用）按协议释放；记录绑定 mapGetStr('name')（interned BSFixedString 镜像 8B）/mapGetInt('slot')。probe 输出 `creates=4 releases=4 retains=0` 平衡。
- 四部位复制为自有记录：每部位独立 `vector<SlaveTatsPlacement>{name,slot}`+stable_sort by slot；容量截断 per-area 256（`truncated=true`）、名字 >128B/空 entry/slot<0 → 拒绝计数；记录协议来源 tattoo.cpp/cachecpp（`area` 字段由批量参数决定，不读 entry 字段）。
- `SlaveTatsSlotFacts(records,capacity)` → `gameplay::TattooSlotFacts`：slot 越界 cap 值只给 identity 事实；`subcategory=Unknown` 留给 C++ 业务模型（旧版 MCM 配置字段，非记录协议）。
- 换档：`OnSessionChanged(epoch)` 释放当前会话对象（JcHandle、表镜像所有权清空）并置 dirty 重采；`Collect(epoch)` 传当前 epoch，否则 Stale 不写快照。

## 实际运行记录（命令、退出码、日志）

环境：Windows，`xmake.lua`（MSVC /utf-8）；`LLVM+MSVC STL`；测试运行于 CI 本地（catch2 + 20 分片 rand 序）。

| # | 命令 | 退出码 | 说明 | 日志 |
| --- | --- | --- | --- | --- |
| 1 | `xmake build -y SLSFCore SLSFCoreTests SLSFCoreTestsAsan SLSFBoundaryTests SLSFAppearanceProbe` | 0 | 交付目标全编译通过 | 摘录 |
| 2 | `powershell -File tools/run-native-tests.ps1 -Mode all -Seed 1170` | 0 | 全套 PASS `PASS native tests: mode=all seed=1170`（含 core 20 分片、boundary、ASan core/boundary、StateCodecFuzz、differential、SLSFAsanProbe、SLSFAppearanceProbe） | `docs/evidence/task-groups/logs/2026-11-01-group09-final-all.log` |
| 2a | 同上 core 段 | 0 | 379 cases / 149650 assertions（每分片 19 cases，末分片 18）；本组 19 用例全部绿 | 同上 |
| 2b | boundary | 0 | 4 cases（22 assertions ×2 含 asan） | 同上 |
| 2c | core-asan + boundary-asan | 0 | 7.674s + 0.098s；SLSFAsanProbe 8 个预期场景（含 oob 预期崩溃） | 同上 |
| 2d | `RUN StateCodecFuzz -runs=100000 -seed=1170` | 0 | `DONE cov: 872 corp: 267/114Kb`（694 seed corpus 文件） | 同上 |
| 2e | `RUN SLSFAppearanceProbe` | 0 | `probe_result=OFFLINE_PASS`；`providers-pending(racemenu,skee,slavetatsng,jcontainers)` | 同上 |

本组用例（AppearanceStateTests.cpp）：

1. `jc handle owns and releases exactly once`
2. `racemenu gather waits until the exchange produces the map`
3. `racemenu rejects unregistered or incompatible overlay interfaces`
4. `racemenu lifecycle tracks dirty stale and sessions`
5. `racemenu random exchange sequences stay consistent`
6. `slavetats interface broadcast protocol`
7. `slavetats collects areas into owned records`
8. `slavetats failure paths retry toward failed`
9. `slavetats rejects malformed records and truncates`
10. `slavetats truncates oversized areas at the record limit`
11. `slavetats random placements keep ordering and bounds`
12. `slavetats random messages stay consistent with rejections`
13. `slavetats concurrent broadcasts and collects are stable`

（随机用例 5/11/12 以 seed=1170 重放；并发用例 13 在核心与 ASan 两种构建下均执行。）

### 修复后的开发期失败与复跑

| 失败 | 根因 | 修复 | 复跑 |
| --- | --- | --- | --- |
| `slavetats truncates oversized areas` 首跑 placements 为空 | 测试把 `Snapshot().Area()` 子对象引用绑到临时快照（悬空） | 断言前先把快照复制到局部值 | 同 seed 1170 复跑通过 |
| fake 数组写入与收集读取错位 | 测试 fake domain 跨用例复用同一对象 id，重置不干净 | fake domain 重置时重建容器映射 | 同 seed 1170 复跑通过 |
| 首次 Ensure 返回 Stale | 未武装实例的 epoch 守卫 | `epochSeen_` 吸收首个 epoch（两 adapter 一致） | 同 seed 1170 复跑通过 |

无失败输入 seed 需要留存；随机用例 seed 1170 dice 在 bin；fuzz corpus 见第 2d 行。

## 接口表 ABI 约定与来源

| 项 | 事实 | 来源（本地源码） |
| --- | --- | --- |
| 交换消息常量/数据布局 | `0x9E3779B9`、`'SKEE'`、8B、`InterfaceExchangeMessage{IInterfaceMap* interfaceMap}`（无版本字段）| `E:/source/repos/RaceMenu/skee64/IPluginInterface.h:42-50`、`RaceMenu/skee64/main.cpp:559` |
| 'Overlay' 注册名 | `AddInterface('Overlay', overlay)` | 同上 `RaceMenu/skee64/main.cpp:944` |
| IOverlayInterface 主基槽 | slot0=QueryInterface；主基 vtable slot1=GetVersion、slot11=GetOverlayCount（type 0=Normal） | 同上 `RaceMenu/skee64/OverlayInterface.h/.cpp` |
| 广播数据布局 | `AddressesTable{u32 current_version; void* pointers[]}`，slot13/14 为在用入口 | `E:/source/repos/SlaveTatsNG-master/include/SlaveTatsNG_Interface.h:50-51/142-144` |
| 批量入口语义 | target=ResolveActor 返回 RE::TESObjectREFR*；内部 per-area 数组添加；广播 `kPostPostLoad` | 同上 `SlaveTatsNG-master/src/main.cpp:90-92`、`src/tattoo.cpp` |
| JC 生命周期 | `JValue` 引用计数；借出句柄 retain/release 对称 | `E:/source/repos/SlaveTatsNG-master/include/JContainers/jc_interface.h` |
| 游戏版本 | 目标=SkyrimSE 1.6.1170（`D:/tuanzimouse/Skyrim Special Edition/SkyrimSE.exe`，安装检查见下） | 目标目录检查 |

## 目标安装环境与 probe 待运行项

目标机已检查：SkyrimSE 1.6.1170 + skse64 已装；**未装 RaceMenu/skee、SlaveTatsNG、JContainers**（`D:/tuanzimouse/Skyrim Special Edition/Data/` 无对应 plugin），故目标安装版公共接口最小查询未执行。

待运行项（装好插件后一次 PowerShell/DOS 即可）：

1. 环境：目标机装 RaceMenu（>=1.6.1170 当前版）、SlaveTatsNG、JContainers；开启 SKSE 日志。
2. 接口：`modules/slsf` 载入后 probe 输出 pending 段使用真实分发/广播：
   - RaceMenu：dispatch `0x9E3779B9` sender `'SKEE'` dataLen=8；期望 mapper → `QueryInterface('Overlay')` !null、`GetVersion()==2`、`GetOverlayCount(0, 0..3)` 有效 u32。
   - SlaveTats：message 监听；messageType==1、dataLen==8；期望 `current_version==1`，slot13/14 指针非空；批量返回 false=成功（空列表也算成功，SlaveTatsNG `tattoo.cpp:61-96`），true=失败（target 缺失等），M29 按此读取。
   - JC：`JFormDB.getObj(actor,'.SlaveTats.applied')`；最终 creates==releases。
3. 操作：在安装机上直接运行 `SLSFAppearanceProbe.exe`（已有 probe 结构，后续接真接口只开 pending 段）；`tools/run-native-tests.ps1 -Mode all` 不需要游戏，回归目标值。

## 下组所需产物（后续接入）

1. **引擎接线（PapyrusSKSE）**：向两适配器注入 `RaceMenuExchangePort`/`SlaveTatsPort` 实现；kPostPostLoad 阶段后 dispatch 交换 + 注册 SlaveTats 监听器；会话/装备事件绑 `MarkDirty/OnSessionChanged`。
2. **Visibility 模型接入（组消费）**：`RaceMenuOverlayCounts` → 模型 `SLOT 容量`；`SlaveTatsAreaRecords` → `gameplay::TattooFacts`（applied/subcategory 细分留给模型）。接线模板参见 08 组 `DeviousDevices` 消费路径。
3. **Papyrus 层职责收窄**：`SLOTS(area)` 旧 Papyrus 查询改为透出快照接口；PSC 文件改动留给 Papyrus 接线组。
4. **probe pending 段**：目标安装后补最小接口查询结果（段已就绪）。

## 对抗审核（4 子代理独立复核）

复核范围：M28/M29 源码与 ABI、测试与 probe、合规与执行链；以实际代码为准。结论均可交付，修复项已全部落地并复跑：

1. M28：EnsureLocked 缺失/不兼容分支改为正确 `changed` 语义并刷新快照 epoch 合柜；死分支与无用 `enable_shared_from_this` 移除。
2. M29：`ConsumeLocked` Incompatible→Available 恢复时补 Reset 门；`Area()` 去 noexcept（越界抛出而非终止）。
3. 测试增强：Face/Feet 非空部位、dataLen 16 负例、负数组计数 SECTION、并发轮不变量断言、DIAG 临时输出清理。
4. 文档修正：断言总数 148562→149650（末分片合计）；`InterfaceExchangeMessage` 表述统一为 8B 单指针结构（`IPluginInterface.h:42-50` 源码核实）。
5. 复跑：`-Mode all -Seed 1170` 全套 PASS（379 cases / 149650 assertions），SLSFAppearanceProbe `OFFLINE_PASS`。

---

**执行备注**：所有工作完成于 `SLSF Reloaded NG` 仓库工作区分支；先后接 08 组完成盘对应 `M28/M29` 模块并以 `ExternalState.h`、`Types.h`、`Visibility.h` 为同基调。
