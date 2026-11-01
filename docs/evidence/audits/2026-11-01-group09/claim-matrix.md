# 第 09 组交接报告逐项对抗核验

被审报告：`docs/evidence/task-groups/09-racemenu-and-slavetats.md`，固定 HEAD `5529ed98332ade1f8167f9cc2dac210ec0c393e5`。行号均为该固定文件行号，可用 `git show 5529ed98332ade1f8167f9cc2dac210ec0c393e5:docs/evidence/task-groups/09-racemenu-and-slavetats.md` 查阅；当前交接报告已更新，后续结果见[修复报告](../../2026-11-01-09-fixes.md)。表中合并重复主张，覆盖交付、协议、执行数字、开发期失败、目标验收和下组交接。裁决以真实实现/来源为准；历史执行过程无法由当前通过反推为真实。

| 原报告位置 / 主张 | 对抗裁决 | 代码或证据 |
| --- | --- | --- |
| 9–12：交付 RaceMenu/SlaveTats 头与实现 | 文件存在；核心 seam 和复制状态机存在，生产端口未交付 | include/slsf/integrations/RaceMenu.h、SlaveTats.h；对应 cpp；src/main.cpp 无对应装配 |
| 11、32：JcString/JcTable 是真实 ABI 镜像、JcMirrorTable 已接 | 过度：实际仅内部 seam；没有 JcMirrorTable 类型/生产 BSFixedString thunk | SlaveTats.h:60–88；上游 jcontainers_wrapper.h:42；见主报告字符串 ABI 裁决 |
| 13、45、51–65：本组 19 用例、RAII 6 SECTION | 不准确：13 个 TEST_CASE；RAII 一个 case 内五个嵌套作用域，若分计移动构造和移动赋值则有六项操作，不是六个 Catch SECTION，也不能额外算成六用例 | tests/core/AppearanceStateTests.cpp:305–352；当前测试清单；全套 379 总数可独立确认 |
| 14：离线 probe + 目标 pending checklist | 成立；真实接口查询未实现 | tests/probe/AppearanceProbe.cpp:424–445 |
| 15：构建/runner 注册 | 注册存在；“未动他人块”是工作过程描述，无法仅凭最终 diff 证明 | xmake.lua、tools/run-native-tests.ps1；固定基线 diff |
| 17：所有 probe 注释是必要例外 | 不成立：文件用途导读等普通说明违反 AGENTS；协议/ABI来源说明另按例外核验 | AGENTS.md:9–12；AppearanceProbe.cpp:1–4、434 |
| 23：交换常量、SKEE、8B 单指针消息 | 本地上游源级相符；当前生产 Exchange 缺失 | RaceMenu/skee64/IPluginInterface.h、main.cpp；RaceMenu.cpp:86 |
| 24：QueryInterface/GetVersion/GetOverlayCount 槽位与版本验证 | 本地上游源级相符；fake 与非空检查不等于目标二进制 ABI 已验证 | RaceMenu.cpp:94–160；RaceMenu/skee64/IPluginInterface.h、OverlayInterface.cpp |
| 25：Body/Hand/Feet/Face 容量、3 条 capability | 成立；源级结构/赋值可核 | RaceMenu.h；RaceMenu.cpp:93–160 |
| 26：首 epoch、旧 epoch 不改快照 | 首 epoch 和快照字面成立；旧请求会修改共享 gate，后续有效缓存仍 Stale，G09-S1 | RaceMenu.cpp:75–84；counterprobe-results.log:2 |
| 26：dirty/session/Cancel/Reset、20 次上限 | 状态转换基本存在；Reset 保留 epoch；maxAttempts 只控制 Failed 阈值，不限制请求次数 | RaceMenu.cpp:64–68、226–281；SlaveTats.cpp:274–278、401–460；counterprobe-results.log |
| 27：非 Ready 时暂停新计分、保留设置/声望并恢复 | gate helper 可供调用；没有真实消费者，不能算端到端行为验证 | ExternalState.h；src 内缺该业务装配；任务书:25 |
| 31：一次性 type1/8B/v1/15 指针/slot13、14/部位顺序 | 布局与批量参数源级相符；监听接线未完成 | SlaveTats.cpp:15–35、172–205、318–333；上游 SlaveTatsNG_Interface.h、main.cpp:90–99 |
| 31：非 v1 → Incompatible + rejections | 仅前半成立：非 v1 设 Incompatible，但该分支未增加 rejections | SlaveTats.cpp:172–205 |
| 32：Owns 等于非零句柄且 table 能力集可用 | 不准确：Owns 仅检查 handle!=0、table!=nullptr；正常 Collect 的绑定校验另在调用前 | SlaveTats.cpp:120–132、304–308 |
| 32、88：creates=4/releases=4/retains=0 证明 JC 所有权平衡 | 只证 fake 调用次数。新对象宽限允许同步使用；零 owner release 的目标版行为与真实字符串引用未验证 | AppearanceProbe.cpp:405–413；JC.md object lifetime；真实 port 未实现 |
| 33：每区自有记录、stable_sort、最多保留256 | 成立；256 是输出上限，仍扫描完整 count，名字先完整复制 | SlaveTats.cpp:235–270；counterprobe-results.log:8 |
| 33：>128B / 空 entry / 负 slot 拒绝 | 成立。空 entry=对象句柄0，不是空名称；空名称确会保留，原句没有声称拒绝它 | SlaveTats.cpp:243–259 |
| 34：SlotFacts 投影 applied/Unknown subcategory | 完整输入时成立；未传播拒绝/截断的不完整性，确定存在 G09-S2 | SlaveTats.cpp:83–94；counterprobe-results.log:5、7 |
| 35：session 切换释放 JcHandle/清表所有权 | 所有权描述不实：句柄为 Collect 局部，早在返回时释放；session 只清快照/状态，借用进程全局地址表继续保留，后者本身合理 | SlaveTats.cpp:310–317、431–442；上游静态 addresses_instance |
| 43：构建目标成功 | 本次 runner 所列测试、ASan 和 probe 目标构建运行成功；插件 DLL 构建未纳入本次归档验证，也不能由当前通过证明历史相同命令确已运行 | tools/run-native-tests.ps1；fresh-native-all.log 与本次执行结果；固定源 |
| 44–46、119：全套PASS/379/149650/Boundary4、22 | 本次独立 runner 和聚合 XML/测试清单确认相同数量；新旧执行相互区分 | test-artifact-manifest.json；test-results/core*.xml、boundary*.xml 及 *.tests.xml |
| 47：ASan 耗时及8个场景 | 原耗时只属历史记录；实际 runner 调 probe index7 正例、index8 负例，是两个调用场景，不是8个。ASan正/负检查及ASan套件本次通过 | tools/run-native-tests.ps1；ASan Probe dispatch；本次 runner 退出成功 |
| 48、77：100000 fuzz、精确cov/corpus、无失败seed | 本次 runner 100000 次完成；精确历史覆盖/语料数字只见原记录，临时stdout未保存；未发现新的失败seed不能反证无其他失败输入 | runner 及本次wrapper；fresh-native-all.log不含子进程stdout |
| 49：Appearance OFFLINE_PASS/providers-pending | 本次归档输出确认；仅为fake接口行为 | test-results/appearance-probe.txt |
| 67：随机 seed1170、并发与ASan均执行 | seed与执行存在。并发仅覆盖广播对顺序Collect，alternate表缺有效batch；不证明有效外采集/析构等任意线程安全 | AppearanceStateTests.cpp:868–894；普通/ASan测试清单 |
| 73–75：开发期三次失败根因和修复 | 当前源码具备持有快照值、fake容器状态和首epoch吸收措施；不能从最终树/通过结果独立证明历史失败诊断、顺序或全部根因 | AppearanceStateTests.cpp；RaceMenu.cpp:75–82；SlaveTats.cpp:285–292 |
| 83–86：交换/Overlay/地址前缀ABI表 | 本地上游源级核实；“主基 slot0 QueryInterface”应精确区分为 map 的 QueryInterface，非 Overlay slot0 的方法名 | RaceMenu.cpp:94、112、140；上游IInterfaceMap/Overlay类声明 |
| 87：ResolveActor 返回 TESObjectREFR*、batch目标 | 不准确：核心声明 void*；上游要求 RE::Actor*。真实 port 必须按 Actor 解析 | SlaveTats.h:85；SlaveTats.cpp:298–323；SlaveTatsNG_Interface.h:51 |
| 89、93：目标版本/当前缺提供方 | 属原报告安装检查；本次未重新扫描安装目录，不把历史环境事实当当前机状态证明。真实运行记录缺失可确定 | 本次未运行游戏；报告自己标pending |
| 95–102、109：安装后直接跑现成probe就能真查询，pending段已就绪 | 不成立：main无真实端口、dispatch或loader切换，仅fake与printf清单。还需实现真实宿主probe/port，安装和运行同一EXE不能完成验收 | AppearanceProbe.cpp:424–445；生产src无两个port实现 |
| 99–100：目标应验证的交换/广播/false=成功 | 作为未来检查清单合理，尚非实际结果 | 上游源匹配，真实执行pending |
| 101：JFormDB查询、creates==releases即最终验收 | 只是计划；尚无真实JC绑定，单一计数相等不足验证owner/refcount和BSFixedString引用 | JcTable内部seam；缺生产port；主报告JC边界 |
| 106：PostPostLoad阶段后注册SlaveTats监听 | 明确错序，照此执行会漏一次性广播，G09-S4 | 集成计划:43；上游SlaveTatsNG main.cpp:90–99 |
| 106–108：dirty、Visibility、Papyrus接线留后组 | 真实状态披露成立，但本组任务书本身也要求相关行为，故验收须标延期；不重复归因一般B03/C02待办 | 09任务书:19、23–25；当前src装配缺失 |
| 111、113：4子代理复核、均可交付/全部落地 | 历史代理数量/过程本次无法独立验证；当前两项反例和目标验收缺口足以否定“全部行为已正确且验收完成”的解释 | G09-S1–S4；本次3个Luna/xhigh独立审核 |
| 115：M28 changed/epoch更新和删无用继承 | 当前Missing/Incompatible分支确有changed/epoch更新，M28无enable_shared_from_this；不能用M29剩余继承反证这句。未修复G09-S1 | RaceMenu.cpp:103–151、83–84；SlaveTats.cpp:135是另一个模块 |
| 116：M29恢复门、Area去noexcept | 当前代码可核；不推出全部生命周期路径正确 | SlaveTats.cpp:172–205、79–80；对应测试 |
| 117–118：Face/Feet、16B负例、负count、并发断言、总数修正 | 当前测试可找到这些内容；原数字148562的历史修改过程不独立背书，149650本次实测 | AppearanceStateTests.cpp；本次XML归档 |
| 123：全部在本仓分支完成、与同基调 | 仓库与提交区间确认；历史工作过程/风格判断不能充当实现正确性证据 | git rev-parse/log/diff；当前代码优先 |

以上核验保留原报告可成立的部分，没有把所有待接线行为都定性为已发生的游戏故障。初步ABI崩溃、确定重入死锁、零owner必然泄漏、M29继承反证M28、空entry等于空name等判断均未采用；理由见[主报告](../../2026-11-01-09-adversarial-audit.md)。
