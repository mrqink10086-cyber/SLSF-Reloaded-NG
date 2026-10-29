# 05 组：时钟、定时协程与同步事务

任务：B01、B02。前置组：03。

先读[项目规格](../../../../.scratch/native-migration/spec.md)、[执行说明](00-execution-guide.md)和[测试说明](00-native-testing.md)。具体接口及验收见[B 任务说明](../2026-10-27-runtime-and-compat-plan.md)中本组任务。

## 负责文件

- M03：`src/runtime/Dispatcher.cpp`
- M04：`src/runtime/ClockService.cpp`
- M05：`src/core/time/Scheduler.cpp`

同时维护对应头文件、测试及必要构建项；模块归属以 [manifest.json](manifest.json) 为准。

## 实施顺序

1. 以假时钟实现三种时间各自的 vector 期限堆、任务槽、取消代次和统一执行器；按[优化实施方案](../../../../.scratch/native-migration/optimization-plan.md)合并同目标 dirty 请求。
2. 用离线 EXE 验证主线程唤醒与协程等待，再接短锁事务、02 组公开查询快照、同步命令发布和 epoch 检查。

## 通过标准

- 调度器应按各钟源判断到期和暂停，同期限任务应按稳定入队顺序执行；到期集合应与假时钟参考一致。
- 取消及会话换代应使对应任务失效；执行器应校验 epoch，并各释放一次等待资源及协程帧。
- 同目标 dirty 请求应合并，实际业务事件应逐个消费；同步命令应在返回前提交并发布查询结果。
- 离线 EXE 应使用真实 Dispatcher 和模拟主线程队列，在长等待期间持续执行心跳；覆盖菜单、取消、切会话和停止唤醒。

运行相关核心/边界检查；数值或生命周期模块运行对应 ASan 检查，解析器使用现有坏输入/模糊测试。随机输入记录 seed（默认 1170）及可重放用例，日志保存实际命令和退出码。

## 交付记录

报告写入 `docs/evidence/task-groups/05-scheduler-and-dispatcher.md`，记录实现、接口、验证结果和下组接入事项。报告应列明实际游戏/提供方版本、各项执行结果及待执行项目。

当前 B01/B02 及本轮修复已完成，见[05 修复与离线探针](../../../evidence/2026-10-29-05-fixes-and-offline-probe.md)。B03 接实际 SKSE 唤醒入口及会话装配。
