# Fix-05 独立 Spec review

固定比较：`4c72b3da398994ae3e1c0d4567545ee7c1713696` → `963ba4608f52c9dba83745b17ab75036e16ac57c`。审阅 `build/diagnostics/fix-05/review.diff` 及对应源码。

## (a) 需求缺失或部分

未发现缺失或部分交付。F1/F2 符合先结算再写及同刻 FIFO（`.scratch/native-migration/design.md:65,97`；`src/runtime/Dispatcher.cpp:348-373,388-403`）。F3/F4 符合会话代次复核与取消回收（`.scratch/native-migration/spec.md:65`、`.scratch/native-migration/optimization-plan.md:103`；`src/runtime/Dispatcher.cpp:541-634`、`src/core/time/Scheduler.cpp:107-131,244-265`）。F5 在锁外准备并以生命周期代次复核提交（`.scratch/native-migration/design.md:103`；`src/runtime/Dispatcher.cpp:234-275`）。F6 按更新后的离线 EXE 门槛交付：真实 Dispatcher 配模拟 AddTask 主线程队列，覆盖 3600 帧心跳、菜单时钟、长等待、取消、换会话及 Stop（`docs/superpowers/plans/2026-10-27-runtime-and-compat-plan.md:15-17`；`tests/probe/DispatcherOfflineProbe.cpp:26-56,67-150`；`xmake.lua:47-52`）。O1/O2/O3 分别保留逐项事件消费、清理死根并检查有效槽、以及校验 dense identity 后回退查找（`docs/evidence/2026-10-29-05-independent-review.md:74-78`；`src/runtime/Dispatcher.cpp:608-634`、`src/core/time/Scheduler.cpp:183-213`、`src/core/reputation/ReputationState.cpp:533-540`）。

## (b) 未要求的范围扩张

未发现。diff 限于授权的 `include/`、`src/`、`tests/`、`xmake.lua` 和 `tools/run-native-tests.ps1`；离线探针及运行器目标对应 F6 验收。

## (c) 看似实现但错误

未发现。C1：槽体回收先把空槽登记到 free list，再移出 body；NewSession 预留 retired 容量，捕获对象在 Scheduler 锁外释放，Dispatcher 的 Attach/Detach 也在外层锁释放后销毁 retired（`src/core/time/Scheduler.cpp:39-46,107-131,249-265`；`src/runtime/Dispatcher.cpp:255-275,278-315`）。RetireWait 在 Dispatcher 锁内取消的仅是内部 weak-pointer timer closure。C2：时钟 supplier 在 ClockService 锁外调用，替换的旧 supplier 也在锁外析构（`src/runtime/ClockService.cpp:30-80`）。

未运行测试；`build/diagnostics/fix-05/all.log` 仅作外部结果参考，不作为本审阅的验证记录。
