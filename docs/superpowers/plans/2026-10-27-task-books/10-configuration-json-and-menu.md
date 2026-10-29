# 10 组：配置、JSON 读写与 SKSE 菜单

任务：B07、C08。前置组：02、03、06、07。

先读[项目规格](../../../../.scratch/native-migration/spec.md)、[执行说明](00-execution-guide.md)和[测试说明](00-native-testing.md)。具体接口及验收见[B 任务说明](../2026-10-27-runtime-and-compat-plan.md)、[C 任务说明](../2026-10-27-integrations-and-delivery-plan.md)中本组任务。

## 负责文件

- M35：`src/config/Configuration.cpp`
- M36：`src/io/JsonTransfer.cpp`
- M37：`src/ui/Menu.cpp`

同时维护对应头文件、测试及必要构建项；模块归属以 [manifest.json](manifest.json) 为准。

## 实施顺序

1. 按[配置与菜单设计](../../../../.scratch/native-migration/configuration-design.md)实现统一 Configuration：`Data/SKSE/Plugins/SLSFReloadedNG.json` 保存跨档设置/用户连接偏好，Reload/Update/Snapshot 和有类型通知共用编译结果。
2. 实现编辑完成保存后重读、菜单关闭合并、打开/刷新配置页及游戏重载读取；相同内容复用 revision，坏文件或写失败保留有效配置。接入 06 组生命周期请求。
3. 按[优化实施方案](../../../../.scratch/native-migration/optimization-plan.md)编译类别参数、关系 mask 和图参数，先结算旧参数，再同步应用变化及资格集合。
4. 锁定本地框架 3.8/API 1 的公共 header/真实导出并 probe；框架 open/close 与自有控件/窗口事件投递配置请求。逐帧公开查询快照渲染，声望操作走公开命令。
5. 冻结旧公开 JSON schema，按各自协议实现导入/导出；本档状态转移与跨档设置读取分别验证。

## 通过标准

- 旧公开文件及 NG 状态文件应按各自 schema 往返；无效导入应返回校验错误并保留当前已提交状态。
- 配置更新应在同一次提交中应用阈值、关系 mask、图及配置 revision。
- 菜单编辑完成/关闭、外改文件后的 UI 刷新及游戏重载应读取当前 JSON；A→B 和退出重进应共用设置，各档声望及注册应独立恢复。
- 同轮重复关闭应合并为一次请求；逐帧渲染应读取缓存，文件读写应由配置更新入口执行。
- 菜单应显示孤立节点、命令结果及写入错误；写失败时应继续使用最近一次有效配置。

运行相关核心/边界检查；数值或生命周期模块运行对应 ASan 检查，解析器使用现有坏输入/模糊测试。随机输入记录 seed（默认 1170）及可重放用例，日志保存实际命令和退出码。

## 交付记录

报告写入 `docs/evidence/task-groups/10-configuration-json-and-menu.md`，记录实现、接口、验证结果和下组接入事项。报告应列明实际游戏/提供方版本、各项执行结果及待执行项目。
