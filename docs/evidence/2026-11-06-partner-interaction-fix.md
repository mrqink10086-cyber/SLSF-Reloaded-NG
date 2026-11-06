# FFM／MMF 偏好误判修复

日期：2026-11-06。固定起点：`6be4b564d137a81ac211daeefa4c06ab9be64e55`。

## 问题与行为

旧分类器按场景男女数量推断 LikesWomen／LikesMen：女性玩家参加 FFM 时，只要另一名女性在场就满足 LikesWomen 条件，与双方是否互动无关。新增回归先在旧实现下失败，随后使用同一案例验证修复。

现在由 P+ 薄采集脚本取得玩家与每位具体伙伴的双向交互，C++ 投影为男性／女性伙伴交互事实，分类器据此选取 LikesMen／LikesWomen。FFM／MMF 中旁观同场伙伴不会仅凭在场触发对应类别；确认存在同性伙伴交互时仍可触发。受害者设置、朋友／恋人过滤及原计分节奏继续由已有分类流程处理。

## 采集与未知状态

- 依据只读参考 `E:/source/repos/sexlab P+/Source/Scripts/SexLabThread.psc:129–158` 的 `HasInteractionType(Int, Actor, Actor)`，分别查询玩家→伙伴及伙伴→玩家。范围为 CTYPE 1–13，排除 12（AnimObjFace）；伙伴参数使用具体 Actor。
- 仅在 `IsInteractionRegistered()` 为真时报告有／无交互；未就绪为 Unknown。采集末尾继续检查线程、场景、阶段及物理就绪状态，拒绝跨状态混合结果。
- actor wire 增加可选第 11 列 `true`／`false`／`unknown`。旧 8–10 列仍能解析，缺失的伙伴交互保持 Unknown，不回退到场景人数。非法字段拒绝整个载荷。
- 排除玩家自身；男女偏好沿用原 P+ sexCode 0／1 规则，2／3／4 不折算为男性或女性伙伴。三态聚合允许“已知无接触”消去未知性别的影响；无法判定且影响候选时延后分类，不提交部分候选。
- 本次不改变已有存档声望。更新必须包含 DLL 和新版 `SLSF_Reloaded_PPlusProbe.pex`，避免旧采集器持续缺失新事实。

## 验证

| 验证 | 结果 |
| --- | --- |
| 原实现回归 `[partner-interaction]` | 预期失败：FFM 错误包含 LikesWomen |
| 修复后的 `[partner-interaction],[g08-fixes]` | 13 cases／191 assertions，通过 |
| 普通核心与 ASan 核心 | 各 434 cases／150326 assertions，通过 |
| 普通边界与 ASan 边界 | 各 4 cases／22 assertions，通过 |
| Differential | 11 cases／19605 assertions，通过 |
| StateCodecFuzz | 100000 runs，seed 1170，通过 |
| 离线唤醒、外观、菜单缺模块路径、benchmark | 通过 |
| `xmake build -y SLSFReloadedNG SLSFConfigProbe` | 通过 |
| `tools/compile-papyrus.ps1 -Clean` | 25 个自有脚本，0 失败 |
| `python tools/verify-papyrus-frozen.py` | 0 mismatches |

全套命令：`tools/run-native-tests.ps1 -Mode all -Jobs 8`，退出码 0。ASan 越界负例按预期报告错误，用于确认检测器有效；不是业务测试失败。原始日志位于 `docs/evidence/audits/2026-11-06-partner-interaction/`。

独立 Standards 与 Spec 两轴只读复核均未发现确定阻断；没有重复运行测试或宣称实机验证。非阻断建议为增加 sexCode 2／3／4 接触为真时的直接聚合断言；本次已保留该排除策略，实现已审查。

本次未安装或启动游戏。核心测试覆盖 wire、投影和分类；生产 VM 与 P+ 碰撞数据的实际传递仍由游戏测试确认。外部参考源码保持只读，设置仍仅在 SKSE 菜单中提供。

## 交付

中文 MO2 包仍使用 Data 根目录布局，名称为 `SLSF声望重制NG-0.1.1-中文测试版-MO2.zip`。源码提交后执行 `tools/build-staging.ps1`；包内 `文件清单.json` 记录精确提交、脚本输入和载荷哈希。

打包后使用本地 `build/mo2-package-smoke.py` 校验载荷、DLL／PEX／PSC 与构建输入、全新安装、保留配置、路径末尾分隔符及损坏文件拒绝路径。最终包身份和验证结果随交付回复提供。
