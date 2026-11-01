# 第 09 组独立审核与对抗验证

日期：2026-11-01。审核基线 `5062105aeec71df206c491d241892e847a4cfd60`，固定 HEAD `5529ed98332ade1f8167f9cc2dac210ec0c393e5`。对比命令：`git diff 5062105aeec71df206c491d241892e847a4cfd60...5529ed98332ade1f8167f9cc2dac210ec0c393e5`；区间只有 `5529ed9 feat: implement group 09 racemenu and slavetats native reading`。

本次按用户要求只读审核实现；新增审核文档、复现程序和测试归档，没有修改生产代码、原测试或原交接报告，没有提交 Git。源码、上游源码和可重放反例优先于交接报告。三个独立子代理均使用 `gpt-6-luna / xhigh`，分别审核 Spec、算法、Standards/内存/栈/线程；主代理统一构建、运行测试、反证并裁决。三名子代理已完成审计稿与逐项表的反证复核，提出的 RAII 作用域计数和构建证据范围修正已纳入。

本文件记录上述固定 HEAD 的历史裁决；后续生产端口、两项逻辑修复及新验证见[09 修复报告](2026-11-01-09-fixes.md)。原交接报告可用 `git show 5529ed98332ade1f8167f9cc2dac210ec0c393e5:docs/evidence/task-groups/09-racemenu-and-slavetats.md` 查阅，本文件行号不指向修订后的交接报告。

## 结论

确认两项 P2 模块逻辑缺陷：旧 epoch 请求污染 RaceMenu 的共享 gate；SlaveTats 对不完整采集结果生成已知不存在的槽位事实。没有证明当前正常生产路径发生内存破坏、UAF、栈溢出或数据竞争；生产 port 和游戏接线未交付，也不能据离线测试宣称真实提供方安全或验收完成。

C05/C06 的真实目标查询、一次性广播接收、dirty/业务边界接线及 Unknown→恢复业务验收仍未闭合。原报告已披露部分 pending，但其“均可交付”和“安装后直接运行 probe”不能作为这些要求已满足的证据。逐项裁决见[报告核验表](audits/2026-11-01-group09/claim-matrix.md)。

## Spec

### G09-S1 / P2：旧 epoch 请求使有效缓存永久返回 Stale

[RaceMenu.cpp:75](</E:/source/repos/SLSF Reloaded NG/src/integrations/RaceMenu.cpp:75>) 对不匹配的 epoch 执行 `gate_ = ExternalGate::Stale`。随后有效 epoch 请求命中 [RaceMenu.cpp:83](</E:/source/repos/SLSF Reloaded NG/src/integrations/RaceMenu.cpp:83>) 的 Available/非 dirty 缓存分支，直接 `return {gate_, false};`，不恢复 Ready。快照虽然未被旧请求覆盖，共享可用性却已被污染。

最小序列：`OnSessionChanged(E1) → Ensure(E1) 成功 → Ensure(E2) 被拒绝 → Ensure(E1)`。最后一次仍返回 Stale；继续使用同一有效缓存也不恢复，直到 dirty/session/reset 等另一路径改变状态。依赖 gate 的新计分会被持续阻断。旧请求应只得到局部拒绝结果，或有效缓存返回时应恢复本会话正确 gate；不能由旧请求破坏有效会话的就绪状态。

主代理独立反例输出：

```text
race_stale first_ready=1 stale=1 valid_ready=0 gate_stale=1 exchanges=1
```

这是实际模块执行的反例，未声称游戏内复现。现有生命周期测试检查了旧请求后的快照，但未覆盖“旧请求后再读有效缓存”的 gate 恢复。原报告第 26 行“旧 epoch 不改快照”字面成立，却不足以证明旧请求无副作用。

### G09-S2 / P2：被拒绝或截断的纹身变成 Known(false)

[SlaveTatsSlotFacts:83](</E:/source/repos/SLSF Reloaded NG/src/integrations/SlaveTats.cpp:83>) 将全部 capacity 槽初始化为 `Fact::Known(false)`，仅将保留下来的 placement 改为 true，完全忽略 `rejectedEntries` 和 `truncated`。而 [ReadAreaLocked:253](</E:/source/repos/SLSF Reloaded NG/src/integrations/SlaveTats.cpp:253>) 会因名字超过 128 字节或保留数超过 256 丢弃实际存在的记录。

两类反例均经过真实 Collect 和投影：其一，257 条应用记录的前 256 条都在 slot 0，第 257 条在 slot 1，capacity=2；其二，单条应用记录在 slot 1，名字长 129 字节。Collect 均返回 Ready，但 slot 1 被投影为已知不存在。

```text
tats_trunc ready=1 placements=256 rejected=1 truncated=1 actual_slot1=1 slot1_known_false=1
tats_reject ready=1 rejected=1 actual_slot1=1 slot1_known_false=1
```

任务书要求未知时暂停依赖事实的新计分、保留设置和声望；该投影把信息缺失消成了确定的否定事实。应使未确认槽保持 Unknown，或另外保留被省略记录的槽位存在信息；保留下来的正向记录仍可确认 true。现有测试分别测完整数据投影和截断标志，没有组合验证。当前没有业务消费者接线，实际玩法影响待集成验证。

### G09-S3 / 验收未完成：实际提供方与本组行为尚未闭合

[09 任务书第 18–25 行](</E:/source/repos/SLSF Reloaded NG/docs/superpowers/plans/2026-10-27-task-books/09-racemenu-and-slavetats.md:18>)要求目标 Steam 1.6.1170 公共接口最小查询、一次性广播、dirty/业务使用边界刷新，以及 Unknown 后保留设置和既有声望、恢复计分。[C05/C06](</E:/source/repos/SLSF Reloaded NG/docs/superpowers/plans/2026-10-27-integrations-and-delivery-plan.md:35>)明确实际 ABI 和广播时序。

当前实现交付的是可注入端口的核心适配器；没有生产 RaceMenuExchangePort、SlaveTatsPort、真实 JC 字符串转换桥或向 SlaveTats 消费器转发广播的接线。MarkDirty/OnSessionChanged、gate 与 DTO 接口存在，但真实装备/外观事件、容量/纹身业务消费及恢复行为未接。`SLSFAppearanceProbe` 只执行同形 fake，打印 pending 检查清单，没有真实 dispatch、模块载入或提供方切换路径；安装插件本身不能将该 EXE 变成真实查询 probe。

原报告第 93 行披露目标缺提供方，故不是隐瞒目标运行结果。本次没有安装插件、启动游戏或执行真实提供方查询，也未重新确认安装目录现状。任务书自身要求的验收应标未完成；一般 B03 pump、C02 VM host 和下游评分装配仍保持既有跨组边界，不重复当作新算法缺陷。未发现本组有需要另报的范围扩张。

### G09-S4 / P2 交接错误：广播之后注册会漏掉唯一消息

原报告[第 106 行](</E:/source/repos/SLSF Reloaded NG/docs/evidence/task-groups/09-racemenu-and-slavetats.md:106>)要求“kPostPostLoad 阶段后……注册 SlaveTats 监听器”。上游 [SlaveTatsNG main.cpp:90](</E:/source/repos/SlaveTatsNG-master/src/main.cpp:90>) 在 PostPostLoad 中只广播一次 type 1。项目要求是 PostLoad 订阅、PostPostLoad 接收；阶段结束后再订阅会错过消息。应在 PostLoad 阶段完成针对 SlaveTats 的订阅，保证它在 PostPostLoad 广播前已生效。

当前主程序有通用 SKSE listener，但 [PostPostLoad 分支](</E:/source/repos/SLSF Reloaded NG/src/main.cpp:474>)只 EnsureAll，没有实例化这两个适配器或转发 SlaveTats 消息；通用 listener 的存在不等于已订阅提供方。这是确定的交接顺序错误与未完成接线，未声称当前游戏已经因此挂死。

## Standards

### G09-T1 / P3：probe 用途导读违反仓库注释规则

[AppearanceProbe.cpp:1](</E:/source/repos/SLSF Reloaded NG/tests/probe/AppearanceProbe.cpp:1>)的文件用途/离线阶段介绍属于 [AGENTS.md:11](</E:/source/repos/SLSF Reloaded NG/AGENTS.md:11>)明确禁止的文件职责导读；打印 pending 清单前的普通位置说明也可直接由代码表达。原报告第 17 行把这些统称为必要例外不成立。真正的外部 ABI 参数序/槽位约束与来源说明应分别按必要例外核验。本次只读，实际删除注释 0 条。

### G09-T2 / 启发式：未使用的继承

[SlaveTats.cpp:135](</E:/source/repos/SLSF Reloaded NG/src/integrations/SlaveTats.cpp:135>)仍继承 `std::enable_shared_from_this<Impl>`，没有使用相应能力；属于可删除的无用继承/可能的 Speculative Generality，非安全缺陷或硬性标准违规。原报告第 115 行的删除声明明确针对 M28，不能用 M29 的这个继承反证该句；本项单独作为代码判断。其余 Fowler 类别未发现值得新增的具体问题。

## 算法、内存、堆栈与线程核查

| 范围 | 源码结论与实际边界 |
| --- | --- |
| RaceMenu ABI | 本地上游支持 map slot 0 QueryInterface、Overlay 主基 slot 1 GetVersion、slot 11 GetOverlayCount、Normal=0、location=0..3；源级用法相符。函数非空和版本检查不能验证任意指针或目标安装二进制 ABI。 |
| SlaveTats 前缀与批量接口 | v1 15 指针共同前缀、slot 14、参数 body/face/hands/feet、false=成功与本地上游相符；复制到四区域自有 vector，稳定排序。核心 ResolveActor 返回 void*，生产桥必须保证实际 RE::Actor*，不能只保证 TESObjectREFR*。 |
| JC 临时资源 | Collect 局部四个 move-only JcHandle，正常/失败返回按作用域释放，输出不保留 entry 指针。正常创建前要求 release 等绑定存在。Owns 只查 handle/table 非空，公开直接用不完整 table 构造不代表能力已校验。 |
| JC 外部所有权 | fake 的 creates=4/releases=4/retains=0 只证 fake 调用次数。JC 新对象有约 10 秒无 owner 宽限，同步读取不因未 retain 就错误；现有一手文档未证明对零 owner 新对象调用 release 的目标版本行为，不能宣称已证明真实引用平衡，也不能据此断言泄漏。 |
| 字符串 ABI | JcTable/JcString 是内部 seam，非已交付的真实 BSFixedString 桥。上游 mapGetStr 按值传/返回非平凡 RE::BSFixedString；MS x64 不能按相同大小就视为 trivial JcString ABI 相同。未来 port 可用正确 thunk 转换并管理 fixed-string 引用，因此撤回“当前确定错调真实 ABI/必然崩溃”的初步判断。 |
| 复杂度与堆 | RaceMenu 四次容量查询为固定规模。SlaveTats 仍遍历每区原始 count；每区保留 K≤256，时间 O(ΣN + Σ名字复制字节 + 4K log K)，另有上游批量复制成本。每个名字先完整 Text 复制再检查 128 字节。256/128 是输出限制，不是外部数组、临时字符串、CPU 或整体堆内存硬上限。 |
| 栈 | 所查路径无输入控制递归，四句柄及固定小数组使用固定栈空间，记录与字符串在堆上；未发现可复现栈溢出。并未测真实提供方内部栈。 |
| mutex/线程 | 两适配器在私有 mutex 内执行端口、vtable、JC 查询和读取。慢查询会阻塞 Cancel/OnSessionChanged；上游已查同步查询未发现反调同一适配器，因此不报正常路径重入死锁。设计第 103 行是 Session 事务锁上下文，不能直接判私有 adapter mutex 违反硬合同。 |
| 并发测试强度 | 原并发测试为一线程广播、主线程顺序 Collect；alternate table 缺有效 batch。没有覆盖并发有效外采集、慢查询中的取消或重入、析构竞争。ASan 与该测试通过均不能证明任意线程安全。未发现当前合法路径的确证 data race/UAF。 |
| 第二次 Table() | 首次校验后经 ResolveActor/Intern 再取 Table，第二次未判空；未找到上游静态接口在该同步窗口变空的合法路径，且生产 port 不存在。只保留端口稳定性约束，未列确定空指针缺陷。 |
| 异常/信任边界 | 虚表及广播地址依赖同进程受信提供方，非沙箱指针校验器。外部可抛行为、引擎对象寿命和返回字符串所有权须由真实 port 明确；没有来源证明的异常或恶意指针场景不升级为当前缺陷。 |

字符串 ABI 的依据为[Microsoft x64 调用约定](https://learn.microsoft.com/en-us/cpp/build/x64-calling-convention?view=msvc-170)、本地 [BSFixedString.h:23](</E:/source/repos/SLSF Reloaded NG/lib/commonlibsse-ng/include/RE/B/BSFixedString.h:23>)与 [jcontainers_wrapper.h:42](</E:/source/repos/SlaveTatsNG-master/include/jcontainers_wrapper.h:42>)。JC 生命周期依据为[JContainers 官方对象生命周期规则](https://github.com/SilverIce/JContainers/blob/develop/JC.md#object-lifetime-management-rules)，没有将未核对的目标版本行为写成已验证。

## 条件合同与未升级项

1. 两个 Reset 保留 epochSeen/epoch；`OnSessionChanged(E1) → Reset → Ensure/Collect(E2)` 返回 Stale。若 Reset 只重启同一会话且换会话必须 OnSessionChanged，则行为可接受；没有生产调用者可证明违反这个前置条件，列调用合同待明确。
2. maxAttempts 是 Failed 状态阈值，没有请求短路。反例 maxAttempts=2 后调用 5 次，RaceMenu 实际交换 5 次、两者 attempts=5。下游若要求严格有界刷新须在 Failed 后门控，或模块明确实现上限；当前不把模糊的阈值命名升级为无条件缺陷。
3. 大 count 反例每区 100000，累计 arrayGetObj=400000，仍 Ready；它证明扫描没有 256 上限，没有证明正常提供方会制造此规模，也不构成实际性能基准。
4. 空 entry 是 arrayGetObj 返回 0，不等于空字符串名称。原报告第 33 行只写空 entry，不能反证成其声称拒绝空名称。实际空名称会保留；没有明确需求证明必须拒绝空名称，因此未报缺陷。

## 独立验证与可重放证据

主代理重新构建相关普通、ASan 测试和 probe 目标，并运行 `tools/run-native-tests.ps1 -Mode all -Jobs 4 -Seed 1170`。本次归档验证范围为 runner 所列的测试、ASan 和 probe 目标。wrapper 成功退出；[fresh-native-all.log](audits/2026-11-01-group09/fresh-native-all.log)只包含 wrapper 元数据，没有捕获子进程完整 stdout。下列数字来自这次运行生成的聚合 JUnit 与测试清单，未混用残留 shard 文件：

| 目标 | 用例 | 断言 | failures / errors |
| --- | ---: | ---: | ---: |
| Core | 379 | 149650 | 0 / 0 |
| Core ASan | 379 | 149650 | 0 / 0 |
| Boundary | 4 | 22 | 0 / 0 |
| Boundary ASan | 4 | 22 | 0 / 0 |
| Differential | 11 | 19605 | 0 / 0 |

完整 runner 还执行 100000 次 StateCodec fuzz、预期正/负 ASan probe、dispatcher 离线 probe 与 AppearanceProbe，成功退出。fuzz 临时 stdout 未保留，因此原报告的精确 cov/corpus 和耗时不作为本次复测可复核结论。AppearanceProbe 输出 OFFLINE_PASS/providers-pending，仅支持 fake 路径。

[测试归档](audits/2026-11-01-group09/test-results/appearance-probe.txt)、[SHA256 与聚合统计](audits/2026-11-01-group09/test-artifact-manifest.json)、[归档脚本](audits/2026-11-01-group09/archive-results.ps1)保存聚合 XML/清单及 text；脚本只复制、解析既有结果，不重跑测试。

反例源码 [counterprobe.cpp](audits/2026-11-01-group09/counterprobe.cpp)复用 fake fixture，但链接固定 HEAD 的真实 SLSFCore；[replay.cmd](audits/2026-11-01-group09/replay.cmd)保存编译/执行命令，[counterprobe-results.log](audits/2026-11-01-group09/counterprobe-results.log)保存完整输出。它验证模块状态机和 DTO 投影，未执行真实 RaceMenu/JC、引擎 Actor 或游戏业务。

轴内统计：Spec 4 项（2 项 P2 代码缺陷、1 项验收缺口、1 项 P2 交接时序错误），最重代码问题为 P2；Standards 1 项 P3 硬性注释违规、1 项可选继承启发式，最重硬性问题为 P3。两轴独立计数，条件合同和未验证风险未计作确定缺陷。
