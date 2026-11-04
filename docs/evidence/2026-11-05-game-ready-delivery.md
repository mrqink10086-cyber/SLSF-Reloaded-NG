# 第 11 组修复与游戏测试包交付

日期：2026-11-05（Asia/Shanghai）。修复基线为
`b5f47eb9ec70729ca24c32e7d0e7a194fee09c18`，在当前 `main` 分支提交。
原审核及原始日志保留于 [历史审核](2026-11-05-group11-review.md)。
最终交付版本及载荷身份以生成包的 `manifest.json` 为准。

设置统一使用 SKSE Menu Framework。保留的同名 MCM 脚本仅保存挂载属性并阻止
继承的菜单注册，没有新增 MCM 页面或设置。外部 MOD 源码保持只读。
`tools/papyrus-imports` 仅提供编译类型/签名，不生成或安装这些 MOD 的 PEX。
Public Whore 源码本机缺失，其声明依照冻结的 SLSF 调用点；其余声明核对本地真实接口。

## 审核项闭合

| 项目 | 修复与验证 |
| --- | --- |
| ST1 | 修正 CommonLib 日志 sink 签名；实际 DLL 目标重建通过。 |
| ST2 / SP5 | ModEvent 使用整数 handle，按 global Create/Push/Send 顺序；终态转移所有权，过期会话丢弃回调。 |
| ST3 / ST7 | 具体对象/数组类型、自实例和脚本句柄封送；逐项记录绑定失败，固定预期数量，helper 失败纳入结果。 |
| ST4 / SP6 | 旧导出文件名；单快照导出完整模组/flags/地点/spread；导入在候选状态完整校验后一次提交。 |
| ST5 / SP4 | 独立 Data 包、默认 JSON、许可、依赖、源版本与文件哈希；安装前验哈希并保留既有 JSON。 |
| ST6 | 数转整数先检查范围；核心边界回归及 JSON fuzz 通过。 |
| ST8 | 自有违规注释和四块 docstring 清理；工具指令保留。原审核的 81 行外部反编译参考按用户后续只读要求保留。 |
| SP1 | 86 个冻结公开入口由 82 个 native 与 4 个 PSC getter/采集转发承接；52 个输入事件、7 个回应保持冻结协议，消除恒值替代业务。 |
| SP2 | 配置切换先按旧参数结算并发布，再提交新规则及 revision；会话 RNG 保持共享所有权。Gameplay 使用已应用的有类型设置。 |
| SP3 | 主线程执行泵接入游戏钟、地点、匿名、目击、外部事实、P+ 场景和兼容输出；同名 PSC 转发 typed 场景与装备事件。 |
| SP7 | 前戏、关系资格与实际游戏时间进入同一规则链；直接 SubAnalyze 保留七参数且不重复抽关系概率。 |

补齐默认 21 地区及公开别名、自定义地区独立归属、全部地区批量命令；匿名驻留和
识别时间持久化；缺来源类别的独立 Enable 位；真实 SLACS storage key 采集；
纹身附加类别、AND/DD 资格、Skooma 累加衰减、卸载停止三个任务并在重载后维持停用。
StorageUtil 输出按批串行，旧 epoch 不阻塞新会话，迟到 completion 以请求身份过滤。

## 最终验证

| 检查 | 结果 |
| --- | --- |
| 普通核心 / ASan 核心，seed 1170 | 各 425 用例、150,177 断言通过 |
| 普通边界 / ASan 边界 | 各 4 用例、22 断言通过 |
| 差分测试 | 11 用例、19,605 断言通过 |
| StateCodecFuzz / SerializationFuzz / JsonTransferFuzz | 各 100,000 次通过，seed 1170 |
| 离线 wake / appearance / menu / benchmark | 执行成功 |
| DLL 与默认配置工具 | `xmake build -r -y SLSFReloadedNG SLSFConfigProbe` 成功 |
| Papyrus clean 编译 | 25 个 SLSF 脚本成功，未编译外部 imports |
| 冻结协议核对 | `verify-papyrus-frozen mismatches: 0` |
| 包装预检 | 72 项载荷哈希匹配，25 对 PSC/PEX，仅一个 NG DLL，原 ESP/Interface 保留 |
| 安装烟测 | 首次安装成功；再次安装保留自定义 JSON；两种根路径末尾斜杠均通过 |

ASan 故意越界的负向探针按预期失败，证明 sanitizer 已启用；它不是产品测试失败。
fuzz 发现旧 v2 解码接受不可再次编码的空名地区，现统一元数据验证并补回归。
JSON fuzz 的旧断言误要求不完整自定义地点也能导入，现允许明确拒绝，并验证该拒绝。

受控回归直接使用生产 `StorageOutputQueue`、`GameplayState`，覆盖旧 epoch 无回调、
迟到 completion、仅保留最新 pending、相等进出时间、驻留恢复和重复地点采样。
另外验证批次衰减的账户键/保护期限、自定义倍率、直接 SubAnalyze 的前戏与双关系、
独立 Enable 位保存往返，以及没有可用 gameplay owner 时不伪造回应。

完整 `Gameplay.cpp`、`EngineVm.cpp`、`VmNatives.cpp` 适配器由 DLL 编译和独立源码
审核覆盖，不在 ASan core 的执行范围内。真实游戏引擎的 VM 分发、已安装 MOD 接口和
画面行为由用户游戏测试验收；本轮未运行或安装到真实游戏目录。

## 独立收口审核

Standards 轴确认事务发布、元数据校验、单快照、活动配置、驻留恢复、输出队列、
VM self/latent/注册计数与宽化计数均已修复，没有剩余确定源码阻断。
装配审核发现并修正安装路径末尾斜杠问题，随后重新装配、安装烟测通过。

Spec 轴确认没有剩余确定代码阻断，卸载持久行为已承接。其装配建议补 SkyUI 父类
依赖并在最终提交后重打包，均纳入交付流程。两轴均只读，未把注册数量或核心通过
表述为真实游戏执行证明；未发现额外范围扩张。

## 重放与交付位置

```powershell
xmake build -r -y SLSFReloadedNG SLSFConfigProbe
powershell -NoProfile -ExecutionPolicy Bypass -File tools/compile-papyrus.ps1 -Clean
python tools/verify-papyrus-frozen.py
powershell -NoProfile -ExecutionPolicy Bypass -File tools/run-native-tests.ps1 -Mode all -Seed 1170 -Jobs 8
powershell -NoProfile -ExecutionPolicy Bypass -File tools/run-native-tests.ps1 -Mode fuzz -Target SerializationFuzz -Seed 1170
powershell -NoProfile -ExecutionPolicy Bypass -File tools/run-native-tests.ps1 -Mode fuzz -Target JsonTransferFuzz -Seed 1170
powershell -NoProfile -ExecutionPolicy Bypass -File tools/build-staging.ps1
```

测试日志：[本轮证据目录](audits/2026-11-05-game-ready/)。CommonLib 固定为
`15ad90b31b2ca09cf477c7e1a43ff5ff05af120f`。目标为 Skyrim 1.6.1170 / SKSE 2.2.6 / x64。

包路径：`build/staging/SLSFReloadedNG-0.1.0-game-test.zip`；展开目录：
`build/staging/SLSFReloadedNG`。包中包含安装器、说明、依赖和 SHA-256 清单。
最终提交后重生成 manifest；manifest 排除自身，记录精确载荷及源提交。
构建可能改变 PE/PEX 时间戳，因此不承诺跨机器二进制字节完全相同。
