# 第 09 组交接报告：RaceMenu 与 SlaveTats 原生读取

更新：2026-11-01。范围：C05、C06。当前修复与验证见[09 修复报告](../2026-11-01-09-fixes.md)；原始交付以 `5529ed98332ade1f8167f9cc2dac210ec0c393e5` 中的本文件为准，参见[历史审核](../2026-11-01-09-adversarial-audit.md)。

## 已交付

| 层 | 文件 | 行为 |
| --- | --- | --- |
| 可移植适配器 | `include/slsf/integrations/RaceMenu.h`、`src/integrations/RaceMenu.cpp` | 交换结果、Overlay 能力与四部位容量、epoch/dirty/gate |
| 可移植适配器 | `include/slsf/integrations/SlaveTats.h`、`src/integrations/SlaveTats.cpp` | v1 广播校验、JC 临时句柄、四部位自有记录、槽位事实 |
| 引擎端口 | `include/slsf/boundary/AppearanceProviders.h`、`src/boundary/AppearanceProviders.cpp` | SKSE 消息、JC 反射桥与字符串寿命、类型化 Actor 解析 |
| 原生查询 | `src/boundary/ProviderCollectors.cpp`、`Scripts/Source/SLSF_Reloaded_ProbeNative.psc` | `CollectRaceMenu()`、`CollectSlaveTats(Actor)` 的 latent String 查询 |
| 生命周期 | `src/main.cpp` | PostLoad 订阅、DataLoaded 初始化；沿用 epoch、VM handle 和 SLCQ 恢复机制 |
| 离线验证 | `tests/core/AppearanceStateTests.cpp`、`tests/probe/AppearanceProbe.cpp` | 13 个测试用例、可控端口及离线 probe；目标插件验收见待办 |

## C05 RaceMenu

生产端口向 `skee` 分发 `0x9E3779B9`，payload 为单一 `IInterfaceMap*` 的交换结构。核心查询 `Overlay`，检查版本至少为 2，按已核对的 vtable 入口读取 Normal overlay 的 Body、Hand、Feet、Face 容量。对应 slot 为 QueryInterface 0、GetVersion 1、GetOverlayCount 11；依据是本地 `RaceMenu/skee64/IPluginInterface.h` 及实现，目标二进制 ABI 仍待验证。

旧 epoch 请求只返回 Stale，不改变当前 gate 或缓存。有效 epoch 的缓存读取可继续 Ready。生产查询在业务使用时显式 MarkDirty 后刷新；装备/外观事件到下游业务模型的接线仍待统一装配。

## C06 SlaveTats 与 JC

PostLoad 注册 `SlaveTatsNG` 监听，早于提供方在 PostPostLoad 的一次性广播。消息 type 1、dataLen 8，data 直接指向版本 1 地址表；检查 15 指针共同前缀，调用 slot 14 的批量查询。参数为 `RE::Actor*`，输出顺序 Body、Face、Hands、Feet；false 表示成功，合法空数组表示没有应用记录。

`JContainers64` 根接口广播只缓存指针，不在广播回调内查询接口。DataLoaded 获取版本 1 reflection/domain 接口，并绑定七个 JC 函数。`JcTable` 是核心自有端口接缝；生产 thunk 对 `mapGetStr`、`mapGetInt`、`retain` 使用真实 `RE::BSFixedString` 签名。键和返回字符串由端口持有，核心复制成自有字符串后清理返回值暂存。四个新建临时数组先 retain，再由 `JcHandle` 在本次采集结束时 release；离线 fake 的计数不能证明外部引用计数行为。

每个部位最多保留 256 项；空 entry、负 slot、超过 128 字节的名称计入拒绝，空名称本身未被拒绝。记录按 slot 稳定排序。256/128 限制输出，不能推导为完整扫描时间或临时分配上限。存在截断或拒绝时，未观察到的槽位为 Unknown；保留记录明确观察到的槽位为 Known(true)。完整数据才可将未出现槽位设为 Known(false)。subcategory 保留 Unknown，交由原有配置和业务模型决定。

`OnSessionChanged` 清除会话快照并重采；进程级地址表、JC 绑定和 interned keys 跨会话保留。临时 `JcHandle` 属于一次 Collect 的局部作用域。Reset 保留同会话 epoch；换会话使用 `OnSessionChanged`。maxAttempts 是 Failed 状态阈值，调用频率与停止重试由业务边界控制。

## 查询与生命周期

两个原生入口经现有 SKSE task 路径串行执行；排队、执行前检查 `IsEpochCurrent`，包含业务开关。Actor 用 VM handle 固定并重新解析；正常执行时 Actor 无效返回明确 Unknown。取消、stale、异常及存档恢复沿用类型匹配的空返回值合同，调用方须将其视为无可用事实。

`SLSFRM1` v1 返回 epoch、availability、interface 与四部位 capacity。`SLSFST1` v1 返回 epoch、availability、Actor 稳定身份、interface，以及各 area 的 rejected/truncated 和逐项 slot/name；名称按 wire 规则转义。非 Ready 结果不会把旧快照发布成 Available。

引擎端口及其字符串暂存要求游戏线程串行调用。SLCQ 只保存旧 stack ID/返回类型，载入后以空值结束旧查询，不重放采集。实际 AddTask/VM 线程、存读档交界及目标插件行为仍待游戏验收。

## 验证与交接

修复后普通与 ASan core 各 379 cases / 149669 assertions；boundary 各 4 / 22；differential 11 / 19605；fuzz 100000 次。完整 runner、单独 DLL 构建和 Papyrus 声明编译通过，命令及归档见[修复报告](../2026-11-01-09-fixes.md)。原始交付记录的 379 / 149650 是历史结果；13 个本组 TEST_CASE 不应按 RAII 内部作用域扩算为 19 个。

`SLSFAppearanceProbe.exe` 只验证 fake 端口，结果 `OFFLINE_PASS` 与 providers-pending 不代表真实查询。它不能通过安装插件自动变成游戏内 probe。

待统一装配：B03 生产泵、C02 VM 宿主、装备/外观事件、Visibility/计分消费者，以及 Unknown→恢复时保留注册和既有声望的业务验收。待目标运行时验证：Steam 1.6.1170 的 SKEE 实际容量、SlaveTatsNG one-shot 广播、JC 名称/slot 读取、四部位空/非空数据及 VM 存读档。须由目标进程加载本 DLL、调用原生查询并记录结果；当前没有这些实机查询证据。
