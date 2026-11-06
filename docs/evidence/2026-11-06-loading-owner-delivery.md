# 加载生命周期与菜单状态优化交付

日期：2026-11-06。实现基线：`c97cea566487c57dbae48830755b70d387a5e65c`。需求为落实[线程与加载影响评估](../research/2026-11-06-skse-threading-loading-impact.md)中的两项优化；设置仍仅使用 SKSE 菜单。

## 实现

- `ControlExecutor` 为生命周期提供统一 FIFO。所有调用方都入队，首次执行 Drain 的 SKSE task 绑定 owner；其他线程或重入 Drain 被拒绝。PreLoad、序列化 reset/stage/revert、PostLoad、NewGame、DataLoaded 与配置应用共用入口，业务泵先排空该队列。
- 加载入口立即关闭共享 SessionGate，再排队重置。控制命令使用单独的加载代次，不被旧业务 epoch 丢弃。旧激活只能打开自己捕获的代次，不能重新打开后续加载；关闭后新的命令、保存捕获及后续 Dispatcher 派发被拒绝。已经开始的工作不会被强行中断。
- 序列化 record 读取、解码和写出仍在 SKSE 回调有效期内完成。读入的 PersistentState 与 collector continuations 由插件拥有，再交给 owner 做引擎 Form 规范化、准备和激活；不在回调中等待 task。
- 生命周期 owner 发布不可变 LifecycleStatus。菜单调用 Status 只复制已发布快照，不再读取 active/pending/message 的可变内部状态。入口关门至 owner 发布重置之间，菜单可以短暂显示上一版状态；业务许可独立检查即时 gate。
- 控制日志记录 received/enqueued/started/completed、顺序号、加载代次、PID/TID；序列化入口记录 PID/TID，可用于游戏内核对实际执行线程。

## 验证

最终命令：

```powershell
xmake build -y SLSFReloadedNG SLSFConfigProbe
powershell -NoProfile -ExecutionPolicy Bypass -File tools/run-native-tests.ps1 -Mode all -Jobs 8
```

两条命令均退出 0。完整原始输出保存在 `docs/evidence/audits/2026-11-06-loading-owner/native-all.log`。

| 检查 | 结果 |
| --- | --- |
| DLL / 默认配置生成器 | 构建通过 |
| 普通核心 | 431 个用例通过 |
| ASan 核心 | 431 个用例通过 |
| 普通 / ASan 边界 | 各 4 个用例通过 |
| StateCodecFuzz | 100000 次，seed 1170，通过 |
| 差分 | 11 个用例，通过 |
| 性能、离线唤醒、外观、菜单缺依赖探针 | 通过 |
| ASan 人工越界探针 | 按预期检测 heap-buffer-overflow，测试执行器确认该负向检查成功 |

新增 6 个 `[loading-owner]` 用例覆盖 worker/owner FIFO、重入和异线程 Drain、关门后延迟重置与协程在 owner 取消、旧激活不能开新代次、同一 Step 停止后续派发、失败/损坏/缺失 co-save 后的新游戏，以及菜单并发读取快照。

## Standards

主代理本地复核未发现剩余确定阻断：新增实现没有增加解释性注释；外部参考源码保持只读；菜单保持 SKSE-only；控制队列、门禁与状态发布职责各自集中。`git diff --check` 通过。

## Spec

主代理本地复核确认两项需求已落地。生命周期控制与普通业务 epoch 分离，序列化接口未被带出回调，PostLoadGame 继续按指针值解析成功标记，菜单状态查询使用不可变发布。没有添加 MCM 或改写事件协议。

已请求独立 Standards/Spec 子代理复核；两者均因服务使用额度上限失败，未产生本轮审查结论。上述两轴结论来自主代理本地复核，不计作独立审核通过。

## 交付与边界

提交后重新生成中文 MO2 测试包，manifest 应指向该提交并显示 `sourceDirty=false`。保留原有 25 对 PSC/PEX、中文文档及配置保留安装器；本轮未修改 Papyrus 源码。

本轮没有运行或安装游戏。测试执行了生产 ControlExecutor、SessionGate、Lifecycle 与 Dispatcher；完整 main/Gameplay/EngineVm/VmNatives 适配器未由 ASan 核心执行，真实 SKSE 消息、序列化与渲染线程组合仍需用游戏日志验收。
