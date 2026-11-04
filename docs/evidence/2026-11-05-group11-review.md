# 第 11 组固定提交审核：DLL 与模拟验收

日期：2026-11-05（Asia/Shanghai）。审核提交为 `b5f47eb9ec70729ca24c32e7d0e7a194fee09c18`，基线为 `85acee963eb3fcf1f510ae93e944a419400af72a`；比较命令为 `git diff 85acee963eb3fcf1f510ae93e944a419400af72a...b5f47eb9ec70729ca24c32e7d0e7a194fee09c18`。本次修改仅为验收文档、报告和探针证据，没有修复产品源码。

**当前未通过 DLL 验收。** 核心检查通过，但新增 VM 层不能编译，且注册、回应、活动配置与导入存在确定缺口。结论来自源码、当前 DLL 构建和 C++ 模拟；不以缺少真实游戏或其他 MOD 联测为失败依据。

## 验收范围与 Git 状态

用户已将验收范围调整为 DLL 功能及模拟探针，并要求审核第 11 组新增、修改的代码。脚本、ESP、其他 MOD 和真实游戏执行不作为完成前提；边界仍检查参数、结果、失败日志及取消语义。

审核开始时 `main` 工作树干净。基线之后只有上述一个提交，标题为 `group11: vm native binding layer (135 fns), staging pipeline, frozen probe diagnostics`；实际差异为 93 文件、26,391 行新增、447 行删除，包含配置、JSON 和菜单实现，因而本次也检查这些实际改动。

Git 同时记录了 `tools/__pycache__/verify-papyrus-frozen.cpython-313.pyc`、`tools/gen-papyrus-bindings.py.bak` 及 `.scratch/` 中 PEX/PAS/反编译参考。它们不是功能完成的证据；建议后续清理生成缓存和备份，并区分发布资产与研究材料。本次保留全部文件。

## 实际验证

| 检查 | 结果 | 证据及覆盖范围 |
| --- | --- | --- |
| `run-native-tests.ps1 -Mode all -Seed 1170 -Jobs 8` | 退出 0 | 普通、ASan 核心各 414 用例、150,051 断言；边界各 4 用例、22 断言；差分 11 用例、19,605 断言；离线 wake、appearance、menu probe 通过 |
| all 中的 StateCodecFuzz | 100,000 次通过 | `cov=878 ft=2441 corp=272/118Kb`；日志里的 ASan 越界负向探针是工具自检 |
| 单独 JsonTransferFuzz | 100,000 次，退出 0 | `cov=1560 ft=5818 corp=716/178Kb`；all 没有运行这个新增目标，因此另行执行 |
| `xmake build -y SLSFReloadedNG` | 退出 -1 | `VmNatives.cpp:106` 的 `C3668`，随后 `C2259`；当前 DLL 未构建成功 |
| 配置/导入行为探针 | 退出 0，复现缺陷 | 链接真实 `SLSFCore.lib`，使用 ConfigurationStore、Lifecycle、Dispatcher、PublicApi 与 JsonTransfer；没有执行 VM bridge |
| 数值转换探针 | 报 UBSan 错误 | 真实 JsonTransfer 对 `1e20` 转整数溢出；sanitizer 恢复模式退出 0 不代表通过 |

核心和 ASan 目标在 [xmake.lua:34](../../xmake.lua#L34)、[xmake.lua:128](../../xmake.lua#L128) 排除 `VmNatives.cpp`。因此前两行通过不能证明新增 native 注册/封送/回应层正确。核心用例比基线增加 35 个，也不等于验证了 135 个 native 的生产行为。

全部日志、探针源码及重放命令见 [2026-11-05-group11](audits/2026-11-05-group11/README.md)。fuzz 数字是此次运行观察值，复跑既有 corpus 后不保证完全相同。

## Standards

独立标准/安全轴报告 8 项。这里保留其严重度与顺序；不与 Spec 轴去重或重新排序。

### ST1 · P1 · DLL 构建阻断

[VmNatives.cpp:106](../../src/compat/VmNatives.cpp#L106) 用 `const LogEvent&` 覆盖 `ProcessEvent`；锁定 CommonLib [BSTEvent.h:188](../../lib/commonlibsse-ng/include/RE/B/BSTEvent.h#L188) 要求 `const Event*`。本次 DLL 构建复现 `error C3668` 与 `error C2259`，见 [plugin-build.log](audits/2026-11-05-group11/plugin-build.log)。应修正签名并对实际 DLL 目标构建。

### ST2 · P1 · ModEvent 回应调用合同错误

[VmNatives.cpp:129](../../src/compat/VmNatives.cpp#L129) 对 `ModEvent.Create` 结果调用 `GetObject()`，而实际 API 返回整数 handle。后续 [VmNatives.cpp:178](../../src/compat/VmNatives.cpp#L178)、[VmNatives.cpp:182](../../src/compat/VmNatives.cpp#L182) 又将 Push*/Send 当作实例方法，缺少首个 handle 参数。本地 SKSE `scripts/modified/ModEvent.psc:32–51` 声明它们为 global native。

触发任一需要回应的请求即可走此路径，7 个回应均受影响。整数被读成对象违反 API 合同；不能据静态分析声称已观察到游戏崩溃。应按整数 handle 顺序调用 global Create/Push*/Send，并用模拟 VM 校验实际调用和参数。

### ST3 · P1 · 对象类型缺少 VM 类元数据

[VmNatives.cpp:273](../../src/compat/VmNatives.cpp#L273)、[VmNatives.cpp:275](../../src/compat/VmNatives.cpp#L275) 对 Form/Actor/Quest/Alias/Script 及对象数组仅设置 `kObject`/`kObjectArray`。它们是原始类别标记，不能携带具体 `ObjectTypeInfo*`；CommonLib [TypeInfo.cpp:81](../../lib/commonlibsse-ng/src/RE/T/TypeInfo.cpp#L81) 及 `GetRawType<T>()` 路径表明注册类型还需绑定 VM 类信息。

结果无法准确描述生成 PSC 所声明的对象参数/返回类型。应使用实际类型解析和对象/数组封送；本次未执行 VM，未断言所有声明都必然崩溃。

### ST4 · P2 · 旧 JSON 文件名和内容不能往返

[VmNatives.cpp:637](../../src/compat/VmNatives.cpp#L637)、[VmNatives.cpp:650](../../src/compat/VmNatives.cpp#L650) 使用 `<FileName>.json`；原 Exporter/Importer 使用 `<FileName> SLSF Reloaded Data`，由 JsonUtil 加 `.json`。同样传入 `Alice` 无法定位旧导出的文件。

[VmNatives.cpp:616](../../src/compat/VmNatives.cpp#L616) 导出只写内置地点声望，没有完整保留原模组、flags、自定义地点身份及 spread；导入也没有完整应用这些数据。应以 `tests/fixtures/json/legacy/legacy_exporter.json` 及旧公开合同检查往返，见 Spec SP6 的事务缺陷。

### ST5 · P2 · staging 与安装配置路径矛盾

[build-staging.ps1:7](../../tools/build-staging.ps1#L7) 清空 staging，后续复制项从未生成 `SLSFReloadedNG.json`；[group11-game-run.ps1:38](../../tools/group11-game-run.ps1#L38) 却无条件复制这个文件，且错误策略为 Stop。按 staging→默认安装顺序会在启动前因源文件缺失退出。这是工具本身的确定分支，无需实际运行游戏。

根代理补充：若手工提供源 JSON，同一 `Copy-Item -Force` 会覆盖已安装用户 JSON，违反任务书的保留配置要求。应补默认配置并对安装目标采用保留策略。本次没有运行安装或删除工具。

### ST6 · P2 · JSON 数转整数越界

[JsonTransfer.cpp:635](../../src/io/JsonTransfer.cpp#L635) 未验证范围便 `static_cast<std::int64_t>(number_)`；[JsonTransfer.cpp:877](../../src/io/JsonTransfer.cpp#L877) 的夹紧发生在转换后。合法有限 JSON 数 `1e20` 可经 `modtotal` 或 flag 入口到达。

真实解析器探针输出：

```text
parsed_number=1e+20
integer=-9223372036854775808
import_success=1 mods=0
src\io\JsonTransfer.cpp:635:38: runtime error: 1e+20 is outside the range of representable values of type 'long long'
```

应先做可表示范围检查，再按导入合同拒绝或处理超界值。当前常规 ASan/fuzzer 通过没有覆盖此 float-cast 检查。

### ST7 · P2 · 注册失败被成功数量掩盖

[VmNatives.cpp:817](../../src/compat/VmNatives.cpp#L817) 将 `expected` 赋为实际 `registered`，而不是应注册总数。公共函数绑定失败后，只要后续 52 个事件绑定成功，[VmNatives.cpp:847](../../src/compat/VmNatives.cpp#L847) 仍可返回成功。应保留固定预期数并记录失败函数身份；注册成功还须区别 SP1 的业务空实现。

### ST8 · P3 · 新增注释违反默认零注释规则

独立检查 121 行新增/修改注释及 4 个 Python 文档字符串块；6 行符合工具指令/可核对来源例外，其余 115 行和 4 块不符合 [AGENTS.md:9](../../AGENTS.md#L9) 的必要例外。实际删除 0。

自有代码例子包括 [VmNatives.cpp:376](../../src/compat/VmNatives.cpp#L376) 的章节横幅、[VmNatives.h:57](../../include/slsf/compat/VmNatives.h#L57) 的 namespace 说明，以及 Python 工具的职责/章节说明；其中 81 行来自提交的 `.scratch/group08-probe` 反编译参考，应保持低严重度。可保留项包括 shebang、生成/再生指令、可核对合同来源，以及 `# noqa: E402` 指令本身。未发现需要另报的 Fowler 启发式问题。

## Spec

独立规格轴报告 7 项：缺失/部分实现 4 项、范围扩张 0 项、看似实现但错误 3 项。SP5 与 ST2 是两条轴各自的结论。

### SP1 · P1 · 135 项注册不等于已实现

冻结公开表有 86 项；排除 3 个 ModIntegration getter 后，注册层尝试 83 个公开函数和 52 个事件，共 135 项。`MakeImpl` 只覆盖 32 个公开 ID，其中 `CleanExternalModList`、`EnableLogging`、`SetLogLevel` 是明确空 body；另 51 个公开 ID 在 [VmNatives.cpp:802](../../src/compat/VmNatives.cpp#L802) 绑定为恒 false/0/None。

例如地点注册/归属、匿名与纹身查询、`UpdateFame`、`FameGainRoll`、`DecayFame` 没有对应业务处理。PSC 已改 native，因此不能继续依靠原脚本实现。违反项目规格“保留……查询含义”和原玩法要求。应逐项接真实业务，并用模拟调用验证效果；29 个非空分支的数量也不能证明其行为正确。

### SP2 · P1 · 活动配置没有提交给当前会话

[main.cpp:485](../../src/main.cpp#L485) 菜单操作只 Reload/ApplyEdit；[main.cpp:740](../../src/main.cpp#L740) 的唯一通知回调只日志。生命周期资产仍保存激活时的参数和图。违反 [configuration-design.md:46](../../.scratch/native-migration/configuration-design.md#L46) 的旧配置先结算、再一次提交替换规则及 revision 要求。

真实核心组件模拟输出：

```text
config_update_success=1 notifications=2 initial_revision=1 store_revision=2 status_revision=2 published_revision=1
```

`Lifecycle::Status()` 读取配置源而显示 2，不能证明 executor 已应用；发布状态仍为 1。探针通知只计数，对应生产通知仅日志，没有调用 VM 或伪装完整宿主执行。

### SP3 · P1 · 最终执行泵仍未装配（既有缺口）

[main.cpp:268](../../src/main.cpp#L268) 的 EventBridge 消费只 Post 空 body；生产树没有调用 `Dispatcher::Step/WakeTick`、`EventBridge::Flush` 或 `PublicApi::SetCurrentLocation`。规则模块存在不能替代实际调度。

这是前组已延期、最终装配仍未完成的项目缺口，**不是本提交新引入的回归**。缺少自动推进和地点刷新，无需以其他 MOD 实测推论。应提供可模拟的生产泵入口，验证时间、事件、地点和规则到状态提交的路径；原基础计划要求业务事件及原周期驱动计分。

### SP4 · P2 · 包清单交付不完整

[build-staging.ps1:15](../../tools/build-staging.ps1#L15) 到文件末尾只复制部分资产并打印计数，没有默认配置、许可、依赖和可复现 hash 清单。违反 [11 组任务书](../superpowers/plans/2026-10-27-task-books/11-packaging-and-acceptance.md) 的明确交付要求；该要求不依赖游戏或 PEX 测试。

固定审核提交也没有任务书指定的 11 组报告。本次补写[验收状态](task-groups/11-packaging-and-acceptance.md)，只关闭文档缺失，不代表包内容已经补齐。

### SP5 · P1 · 7 个回应的 ABI 接线错误

[VmNatives.cpp:128](../../src/compat/VmNatives.cpp#L128) 将 `ModEvent.Create` 的整数 handle 当对象，后续漏 handle 且调用方式不符。违反项目规格保持 7 个回应一致的要求；具体机制及修正方向见 ST2。

### SP6 · P1 · JSON 导入会部分提交

[main.cpp:590](../../src/main.cpp#L590) 和 [VmNatives.cpp:663](../../src/compat/VmNatives.cpp#L663) 逐条直接 `SetFameValue`。JSON 先包含有效地点、后包含未知地点时，报拒绝仍改变前面的声望；VM 路径还忽略逐项失败。违反 [design.md:153](../../.scratch/native-migration/design.md#L153) 的完整候选、一次提交，以及失败保留原状态要求。

行为探针镜像当前逐项循环，调用真实解析器和 PublicApi：

```text
import_before=20 applied=1 rejected=1 import_after=12
```

输入是 `{"int":{"whiterun.whore":12,"unknown.whore":7}}`。应完整准备并验证候选，再由 Session 一次提交，而不是靠前置 JSON 语法正确证明事务成功。

### SP7 · P2 · GainFame 丢弃 IsForeplay

冻结第三参是 `IsForeplay Bool`，但 [VmNatives.cpp:564](../../src/compat/VmNatives.cpp#L564) 的实现不读取 `args[2]`，`GainFameInputs.foreplay` 保持 Unknown；[GainRules.cpp:60](../../src/core/reputation/GainRules.cpp#L60) 只在 known true 时执行夜间前戏减半。违反基础计划明确的“夜间与前戏同时成立才减半”。应封送真实布尔输入并验证 true/false 的结果差异。

Category/Location 参数顺序和 ClearFame 类别遍历核对正常。`CalculateFameSpread` 与旧配置随机算法存在差异，但公开计算器与自动传播例外的合同归属未决，本次不列为确定缺陷。

## 条件观察与修正顺序

尚无证据将普通计数器列为当前数据竞争：已核对的生产读写没有确定并发路径，异步 VM callback 的实际线程语义仍需宿主合同。回应 callback 未携带 epoch；在修正当前 ABI 后，应通过跨会话模拟确认旧回应被取消。两项不加入上面的确定发现计数。

后续先恢复 DLL 构建，再为注册类型、参数/数组、真实业务分支及 7 回应建立模拟入口；接通活动配置和导入事务，完成最终执行泵；随后补包清单、保留配置、错误日志及 Git 生成物整理。脚本和其他 MOD 的实测无需增加为前置条件。

Standards：8 项，最高 P1；Spec：7 项，最高 P1，范围扩张 0 项。当前待修问题均可在 DLL/C++ 模拟范围内处理。
