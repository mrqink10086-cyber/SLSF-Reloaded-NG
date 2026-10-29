# 类别启用、清理与传播：原脚本规则及本次决定

调研日期：2026-10-26。对象：`E:/source/repos/SLSF Reloaded/Scripts/Source`，并对照本地 CommonLibSSE-NG。本文保留类别、清理和原传播的源码依据，当前需求见[项目规格](../../.scratch/native-migration/spec.md)。

自动传播已选固定图、逐跳减弱和多来源取强，见[传播设计](../superpowers/specs/2026-10-26-weighted-reputation-diffusion-design.md)。第 3 节说明原随机加值算法，便于对照其行为差异。

## 1. 本次已明确的方向

- 夜间/前戏减免改为 AND：仅当夜间减免已启用、当前处于夜间、当前为前戏时，将增长倍率减半；单独夜间或单独前戏不减半。夜间区间仍按配置判断。
- 转整数时截断，自动增长保持最小增量 1、上限 150。
- 身体部位/设备使用固定槽位到业务语义的映射作为主要入口。槽位内的设备类型、覆盖规则及必要的 Keyword 细化仍需对照旧判定，不进行几何遮挡模拟；用户已决定移除 AND 模组集成，采用其余非 AND 路径。
- NPC 先选出候选，再按需查询关系；不建立全世界 NPC 的关系预计算表。筛选范围与提交规则应覆盖旧玩法需要的目击者。
- 四类可选清理沿用下述条件，原传播按第 3 节与新模型对照。

## 2. 四类可选声望的清理规则

`SLSF_Reloaded_ModEventListener.psc:996-1006` 的 EnableFameType 接受下表四类。

| 类别 | 内置提供者判定 | 外部启用类别设置 |
|---|---|---|
| Cum Dump | `Mods.IsFHUInstalled` | `EnableCumDump` |
| Cuck | `Mods.IsFameCommentsInstalled` | `EnableCuck` |
| Unfaithful | `Mods.IsFameCommentsInstalled` | `EnableUnfaithful` |
| Airhead | `Mods.IsBimbosInstalled` | `EnableAirhead` |

`DataManager.SetExternalFlags` 按已注册 Mod 保存各自类别设置，`CheckFlags` 对每个类别查找是否存在 true；因此外部类别设置是 OR 聚合。某个 Mod 发送 false 是撤销自己的类别设置，其他 Mod 的 true 仍然有效；也不否决内置判定。普通的 25 类增长 flags 与上表的四类启用 flags 是不同用途。

`FameManager.UpdateFame:198-224` 在“内置提供者不存在 AND 外部启用类别设置为 false AND 本次失效阶段尚未清理”时，调用相应 Wipe 函数，将该类别在全部默认和已注册自定义地点的声望设为 0，并设置 Cleared。提供者或外部启用类别设置恢复后，把 Cleared 重置为 false，为下次失效准备。

**每个失效阶段清理一次。** 后续入口仍可写入并查询该类别。`GetFameValue:446-548` 直接读存储；`UpdateGlobals:2726-2782` 直接读当前地点数值；Set 和传播同样按各自数值规则执行。临时 NotReady/Stale 保留类别设置的核心修复见[记录](../evidence/2026-10-27-provider-readiness-fix.md)。

自动增长分别检查各自条件：Cuck / Unfaithful 的 FameGainRoll 分支要求安装 FameComments，Airhead 分支要求安装 Bimbos（FameManager:1168-1224）。外部启用类别决定保留该类声望，自动增长仍检查这些来源条件；外部直接写入按原 API 合同执行。

### 明确的实现错误

`DataManager.psc:1975-1985` 在存在 EnableCumDump 设置时写入 `DisplayFameFlags[1] = True`，随后 Cuck 分支又重写 `[1]`。NG 将 Cum Dump 正确映射到 `[0]`，实现该类经外部模组启用的原约定。

`ModEventListener.psc:719-763` 的手动传播在旧 PEX 中先执行整数 `PercentToSpread / 100`，导致 10–99% 的请求没有正常增长。NG 按 `min(150, int(目标旧值 + 来源旧值 × (double(percent)/100.0)))` 计算，最后截断；例如来源 100、目标 20、30% 得到 50。保留来源至少 10、百分比 clamp 到 10–100 的边界。

## 3. 原传播算法

### 3.1 每个来源地点的传播机会

`FameManager.SpreadFameRoll:1530-1601` 按默认地点、再按自定义地点的顺序遍历，只处理缓存标记为“有可传播声望”的地点。设该地点当前传播概率为 P：

- P = 0：本次不传播，增加 FailedSpreadIncrease。
- P = 100：直接调用 SpreadFame，减去 SuccessfulSpreadReduction。
- 0 < P < 100：从 1–100 均匀抽样；命中则调用 SpreadFame 并降低 P，否则提高 P。
- P 超出 0–100：本次将 P 重置为 30。
- 调整后将 P 夹在 0–100。

成功分支指概率判定成功，不保证目标数值真的发生增长；即使 SpreadFame 因无候选返回，或目标已经封顶，调用方仍执行成功减概率。

### 3.2 候选类别与目标

`SpreadFame:1603-1626` 遍历 25 类，来源数值 >= MinimumFameToSpread 就进入类别候选，**不检查 DisplayFameFlags 或上表的提供者**。候选为零时结束。

开启 FameSpreadRestrictions 时，按代码定义的城镇联系和注册地区列表构建目标，未定义归属的来源使用全部地点。关闭限制时使用全部默认与已注册自定义地点；随机选出一个来源以外的目标，原实现通过反复抽样排除来源。证据：FameManager:1633-2452。

类别数量通常从 `1..min(候选类别数, MaximumSpreadCategories)` 随机选择；最大类别数 <= 1 时仍取 1。旧实现重复抽取并查重，最多尝试 50 次，可能未选满。自动传播现已采用固定图模型；仍沿用旧抽样的计分入口保留尝试次数、候选顺序和不足额结果。证据：FameManager:2456-2608。

### 3.3 数值变化

`CalculateFameSpread:2634-2652` 在正常配置范围内，以 10% 为步长随机选择比例，最高档为 `int(MaximumSpreadPercentage) / 10`；最大档 <= 1 时仍使用 10%。各类别单独抽取传播比例。

```text
目标新值 = min(150, int(目标旧值 + 来源数值 × 抽取比例))
来源数值保持不变
```

例如来源 100、目标 20、抽到 30%，目标变为 50，来源保持 100。

### 3.4 顺序依赖与保护期

原实现即时写回。后续已有传播资格的来源可能读到本轮刚传入的值，资格本身又依赖 HasSpreadableFame 缓存，因此存在遍历顺序影响。当前模型已选择冻结步初快照、统一提交，中转从下一步发生。

传播结束意图给目标设置衰减保护，但 `FameManager:2618-2620` 把候选列表下标写进默认地点保护数组，自定义目标也走这条路径，存在串位错误。当前 L/R 模型按地点记录本地保护，被动传播只更新 R。

## 4. 固定槽位映射的源码依据

旧 `VisibilityManager.CheckBondage:793-797` 已使用：32 身体、45 项圈、49 贞操带、56 贞操胸罩、58 harness。可把这些映射整理为数字规则和位集合，装备变化后生成当前状态快照。

旧代码允许 58 槽装备带有 DD_Belt Keyword（849-856）。先按槽位定位候选，再通过 Keyword 等字段区分 belt、harness 等实际类型并缓存。身体覆盖、项圈配置、轻/重束缚使用各自规则和缓存；旧 AND 部位暴露数据仅作历史记录。

## 5. NPC 关系按需查询

本地 CommonLib 的 `include/RE/B/BGSRelationship.h:62-67` 提供 GetRelationship，记录枚举为 Lover 0 至 Archnemesis 8；旧 Papyrus 使用 -4 至 +4 rank。适配通过下面的运行时条件查询取得旧 rank 语义。

同库 `include/RE/T/TESCondition.h:514` 定义引擎条件 GetRelationshipRank，`TESCondition` 提供原生求值入口。关系适配任务通过该条件取得当前 rank，并在游戏内核对关系变化后的结果。

旧 `NPCScan.psc:52-77` 的业务分组为 rank < 0 敌人、= 0 中立、1–3 朋友、>= 4 恋人，并分别读取配置概率。适合先筛已加载附近候选，再按需查询关系/感知；保持一次检测最多一个成功提交的旧规则，不为此遍历整个世界或维护巨大的关系缓存。
