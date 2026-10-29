# 04 组：玩法规则与引擎事实查询

任务：A07、B04。前置组：02。

先读[项目规格](../../../../.scratch/native-migration/spec.md)、[执行说明](00-execution-guide.md)和[测试说明](00-native-testing.md)。具体接口及验收见[A 任务说明](../2026-10-27-foundation-and-core-plan.md)、[B 任务说明](../2026-10-27-runtime-and-compat-plan.md)中本组任务。

## 负责文件

- M06：`src/engine/GameData.cpp`
- M07：`src/engine/ActorQueries.cpp`
- M17：`src/core/gameplay/Anonymity.cpp`
- M18：`src/core/gameplay/Visibility.cpp`
- M19：`src/core/gameplay/Witness.cpp`
- M20：`src/core/gameplay/AnimationClassifier.cpp`
- M21：`src/core/gameplay/Comments.cpp`

同时维护对应头文件、测试及必要构建项；模块归属以 [manifest.json](manifest.json) 为准。

## 实施顺序

1. 从原源码为匿名、可见性、目击、分类和评论整理可观察场景，接入自有 DTO。
2. 按[优化实施方案](../../../../.scratch/native-migration/optimization-plan.md)组合入口各自的候选 mask；自动目击保留原候选顺序、随机调用、selectedMask 去重及 50 次尝试。
3. 实现合法引擎上下文采集，在 1.6.1170 probe 关系等级、感知和 LOS，按目标合并 dirty 事实刷新。

## 通过标准

- 匿名、可见性、目击、分类和评论应满足原玩法典型及临界场景；候选集合应与直接逐类判定一致。
- 自动目击应保留原候选顺序、随机调用次数和 50 次尝试规则；固定随机序列应得到对应的计分类别及不足额结果。
- dirty 刷新应更新事实缓存；业务事件及原计分周期应驱动计分。部分未知应暂停依赖该字段的新计分。
- 目标运行时 probe 应记录关系编码、查询上下文及 unknown 返回条件，并用相同 DTO 驱动核心规则。

运行相关核心/边界检查；数值或生命周期模块运行对应 ASan 检查，解析器使用现有坏输入/模糊测试。随机输入记录 seed（默认 1170）及可重放用例，日志保存实际命令和退出码。

## 交付记录

报告写入 `docs/evidence/task-groups/04-gameplay-and-engine-facts.md`，记录实现、接口、验证结果和下组接入事项。报告应列明实际游戏/提供方版本、各项执行结果及待执行项目。
