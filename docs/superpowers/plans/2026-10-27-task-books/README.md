# 原生迁移分组入口

先读[项目规格与实施路线](../../../../.scratch/native-migration/spec.md)，再按下表进入任务。规格定义需求和已选行为；分组只组织执行。

[代码库设计](../../../../.scratch/native-migration/design.md)说明模块、接口、状态归属和现有实现接入。处理跨文件业务或运行时接线时，先按该设计对齐职责。

[优化实施方案](../../../../.scratch/native-migration/optimization-plan.md)维护已采纳的布局、索引、类别集合和时间/SIMD 执行方式。01/02 数据基础及 03 的整链优化已完成，当前进入 04 的玩法规则与后续会话接入。

[配置与菜单设计](../../../../.scratch/native-migration/configuration-design.md)统一跨存档 JSON、更新通知及菜单事件；01/02 补数据约定和保存边界，06 接生命周期入口，10 完成文件与 UI 接入。

| 组 | 任务 | 说明 | 前置组 |
|---|---|---|---|
| 01 | A01、A02 | [公开接口清单、共享类型与测试工程](01-contracts-and-build.md) | 无 |
| 02 | A03、A04、A08 | [原生状态、地点索引与存档编码](02-state-location-and-codec.md) | 01 |
| 03 | A05、A06 | [AVX2 传播、增减公式与时间结算](03-diffusion-and-settlement.md) | 02 |
| 04 | A07、B04 | [玩法规则与引擎事实查询](04-gameplay-and-engine-facts.md) | 02 |
| 05 | B01、B02 | [时钟、定时协程与同步事务](05-scheduler-and-dispatcher.md) | 03 |
| 06 | B03、B05、C01 | [生命周期、SKSE 保存与依赖缓存](06-lifecycle-save-and-capabilities.md) | 02、04、05 |
| 07 | C02、B06 | [异步 VM 与旧版对外 API](07-vm-and-public-api.md) | 03、04、06 |
| 08 | C03、C04、C07 | [P+、DDNG 和其他模组状态](08-pplus-ddng-and-external-state.md) | 04、05、06、07 |
| 09 | C05、C06 | [RaceMenu 与 SlaveTats 原生读取](09-racemenu-and-slavetats.md) | 04、06 |
| 10 | B07、C08 | [配置、JSON 读写与 SKSE 菜单](10-configuration-json-and-menu.md) | 02、03、06、07 |
| 11 | C09、C10 | [发布资产与游戏验收](11-packaging-and-acceptance.md) | 01、02、03、04、05、06、07、08、09、10 |

[manifest.json](manifest.json)记录 25 项任务和 37 个 CPP 的唯一归属。[执行说明](00-execution-guide.md)介绍工作及报告流程，[测试说明](00-native-testing.md)列出现有命令和适用范围。

01/02 的补做与修复、03 的当前实现和检查结果分别保存在各组交付报告；[03 整链实施报告](../../../evidence/2026-10-28-03-chain-implementation.md)记录最新源码、回归及真实 Advance 对照。
