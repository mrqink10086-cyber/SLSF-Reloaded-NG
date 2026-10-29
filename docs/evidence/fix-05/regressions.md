# 05 公开入口回归

以下修复前结果记录于本轮红绿验证；修复后由最终完整套件复跑。用例统一标签 `[fix05]`，共 9 cases。

| 公开入口 | 修复前结果 | 修复后要求及结果 |
|---|---|---|
| 同刻 ScheduleChange(Set10)、ScheduleChange(Set20) | 最终值 10 | 按 FIFO 得到 20 |
| WakeTick(day1) 后 Execute(Set20)，再 Advance(day1) | 值 18 | 先结算旧时间，最终 20 |
| 已排队旧 Set10，当前游戏钟 day2，再 Execute(Set20) | 旧命令尚有 1 项、后续值 8 | 到期旧命令先结算，再同步写入 20 |
| 同批首 timer Detach/Attach，随后旧 raw/dirty timer | 旧任务执行 2 次，新 dirty 被消费 | 旧任务执行 0 次，新 dirty 保持 1 项 |
| 取消 1000 个远期任务 | HeapSize=1000 | 触发压缩后 HeapSize≤16 |
| 1000 个远期任务后 NewSession | HeapSize=1000，累计清理 0 | HeapSize=0，累计清理 1000 |

此外覆盖 HasDue 的失效根清理及活项顺序、capture 析构调用 PendingCount/Attached、时钟 supplier 调用 Real/替换来源/析构重入，以及公开 Query 对正常密集行和重排 `regionIds={7,2}` 的查询值 22。

源码：[DispatcherTests.cpp](../../../tests/core/DispatcherTests.cpp:782)、[SchedulerTests.cpp](../../../tests/core/SchedulerTests.cpp:354)、[ClockServiceTests.cpp](../../../tests/core/ClockServiceTests.cpp:38)。

最终普通/ASan 核心各 219 cases、129271 assertions，其中包含上述回归；完整输出见 [all.log](all.log)。
