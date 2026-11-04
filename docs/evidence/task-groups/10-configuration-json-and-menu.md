# 第10组交接报告：配置、JSON 读写与 SKSE 菜单（B07 + C08）

日期：2026-11-02（审计修复轮更新）。范围：本组任务（B07PRE-16、C08PRE-16）。前置组 02/03/06/07 的语义未改动。

## 改动文件

新增：
- `include/slsf/io/JsonTransfer.h` + `src/io/JsonTransfer.cpp`（M36）：严格 RFC8259 JSON 解析/写出（大小/深度上限、非有限数拒绝、byte 级错误信息）；旧版 PapyrusUtil `JsonUtil` 语义镜像 `LegacyDocument`（int/float/string/form 四组、key ASCII 小写、`0x…|插件` form 字符串、未知根组保真回写）；`ExtractLegacyImport` 解析旧导出器布局（fame/spread/ModsIndex/mod flags，按旧合同键小写查找）。
- `include/slsf/config/ConfigurationStore.h` + `src/config/Configuration.cpp`（M35）：统一配置源（`IConfigurationSource` 实现），磁盘临时文件替换式写入 / 内存注入失败适配器；读取→解析→base 合并解码→校验→编译快照→revision；`Open/BindNotify/Reload/ApplyEdit/Current`；`Encode/Decode/LegacyDefaults/Validate` 为纯函数；通知在锁外发出（回调可重入 `Current()`），`NotifyCount` 原子。
- `include/slsf/ui/Menu.h` + `src/ui/Menu.cpp`（M37）：framework 绑定（自含函数指针 + GetProcAddress 门禁 3.x/API1），页面 Overview/Categories/Connections/Balance/Commands/Legacy/Diagnostics；`MenuRequestQueue` 同轮合并；close 语义＝提交脏草稿+无草稿时一次重读；`Ports` 增加 `fameCommand`（公开命令执行器）、`legacyFiles`/`legacyImport`。
- `tests/core/ConfigurationStoreTests.cpp`（17 用例）、`tests/core/JsonTransferTests.cpp`（11 用例）、`tests/core/MenuRequestTests.cpp`（7 用例，文件内 `grep -c TEST_CASE` 可核）。
- `tests/fuzz/JsonTransferFuzz.cpp`：解析往返幂等 + LegacyDocument 写读幂等 + 根组保留不变式（编辑后原 int 组键不丢失）+ 提取器不变式。
- `tests/probe/MenuFrameworkProbe.cpp`（xmake 目标 `SLSFMenuFrameworkProbe`）。
- 夹具 `tests/fixtures/json/{config,legacy}`（valid_config、bad_types、legacy_exporter、truncated；legacy_exporter 同时被 JsonTransferTests 用 `SLSF_FIXTURE_DIR` 引用并进 fuzz 语料）。

修改：
- `src/main.cpp`：删除 `BootstrapConfigurationSource`；`ConfigurationStore::Open(DiskConfigurationFile("Data/SKSE/Plugins/SLSFReloadedNG.json"))`；`BindNotify` 日志；`kDataLoaded` 调 `InstallSlsfMenu()`（幂等）；ports 绑定 snapshot/reload/commit/regions/status + `fameCommand`（单例 `MenuPublicApi()`：SetFameValue/ClearFame/ClearAllFame/ManualSpread/ManualDecay/ManualGain/SpreadPulse，状态与消息回显）+ `legacyFiles`（枚举 `Data/SKSE/Plugins/StorageUtilData/SLSF_Reloaded/*.json`）+ `legacyImport`（读文件→`ExtractLegacyImport`→`SetFameValue` 逐地点+新注册 mod 的 flags 导入；汇总 applied/rejected/first-rejection）。
- `xmake.lua`：SLSFCore / SLSFCoreAsan 加入 `src/io/**`、`src/config/**`、`src/ui/**`；probe 目标。
- `tests/fuzz/xmake.lua`：`JsonTransferFuzz` 目标（必须 `-D_DISABLE_STRING_ANNOTATION -D_DISABLE_VECTOR_ANNOTATION`，否则 libFuzzer↔MSVC STL 容器注解互撞）。
- `tools/run-native-tests.ps1`：fuzz 允许表+语料（json/config+legacy+serialization v1 混合）；`all` 模式追加 menu-framework probe（Tee 至 `build/test-results/menu-framework-probe.txt`）。

未改动：组 06 `Lifecycle.cpp`、既有测试与其它组文件零触碰（除上述四个接线文件）。

## 对抗审计与修复轮（用户要求的独立子代理审核结论 + 落实）

三个子代理审计（config/JSON、菜单/装配、需求覆盖）；代码事实为准。已落实修复：
1. Install 幂等被打穿：生产 `kDataLoaded` 曾调 `Menu::ResetForTest()` 且重复 kDataLoaded 会重复 `AddSectionItem`（框架同名追加不覆盖）→ 移除生产调用；`Install` 已安装时改为"重绑端口、返回成功"。
2. CloseCommit 掩错：提交失败信息被尾随 reload 的 "configuration unchanged" 覆盖（测试桩恰好同串使断言假绿）→ close 现在仅在"无事可提交"时重读；提交失败保留错误消息；测试桩改为必不同串（`reloaded from disk` vs `configuration write failed: injected`）。
3. 关闭即全量写盘：草稿与当前快照相等时不再写盘/不提交（`*draft_ == *CurrentData()` 短路），mtime 不抖动；测试断言 commit 0 次。
4. `LegacyDocument` 载入改存丢数据（root 组存在时 Group 读 root、EnsureGroup 建空白、ToJsonPtr 固定四组）→ Group 以 mutables 优先、EnsureGroup 克隆 root 组、ToJsonPtr 追加未知根组成员；fuzz 增加"编辑后原 int 组全部键保留"不变式与单测。
5. 空指针 JSON 值 UB：`MakeNumber` 非有限返回 null 后 `AttachMember`/`AppendArrayItem` 接收、Writer 无条件解引用 → 两者将 null 值物化成 JSON null；Writer 增 `WriteChild`（null→"null"）。
6. 夜间小时边界不等价（Validate 允许 <24、Decode 拒 >23.999，ApplyEdit 写盘后重读被拒=文件自毒）→ 双侧统一 <24 上限（Decode 尾部还有 ValidateData 兜底），并加 23.9995 往返回归。
7. 通知在锁内回调（回调调 `Current()` 即死锁；计数无锁读）→ 通知改为锁外发出、拷贝快照；`notifyCount_` 原子；加"回调重入 Current"回归。
8. 首开生成失败仍报 Applied → IoError（快照与 revision 仍在内存，数据可用）。
9. BOM 整拒 → 解析器剥 UTF-8 BOM；单测。
10. 死码/精度/降级：删 `FrameTick`、`cachedRevision_`、igProgressBar 残留；乘数滑条上限 10→100（store 仍允许 1e6）；`Usable()` 补齐 14 个已消费导出，页面缺函数时显示降级提示而非静默失效。
11. n11/n12：补 `#include <algorithm>`，删未用 `<charconv>/<cstdio>`、重复 lover 阈值校验与死语句。
12. 命令结果 0 实现（审计③披露缺口）→ 新增 Commands 页：Location/Target/Category combo + Value/Min/Max/Percent 滑条，七个按钮经 `Ports.fameCommand`→`PublicApi` 公开命令执行，结果显示在页面；共享单例 `MenuPublicApi()`（mod 注册表状态单实例）。
13. 旧导入入口未兑现（审计③）→ `ExtractLegacyImport`（键小写解析 `<地点>.<fameType>`/`famespreadchance`/`ModIndex[n]`/`ModTotal`/`<mod>.<flag>`，饱和 int）+ 菜单 Legacy 页按目录枚举逐文件导入 + main.cpp 接线 `SetFameValue`/`RegisterMod`/`SetExternalFlag`（仅新注册 mod 导入 flags，同旧 PSC 语义）。范围披露：未注册的自定义地点（`Location_Form_*` 表单解析属于 VM 侧资产行为）不自动注册、逐条被 PublicApi 以 "unresolvable" 拒绝并计入 rejected；spread 值提取后暂不落状态（无公开 spread 设置器），仅在摘要计数。
14. 报告数字失实修正：原"19 用例"实为 17/11/7（文件 grep 可核）；asan 分片断言数无溯源的旧写法删除，改记实测命令与 PASS 行；config-scratch 7 个残留已清（测试运行会自建）。

审计三条"指控"经复核不成立（记录在案防复提）：页面无空语句；孤儿/未解析连接真实现（`IsKnownRegion` 扫描 + `(unresolved)` 标注）；`sourceIndex_/targetIndex_` 被 Source/Target combo 消费。cimgui 15 个函数指针签名逐一核对通过；x64 单调用约定下无捕获 lambda 转函数指针安全；probe 只覆盖守门路径（下述待办之一）。

## 配置契约（Data/SKSE/Plugins/SLSFReloadedNG.json）

- 顶层 `schemaVersion`=1（独立于旧公开 JSON 与存档编解码）；>1 → Invalid "unsupported schema version"；缺失按 1 接受（双向行为有钉）。
- 分组 features/categories/relationships/timing/parameters/connections/menu；未知顶层键容错；未知乘数/掩码类别键拒绝；connections/selectedRegions 上限 1024；写入规范化排序去重（从<到、字典序）。
- 读流：缺失→默认值生成文件（rev1 Applied；生成失败→IoError 且内存快照可用）；解析/解码失败→Invalid 保留上次有效快照；读失败→IoError 保留；同内容→Unchanged 复用 revision；不同→revision+1 并通知（锁外）。
- 写流（编辑完成/Apply）：校验→`*.tmp`+rename 替换→立即 `ReloadLocked()` 读真实磁盘内容。
- 默认值＝旧 MCM 表（30/5/30、0/50/50/25、decay 0.5 天、spread 1.0 天、night 22/6、failed/left 10/10、parameters 2/1/30/0.6/150）。

## C08：框架事实（源码 vs probe）

- 源码事实（只读自 `E:/source/repos/SKSE-Menu-Framework-3-master`，未修改）：`MENU_FRAMEWORK_VERSION 3.8f`、`MENU_FRAMEWORK_API_VERSION 1`（src/SKSEMenuFramework.cpp:10,12）；事件 `EventType{kNone=0,kOpenMenu=1,kCloseMenu=2,kBeforeRender=3,kAfterRender=4}`；`RegisterEventPriority/UnregisterEvent/AddSectionItem`（path=根+段名）；每帧 section 回调在框架自身 ImGui::Begin 内（页面不得自行 Begin）；kCloseMenu 同步广播。ImGui 导出实名出自 cimgui.cpp（15 个消费中的函数全部签名核对）。头文件 `IsInstalled()` 只查文件存在，绑定一律走 `GetModuleHandleW(L"SKSEMenuFramework")+GetProcAddress`+API 版本门禁。
- 运行时 probe（开发环境无游戏进程）：`module_loaded=0 install_without_module=0 MENU_FRAMEWORK_PROBE=PASS`（守门路径已验证）；模块在位分支（20 个导出逐一解析+版本打印）在游戏内生效——列入下组待办。

## 菜单行为与验收对应

- 打开/切入配置页/游戏重载：kOpenMenu→`Configure`→一次读盘。
- 编辑完成/Apply/close：同轮框架 kCloseMenu+自身请求合并为一次 `CloseCommit`；有脏草稿→逐值校验+提交+内部重读（保存后读到的=盘上事实），无草稿→零 IO；失败保留草稿与错误。
- 渲染帧：页面只读 `snapshot()` 缓存与草稿（revision 变更自动重置）；文件 IO 只发生在 Configure/CloseCommit/ApplyDraft 入口。
- 声望/注册（每档数据）走 Commands 页公开命令与 SKSE 存档（组 02/08）；Legacy 页仅迁移旧公开 JSON。
- 同轮重复 close 恒等合并、逐帧缓存渲染、菜单可见孤儿节点/写入错误/命令结果、导入摘要——均有对应测试（MenuRequestTests/JsonTransferTests）。

## 旧公开 JSON（B06/B07 合同）

- 布局按 PapyrusUtil 旧式实现锁定：键小写+四组；`JsonTransferTests` 以导出器形状夹具与 `ExtractLegacyImport` 双向验证（`whiterun.whore=12`、`modtotal=2`、`famespreadchance=55`、form `0x00085248|Skyrim.esm`）。
- 本沙箱无 PapyrusUtil/game Data；"真 PapyrusUtil 落地核对"仍列为下组前端待办（解析器与布局自洽 + 夹具为合同验证手段）。

## 测试与命令（seed 1170，本轮全部实跑）

| 命令 | 结果 |
|---|---|
| `powershell -NoProfile -ExecutionPolicy Bypass -File tools/run-native-tests.ps1 -Mode core -Seed 1170 -Jobs 8` | PASS；core.xml `tests=9545 failures=0`；suite-wide 531 unique TEST_CASE（8 分片各含子集，单分片日志如 7808 assertions/52 cases）；组 10 文件含 17+11+7=35 用例 |
| `... -Mode boundary -Seed 1170` | PASS（1 case/5 assertions） |
| `... -Mode asan -Seed 1170 -Jobs 8` | PASS（core-asan 4 分片 + boundary-asan 全绿；`SLSFAsanProbe` 正例 exit0/负例预期 heap-buffer-overflow） |
| `... -Mode fuzz -Target JsonTransferFuzz -Runs 20000 -Seed 1170` | PASS，`Done 20000 runs in 4 second(s)`，无 crash、语料含 json+bin 混合语料 |
| `... -Mode all -Seed 1170 -Jobs 8` | PASS：core/boundary 复测 + 两 fuzz 目标 + dispatcher-offline/appearance/menu-framework 三 probe 全 PASS（probe 输出 UTF-16→`menu-framework-probe.txt`） |
| `xmake build -y SLSFCoreTests SLSFBoundaryTests SLSFReloadedNG JsonTransferFuzz` | exit0（插件 DLL 链接成功） |

fuzz 说明：ps1 fuzz 模式的 stdout 不落盘（沿用组 02 设计），本轮以独立命令记录 runs 数量；"all" 模式 fuzz 走默认迭代（100k）未保存原文——已在下条待办列出"runs 落盘"。

审计修复轮内失败的固定用例输入与复盘（全部复跑通过）：
1. `tests/core/ConfigurationStoreTests.cpp` 两处对成功 `Result` 调 `error().message`（std::expected UB，ASan `__msvc_string_view.hpp:574`）→ 删除成功路径 INFO；规则＝仅在失败分支打印。
2. 分片临时文件撞名 → `MakeUniqueFileName` 加 PID+原子计数；清理 `build/test-results/config-scratch/`（7 个文件）。
3. 本轮新增失败：mod flag 夹具键未按 JsonUtil 全小写合同（`Example Mod.esp.sex worker`）→ 修夹具为 `example mod.esp.sex worker` 后全绿（提取器按小写匹配是正确行为）。

## 注释审计

全部新增/修改文件 `grep "//"` 仅余 namespace 收尾标注；实质注释 0 行（AGENTS.md 规则）。

## 对下一组（组 11 / 发布组装）交接

- 直接可用：`slsf::config::ConfigurationStore`（磁盘/内存适配器+锁外通知）、`slsf::io::JsonValue/LegacyDocument/ExtractLegacyImport`、`slsf::ui::Menu`（Ports 含 fameCommand/legacyFiles/legacyImport）；main.cpp 已把三件接进 SKSE 生命周期。
- 仍开放（按优先级）：
  1) 活动会话内配置变更→执行器重建：`BindNotify` 只打日志；"阈值/关系 mask/图一次提交"的 runtime 侧替换需组 06 Lifecycle 装配决策（Typed 通知已携带 revision+compiled config，接口就绪）。
  2) 游戏内验证项：SKSEMenuFramework.dll 在位时 20 导出全部解析+3.8/API1 实值、页面渲染、A→B/退出重进跨档共享设置（需真存档栈）。
  3) 真 PapyrusUtil⇄LegacyDocument 端到端（PSC 路径 `Data/SKSE/Plugins/StorageUtilData/SLSF_Reloaded/<名> SLSF Reloaded Data.json`）。
  4) Menu 通知设置（`NotificationSettings`）的 VM 消费端接线；spread 导入值落状态（需公开设置器）；菜单打开时文件外改监视（目前"打开/关闭/显式刷新"读，菜单常开不感知外部改动——按 design.md L78 即"打开即读"边界实现）。
  5) ps1 fuzz runs 落盘（保留 stdout）与独立 probe 模式（-Mode probe）。
- 技术债已知未修（低优先级，行为可接受并披露）：非 ASCII control 之外不做 UTF-8 纠错（RFC8259 correction mode 不实现）；Writer 递归无深度上限（只用于已校验树）；LegacyDocument 拷贝共享 mutables；根组大小写不镜像（非小写组作为未知组保真回写）；进程退出时事件令牌不移除（框架先卸载场景）；每帧 snapshot 浅拷贝成本（既定缓存设计）。
- `Menu::ResetForTest()`/`Menu::EditDraftForTest()` 仅测试隔离/构造草稿使用；生产组装只用 `Install/Bind/Unregister`。
- 配置 JSON schema 以 `include/slsf/core/Configuration.h` + `ConfigurationStore::Encode` 输出为唯一真源；schemaVersion 2 兼容策略后续决定。
