# 02 组：原生状态、地点索引与存档编码

任务：A03、A04、A08。前置组：01。

先读[项目规格](../../../../.scratch/native-migration/spec.md)、[执行说明](00-execution-guide.md)和[测试说明](00-native-testing.md)。具体接口及验收见[A 任务说明](../2026-10-27-foundation-and-core-plan.md)中本组任务。

当前：本组数据基础及审核提出的六项修复已完成，见[审核与修复记录](../../../evidence/2026-10-28-01-02-supplement-review.md)。所属地区解析采用迭代记忆化并通过正反 20000 节点长链及长环检查，计时见[性能记录](../../../evidence/2026-10-28-region-owner-and-parallel-tests.md)。03/04 组接入现有 RegionId 声望读写、类别汇总、公开查询快照和 Capture/Restore；具体要求按[优化实施方案](../../../../.scratch/native-migration/optimization-plan.md)。

## 负责文件

- M09：`src/core/location/LocationIndex.cpp`
- M10：`src/core/location/RegionRegistry.cpp`
- M12：`src/core/reputation/ReputationState.cpp`
- M13：`src/core/reputation/ProviderRegistry.cpp`
- M22：`src/core/persistence/StateCodec.cpp`

同时维护对应头文件、测试及必要构建项；模块归属以 [manifest.json](manifest.json) 为准。

## 实施顺序

1. 运行已有状态、provider、归属及 v1 codec 回归，接入 01 组新的共享约定。
2. A04 复核数字 canonical owner、父先子后的全局优先级归并、规范化 alias 表和稳定键二分；保留公开索引规则及 O(1) ResolveRegion。owner 求解采用连续索引、活动路径判环及整段路径结果复用，每个节点最多进入求解路径一次。
3. A03 数值命令直接接收 RegionId，调用侧通过 WorldModel 解析地点所属地区；多个地点共享同一份地区声望。重新绑定按稳定身份迁移，暂缺地区保留完整记录。
4. 分别保存可变声望、公开查询快照和完整 Capture。声望类别 mask、模组启用类别 mask/activeCounts、显式 flags 各自维护，同次更新完成。单项写入只重算目标地区，通过变化位更新类别计数；批量操作重建汇总。
5. A08 接 Capture/Restore；按[配置与菜单设计](../../../../.scratch/native-migration/configuration-design.md)把本档业务与跨档设置分开，明确 v1 settings/用户 graphEdits 的版本处理。恢复时重建 owner/alias、类别汇总等运行期数据，当前配置参与绑定。

## 通过标准

- 显式命令应在返回前提交；随后查询应读到该值或更新值。未知→恢复、两提供者撤销、四类候选及 codec 的既有回归应继续通过。
- 归属绑定应沿用全局 owner 优先级、嵌套 owner、alias 规范化及 customOrder 索引规则；用深链、跨树、重排和查找边界检查结果。
- 同一 owner 的多个地点应访问同一地区声望数据；独立区域应使用各自地区声望数据；重绑定应按稳定区域身份迁移数值。
- Set/Clear/Wipe 应在同次提交中更新类别汇总；一个模组停用某类后，其余模组对该类的启用继续生效。汇总应等于各地区声望和各模组启用类别的直接计算结果。
- 每次发布应以同一 revision 关联区域映射、数值及已应用配置版本；读者已取得的快照应在持有期间保留其发布内容。
- Capture→Encode→Decode→Bind/Restore 应恢复声望、flags、保护、时间及模组注册，包括暂缺地区和零类别注册；根据恢复后的数据及当前配置重建类别汇总。
- 加载应采用当前跨档 JSON 的设置和用户连接偏好，并按 codec 版本规则处理 v1 settings/用户 graphEdits 字段。

具体字段和通过标准统一见优化实施方案的“02 组”。比较实际绑定、查询和发布入口在默认/扩展规模下的成本。

运行相关核心/边界检查；数值或生命周期模块运行对应 ASan 检查，解析器使用现有坏输入/模糊测试。随机输入记录 seed（默认 1170）及可重放用例，日志保存实际命令和退出码。

## 交付记录

报告写入 `docs/evidence/task-groups/02-state-location-and-codec.md`，追加本次数据基础补做、验证结果和 03/04 组接入事项，保留原交付和 readiness 修复证据。报告应列明实际游戏/提供方版本、各项执行结果及待执行项目。
