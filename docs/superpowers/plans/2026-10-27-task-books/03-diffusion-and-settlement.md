# 03 组：AVX2 传播、增减公式与时间结算

任务：A05、A06。前置组：02。

先读[项目规格](../../../../.scratch/native-migration/spec.md)、[执行说明](00-execution-guide.md)和[测试说明](00-native-testing.md)。具体接口及验收见[A 任务说明](../2026-10-27-foundation-and-core-plan.md)中本组任务。

## 负责文件

- M11：`src/core/reputation/DiffusionGraph.cpp`
- M14：`src/core/reputation/GainRules.cpp`
- M15：`src/core/reputation/DiffusionKernel.cpp`
- M16：`src/core/reputation/Settlement.cpp`

同时维护对应头文件、测试及必要构建项；模块归属以 [manifest.json](manifest.json) 为准。

## 实施顺序

1. 接入 02 组地区声望数据/布局后，实现入边 CSR、标量传入计算，再按[优化实施方案](../../../../.scratch/native-migration/optimization-plan.md)接直接 L/R 读取、独立累加器和 AVX2 行内核。
2. 按旧增长/衰减基线接入 L/R 结算与兼容单跳命令，保留批量抽样含义，共同时间系数移出类别循环，记录有效默认参数。
3. 在现有逻辑中复用 `PrepareSettlement` 和 `SettlementWorkspace`；连接按 canonical owner 投影后编译 CSR，同 owner 边删除，平行边取最大权重。绑定、连接、配置或自定义地区注册更新时替换准备结果，数值更新继续复用。

## 通过标准

- 传播应逐跳应用权重及门槛，同类传入 25/20 应取 25；来源降低后应按剩余入边接替，兼容脉冲应执行一跳。
- 显式减值应反映到公开结果；结算应按保护边界推进，同一区间消费一次，本批传入值保留本批传播结果。
- 相同输入下，标量与 AVX2 的有效类别、补槽和公开结果应符合相同规则。
- 应检查 Release 内核的连续加载及独立累加器，并记录实际结算与发布成本。
- 到期命令应先结算到其业务时间，再应用命令并处理后续区间；迟到命令应在已消费时刻应用。同时间命令按入队顺序处理，失败时保留业务状态与原命令队列。
- 配置倍率/档位及自定义地区资格应在准备阶段编译，重复结算应复用 CSR 和数值工作缓冲；原配置入口与准备入口应得到相同业务值及随机调用序列。

运行相关核心/边界检查；数值或生命周期模块运行对应 ASan 检查，解析器使用现有坏输入/模糊测试。随机输入记录 seed（默认 1170）及可重放用例，日志保存实际命令和退出码。

## 交付记录

报告写入 `docs/evidence/task-groups/03-diffusion-and-settlement.md`，记录实现、接口、验证结果和下组接入事项。报告应列明实际游戏/提供方版本、各项执行结果及待执行项目。
