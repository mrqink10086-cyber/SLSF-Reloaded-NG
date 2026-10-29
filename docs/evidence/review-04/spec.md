# 04 组独立规格审计：A07 / B04

基线 `5222a3c31841426bb26c3635a37638abb8f707e5` → 当前树 `2b3912e81d62b4a26dbe43b20b92e18f5ce522cf`。只读审查；未运行 xmake/tests，也未修改生产文件或现有证据。

## 结论

### (a) 遗漏或部分交付

**B04 是部分交付。** 规格要求“在目标运行时验证实际关系等级 `-4..4`、感知/LOS、距离和每 NPC 可见性”，并让 1.6.1170 probe 记录调用上下文及 unknown 条件（`docs/superpowers/plans/2026-10-27-runtime-and-compat-plan.md:41-43`；组验收 `docs/superpowers/plans/2026-10-27-task-books/04-gameplay-and-engine-facts.md:23,30,36`）。当前 probe 只静态核 ABI 并翻译合成 DTO；日志明确写 `probe_runtime_game_queries=pending:no-running-game-backend`（`build/test-results/04-probe.log:5-13`），且尚无 CommonLib backend（`docs/evidence/task-groups/04-gameplay-and-engine-facts.md:124,227-231`）。触发条件是目标游戏中读取实际 Actor；影响是上述游戏事实尚未验证，fake backend 只能验证服务逻辑。最小修正：接入目标运行时 backend，完成逐 NPC probe 并记录关系、检测、LOS、距离及 Unknown 结果；完成前保持 B04 partial。报告已经披露缺口，未把静态结果冒充实机结果。

### (b) Scope creep

未发现超出 A07/B04 的生产实现；新增服务方法、DTO、玩法模块、对应测试和 probe 均落在任务书维护范围内。后续 B02/B03/B05/B07/C 运行时组装未接线不作为本组数值缺陷。

### (c) 表面实现但行为错误

**明确的兼容偏差，未找到规格批准记录。** 总规格规定“除已选定的传播重写和明确的旧错误修复外，玩法以原脚本为基线”（`.scratch/native-migration/spec.md:17`）。DD 未安装时，旧脚本只清三个 bound Global 后返回，保留 collar/bra/belt/harness 可见值及 `IsWearingUnhideable`（`E:/source/repos/SLSF Reloaded/Scripts/Source/SLSF_Reloaded_VisibilityManager.psc:777-790`）；新规则则把这些可见结果设为 false（`src/core/gameplay/Visibility.cpp:489-500`）。触发于 DD 不可用且上一轮可见结果为 true；会改变后续读者看到的状态。交付报告称这是“刻意近似”，但未给出规格批准依据（`docs/evidence/task-groups/04-gameplay-and-engine-facts.md:207`）。最小修正：保留该分支的原状态语义，或把这项行为变更作为批准的规格更新记录。

**条件建议（不计为明确规格缺陷）：** `GameDataService::RefreshTargetsImpl` 只校验返回 Actor key（`E:/source/repos/SLSF Reloaded NG/src/engine/GameData.cpp:222-240`），通用校验只要求 player key 格式有效（`E:/source/repos/SLSF Reloaded NG/src/engine/GameData.cpp:37-43`）；相比 `ActorQueriesService::Query` 检验 Actor 与 player 都匹配请求（`E:/source/repos/SLSF Reloaded NG/src/engine/ActorQueries.cpp:19-29,101-105`），此缓存路径可接受错误 player 的 Ready 快照。若 backend 契约不保证返回同一 player，影响是按另一玩家观测的事实进入扫描；最小改正为比对 `snapshot.player == player` 并增加错误配对回归。现无真实 backend 证明该触发会发生，故仅列条件建议。

## 交付核验矩阵

| 核验项 | 结果 | 依据 / 限制 |
| --- | --- | --- |
| A07 实现与 API | 五个纯规则模块及 DTO 已交付：匿名、可见性、目击、动画分类、评论；接口与规则说明齐全。 | `docs/evidence/task-groups/04-gameplay-and-engine-facts.md:13-27,32-80`。原源码对照及候选顺序/RNG/50 次尝试要求见 `docs/superpowers/plans/2026-10-27-foundation-and-core-plan.md:75-83`；未发现明确数值错配。事件调度和统一装配由后续组完成，不在本审计当成新数值 bug。 |
| B04 实现与 API | `IEngineBackend`、`GameDataService`、`ActorQueriesService`、facts DTO、dirty/cache seam 已实现；实际 CommonLib 查询 backend 未实现/未接插件。 | `docs/evidence/task-groups/04-gameplay-and-engine-facts.md:82-92,209-230`。这是有用的接口交付，但不满足实机验证标准。 |
| 游戏版本 / 实机事实 | 编译期目标为 Skyrim SE 1.6.1170、SKSE 2.2.6、仓库 vendored CommonLib；安装目录与 SKSE DLL 存在。 | 报告 `:6,124,229`；探针通过 target runtime 静态断言。没有运行中的游戏 backend。运行时关系映射、detection、LOS、距离和 NPC 可见性均 pending。静态关系等级 `0..8` 到运行时四档的 `assumed:4-level;unverified` 是已披露假设，不列为缺陷。 |
| 静态 probe 与实机 | `STATIC_PASS` 只证明编译期 ABI/函数签名/装备槽及合成 DTO 翻译；不是实际游戏查询。 | `tests/probe/TargetProbe.cpp:29-70,88-133`；`build/test-results/04-probe.log:1-14`。unknown relationship DTO 得到保留，但真实 unknown 生成条件待 backend 验证。 |
| 测试计数与日志 | build exit 0；全量 120451 assertions / 178 cases；gameplay 362/13；engine 252/18；core shards 20/178；boundary shards 4/4/22；ASan core/boundary 同计数并捕获预期 OOB；fuzz 100000、cov 885、corpus 239。 | 报告 `:130-144`；专属日志 `build/test-results/04-build.log`、`04-full.log`、`04-gameplay.log`、`04-engine.log`、`04-core.log`、`04-boundary.log`、`04-asan.log`、`04-fuzz.log`、`04-probe.log`。本次规格审计未重跑测试。 |
| 四个只读审计关闭声明 | 交付报告称四项均 `PASS WITH FINDINGS`、无 P0/P1/P2；报告列出八项 P3 的修复/记录状态及四个审计标识。 | `docs/evidence/task-groups/04-gameplay-and-engine-facts.md:148-168`。当前交付物只含汇总声明，没有四份原始 transcript；本审计核验的是报告中的关闭记载，不独立复判其子审计过程。 |
| 已知偏差与规格批准 | DD 未安装分支清除可见结果是明确行为差异；未在项目规格/任务书找到批准。动画宽限由执行器注入、ESL FormID 真实采集缺失、引擎未接插件分别属于 seam/后续适配或集成待办；`QueryAll` 部分缓存副作用及无内锁已公开记录。 | `docs/evidence/task-groups/04-gameplay-and-engine-facts.md:207-213`；总规格基线 `.scratch/native-migration/spec.md:17`。 |
