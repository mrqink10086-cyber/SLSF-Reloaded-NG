# 发布包中文文件名 UTF-8 标志核查

日期：2026-11-06，修正于 2026-11-10。

## 结论

之前记录的「0.1.0 包 75 个条目中有 64 个缺少 zip 通用位 11，中文文件名为 GBK 字节」不成立。

实测已发布的 0.1.0 资产（`SLSF-Reloaded-NG-0.1.0-zhCN-test.zip`，1,023,780 字节，SHA256 `8b053badec8659f1b70924caed4a806c8ce59cdcb6479928054dea9e578fc60e`）：

- 75 个条目 = 64 个纯 ASCII 名 + 11 个非 ASCII 名。
- 11 个非 ASCII 条目全部带通用位 11（`0x800`），内容就是 UTF-8 字节。
- 64 个不带该位的是纯 ASCII 文件名；按 zip 规范只有出现非 ASCII 名时才需要设置这一位，它们本就不该有。

原文把「不带 `0x800` 的条目数」当成了「中文名条目数」，于是把 64 个 ASCII 条目误判成 64 个乱码的中文名。

## 复核方法

```python
import zipfile
z = zipfile.ZipFile("SLSF-Reloaded-NG-0.1.0-zhCN-test.zip")
tot = na = 0
for i in z.infolist():
    tot += 1
    if any(ord(c) > 127 for c in i.filename):
        na += 1
        assert i.flag_bits & 0x800, i.filename
print(tot, na)   # 75 11
```

同样的检查对 0.1.1、0.1.2、0.1.3、0.1.4 的发布包结果一致：非 ASCII 条目全部带标志，缺失数为 0。

## `tools/make-zip.py` 的定位

该脚本在 0.1.1 加入，行为不变：按路径排序写入，结束时复核所有非 ASCII 条目都带 `0x800`，否则以非零退出。它现在是**防御性校验**，不是缺陷修复——0.1.0 的包本来就是对的。保留它是因为 Windows PowerShell 5.1 的 `[IO.Compression.ZipFile]::CreateEntryFromFile` 确实不设置这一位，换用别的建包路径时仍可能踩到。

## 边界

- 0.1.0 与 0.1.1 的发布资产未重打，哈希不变。
- 包内 `文件清单.json`、`SHA256校验.txt` 与压缩容器编码无关，不受影响。
