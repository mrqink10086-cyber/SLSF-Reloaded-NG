# C：集成与交付任务说明

[项目规格](../../../.scratch/native-migration/spec.md)定义范围与玩法，适配器负责把公开外部事实交给核心。模块归属见[清单](2026-10-27-task-books/manifest.json)。

模块接口、事实消息及 adapters 的职责按[代码库设计](../../../.scratch/native-migration/design.md)。集成事实通过 Observations 交回 Session，模组注册和类别设置和清理由 ReputationModel 处理。

## C01 能力查询与依赖版本

实现 Unqueried/Waiting/Available/Missing/Incompatible 能力状态及串行 Ensure。查询已加载 DLL 的公开能力；监听先于一次性广播注册，安装标记和角色动态数据分别管理。

为实际使用的 SDK/运行时补齐来源、commit/版本、header hash、license、结构布局、线程及所有权约定。已有 toolchain/CommonLib 锁定继续沿用；研究引用转为经过验证的提供方版本矩阵。

交付要求：延迟出现的能力应完成查询，重复请求应共用查询结果；版本、大小及必要函数指针校验应由假提供方和实际 probe 验证。能力矩阵应列明可选能力缺失时受影响的业务。

## C02 异步 VM 传输

支持实例和 Global 目标、参数复制、异步结果、超时、取消和一次完成。需要多项事实时可用薄 PSC 批量读取，返回自有 DTO；业务判定和计分留在核心。

交付要求：成功、失败、超时及取消应各完成一次请求；重入及旧 epoch 结果应按请求状态和会话校验处理；异步等待应在核心事务外执行。目标游戏应确认调用声明及返回时序。

## C03 SexLab P+

使用公开 VM 数据接口作为默认传输。原生接普通 AnimationStart、LeadInEnd、AnimationEnd，从 strArg 解析 thread ID；同一业务只消费这条通知通道。

读取实际角色、28 项交互 flags、tags 与注解的对应作用域。前戏切主场景时，事件后按新阶段 readiness 作有界重查/超时，结果应在 scene epoch 校验后提交。保留原十秒取消/换动画宽限，刷新与首次/后续计分分别调度。

交付要求：正常、前戏切换、取消、超时、读档及多场景应记录实际角色、交互和计分结果；动态事实待就绪时应暂停依赖该事实的新计分。源码依据见[审计](../../research/sexlab-pplus-2.18.1-integration-audit.md)。

## C04 DDNG

在 DataLoaded 后按需取得 v2 接口并验证实际设备数据；以槽位、Keyword 和已验证可用查询构造设备快照。研究中固定返回 false 的 `IsDevice`、`ActorHasBlockingGag` 包装记录为 unavailable。

交付要求：设备快照应反映目标版本中的阻塞口塞、腰带及装备变化；缺失能力应返回对应状态。每个采用函数应有可用性证据，dirty 事件及使用时刷新应采集当前所需数据。

## C05 RaceMenu

通过接口交换查询所需外观数据，按协议核对版本、布局、必要入口和所有权。验证目标 Steam 1.6.1170 安装版 ABI；本地源码的 1.7.99/GOG 1.6.1179 标识只作来源信息。

交付要求：采用的公共接口应在目标安装版通过最小查询 probe；待就绪数据应返回 unknown，并暂停依赖该外观事实的新计分。

## C06 SlaveTats NG / JC

以已研究的 SlaveTats NG 0.8.2 v1 为起点锁定实际版本。PostLoad 订阅、PostPostLoad 接收一次性广播；按生产端定义校验 dataLen/指针协议，纹身查询只使用已核实的共同前缀。v1 追加的 compile_cache 尾部留在本次需求之外。

按 JC retain/release 规则用 RAII 持有短期句柄，复制为 body/hands/feet/face 自有记录。装备及已验证外观事件置 dirty，在业务使用边界执行有界刷新。

交付要求：重复通知应复用已确认能力；换档及结束查询应按协议释放句柄；四部位实际应用记录应复制为自有数据。缺失/未知数据应返回对应状态，保留既有声望并暂停相应新计分。

## C07 其他外部状态

实现 FHU、Public Whore、Bimbos、FameComments 等原已支持事实的适配。区分安装事实、角色动态状态和本档外部注册；按所需公开入口经引擎或 VM port 传输。

交付要求：能力矩阵应列明每项依赖、实际查询及 unknown/取消/重载结果，并用对应原玩法场景验证计分行为。

## C08 SKSE 菜单

采用本地 `E:/source/repos/SKSE-Menu-Framework-3-master/resources/SKSEMenuFramework.h`，源码自报 3.8/API 1。C01 锁定实际 header/来源及目标 DLL，先完成最小注册 probe，再实现概览、类别/地点、配置、连接编辑和诊断。事件事实和具体流程见[配置与菜单设计](../../../.scratch/native-migration/configuration-design.md)。

框架 open/close 通知接入 Configuration；SLSF 控件发送编辑完成通知，自有窗口关闭也提交脏编辑。回调投递自有请求，配置顺序由现有执行器管理；before/after-render 每帧只消费缓存。操作声望走公开命令，用户设置和稳定连接偏好通过 JSON 更新。

交付要求：目标运行时应完成菜单显示、关闭和重开；修改/关闭及配置刷新后应显示已应用版本。UI 刷新/读档应读取当前跨档 JSON；同轮关闭请求应合并，渲染帧应读取缓存。连接界面应显示孤立节点及 unresolved 引用。

## C09 兼容资产与发布包

锁定 Papyrus compiler，用原公开声明生成薄 PSC/PEX，保留 `SLSF Reloaded.esp`、记录身份和 VMAD/alias/实例绑定。打包 DLL、插件、PEX、默认配置及所需许可；旧时钟、扫描业务和 MCM 由新入口接管。

在独立 staging 目录生成发布清单和哈希，记录依赖版本及实际运行时。默认配置支持首次生成，更新安装保留已有用户 JSON。冻结客户端使用替换前编译的 PEX。

交付要求：staging 应提供资产清单、导出接口、脚本声明、默认配置及许可；安装后应由原生模型及统一入口管理业务状态和调度。

## C10 目标游戏验收

先运行相关核心、边界及包检查，再在 Skyrim 1.6.1170/SKSE 2.2.6 验证冻结客户端、25 类、52 输入/7 回应、公开 Global/StorageUtil/JSON 和记录绑定。

验证新档、保存重载、退出重进、A→B、失败加载及加载顺序变化；覆盖跨档 JSON 设置/用户连接、计时、菜单、实际提供方、缺失/未知状态和旧回调取消。记录热查询、结算、dirty 刷新成本及可见玩法结果。

交付要求：项目验收表应逐项记录执行环境、结果及证据；待运行项目应列明所需环境和操作，核心检查与目标游戏检查应分别报告。

## 资料与记录

外部来源依据见[运行时/集成研究](../../research/migration-assessment.md)及对应审计；状态边界见[生命周期设计](../specs/2026-10-26-integration-state-lifecycle-design.md)。每项实际采用的版本、probe 命令、日志和结果随分组报告落入 evidence。
