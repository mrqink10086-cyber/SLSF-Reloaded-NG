# 03 Standards Review

范围：固定 tree `7a43e4e4b3a67b3ac5fa2f4d33990ac4e0101e70` → `28b4160cbba5afc84ce869d3f990648510db74fa`；复核完整固定 diff，并增量检查新测试/reference 及注释；未运行构建或测试。

## 硬标准

未发现可确认的仓库规范违规。`PreparedSettlement` 以 `shared_ptr<const ...>` 发布并复制稳定键、图、配置和地区快照；工作区复用连续缓冲且明确限定串行执行（`include/slsf/core/Commands.h:75-80`、`src/core/reputation/Settlement.cpp:22-43,205-246`）。事务阶段暂存账号、汇总及必要时的 unresolved 队列，成功后才提交并移除到期命令；拒绝路径不写入 `State` 或原命令队列（`src/core/reputation/Settlement.cpp:634-697`）。AVX2 指令留在独立 TU，能力检测在 baseline TU（`src/core/reputation/DiffusionKernel.cpp:9-40`、`src/core/reputation/DiffusionKernelAvx2.cpp:1-6`）。规则编译为类型化数组，结算热路径读编译结果（`src/core/reputation/GainRules.cpp:115-145`、`src/core/reputation/Settlement.cpp:271-274`）。这符合设计中的“CSR、double 32 槽、AVX2 和内部缓冲留在这里”及“失败命令保持其目标状态”（`.scratch/native-migration/design.md:77,103`）。

## 判断建议（性能接线）

仓库内暂无生产调用点使用显式 prepared API。生产 raw 入口已通过真实 `Advance` 对照；默认 21×78 active 小步为 raw 2.324 µs、prepared 1.831 µs，准备成本 12.200 µs，且 checksum 一致（`docs/evidence/implementation-03/advance-probe.md:5,12,19,27`）。显式 prepared 仍待 B02 会话接入。研究建议“由不可变准备结果持有 CSR 及其身份……内部连续调用使用该结果”，设计也要求“图重编译……在准备阶段完成”（`docs/research/native-chain-preparation-and-layout.md:72`、`.scratch/native-migration/design.md:103`）。这是接线与摊销建议，不列作硬违规。

复杂度与 API：`Advance` 承担校验、边界结算及提交，符合单一业务执行器设计；本轮 helper 和重载围绕准备数据/工作区，未见需要以 Fowler baseline 判定为违规的拆分。Fowler 项仅作可能线索，不构成额外强制规范。

## 本轮增量

生产非注释代码未变。新 `DiffusionReference` 按命令业务时刻分段，以独立标量行遍历执行衰减与一跳；新增 Set/Clear 差分用例比较结果和时钟，新 settlement 用例覆盖已消费时间后的迟到命令（`tests/reference/DiffusionReference.cpp:156-248,252-338`、`tests/core/RandomTests.cpp:748-835`、`tests/core/SettlementTests.cpp:829-848`）。未发现新增硬规范问题。参考实现与生产语义重复是差分 oracle 所需的对照结构；Fowler“Duplicated Code”在此仅为可能线索，不建议为消除表面重复而耦合参考算法与生产 helper。
