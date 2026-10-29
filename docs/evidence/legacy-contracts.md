# 旧版公开接口清单（A01）

本文件记录 `contracts/legacy-*.json` 的提取规则与核对结果。来源为旧仓库
`E:/source/repos/SLSF Reloaded` 的 17 个 `Scripts/Source/*.psc`、18 个 `Scripts/*.pex`
以及 `SLSF Reloaded.esp`；FormID、默认值和 VMAD 绑定从这些来源提取。

## 输入与 SHA-256

- ESP：`SLSF Reloaded.esp`，23775 字节
- SHA-256：`8ebd5ab79740e85a2a8d553e47391bdfdecd82d278e30bcfcd239c28fb544812`
- 解析器按 TES4 头（276 字节头数据）、GRUP/record/subrecord 与 VMAD v5 读取；
  58 个记录全部取得真实 FormID/EDID。

## 计数

| 项目 | 数量 | 说明 |
|---|---:|---|
| PSC 文件 | 17 | MCM 只有 PEX，没有 PSC |
| 有效 Papyrus Function | 124 | 126 个文本命中减去 Logger 块注释中的 `DuplicateCheck`、实例 `Log` |
| 公开函数 | 86 | `legacy-api.json` 中标为 `public` 的兼容入口 |
| 内部函数 | 38 | 用于理解旧实现，新项目按已选设计实现其职责 |
| 输入 ModEvent | 52 | 注册顺序 1–52 |
| 回应 ModEvent | 7 | Push 顺序 1–7 |
| Fame 类别 | 25 | PSC 注释顺序与 ESP VMAD 顺序分别记录 |
| ESP 记录 | 58 | QUST 4、MGEF 1、SPEL 1、KYWD 10、GLOB 42 |
| 公开记录 | 57 | 1 个内部 SPEL 不计入公开身份；MCM QUST 仅保留识别身份 |

公开函数的判定规则：`public` 指旧版可供外部脚本、控制台或 MCM 调用的入口，
或公开事件处理器依赖的查询/状态函数；脚本内部启动、定时推进、数组维护和
计算辅助函数标为 `internal`。483 个属性和内部数组用于旧实现分析，兼容要求以公开入口和输出为准。

公开函数按脚本统计：VisibilityManager 18、FameManager 15、LocationManager 14、
DataManager 12、ModIntegration 9、DynamicAnonymity 7、Logger 3、Data_Exporter 2、
Data_Importer 2、SexAnimationAnalyzer 2、PlayerScript 1、Uninstall 1。
其中 Logger 的 3 个为 global native；`isLatent` 全为 false。

## 四份清单

- `contracts/legacy-api.json`：124 个函数逐项分类；public 项包含 `scriptType`、
  `isGlobal`/`isNative`/`isLatent`、参数顺序与默认值、返回类型、实例绑定
  `{kind, formId, editorId, attachment}`、`invalidInputResult` 和 PSC 源码行。
- `contracts/legacy-events.json`：52 个输入事件及 handler 参数、7 个回应事件及
  参数顺序、7 个查询到回应的映射、25 个类别（PSC 顺序与 ESP 顺序）与 21 个默认地点。
- `contracts/legacy-records.json`：58 个记录的插件名、FormID、EDID、签名、是否公开、
  记录头 flags、QUST 的 DNAM quest flags 及 VMAD 脚本列表。实例绑定直接提取自 VMAD。
  Main QUST 的 alias 0 也已解码：`PlayerAlias` 绑定
  `SLSF_Reloaded_SexAnimationAnalyzer`（14 个属性）和
  `SLSF_Reloaded_PlayerScript`（17 个属性）；API 的 4 个 ReferenceAlias 函数带
  `questFormId=0x06000808`、`aliasIndex=0`。
- `contracts/legacy-rules.json`：23 条规则，标为 `preserve`（14）、
  `approved-change`（6）、`internal-only`（3）；包含数值常量、地点解析优先级、
  传播公式、外部 flags 聚合和持久化/端口约束。

`tests/fixtures/contracts/legacy-cases.json` 是固定 oracle：保存 52 个输入事件、
7 个回应、25 个类别顺序、21 个地点、地点别名、58 个记录身份和 Main alias。
它把合同值与旧源码提取结果固定下来；校验器另外锁定 ESP SHA-256、负例集合和
失败原因。完整逐函数 PSC/VMAD 复解析仍作为 A01 的来源审计记录，不在本脚本内重复。

## 关键事实与差异

- 58 个记录的 TES4 记录头 flags 实际全部是 `0x00000000`：该字段位于记录头偏移 8，
  本插件没有 master/compressed/deleted 等 header flag。QUST 的 Start Game Enabled、
  Run Once 等玩法 flags 单独保存在 `DNAM`。四个 QUST 的
  `questFlags` 实际均为 `0x00000111`，即 `kEnabled (1<<0) | kStartsEnabled (1<<4) |
  kRunOnce (1<<8)`（依据 `RE/T/TESQuest.h` 的 `QuestFlag` 枚举）。
- PSC 注释的类别顺序以 `Slut` 开头；ESP `FameType` 的真实运行时顺序以 `Whore`
  开头，且 `Tattoo`/`Cum Dump` 在 VMAD 中拼为 `tattoo`/`cum dump`。两者都在
  `legacy-events.json` 中保留，C++ 侧分别提供 `kDocumentedFameCategoryOrder`
  与 `kEspFameCategoryOrder`。
- `WhoreEvent` 是 `SetWhoreEventFlag` 事件名，旧查询 flag 名为 `Whore Event`；
  `Sex Worker` 是 Data_Exporter 的 `ModFlags` 成员，二者共用 `SexWorkerFlags`，
  作为独立 flag 保存。`GetModFlagState` 接受这两个 flag 名，
  `GetExternalFlags`/`DoesFlagExist` 不暴露它们。`FameType` 固定 25 项。
- 21 个默认地点按 ESP VMAD 的真实顺序写入：Whiterun、Winterhold、Windhelm、
  Solitude、Riften、Markarth、Morthal、Dawnstar、Falkreath、Raven Rock、Riverwood、
  Rorikstead、Ivarstead、Shor's Stone、Dragon Bridge、Karthwasten、Skaal Village、
  Largashbur、Dushnikh Yal、Mor Khazgur、Narzulbur。地点别名归一与解析优先级
  单独写入 `legacy-rules.json`，分别记录名称匹配和 Major 的 `IsChild` 规则。
- 旧 `GetFameLocation` 对未解析地点返回 `Null`；`GetFameValue` 对未知
  地点/类别返回 0；`GetExternalFlags`/未知 mod 查询返回 false；`SetFameValue`
  没有统一 0–150 clamp。规则文件把这些区分开。
- ManualSpread 的源值小于 10 时无操作，百分比 clamp 到 10–100。旧代码的
  `pct/100` 使用整数除法，导致 10–99% 没有增量；已批准修复为浮点比例，按
  `min(150, int(目标旧值 + 来源旧值 × (double(pct)/100)))` 写回。
- MCM 只有 PEX、没有 PSC；MCM 屏幕不进入 Papyrus 合同，新实现使用 SKSE 菜单
  框架。该决定在 `legacy-rules.json` 中标为 `approved-change`。MCM QUST
  `0x06000809` 保留为旧菜单的识别身份，新菜单由 SKSE 菜单框架提供。
- 483 个属性/数组中的内部数组不作为公开 ABI；只有公开函数、事件、类别和地点
  结果被保留。该决定同样标为 `approved-change`。

## 可重复校验

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/verify-contracts.ps1 -NegativeFixtures
```

校验器会检查四份清单、oracle、跨文件计数、ESP 校验和、VMAD/alias 详情和 58 个
记录身份，并实际施加六个负例：删除一个输入事件、交换事件参数、复制类别、伪造
record FormID/插件名、篡改 `GetFameLocation` 无效结果、反转 Managers VMAD 顺序。
校验通过意味着每个负例均被拒绝，原因匹配 `expectedFailure`。
