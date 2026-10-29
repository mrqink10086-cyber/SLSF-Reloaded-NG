# 独立算法审核：01–03

## 结论

- **确认，P1：** `Advance` 将到期 `ScheduledChange` 按 `now` 先写入，再结算从旧 `settledAt` 到 `now` 的整段本地衰减。它忽略了 `change.at` 的实际执行时点，导致命令效果被追溯衰减。该行为与设计中“先处理待结算时间，再提交命令”及 A06“切分必要的保护/业务边界”冲突。
- **确认，P2 性能问题：** `Advance` 在检查无工作返回前重编译整张 CSR 图。固定图的同一时刻 no-op 也承担完整编译成本；测得成本随边数明显增长。
- **确认，P2 扩展性风险：** 每个被结算的账户都线性扫描 `customRegions`。512 账户、512 个自定义地区的 Advance 测量为零自定义地区的 2.12 倍；算法复杂度为 O(N×C)。
- 其他探针没有发现已确认的 01–03 算法正确性问题。下文区分已排除的解释与超出已选支持域的观察；没有把未来 Session/运行时接线当作本层缺陷。

## 确认的问题

### A1 — 到期命令按 `now` 提前应用，旧时间被追溯到新值上

**触发条件：** `scheduledChanges` 中有 `change.at <= now`，且账户在此期间应发生本地衰减。`Advance` 将队列按 `at` 排序、仅用 `at` 判断是否到期；然后在 `context.clock = now` 下先执行所有到期命令，最后才用 `now.days` 结算账户。

**证据：** [`Settlement.cpp`](../../../src/core/reputation/Settlement.cpp:449) 的 `:449–507` 先应用命令，`:484–486` 将上下文时钟设为 `now`；`:518–525` 随后以 `now.days` 调用 `DecayAccount`。`change.at` 没有被用作结算边界。需求锚点为 [design.md](../../../.scratch/native-migration/design.md:65) `:65`、`:145`，以及 [foundation-and-core-plan.md](../../../docs/superpowers/plans/2026-10-27-foundation-and-core-plan.md:61) A06 `:65`；[spec.md](../../../.scratch/native-migration/spec.md:48) `:48` 要求先本地衰减再传播，`:52` 要求消费真实经过的游戏时间。

**反例探针：** 空图、单账户、`decayPerDay=2`、固定随机源返回最小值、初始本地值 0，排除图传播、随机差异和账户重映射。命令是 `Set=10`、`at=0.5`：

```text
scheduled.at=0.5 now=0.5 queued_fame=9 settle_then_apply_fame=10
scheduled.at=0.5 now=1 queued_fame=8 settle_then_apply_fame=9
```

同一时刻的命令，分段执行先结算至 0.5 再写入时结果为 10；队列路径先写 10 再把旧半天衰减上去，结果为 9。延迟推进到第 1 天时，队列路径把两个应计衰减周期都施加到新值，结果为 8；在 0.5 执行命令再推进到 1 的结果为 9。与其解释为“所有到期事件只在唤醒时刻生效”相比，`ScheduledChange.at` 明确进入排序和到期筛选，且 A06 要求按业务边界切分；因此该输入不只是一个无关的历史时间字段。

**影响与修正方向：** 延迟批处理改变命令后的本地声望，效果取决于 Advance 唤醒时间。应在每个到期业务边界先结算既有状态，再按同一时间戳的稳定顺序应用命令，最后结算至 `now`；明确一次 Advance 内传播消费边界的规则，保持单次提交。

### A2 — 固定图在每次 Advance、包括 no-op 时重编译

**证据：** [`Settlement.cpp`](../../../src/core/reputation/Settlement.cpp:441) `:441–447` 在 no-op 判断 `:474–476` 之前调用 `CompileDiffusionGraph`。[`DiffusionGraph.cpp`](../../../src/core/reputation/DiffusionGraph.cpp:105) `CompileDiffusionGraph` 对每条边用二分查找解析两端、排序 resolved edges，再构建 CSR（`:134–191`）。[`DiffusionKernel.cpp`](../../../src/core/reputation/DiffusionKernel.cpp:144) 的 `ComputeIncomingRows` 另会验证 CSR offsets 和每个 operand（`:157` 起）。

**测量：** 独立探针对每个规模重复调用实际生产 `CompileDiffusionGraph`、同一时刻 no-op `Advance` 和 `IsValidCompiledGraph`；Release 优化参数为 `-O2 -DNDEBUG`。单位为 ns/次：

| 账户数 | 有向边 | Compile | no-op Advance | 仅验证已编译 CSR |
|---:|---:|---:|---:|---:|
| 21 | 78 | 3,961.99 | 3,955.91 | 35.50 |
| 128 | 384 | 33,894.46 | 33,948.06 | 143.86 |
| 512 | 1,536 | 163,706.00 | 164,815.10 | 528.20 |

no-op Advance 几乎等于重编译本身；在 512/1,536 时，编译约为单次已编译图验证的 310 倍。规模输入用于展示增长，不作为推荐业务规模或硬性延迟门槛。图和账户绑定不变时，可在配置/绑定 revision 更新处编译一次并复用；只有图或账号键绑定变化时才重编译。若保留热路径验证，应测量是否需要对可信编译结果重复执行全图检查。

### A3 — 自定义地区判断为 O(N×C)

[`Settlement.cpp`](../../../src/core/reputation/Settlement.cpp:410) `IsCustomAccount` 在 `:415–416` 对每个账户从头 `std::any_of(state.customRegions)`；每次有时间消费的 Advance 都在账户循环 `:518–525` 调用它。对于 N 个账户、C 个自定义地区，最坏比较数为 N×C；稳定键匹配顺序带来约 C/2 的平均扫描。

**实际入口测量：** 512 个账户、空图、`decayPerDay=relaxationPerDay=0`，每轮推进一个新游戏日，40 次 Advance：

```text
custom.accounts=512 iterations=40 custom.none_Advance_ns=306387.50 custom.all_Advance_ns=648792.50 ratio=2.12
```

此项是整体 Advance 时间对比，其中还包括 `State` 暂存复制 512 个自定义记录的固定 O(C) 成本；不能把 2.12 倍全部归因给扫描。源代码已经确认每轮确有 N 次线性查找。若扩展配置允许较大的 C，可在绑定/恢复后建立按账户索引的布尔表或集合，并在自定义成员变化时更新。

## 已排除或仅属域边界的观察

| 状态 | 观察与证据 | 判断 |
|---|---|---|
| rejected | `Set(-42)` 后立即同时间 `Advance(0)` 仍为 `L=R=F=-42`；正向推进 1 天后为 `L=-42,R=0,F=0`。[`DiffusionKernel.cpp`](../../../src/core/reputation/DiffusionKernel.cpp:198) 将传入目标限定为非负；无边时 incoming 默认为 0，正时间结算将 R 推至该目标。它不是 Set 即时钳位；这是时间推进中的 R 目标演进，未作为缺陷报告。要求依据：[spec.md](../../../.scratch/native-migration/spec.md:45) `:45` 的完整 Int32 Set 和 `R=min(R,V)`。输出：`negative.after_set local=-42 remote=-42 fame=-42`、`negative.same_time_noop ...=-42`、`negative.after_advance local=-42 remote=0 fame=0`。|
| domain / pending | 配置 `maxFame=200` 的探针可先使目标 `R=200`；后续手动传播返回并写入 `L=150`，但因 `ApplyManualSpread` 只写 `local`（[`Settlement.cpp`](../../../src/core/reputation/Settlement.cpp:217) `:240–248`），公开 `F=max(L,R)` 仍是 200。默认 `maxFame=150` 的同探针返回并发布 150。`maxFame>150` 是否属于受支持配置没有在所选需求中确认，故不是确认缺陷；若产品允许此配置，需明确手动传播 cap 是否也必须压低既有 R。|
| domain | `ApplyGainFame` 对 `now=NaN`/`+∞` 均成功；探针显示 NaN 时保护期限为 0，+∞ 时保护期限为 `inf`。函数入口无 finite-time 检查，而 `Advance` 有检查（`Settlement.cpp:169–215` 对照 `:419–422`）。所选合同没有要求 `ApplyGainFame` 拒绝非有限时间，故作为域外输入观察，不报缺陷。|
| rejected / caller precondition | `ApplySpreadPulse` 是一跳低层操作，按来源过滤 CSR 后从冻结 L/R 快照计算 incoming（`Settlement.cpp:316–353`）；它没有随机源，不能独立替调用方执行衰减结算。spec `:49` 要求 Spread 入口“先结算实际经过时间，再…一跳脉冲”，03 证据报告也明确 pulse 必须由调用方先结算。因此直接以未结算状态调用不是本层缺陷。函数对收到候选的目标会推进 `remoteSettledAt`，即使没有 fame/revision 变化（`:378–392`）；符合前置结算时该时钟已在当前时点，不造成额外消费。不能据此要求尚未接入的运行时入口。|
| rejected | 02/03 重绑探针把稳定键 `localFormId=4` 的 fame 77 从旧索引迁移到新索引 2；随后按重绑后的 `accountKeys` 编译图，边 source=2、target=1，与新映射一致。`Advance` / `ApplySpreadPulse` 都按当前稳定键重新编译图；手动复用旧的 `CompiledDiffusionGraph` 属调用方持有过期索引，不作为当前实现缺陷。|
| no defect found | AVX2 机器上，四节点/四边图、25 个 live 槽和 7 个填充槽，对 scalar 与 AVX2 每个 double 做 bitwise 比较。输入含 NaN、正负无穷、正负零及脏填充槽，阈值 3.25；结果逐位相同。输出：`kernel.avx2=available special-values=nan,inf,signed-zero dirty-padding=passed`。这是定向边界覆盖，不是随机全域证明。|

## 其他算法与布局核对

- `NumericRow` 为 32 个 double、32-byte 对齐、256 bytes；25 个业务类别，7 个填充槽。AVX2 incoming 核心按四个 double 一组处理，之后将 slot 25–31 清零（`src/core/reputation/DiffusionKernelAvx2.cpp:20–51`）。CSR 按目标排序，Incoming 对每个目标汇总全部入边；权重乘在来源候选上后再按阈值筛除，候选取 max。脉冲构造过滤后的 CSR 并只从冻结输入快照计算一轮；没有级联写读。
- `RelaxRemoteRows` 的 `backend` 参数目前被 `(void)backend` 忽略，实际逐行、逐类别 scalar 推进（`src/core/reputation/DiffusionKernel.cpp:182–210`）。这是实现事实；标量/AVX2公开结果一致性已验证，但本审核没有单独基准量化这段的向量化收益，故不单列为正确性缺陷。
- `DecayAccount` 对每个有正本地值且未被保护覆盖的类别调用一次 `RuleDecayRoll`，随后乘以本次 owed periods（`Settlement.cpp:111–149`）；这与每个合格类别槽每个调用抽样一次、按整批周期计量的代码路径一致。此轮没有另加计数型 RNG 探针。
- 本轮没有发现 CSR 索引/稳定排序、每跳最大来源、旧来源下降后的替代入边、冻结一跳、R 衰退或保护 tick 计算的可复现新错误。它们也由现有 Mode all 基线覆盖；本结论不扩大成额外的运行时接线要求。

## 执行与产物

实际生产入口以独立探针编译，没有改动生产、现有测试或文档。编译器为 `clang version 20.1.0`，目标 `x86_64-pc-windows-msvc`，CPU `13th Gen Intel(R) Core(TM) i5-13600KF`；核心参数 `-std=c++23 -O2 -DNDEBUG -Iinclude -I.`，AVX2 translation unit 额外使用 `-mavx2`。

最终探针命令（PowerShell，`login=false`）及返回码：

```powershell
& 'C:/Program Files/Git/bin/bash.exe' --noprofile --norc 'E:/source/repos/SLSF Reloaded NG/build/diagnostics/review-01-03/algorithm-build-run.sh' 2>&1 | Tee-Object -FilePath 'E:/source/repos/SLSF Reloaded NG/build/diagnostics/review-01-03/algorithm-probes.log'; $code = $LASTEXITCODE; Add-Content -LiteralPath 'E:/source/repos/SLSF Reloaded NG/build/diagnostics/review-01-03/algorithm-probes.log' -Value "EXIT: $code"; exit $code
```

**退出码 0。** `algorithm-public-probes=PASS`，`algorithm-cost-probes=PASS`。一次早先的探针编译因同一作用域变量重名退出 1；改名后使用同一命令完整重编译并通过。Clang/CPU信息命令（`clang++.exe --version`、`Get-CimInstance Win32_Processor | Select-Object -First 1 -ExpandProperty Name`）各退出码 0。父任务另行报告的 Mode all 基线为 130 cases、约 21 秒、退出码 0，记录在 `build/diagnostics/review-01-03/existing-checks.log`；本审核没有重跑该套件。

探针/输出均隔离在 `build/diagnostics/review-01-03/algorithm-*`，本报告为本任务唯一非探针文件。
