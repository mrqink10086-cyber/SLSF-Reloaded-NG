# 09 组：RaceMenu 与 SlaveTats 原生读取

任务：C05、C06。前置组：04、06。

先读[项目规格](../../../../.scratch/native-migration/spec.md)、[执行说明](00-execution-guide.md)和[测试说明](00-native-testing.md)。具体接口及验收见[C 任务说明](../2026-10-27-integrations-and-delivery-plan.md)中本组任务。

## 负责文件

- M28：`src/integrations/RaceMenu.cpp`
- M29：`src/integrations/SlaveTats.cpp`

同时维护对应头文件、测试及必要构建项；模块归属以 [manifest.json](manifest.json) 为准。

## 实施顺序

1. 先验证 Steam 1.6.1170 的 RaceMenu ABI 和 SlaveTats 广播安全前缀。
2. 按 JC 所有权复制四部位纹身，接入 dirty 与业务使用边界刷新。

## 通过标准

- 目标安装版应完成公共接口最小查询；SlaveTats 监听应接收一次性广播，并按已验证共同前缀读取能力。
- body/hands/feet/face 应复制为自有记录；JC 句柄应按协议 retain/release，换档时应释放本会话对象并重新采集。
- 未知状态应保留类别设置及既有声望，暂停依赖该事实的新计分；就绪后按正常计分入口继续处理。

运行相关核心/边界检查；数值或生命周期模块运行对应 ASan 检查，解析器使用现有坏输入/模糊测试。随机输入记录 seed（默认 1170）及可重放用例，日志保存实际命令和退出码。

## 交付记录

报告写入 `docs/evidence/task-groups/09-racemenu-and-slavetats.md`，记录实现、接口、验证结果和下组接入事项。报告应列明实际游戏/提供方版本、各项执行结果及待执行项目。
