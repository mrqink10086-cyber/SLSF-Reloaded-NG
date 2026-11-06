# 发布包中文文件名 UTF-8 修复

日期：2026-11-06。针对 [0.1.0 交付](../evidence/2026-11-05-chinese-mo2-package.md)中记录的问题：0.1.0 包 75 个条目中有 64 个缺少 zip 通用位 11（language encoding flag），中文文件名按 GBK 字节写出，非中文区域系统解压显示乱码。

## 原因

`tools/build-staging.ps1` 原先用 Windows PowerShell 5.1 的 `[IO.Compression.ZipFile]::Open` 与 `CreateEntryFromFile` 建包。该实现把非 ASCII 名称按 UTF-8 字节写入，但不设置通用位 11，解压端于是回退到系统 ANSI 代码页。

## 改动

- 新增 `tools/make-zip.py`：按路径排序遍历暂存目录，用 `zipfile` 逐条写入，并在结束时复核所有非 ASCII 条目都带 `0x800` 标志，否则以非零退出。
- `tools/build-staging.ps1` 的压缩段改为调用该脚本；`python` 缺失或建包失败都会直接报错，不再静默产出坏包。

## 验证

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/build-staging.ps1
```

退出 0，输出 `ZIP_ENTRIES=75` 与 `ARCHIVE_SHA256=…`；用 `zipfile` 复查 0.1.1 包，75 个条目中缺少 UTF-8 标志的条目为 0。包内 `文件清单.json` 与 `SHA256校验.txt` 的内容不受压缩容器编码影响。

## 边界

只改变压缩包条目名的编码标志，文件字节、条目数量、目录结构与清单摘要均不变。旧版 0.1.0 包未重打。
