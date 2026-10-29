# 原生迁移调研结论与来源导航

调研日期：2026-10-26，整理：2026-10-27。本页汇总本地源码和发布包发现。项目范围、已选行为和进度统一见[项目规格](../../.scratch/native-migration/spec.md)。

## 公开兼容与原生核心

原 PSC、PEX 和 ESP 可提取公开函数、事件、类别、地点索引及实例绑定。薄 Papyrus facade 保留公开调用，C++ 管理业务和状态；原生 Global/StorageUtil/JSON 数据输出继续服务外部消费者。

对应来源：

- [Papyrus 源码审计](papyrus-source-audit.md)：17 个 PSC 的函数、事件、属性及业务调用。
- [公开合同证据](../evidence/legacy-contracts.md)：PSC/PEX/ESP、86 个公开函数、52 输入、7 回应、25 类和资产身份。
- [类别与传播](category-and-spread-semantics.md)：原增减、四类清理、外部类别设置和随机传播。
- [时间与轮询](time-and-polling-semantics.md)：计时单位、Decay→Spread→UpdateFame 批次、等待与共享状态。

当前已有显式状态、注册、地点和 codec 纯核心，见[02 组证据](../evidence/task-groups/02-state-location-and-codec.md)；游戏接入在后续任务完成。

## 事件传输

本地 SKSE 2.2.6 源码区分两种通道：

- `E:/source/repos/skse64/skse64/PapyrusForm.cpp:274`：`Form.SendModEvent` 广播 `SKSEModCallbackEvent`。
- `E:/source/repos/skse64/skse64/PapyrusModEvent.cpp:255`：typed `ModEvent.Send` 遍历 Papyrus 注册表，附近第 123 行以 `QueueEvent` 投递。

因此 SLSF typed 协议保留必要 PSC 传输。P+ 2.18.1.0 的 AnimationStart/LeadInEnd/AnimationEnd 同时提供普通通知，可由原生接收，thread ID 位于 strArg。参与者和交互仍从框架公开查询取得。

## 集成事实与采用路线

| 提供方 | 本地发现 | 当前接入方式/待证据 |
|---|---|---|
| P+ 2.18.1.0 | 普通通知及公开 VM 场景查询；未确认公开 C++ 数据 ABI | 普通通知 + 异步公开 VM 事实；验证阶段就绪时序 |
| DDNG | GetAPI v2 提供设备数据；IsDevice/ActorHasBlockingGag 包装固定 false | 采用有效查询，锁定实际安装版并 probe |
| RaceMenu | 接口交换、外观查询和部分回调；本地运行时标识与目标版本不同 | 验证 Steam 1.6.1170 安装版 ABI |
| SlaveTats NG | v1 地址表及应用记录查询，添加/移除通知代码为注释 | 启动接广播，JC 所有权明确后按四部位批量复制；dirty + 使用边界刷新 |
| Devious Curses | 展示普通/typed 事件和设备适配 | 作为接口参考，业务仍按 SLSF 需求实现 |

详细定位见[P+ 审计](sexlab-pplus-2.18.1-integration-audit.md)、[RaceMenu/DDNG/SlaveTats 审计](racemenu-ddng-native-api-audit.md)和[Devious Curses 审计](devious-curses-integration-audit.md)。外部事实未知的处理及换档边界见[生命周期设计](../superpowers/specs/2026-10-26-integration-state-lifecycle-design.md)。

## 算法与工程资料

旧版已有固定默认传播候选，结果为随机加值；当前选择固定图、逐跳衰减和多来源取强。L/R 表示本地来源与传入影响，详见[传播模型](../superpowers/specs/2026-10-26-weighted-reputation-diffusion-design.md)。原增长、衰减和清理作为玩法基线，具体映射在项目规格统一维护。

数字地点、森林区间、连续行、CSR、AVX2、事件合并和受管理协程是执行手段。来源定位及候选比较见[原生检测与调度评估](native-detection-and-scheduling-options.md)；有效版本、实际 probe 和测试结果落入 evidence。

[算法优化研究](native-algorithm-optimization.md)继续比较时间、地点、快照发布和扩散内核，包含实际源码的成本分析、硬件与编译器依据、可复跑探针及实施顺序。
