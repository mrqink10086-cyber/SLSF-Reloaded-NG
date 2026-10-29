# 02组：原生状态、地点索引与存档编码

日期：2026-10-27。工作目录：`E:/source/repos/SLSF Reloaded NG`。

本报告保留各次交付的测试结果。提供者状态及容量描述已按[readiness 修复](../2026-10-27-provider-readiness-fix.md)更正，当轮回归为 47 cases / 100397 assertions；当前完整回归见下方 2026-10-28 记录。

2026-10-28 当前：本组数据基础及审核六项修复已完成，见[审核与修复记录](../2026-10-28-01-02-supplement-review.md)；03/04 组可接入地区声望、类别汇总、公开查询快照和 Capture/Restore。范围与要求见[02 任务书](../../superpowers/plans/2026-10-27-task-books/02-state-location-and-codec.md)。下方按日期保留各次检查结果。

配置归属已按[配置与菜单设计](../../../.scratch/native-migration/configuration-design.md)调整为跨存档 JSON。当前 codec v2 保存本档业务；v1 settings/用户 graphEdits 可解码，Restore 明确处置旧字段并使用调用方传入的当前配置重建集合。JSON 读取与会话激活由 B05/B07 接入；下列历史 codec 结果保留。

## 范围

本组实现五个纯核心模块和对应测试；游戏、VM 和 Papyrus 接入由后续任务完成。

## 改动文件

`src/core/reputation/ReputationState.cpp`、`src/core/reputation/ProviderRegistry.cpp`、`src/core/location/LocationIndex.cpp`、`src/core/location/RegionRegistry.cpp`、`src/core/persistence/StateCodec.cpp`。

测试与辅助文件：`tests/core/{ReputationState,ProviderRegistry,LocationIndex,RegionRegistry,StateCodec}Tests.cpp`、`tests/core/LocationRandomTests.cpp`、`tests/reference/LocationReference.{h,cpp}`、`tests/fuzz/StateCodecFuzz.cpp`、`tests/fuzz/xmake.lua`、`tests/fixtures/serialization/v1/{empty,minimal}.bin`、`tools/run-native-tests.ps1`（fuzz 模式补 `SLSFCoreFuzz` 的可见性检查与构件目录清理）。

## 行为要点

### 声望和 provider
- `ApplyExplicit` 支持 `Set`、`Clear`、`ClearAll`、`Wipe`、`SetFlag`、`ClearFlag`。Set 写 `L=V,R=min(R,V)`，查询返回 `max(L,R)`。Clear 清目标地区/类别的 L、R；ClearAll 清所有地区 25 类的 L/R，保留外部 flags；Wipe 清所有地区的指定类别。四种可选类别为 `CumDump`、`Unfaithful`、`Cuck`、`Airhead`。
- 地区声望写入由 `ApplyExplicit` 处理；模组注册及类别启用由 `UpdateProvider` 处理。
- `UpdateProvider` 的 `ProviderPhase` 顺序是 `Enabled=0,Disabled=1,Missing=2,Incompatible=3,Stale=4,NotReady=5,Unregistered=6`。`Enabled` 添加类别设置；NotReady/Stale 保留类别设置并返回相应状态。确认失效撤销对应类别设置，只有四种可选类别可生成一次性待清理集合，业务层还需确认内置来源不存在。`Unregistered` 只清 provider 自己的类别设置。注册保存在 `ProviderState::contributions`，当前容量为 `kMaxProviderRegistrations = 255`。

### 地点
- `BuildLocationIndex` 以 `StableFormKey` 建立父链与 DFS 区间；重复 `StableFormKey`、坏 parent、自父链和 cycle 都返回失败。测试覆盖到 20000 节点链，不使用递归。
- `ResolveRegion` 与 alias 解析使用 `RegionRegistry`，支持 6.5 万地点索引映射，区域 API 索引独立保存。
- `IsAncestor` 分两个分支：包含节点自身和不包含节点自身，均按 DFS 区间判定；跨树节点不互为祖先。

### 存档
- `EncodeState/DecodeState` 使用 v1，头部带 magic 与 schema version。provider、custom region、graph edit、settings 等字段可 roundtrip。
- 存档上限：`MaxLocations=65535`、`MaxProviders=1048576`、`MaxCustomRegions=65535`、`MaxGraphEdits=100000`、`MaxSettings=2000`。
- `DecodeState` 完成结构、枚举和容量检查后返回完整结果；解析失败返回具体错误。

## 测试事实

以下结果均在根项目执行，并已确认退出码。

| 命令 | 结果 |
|---|---|
| `xmake run SLSFCoreTests "[state],[providers],[locations],[codec]" --rng-seed 1170` | exit 0；35 cases / 100311 assertions 通过 |
| `xmake run SLSFCoreTests --rng-seed 1170` | exit 0；44 cases / 100367 assertions 通过 |
| `powershell -NoProfile -ExecutionPolicy Bypass -File tools/run-native-tests.ps1 -Suite differential -Seed 1170` | exit 2，打印 `SLSFCoreDifferentialTests is not built` 后跳过；没有可报告的通过结果 |
| `powershell -NoProfile -ExecutionPolicy Bypass -File tools/run-native-tests.ps1 -Suite asan -Seed 1170` | exit 0；`SLSFCoreTestsAsan` 44 cases / 100367 assertions 通过；`SLSFBoundaryTestsAsan` 4 cases / 22 assertions 通过；`SLSFAsanProbe` 正向 `probe=42 index=7`，负向在 `tests/asan/AsanProbe.cpp:15` 从 AddressSanitizer 报 heap-buffer-overflow |
| `powershell -NoProfile -ExecutionPolicy Bypass -File tools/run-native-tests.ps1 -Suite fuzz -Target StateCodecFuzz -Runs 100000 -Seed 1170` | exit 0；100000 runs 完成；seed 1170；411 个 corpus 文件；cov 712 / feature 1855；耗时约 1 秒 |

JUnit 报告在 `build/test-results/{core,core-asan,boundary-asan}.xml`。ASan 可执行文件在 `build/asan`。fuzz artifact 目录为空。

## 后续接入

- 数值参考实现由后续组接入 differential 后运行对照。
- ASan/fuzz 检查 C++ 核心；游戏和插件运行时由相应集成组检查。
- 后续新增保存字段时，同步更新 codec 和保存/恢复用例。

## 下组可复用

- `src/core/persistence/StateCodec.cpp` 及 `tests/fixtures/serialization/v1/{empty,minimal}.bin`
- `src/core/location/{LocationIndex,RegionRegistry}.cpp`（供跨组调用）
- `tools/run-native-tests.ps1` 中 core / asan / fuzz 三个已测命令
- 本报告：`docs/evidence/task-groups/02-state-location-and-codec.md`

## 2026-10-28：owner 长链与测试并行

`RegionRegistry.cpp` 将 owner 求解改为连续数字索引、活动路径判环及迭代记忆化，求解阶段 O(n)；完整绑定另有排序及 map 校验成本。20000 节点完整绑定本次普通版 39.224 ms、ASan 591.790 ms。

新增正反 20000 节点链、共享尾部/独立根及无地点引用的长环回归。完整普通/ASan 核心均为 69 cases / 100918 assertions，边界均为 4 / 22，exit 0；ASan 正负探针通过预期检查。

测试脚本按逻辑 CPU 多进程分片，固定 seed 分散重用例，单次执行同时生成控制台与 JUnit。ASan 核心本次由单分片 11.593 秒降至 20 分片 4.931 秒。游戏运行时维持统一执行器。命令、前后计时和日志见[性能修复记录](../2026-10-28-region-owner-and-parallel-tests.md)。

## 2026-10-28：补充交付审核

按初始化提交对固定工作区 tree 审核，Luna/xhigh 三个子代理分别检查规范/注释、需求对应和算法反例。规范轴 0 项需改违规；需求轴 1 项 P1、5 项 P2：未绑定地区声望数据完整记录回存、owner 候选优先级、零类别 provider 注册保存、单项写入全量重算、RegionId 命令入口和 live contribution mask。完整需求对应、生产入口复现及修正方向见[审核报告](../2026-10-28-01-02-supplement-review.md)。

审核时普通/ASan 核心为 69 cases / 100918 assertions、边界 4 / 22，ASan 正负探针及 seed 1170 的 100000 次 codec fuzz 通过。四个独立反例用于本次修复的回归输入。

## 2026-10-28：六项修复完成

暂缺地区的完整声望随存档保留并可重新绑定；地点归属保留声明地区的注册优先级；零类别模组注册可保存、恢复和继续启用类别。单项命令接收 RegionId，只重算目标地区，通过类别计数更新汇总；运行期模组启用类别使用 CategoryMask，按变化位更新计数。

普通与 ASan 核心均为 75 cases / 100987 assertions，边界各 4 / 22，全部通过；ASan 正负探针及 100000 次 codec fuzz 通过。原四个探针均重放；百万次写入在 128 至 65535 地区规模约 0.017–0.018 μs/次。详细命令、日志和前后结果见[审核与修复记录](../2026-10-28-01-02-supplement-review.md)。

## 2026-10-28：A03/A04/A08 优化补做

### 交付范围

五个核心 CPP 及其头文件/测试/保存接入完成本轮补做：

- `src/core/location/LocationIndex.cpp`：`FindLocation` 改为按 `StableFormKey` 的 `lower_bound` 二分；构建期仍保持稳定键排序、重复/缺父/自父/环检查和迭代 DFS 区间。
- `src/core/location/RegionRegistry.cpp`：绑定期预编译规范化 alias 表并按 alias 二分；`ResolveRegion` 保持 `locationOwners` 直接索引；owner 链编译为数字 RegionId，父先子后一次合并“本节点候选 + 父节点最优”，同一 owner 的根/室内/子地点共享账户；公开 `regionApiIndex` 与声明 `customOrder` 规则不变。
- `src/core/reputation/ReputationState.cpp`：状态按 `RegionId` 账户保存 `local/remote/fame`、flags、保护时间、业务时间、匿名度和辅助状态；`accountKeys` 保存稳定区域身份，`locationAccounts` 保存 LocationId→账户映射；`BindStateToRegions` 按稳定键迁移行并原子提交、递增 revision。快照只发布 `fame`、flags、账户/地点映射、revision/epoch/配置 revision；完整业务由 `CaptureState` 导出。
- `src/core/reputation/ProviderRegistry.cpp`：`activeCounts` 继续按贡献位维护，新增 `ActiveCategories`/`RebuildActiveCounts`；撤销只减自己的计数，NotReady/Stale 保留贡献，四类可选类别仅在确认失效且计数归零时产生一次清理候选。
- `src/core/persistence/StateCodec.cpp`：v2 保存本档业务（含 `remoteSettledAt` 与 `ProviderContributionState{registrationId, CategoryMask}`），v1 旧布局继续可解码/可编码；v2 不写 settings 和用户 graphEdits，v1 的这两段由 `RestoreState` 明确忽略并通过 `RestoreReport` 报告。解码各段使用顺序字节预算，拒绝截断、尾随字节、空位置/自定义区域 plugin、非法类别/布尔枚举、容量和重复项；v1 `providerFlags.phase` 按旧格式不透明保留（Restore 只读取 enabled/category），不做枚举拒绝。

### 版本与边界更正

上方历史“存档”段保留原记录，本轮更正：当前 codec 为 **v2**；`MaxGraphEdits=1<<20`、`MaxSettings=1<<16`。v2 是本轮工作区新增、尚未随任何发布存档交付的格式；同一 v2 不再改变布局，后续字段变化必须升 v3。`PersistentState::codecVersion` 默认 2，Capture 写 2；Decode 接受 1/2。跨存档 JSON 设置和用户连接偏好由 B05/B07 在 decode 后重新读取并传入当前 `ConfigurationSnapshot`，本组不保存旧 JSON 设置。

`Query` 仍只有声明（`include/slsf/core/Commands.h`），本轮未实现：`PublishedState` 没有 recognition-time 数据，且公开查询需要 03/04 组补齐业务来源；作为交接项列出，未计入本组验收。

### 审核修复对照

三个只读子代理分别检查算法、内存/栈/arena 和需求对应；其中一次会话被 provider 拦截，算法审查由另一会话完成。随后用独立会话逐条核对报告与源码，又发现并修复 8 项边界问题（v1 展开上限、非法命令 kind、重复账户键、越界 owner、部分 customOrder、闭合 parent 环、空自定义区域 plugin、v1 phase 表述）。确认无内存破坏/UAF/溢出、无递归、无池/arena 使用。已修复：

| 发现 | 处理 |
|---|---|
| 空 `customOrder` 被默认成全部 keys，压过 owner 规则（反例应解析 RegionId(3)，实际 RegionId(1)） | 优先级只遍历声明 customOrder，再 owner 规则，再剩余 keys；新增回归 |
| provider 清理先改后验，`{Nasty, Count}` 会留下部分提交 | 先完整验证全部类别，再统一清理活动/未解析行和 flags |
| `BindStateToRegions` 改映射不升 revision、分配失败可能半提交 | 先构造所有副本，成功后一次性安装并 `NextRevision`；新增断言 |
| 未知地区行在 ClearAll/Wipe/provider 清理后仍会在重绑时复活 | 全局清理同时清 `unresolvedAccounts` 对应列/行；新增组合回归 |
| `RestoreState` 未检查 255 provider 注册上限 | 超限返回 `OutOfRange`，新增回归 |
| 解码各行独立使用同一 remaining 预算，可放大瞬时分配 | 改为 header 后统一顺序预算 |
| 空 plugin 的位置键被 codec 接受、绑定却拒绝 | encode/decode 统一拒绝 |
| 发布测试直接写 L/R 未重算 F，期望值不成立 | 测试调用 `RecomputeAccount`；生产写入路径均已重算 |
| `ResolveRegion` 未校验 owner 索引；循环手建 parent 被静默丢弃 | 增加范围检查和 `order.size()` 失败返回 |
| `regionIds` 注释与 account-major 布局不符；移除无调用者的 LocationId slot 重载 | 注释更正、测试改用 RegionId 重载 |
| 复核：v1 的 100 万 providerFlags 在 255 上限检查前被展开 | 展开循环内达到 255 即返回 `OutOfRange`，新增 256 名称回归 |
| 复核：非法 `ExplicitCommandKind` 落入空 switch 后仍提交 | 增加 `default` 拒绝，新增无写入/无 revision 断言 |
| 复核：`BindStateToRegions` 对重复 `accountKeys` 静默丢弃 | 改为返回 `InvalidState`，新增回归 |
| 复核：`locationOwners` 越界只在读取时检查 | Bind/Restore/Capture 均校验，`MakePublishedState` 对越界投影降为 invalid；新增回归 |
| 复核：部分 `customOrder` 令未列出 key 的 `regionApiIndex` 重复为 0 | 显式前缀后追加剩余 key 生成唯一公开索引；优先级仍只用声明顺序；新增回归 |
| 复核：手建 parent 全存在但成环时 `ParentFirstOrder` 按 entry 排序绕过检测 | 绑定前独立 `ParentsFormForest` 检测并返回 `InvalidState`；新增闭合环回归 |
| 复核：codec 接受空 plugin 的自定义区域键 | encode/decode 同时拒绝，新增回归 |
| 再复核：`RestoreState` 未校验 region 表自身的空/重复键 | 安装前校验并返回 `InvalidArgument`；新增空键/重复键回归 |
| 再复核：绑定失败后的不变式缺少断言；255 注册边界只有拒绝用例 | 补充失败后状态/revision 不变断言及 255 成功回归 |

### 已运行检查

均在工作区根目录、seed 1170 执行；`build/test-results/` 为 JUnit，`build/logs/` 为日志。

| 命令 | 退出码 | 结果 |
|---|---|---|
| `xmake build SLSFCoreTests` | 0 | releasedbg 构建成功 |
| `xmake run SLSFCoreTests "[state],[locations],[codec],[providers],[publish],[accounts],[capture],[regression]" --rng-seed 1170` | 0 | 64 cases / 100676 assertions 通过 |
| `xmake run SLSFCoreTests --rng-seed 1170` | 0 | 130 cases / 119295 assertions 通过（含其他组同仓用例） |
| `powershell -NoProfile -ExecutionPolicy Bypass -File tools/run-native-tests.ps1 -Mode core -Seed 1170` | 0 | PASS；`build/test-results/core.xml` tests=789、failures=0；日志 `build/logs/core-1170-final.log` |
| `powershell -NoProfile -ExecutionPolicy Bypass -File tools/run-native-tests.ps1 -Mode boundary -Seed 1170` | 0 | PASS；`build/test-results/boundary.xml` tests=8、failures=0；日志 `build/logs/boundary-1170-final.log` |
| `powershell -NoProfile -ExecutionPolicy Bypass -File tools/run-native-tests.ps1 -Mode asan -Seed 1170` | 0 | PASS；`core-asan.xml` tests=789、`boundary-asan.xml` tests=8；日志 `build/logs/asan-1170-final.log`；负向探针按预期报 `AddressSanitizer: heap-buffer-overflow` |
| `powershell -NoProfile -ExecutionPolicy Bypass -File tools/run-native-tests.ps1 -Mode fuzz -Target StateCodecFuzz -Runs 100000 -Seed 1170` | 0 | PASS；100000 runs / 1s；seed 1170；cov 839、ft 1879、corp 193；日志 `build/logs/fuzz-1170-final.log` |
| `powershell -NoProfile -ExecutionPolicy Bypass -File tools/research/run-native-algorithm-probe.ps1 -Seed 1170` | 0 | `checks,all_equal`；结果 `build/research/native-algorithm/results.csv` |

固定输入：`tests/fixtures/serialization/v1/{empty,minimal}.bin` 继续作为 v1 兼容种子；随机地点/编码用例 seed 1170。失败重放所需的 case、输入和 seed 均由 Catch2 输出/上述 fixture 保留。

### 成本证据

环境：13th Gen Intel Core i5-13600KF，MSVC 19.44.35229，/O2 /arch:AVX2，固定 logical processor 0，rounds=7；同次运行内比较，绝对值为 ns/μs。

| 入口 | 规模 | 基线 | 当前 | 比值 |
|---|---:|---:|---:|---:|
| 地点查找（线性→二分） | 256 | 569.653 ns | 86.621 ns | 6.58x |
| 地点查找（线性→二分） | 4096 | 9742.627 ns | 127.808 ns | 76.23x |
| owner 链（4 叉） | 4096 | 11.557 μs | 2.651 μs | 4.36x |
| owner 链（长链） | 4096 | 11633.650 μs | 6.125 μs | 1899.4x |
| 快照（Capture→窄发布） | 21 | 1.379 μs | 0.965 μs | 1.43x |
| 快照（Capture→窄发布） | 512 | 75.156 μs | 25.344 μs | 2.97x |
| 快照（Capture→窄发布） | 4096 | 1152.255 μs | 233.510 μs | 4.93x |

同次探针中的 deadline heap 与 AVX2 扩散行属于其他组入口，保留在 CSV 供 03/04 组回归对照；本轮没有修改其算法。

### 03/04 组接入点

- 载入：`DecodeState`（v1/v2）→ 以稳定区域键 `BindStateToRegions` → 传入当前 `ConfigurationSnapshot` 调用 `RebuildDerivedSets`；`RestoreReport` 记录旧 settings/graphEdits 是否被忽略及未解析账户数。
- 运行：命令统一使用 `RegionId` 账户；`locationAccounts` 只做公开 LocationId→账户投影；`AccountForLocation` 校验映射，`MakePublishedState` 产出窄快照，保证同一 revision 内映射与数值一致。
- 保存：`CaptureState` 生成完整 `PersistentState`，再 `EncodeState`；不要从 `PublishedState` 反推 L/R、保护时间或贡献。
- 原子性：`ApplyExplicit`、`ApplyProviderTransition`、`BindStateToRegions` 成功才递增 revision；失败保持已提交状态。
- 集合：`DerivedCategorySets` 由恢复后的账户业务值和当前配置重建；`ProviderState::activeCounts` 由 restore 重建，不由存档直接信任。
- 公开索引：`RegionSnapshot::aliasIndex` 已按规范化 alias 排序；alias/customOrder 的优先级和公开 `regionApiIndex` 不变，03/04 组不要重新排序。
- 待办：`Query` 需要 03/04 提供 recognition-time 与公开投影来源；`differential`/`bench` 目标仍按任务书由后续组接入。

### 游戏侧待运行

本组只完成纯核心和真实 C++ 入口检查；未启动 Skyrim。游戏验证需在 SkyrimSE 1.6.1170 + SKSE 2.2.6（Windows x64、MSVC）中由 B05/B07 接入：

1. 用 `Data/SKSE/Plugins/SLSFReloadedNG.json` 读取当前设置，关闭旧会话后 decode 本档、重读 JSON、按当前连接偏好绑定/重建，再激活。
2. 对同一 owner 的根/室内/子地点写入并重载，确认共享账户；修改地区注册后重绑，确认按稳定区域身份迁移。
3. 触发 provider NotReady→失效和两 provider 同类别撤销，确认贡献 OR、单次四类清理候选及未解析行清理。
4. 保存含 flags/保护时间/贡献/模组注册的存档，换档后确认业务值、贡献和当前配置重建结果；用 v1 fixture 确认旧 settings/graphEdits 被报告为忽略且不覆盖当前 JSON。
5. 记录 SKSE 日志、实际存档/换档步骤和 ABI（SKSE 2.2.6 / CommonLib 固定提交）后再把游戏项标为通过。
