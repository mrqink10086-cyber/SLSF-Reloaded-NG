# NotReady/Stale 类别设置保留修复

日期：2026-10-27。范围：纯核心 `UpdateProvider` 及其回归测试。

## 问题与原因

旧实现把 NotReady/Stale 同时归入 removal 和 invalidation，撤销该提供者的全部类别设置，并在最后一个类别设置消失时为任意类别生成 wipe 请求。测试还用普通 Nasty 类验证了这条路径。

原规则的自动清理只涉及 CumDump、Unfaithful、Cuck、Airhead，并要求内置来源不存在且外部类别设置全无。临时查询未就绪与已确认的提供者失效是不同状态。

## 修改

- `src/core/reputation/ProviderRegistry.cpp`：NotReady/Stale 校验输入后返回相应 Status，保留注册、计数和清理阶段；确认失效仍撤销对应类别设置。
- 最后类别设置消失时，自动待清理集合只包含四种可选类别。业务层接入时还需检查内置来源条件。
- `tests/core/ProviderRegistryTests.cpp`：补充未知→重复未知→恢复→真正失效，以及全部 25 类中只有四类产生候选的回归。保留多提供者、显式撤销和容量检查。

## 先复现再修复

修改测试后、修复实现前，执行：

```powershell
xmake build -y SLSFCoreTests
xmake run SLSFCoreTests "[providers]" --rng-seed 1170
```

结果 exit 1，10 个用例中 2 个失败：临时未知后 `ActiveProviderCount(optional)==1` 实际为 0；全部类别的失效候选包含普通类别。该结果复现了类别设置丢失和清理范围扩大的问题。

## 修复后结果

| 命令 | 结果 |
|---|---|
| `xmake run SLSFCoreTests "[providers]" --rng-seed 1170` | exit 0；10 cases / 386 assertions |
| `powershell -NoProfile -ExecutionPolicy Bypass -File tools/run-native-tests.ps1 -Mode core -Seed 1170` | exit 0；47 cases / 100397 assertions |
| `powershell -NoProfile -ExecutionPolicy Bypass -File tools/run-native-tests.ps1 -Mode asan -Seed 1170` | exit 0；核心 47 cases / 100397 assertions；边界 4 cases / 22 assertions |

ASan 正向探针为 `probe=42 index=7`；故意越界的负向探针在 `tests/asan/AsanProbe.cpp:15` 被检测为 heap-buffer-overflow，随后核心/边界检查通过。

JUnit：`build/test-results/core.xml`、`build/test-results/core-asan.xml`、`build/test-results/boundary-asan.xml`。本轮修改直接复用公开更新入口验证核心行为；游戏运行时尚待接入。
