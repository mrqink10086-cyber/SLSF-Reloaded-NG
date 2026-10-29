# 07 组：异步 VM 与旧版对外 API

任务：C02、B06。前置组：03、04、06。

先读[项目规格](../../../../.scratch/native-migration/spec.md)、[执行说明](00-execution-guide.md)和[测试说明](00-native-testing.md)。具体接口及验收见[C 任务说明](../2026-10-27-integrations-and-delivery-plan.md)、[B 任务说明](../2026-10-27-runtime-and-compat-plan.md)中本组任务。

## 负责文件

- M25：`src/integrations/PapyrusCalls.cpp`
- M31：`src/api/PublicApi.cpp`
- M32：`src/compat/PapyrusBindings.cpp`
- M33：`src/compat/ModEventBridge.cpp`
- M34：`src/compat/LegacyOutputs.cpp`

同时维护对应头文件、测试及必要构建项；模块归属以 [manifest.json](manifest.json) 为准。

## 实施顺序

1. 先冻结旧声明编译的客户端 PEX，逐字段补齐 Global/StorageUtil/原始 SLSF 桥接输出合同。
2. 实现异步 VM、公开 facade、typed 事件及按 revision 发布的数据输出。

## 通过标准

- VM 应支持实例/Global 目标；成功、超时及取消应各完成一次请求，并按相关 epoch 处理结果。
- 同步数值命令应在返回前提交，随后查询应读到提交值；52 个输入及 7 类回应的名称、类型和参数顺序应符合合同。
- Global、StorageUtil 和原始 SLSF 桥接输出已提交版本；用旧声明编译的冻结 PEX 按原记录和实例绑定运行。

运行相关核心/边界检查；数值或生命周期模块运行对应 ASan 检查，解析器使用现有坏输入/模糊测试。随机输入记录 seed（默认 1170）及可重放用例，日志保存实际命令和退出码。

## 交付记录

报告写入 `docs/evidence/task-groups/07-vm-and-public-api.md`，记录实现、接口、验证结果和下组接入事项。报告应列明实际游戏/提供方版本、各项执行结果及待执行项目。
