# 08 组：P+、DDNG 和其他模组状态

任务：C03、C04、C07。前置组：04、05、06、07。

先读[项目规格](../../../../.scratch/native-migration/spec.md)、[执行说明](00-execution-guide.md)和[测试说明](00-native-testing.md)。具体接口及验收见[C 任务说明](../2026-10-27-integrations-and-delivery-plan.md)中本组任务。

注释默认不写，包括文档注释；严格执行 [AGENTS.md 的必要例外](../../../../AGENTS.md#code-comments)。不要复述代码或批量补说明，拿不准就不写。

## 负责文件

- M26：`src/integrations/SexLabPPlus.cpp`
- M27：`src/integrations/DeviousDevices.cpp`
- M30：`src/integrations/ExternalMods.cpp`

同时维护对应头文件、测试及必要构建项；模块归属以 [manifest.json](manifest.json) 为准。

## 实施顺序

1. 接入 P+ 普通通知、公开 VM 事实与前戏阶段 readiness，保留原计分宽限。
2. 实现 DDNG 及其他外部事实快照，将缺失/未知交给业务规则。

## 通过标准

- P+ 分类应使用实际角色、交互及对应作用域；阶段切换应完成 readiness 检查，计分应保留原宽限。
- DDNG 设备快照应反映阻塞设备及装备变化；未知→恢复应保留模组注册和类别设置，并按事实就绪情况恢复新计分。
- 异步完成应校验会话/场景 epoch，取消应结束对应等待；实际采用的接口及版本应有目标环境 probe 结果。
- 本次新增或修改的注释已逐条检查，仅保留 AGENTS.md 允许的必要例外；零新增注释可通过验收。

运行相关核心/边界检查；数值或生命周期模块运行对应 ASan 检查，解析器使用现有坏输入/模糊测试。随机输入记录 seed（默认 1170）及可重放用例，日志保存实际命令和退出码。

## 交付记录

报告写入 `docs/evidence/task-groups/08-pplus-ddng-and-external-state.md`，记录实现、接口、验证结果和下组接入事项。报告应列明实际游戏/提供方版本、各项执行结果及待执行项目。
