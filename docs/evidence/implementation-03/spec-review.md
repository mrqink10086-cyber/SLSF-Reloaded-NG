# 03 组 Spec 独立复核

- 基线 tree：`7a43e4e4b3a67b3ac5fa2f4d33990ac4e0101e70`
- 复核 tree：`28b4160cbba5afc84ce869d3f990648510db74fa`

## 结论

(a) 缺失或部分满足：**0**。
(b) 范围蔓延：**0**。
(c) 表面实现但行为错误：**0**。

## 核对依据

- 事件合同要求“到期命令应先结算到其业务时间，再应用命令并处理后续区间；迟到命令应在已消费时刻应用。同时间命令按入队顺序处理，失败时保留业务状态与原命令队列。”（`docs/superpowers/plans/2026-10-27-task-books/03-diffusion-and-settlement.md:28`）。实现按 `max(boundaryFloor, change.at.days)` 分段、稳定排序，并仅在成功后提交状态和删除到期命令（`src/core/reputation/Settlement.cpp:654-697`）；正常边界与迟到命令回归分别见 `tests/core/SettlementTests.cpp:294-316`、`:829-847`。
- 任务书要求：“连接按 canonical owner 投影后编译 CSR，同 owner 边删除，平行边取最大权重。绑定、连接、配置或自定义地区注册更新时替换准备结果，数值更新继续复用。”（`docs/superpowers/plans/2026-10-27-task-books/03-diffusion-and-settlement.md:20`）。优化方案还要求“准备结果拥有 owner 投影后的 CSR、类别倍率/档位数组和按 RegionId 排列的自定义地区标记”且 raw 入口“按源内容变化刷新”（`.scratch/native-migration/optimization-plan.md:89-90`）。实现比较 account keys、图、owner、custom keys 和配置值，并在图不变时复用 CSR（`src/core/reputation/Settlement.cpp:205-246`）；投影/合并逻辑见 `src/core/reputation/DiffusionGraph.cpp:287-315`。
- workspace 只暂存事务需要的 `AccountState` 行、派生值和计数；`unresolvedAccounts` 只在 ClearAll/Wipe 时复制，不复制 identity mappings 或 provider metadata（`src/core/reputation/Settlement.cpp:634-650`）。接口明示“calls using it must be serialized”（`include/slsf/core/Commands.h:79-80`），符合单执行器边界；失败路径不发布暂存结果或消费原队列（`src/core/reputation/Settlement.cpp:671-697`）。
- 指令基线要求“基础指令集引导代码先检查 CPU/OS 支持，再进入扩展指令代码”（`.scratch/native-migration/spec.md:64`）。CPUID/OSXSAVE/XCR0 检查与回退在 `src/core/reputation/DiffusionKernel.cpp:20-39,182-220`；AVX2 松弛在单独 TU `src/core/reputation/DiffusionKernelAvx2.cpp:55-85`。`xmake.lua:19-27` 保留 `/fp:precise`，AVX2 标志仅限数值及内核 TU。

本复核未运行构建或测试；读取新增回归源码及现有诊断记录。
