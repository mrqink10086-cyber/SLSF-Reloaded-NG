# 地点、地区与固定传播链源码研究

日期：2026-10-28。范围：当前纯核心、原始 Papyrus 和已选设计；未改生产/测试/主规格，未执行构建或游戏验证。下文“已有测试”表示源码内有对应断言，不代表本研究重新运行了测试。

## 结论

- **小镇已经在默认传播图定义里**：21 个节点、39 对双向连接、展开成 78 条有向边。除 9 个天际领地首府外，还包含 Raven Rock、Riverwood、Rorikstead、Ivarstead、Shor's Stone、Dragon Bridge、Karthwasten、Skaal Village 和四个兽人据点 Largashbur、Dushnikh Yal、Mor Khazgur、Narzulbur。节点及完整连接可直接查 [DiffusionGraph.cpp:22](<E:/source/repos/SLSF Reloaded NG/src/core/reputation/DiffusionGraph.cpp:22>)；计数常量见 [DiffusionGraph.h:45](<E:/source/repos/SLSF Reloaded NG/include/slsf/core/reputation/DiffusionGraph.h:45>)。
- **这不等于已在游戏里绑定启用**：默认图 builder 接收调用方提供的稳定地区身份，测试使用 `topology.esp` 合成身份；当前 SKSE 入口只有 `SKSE::Init` 和日志。没有证据可声称已从游戏记录采集并注册这 21 个独立地区。依据：[DiffusionGraph.cpp:194](<E:/source/repos/SLSF Reloaded NG/src/core/reputation/DiffusionGraph.cpp:194>)、[DiffusionGraphTests.cpp:154](<E:/source/repos/SLSF Reloaded NG/tests/core/DiffusionGraphTests.cpp:154>)、[main.cpp:1](<E:/source/repos/SLSF Reloaded NG/src/main.cpp:1>)。规格也明确游戏接入待完成：[spec.md:132](<E:/source/repos/SLSF Reloaded NG/.scratch/native-migration/spec.md:132>)。
- **自定义子地点和独立地区，两种模型都能表达**。地点父子树回答所属地区；传播图另行定义连接。普通房间/子地点可继承同一 RegionId；有独立身份和优先注册顺序的子地点可成为独立地区，独立地区可以有边也可以孤立。依据：[Snapshots.h:17](<E:/source/repos/SLSF Reloaded NG/include/slsf/core/Snapshots.h:17>)、[RegionRegistry.cpp:309](<E:/source/repos/SLSF Reloaded NG/src/core/location/RegionRegistry.cpp:309>)、[location-index-design.md:19](<E:/source/repos/SLSF Reloaded NG/docs/superpowers/specs/2026-10-26-location-index-design.md:19>)、[location-index-design.md:25](<E:/source/repos/SLSF Reloaded NG/docs/superpowers/specs/2026-10-26-location-index-design.md:25>)。运行期注册、菜单连接编辑和世界装配仍属于后续接线：[spec.md:97](<E:/source/repos/SLSF Reloaded NG/.scratch/native-migration/spec.md:97>)、[spec.md:101](<E:/source/repos/SLSF Reloaded NG/.scratch/native-migration/spec.md:101>)。

## 原脚本与默认节点覆盖

旧 ESP 的 DefaultLocation 确实按以上顺序存了 21 个名字；CustomLocation 是另外 21 个可填槽，初始均为空占位。因此研究里用的“42 个声望地区”是 21 默认 + 最多 21 自定义的容量估算，不能解释成 42 个固定地图节点，也不能与 ESP 中另一个 42 GLOB 记录计数混用。依据：[legacy-records.json:5660](<E:/source/repos/SLSF Reloaded NG/contracts/legacy-records.json:5660>)、[legacy-records.json:6068](<E:/source/repos/SLSF Reloaded NG/contracts/legacy-records.json:6068>)、[LocationManager.psc:15](<E:/source/repos/SLSF Reloaded/Scripts/Source/SLSF_Reloaded_LocationManager.psc:15>)。

旧公开说明把 9 个首府列为 Major，把 Raven Rock 及其后 11 个地点列为 Minor；但实际 VMAD `MajorLocations` 有 10 个引用，`GetFameLocation` 使用 `DefaultLocation[LocationIndex + 10]`。因此分类文字与执行数组不能直接混同；用户问题里的“小镇是否在图里”只需要看明确节点，无须依赖这个分类边界。依据：[ModEventListener.psc:93](<E:/source/repos/SLSF Reloaded/Scripts/Source/SLSF_Reloaded_ModEventListener.psc:93>)、[legacy-records.json:5846](<E:/source/repos/SLSF Reloaded NG/contracts/legacy-records.json:5846>)、[LocationManager.psc:218](<E:/source/repos/SLSF Reloaded/Scripts/Source/SLSF_Reloaded_LocationManager.psc:218>)。当前 contract 的 0–8 Major/9–20 Minor 是公开说明分类，不能充当运行期采集数组事实：[legacy-rules.json:422](<E:/source/repos/SLSF Reloaded NG/contracts/legacy-rules.json:422>)。

默认图保留的是旧 FameManager 默认到默认的固定链；旧脚本还在相应分支追加按所属领地归组的自定义地点和 Undefined 地点。新 21 节点图不能代表这些动态连接已经迁移：拓扑 JSON 自称 `design-reference-not-runtime-configuration`，默认 builder 仅要求 21 个固定节点绑定。依据：[FameManager.psc:1654](<E:/source/repos/SLSF Reloaded/Scripts/Source/SLSF_Reloaded_FameManager.psc:1654>)、[LocationManager.psc:306](<E:/source/repos/SLSF Reloaded/Scripts/Source/SLSF_Reloaded_LocationManager.psc:306>)、[default-reputation-topology.proposed.json:3](<E:/source/repos/SLSF Reloaded NG/docs/research/default-reputation-topology.proposed.json:3>)、[DiffusionGraph.cpp:200](<E:/source/repos/SLSF Reloaded NG/src/core/reputation/DiffusionGraph.cpp:200>)。

## 自定义子空间、优先级与独立声望

旧脚本明确先匹配按注册顺序的 Custom，再 Minor，再 Major，注释也明确 Major/Minor 内可以有 Custom 子地点。这就是“城内的自定义地点也能独立”的原始依据：[LocationManager.psc:208](<E:/source/repos/SLSF Reloaded/Scripts/Source/SLSF_Reloaded_LocationManager.psc:208>)。

当前绑定先把 owner 链归并成 canonical owner，再沿地点森林父先子后比较注册优先级。关键逻辑是 `min(priority[声明地区], priority[最终所属地区])`；最终候选指向 canonical owner。父节点优先级更高时继承父地区，因此“离地点更近”不能自动覆盖“先注册”。依据：[RegionRegistry.cpp:243](<E:/source/repos/SLSF Reloaded NG/src/core/location/RegionRegistry.cpp:243>)、[RegionRegistry.cpp:277](<E:/source/repos/SLSF Reloaded NG/src/core/location/RegionRegistry.cpp:277>)、[RegionRegistry.cpp:317](<E:/source/repos/SLSF Reloaded NG/src/core/location/RegionRegistry.cpp:317>)。已有测试刻意覆盖“祖先优先于更近子地点”和“child 注册优先级在 owner 重映射后保留”：[LocationIndexTests.cpp:125](<E:/source/repos/SLSF Reloaded NG/tests/core/LocationIndexTests.cpp:125>)、[RegionRegistryTests.cpp:136](<E:/source/repos/SLSF Reloaded NG/tests/core/RegionRegistryTests.cpp:136>)。

具体使用方式：

| 场景 | 当前纯核心表达方式 | 传播含义 |
|---|---|---|
| 城市内普通房间 | 不独立注册，沿父链取得城市 owner | 与城市读取相同地区声望，无需建传播边 |
| 城市内独立经营区/自定义子空间 | 独立 stableKey + 地区注册，注册顺序应高于外层；不把它 owner 合并到城市 | 有自己的地区声望，连接须显式声明 |
| 跨树独立城市 | 新稳定地区身份，不指向其他 owner | 可通过 EdgeDefinition 接入既有链，未声明边则孤立 |
| 多个已登记地点共享城市 | 用 ownerRules 归并；地点查询返回相同 canonical RegionId | 身份共享不产生传播连接 |

这张表由实际绑定逻辑及数据结构推导：[RegionRegistry.cpp:169](<E:/source/repos/SLSF Reloaded NG/src/core/location/RegionRegistry.cpp:169>)、[RegionRegistry.cpp:309](<E:/source/repos/SLSF Reloaded NG/src/core/location/RegionRegistry.cpp:309>)、[Snapshots.h:34](<E:/source/repos/SLSF Reloaded NG/include/slsf/core/Snapshots.h:34>)。已有独立地区注册保持无关归属测试，以及同 owner 共享数值的测试：[RegionRegistryTests.cpp:162](<E:/source/repos/SLSF Reloaded NG/tests/core/RegionRegistryTests.cpp:162>)、[ReputationStateTests.cpp:330](<E:/source/repos/SLSF Reloaded NG/tests/core/ReputationStateTests.cpp:330>)。

要注意：当前 State 分配 `regions.regions.size()` 行，保存全部已登记稳定地区身份；`locationOwners` 决定地点实际访问哪一行。不能把“多个地点共享 owner”进一步推成“所有非 owner 注册身份已经从行表压缩删除”。依据：[ReputationState.cpp:308](<E:/source/repos/SLSF Reloaded NG/src/core/reputation/ReputationState.cpp:308>)、[ReputationState.cpp:341](<E:/source/repos/SLSF Reloaded NG/src/core/reputation/ReputationState.cpp:341>)。

## 固定图与 owner 合并的现有缺口

1. `BuildLegacyDefaultGraph` 不接受 RegionSnapshot，只有 `nodeKey -> StableRegionKey` 绑定。它校验 21 个名字、非空插件和互不相同的 stableKey，**不解析 canonical owner**。给两个节点同一个 stableKey 会直接拒绝。依据：[DiffusionGraph.cpp:194](<E:/source/repos/SLSF Reloaded NG/src/core/reputation/DiffusionGraph.cpp:194>)、[DiffusionGraph.cpp:218](<E:/source/repos/SLSF Reloaded NG/src/core/reputation/DiffusionGraph.cpp:218>)。
2. 通用 `BuildDiffusionGraph` 只检查稳定地区键存在、无自身边及重复边；`CompileDiffusionGraph` 在 accountKeys 中二分成数字行，也不走 `locationOwners`。因此若两个 distinct 地区身份后来由 owner 合并，当前图层不会自行折叠。依据：[DiffusionGraph.cpp:67](<E:/source/repos/SLSF Reloaded NG/src/core/reputation/DiffusionGraph.cpp:67>)、[DiffusionGraph.cpp:142](<E:/source/repos/SLSF Reloaded NG/src/core/reputation/DiffusionGraph.cpp:142>)。
3. 建议后续 WorldModel 准备阶段明确：图端点是否只允许 canonical 地区。如果支持 owner 合并后折图，准备 `地区身份 -> canonical RegionId`，折叠端点、移除自环、合并重复边；在当前 max-times 同步传播语义下，平行边可取最大保留系数。此项是待实现方案，不是当前代码能力。已选设计的准备阶段职责依据：[design.md:47](<E:/source/repos/SLSF Reloaded NG/.scratch/native-migration/design.md:47>)、[design.md:55](<E:/source/repos/SLSF Reloaded NG/.scratch/native-migration/design.md:55>)、[spec.md:47](<E:/source/repos/SLSF Reloaded NG/.scratch/native-migration/spec.md:47>)。

## 查表与连续性

- 地点稳定身份排序后用二分，地区 stableKey 也二分；热路径 `ResolveRegion(LocationId)` 直接读连续 `vector<RegionId> locationOwners`，已经是 O(1) 地点到地区查询。alias 绑定时规范化/排序，查询只规范化输入一次再二分。依据：[LocationIndex.cpp:147](<E:/source/repos/SLSF Reloaded NG/src/core/location/LocationIndex.cpp:147>)、[RegionRegistry.cpp:368](<E:/source/repos/SLSF Reloaded NG/src/core/location/RegionRegistry.cpp:368>)、[RegionRegistry.cpp:376](<E:/source/repos/SLSF Reloaded NG/src/core/location/RegionRegistry.cpp:376>)、[RegionRegistry.cpp:387](<E:/source/repos/SLSF Reloaded NG/src/core/location/RegionRegistry.cpp:387>)。
- `vector` 的元素是连续的，但 StableFormKey/StableRegionKey 的插件字符串不因此成为统一连续字节池；当前地区/森林构建还用 map 与 `vector<vector<LocationId>>`。这些是准备阶段结构，不能据此说“所有表和算法完全连续”。依据：[Types.h:155](<E:/source/repos/SLSF Reloaded NG/include/slsf/core/Types.h:155>)、[Snapshots.h:17](<E:/source/repos/SLSF Reloaded NG/include/slsf/core/Snapshots.h:17>)、[LocationIndex.cpp:29](<E:/source/repos/SLSF Reloaded NG/src/core/location/LocationIndex.cpp:29>)、[LocationIndex.cpp:53](<E:/source/repos/SLSF Reloaded NG/src/core/location/LocationIndex.cpp:53>)。
- 当前 CSR 的 offsets 和 operands 各自连续，operand 仍通过 source 数字索引读取冻结数值行；邻边来源不保证在空间上相邻。它消除了边链表/字符串运行期查找，不会让任意图的访存顺序变成全局顺序流。依据：[DiffusionGraph.h:15](<E:/source/repos/SLSF Reloaded NG/include/slsf/core/reputation/DiffusionGraph.h:15>)、[DiffusionGraph.cpp:158](<E:/source/repos/SLSF Reloaded NG/src/core/reputation/DiffusionGraph.cpp:158>)。
- 本范围直接可做的优化：把自定义地区资格编译为与 RegionId 同序的连续 byte/bitset，替代 `IsCustomAccount` 每地区 `any_of(customRegions)`；只在绑定/注册变化时重建。当前每次衰减逐地区调用该线性查找，整体 N 个地区、C 个自定义身份是 O(N*C) 比较。依据：[Settlement.cpp:410](<E:/source/repos/SLSF Reloaded NG/src/core/reputation/Settlement.cpp:410>)、[Settlement.cpp:518](<E:/source/repos/SLSF Reloaded NG/src/core/reputation/Settlement.cpp:518>)。另可在未来游戏 adapter 将运行时 FormID 映射缓存为 LocationId，事件入口解析一次后传数字 ID；这是接线建议，当前没有游戏采集实现可以宣称已有缓存。设计已要求运行期使用连续数字 ID：[design.md:137](<E:/source/repos/SLSF Reloaded NG/.scratch/native-migration/design.md:137>)。

## 对扩展“整个链”的建议

保留现有 21 个默认地区，先完整接好游戏稳定身份、owner 和默认连接；额外村镇按玩法选择注册为独立地区，并通过显式连接接入。全部建筑/室内地点仍可进入静态森林并映射到所属地区，无须每个建筑都建立独立传播节点。这样扩展地点覆盖与扩展声望地区数量是两项独立决定，符合已选“城市/室内共享，独立注册单独保存”要求：[spec.md:30](<E:/source/repos/SLSF Reloaded NG/.scratch/native-migration/spec.md:30>)、[spec.md:47](<E:/source/repos/SLSF Reloaded NG/.scratch/native-migration/spec.md:47>)、[design.md:53](<E:/source/repos/SLSF Reloaded NG/.scratch/native-migration/design.md:53>)。

尚需产品选择的是：额外独立子地区默认沿哪条显式边接入、跨树独立城市使用何种默认连接、owner 合并后的固定图采用拒绝还是折叠。C++ 速度本身不能决定这些传播含义。当前规格已经选择用户连接偏好保存在跨档 JSON、未连接独立地区显示孤立，具体编辑和运行期应用尚待 B07：[spec.md:32](<E:/source/repos/SLSF Reloaded NG/.scratch/native-migration/spec.md:32>)、[spec.md:53](<E:/source/repos/SLSF Reloaded NG/.scratch/native-migration/spec.md:53>)、[spec.md:101](<E:/source/repos/SLSF Reloaded NG/.scratch/native-migration/spec.md:101>)。
