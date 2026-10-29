# 06 组：生命周期、SKSE 保存与依赖缓存

任务：B03、B05、C01。前置组：02、04、05。

先读[项目规格](../../../../.scratch/native-migration/spec.md)、[执行说明](00-execution-guide.md)和[测试说明](00-native-testing.md)。具体接口及验收见[B 任务说明](../2026-10-27-runtime-and-compat-plan.md)、[C 任务说明](../2026-10-27-integrations-and-delivery-plan.md)中本组任务。

## 负责文件

- M01：`src/main.cpp`
- M02：`src/runtime/Lifecycle.cpp`
- M08：`src/engine/EventBridge.cpp`
- M23：`src/persistence/Serialization.cpp`
- M24：`src/integrations/CapabilityRegistry.cpp`

同时维护对应头文件、测试及必要构建项；模块归属以 [manifest.json](manifest.json) 为准。

## 实施顺序

1. 建立统一装配和基础指令集引导，注册提供方监听及生命周期业务开关。
2. 按[优化实施方案](../../../../.scratch/native-migration/optimization-plan.md)接入 02 组 Capture/Restore、pending 解码/绑定/激活和 SKSE 保存，Capture 后在锁外编码，锁定提供方能力及版本。
3. 按[配置与菜单设计](../../../../.scratch/native-migration/configuration-design.md)接启动/新游戏/每次读档的重读请求；使用 A02 配置接口和可控 adapter 验证当前配置参与新会话绑定，由 10 组接实际 JSON 实现。

## 通过标准

- 引导代码应在 CPU/OS 检查通过后进入 AVX2 实现；生命周期应在完整绑定后开放业务开关，加载失败时保持关闭。
- A→B 应分别恢复各档的声望、注册和区域；新会话应采用当前跨档 JSON 的设置及用户连接偏好。
- 保存/加载应按稳定身份绑定，缺失引用应进入 unresolved 集合；监听应先于一次性广播注册，延迟出现的能力应完成对应查询。
- 报告应分别记录核心检查和目标运行时场景的输入、执行结果及环境。

运行相关核心/边界检查；数值或生命周期模块运行对应 ASan 检查，解析器使用现有坏输入/模糊测试。随机输入记录 seed（默认 1170）及可重放用例，日志保存实际命令和退出码。

## 交付记录

报告写入 `docs/evidence/task-groups/06-lifecycle-save-and-capabilities.md`，记录实现、接口、验证结果和下组接入事项。报告应列明实际游戏/提供方版本、各项执行结果及待执行项目。
