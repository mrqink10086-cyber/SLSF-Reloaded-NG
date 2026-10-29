# owner 长链与多核测试性能修复

日期：2026-10-28。CPU：Intel Core i5-13600KF，14 核、20 逻辑处理器。固定 seed：1170。

## 问题与修复

原 `BindRegions` 对每个区域重新沿 `childOwner` map 走到根。长链反复访问相同尾部，owner 求解最坏有 O(n²) 次跳转，每跳另有 map 查找成本。01 组历史报告记录普通版 20000 节点用例约 32 秒；全量 ASan 曾在该用例上长时间运行。

`src/core/location/RegionRegistry.cpp` 现在先将 owner 编译成连续 RegionId 数组，再迭代求根：活动标记检查环，已求根直接复用，整段路径写入相同 canonical owner。每个节点最多进入路径一次，owner 求解时间及辅助空间均为 O(n)。完整绑定还包含稳定键排序、map 校验、alias 及地点归并。

`tests/core/RegionRegistryTests.cpp` 保留原归属、优先级和公开索引检查，新增或扩展正向/反向 20000 节点链、共享尾部、独立根，以及没有地点引用的 20000 节点环。单条链采用顺序求解，独立测试用例交给不同 CPU 执行。

`tools/run-native-tests.ps1` 增加 `-Jobs`，默认使用逻辑 CPU 数，并发上限为用例数。Catch2 使用相同 seed、`--order rand`、`--shard-count` 和 `--shard-index` 分片。各用例执行一次，同时输出 console/JUnit；脚本等待全部分片、汇总 XML 并检查退出码。固定随机排序分散同一源文件中的重用例。游戏运行时继续使用统一业务执行器。

## 相同绑定输入的前后比较

诊断直接调用生产 `BindRegions`：N 个 `region.esp` 区域，编号 1…N，区域 i 拥有 i+1，仅末端区域有一个实际地点；结果应归区域 1。计时覆盖完整绑定及结果构造，输入构造在计时外。普通版使用 MSVC `/O2`；ASan 使用 LLVM 20 clang-cl `/Od /Zi /fsanitize=address`。以下为各规模单次测量，单位 ms。

| 节点数 | 普通版改前 | 普通版改后 | ASan 改前 | ASan 改后 |
|---:|---:|---:|---:|---:|
| 512 | 16.2511 | 0.7766 | 152.545 | 12.0851 |
| 1024 | 75.4433 | 1.6611 | 634.720 | 24.4007 |
| 20000 | — | 39.2235 | — | 591.790 |

改前从 512 加倍到 1024，耗时约增至 4 倍；改后约增至 2 倍。20000 节点改前采用 01 组历史用例记录，表中的空项表示本次诊断未运行该测量。1024 节点完整绑定本次普通版约加速 45 倍，ASan 约加速 26 倍。

## 套件执行结果

所有核心运行均为 69 cases / 100918 assertions；边界为 4 cases / 22 assertions，exit 0。ASan 正向探针输出 `probe=42 index=7`，故意越界的负向探针返回非零并报告 `heap-buffer-overflow`。正常和 ASan 目标保留各自原编译选项。

| 执行方式 | 核心套件耗时 | 说明 |
|---|---:|---|
| 普通版，1 分片 | 0.601 s | 修复后单进程 |
| 普通版，20 分片 | 0.466 s | 默认并发，固定随机顺序 |
| ASan，1 分片 | 11.439 s | 首轮串行测量 |
| ASan，1 分片、固定随机顺序 | 11.593 s | 最终脚本串行复核，与多核采用相同顺序 |
| ASan，4 分片、声明顺序 | 9.518 s | 两个随机森林重用例仍位于同一分片 |
| ASan，20 分片、声明顺序 | 9.214 s | 同一重用例分片限制总耗时 |
| ASan，20 分片、固定随机顺序 | 4.931 s | 最终方案 |

耗时从测试子进程启动前计至结果合并，构建、发现和 ASan 探针另计。最终多核核心报告核对为 69 个唯一用例、100918 条断言，全部分片合并后与串行数量一致。ASan 地点标签单进程运行为 16 cases / 97079 assertions，11.372 秒，包括两种 20000 节点链、长环及随机森林对照。

相同最终脚本、seed 和排序下，ASan 核心本次 1→20 分片由 11.593 秒降至 4.931 秒，约加速 2.35 倍。

## 复现与交接

在项目根目录执行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/run-native-tests.ps1 -Mode core -Seed 1170
powershell -NoProfile -ExecutionPolicy Bypass -File tools/run-native-tests.ps1 -Mode boundary -Seed 1170
powershell -NoProfile -ExecutionPolicy Bypass -File tools/run-native-tests.ps1 -Mode asan -Seed 1170 -Jobs 1
powershell -NoProfile -ExecutionPolicy Bypass -File tools/run-native-tests.ps1 -Mode asan -Seed 1170
```

最终命令日志保存为 `docs/evidence/task-groups/logs/2026-10-28-owner-{core,boundary,asan-parallel,asan-serial}.log`。运行期报告保存在 `build/test-results/{core,boundary,core-asan,boundary-asan}.xml`，分片保留在对应 `*-shards/` 目录。重放单个分片使用相同 seed、随机顺序、分片数和编号，具体命令见[测试说明](../superpowers/plans/2026-10-27-task-books/00-native-testing.md)。

02 组继续完成每个地区的声望数据、类别集合、公开查询快照与完整 Capture/恢复；本次修复提供其地点绑定和测试执行基础。
