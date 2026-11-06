#!/usr/bin/env python3
"""Create the release archive with UTF-8 flagged entry names.

Windows PowerShell 5.1's [IO.Compression.ZipFile]::CreateEntryFromFile writes
non-ASCII entry names as UTF-8 bytes without setting the "language encoding
flag" (general purpose bit 11). Extractors then fall back to the system ANSI
code page and show mojibake for the Chinese file names. Python's zipfile sets
the flag whenever a name is not pure ASCII, so the archive is built here.

Usage: python tools/make-zip.py <staging-dir> <archive-path>
"""

from __future__ import annotations

import os
import sys
import zipfile


def main() -> int:
    if len(sys.argv) != 3:
        print(__doc__.strip().splitlines()[-1], file=sys.stderr)
        return 2
    stage = os.path.abspath(sys.argv[1])
    archive = os.path.abspath(sys.argv[2])
    if not os.path.isdir(stage):
        print(f"staging directory missing: {stage}", file=sys.stderr)
        return 1

    entries: list[tuple[str, str]] = []
    for root, _dirs, files in os.walk(stage):
        for name in files:
            full = os.path.join(root, name)
            rel = os.path.relpath(full, stage).replace(os.sep, "/")
            entries.append((rel, full))
    entries.sort(key=lambda item: item[0])

    os.makedirs(os.path.dirname(archive) or ".", exist_ok=True)
    if os.path.exists(archive):
        os.remove(archive)

    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as zf:
        for rel, full in entries:
            info = zipfile.ZipInfo.from_file(full, rel)
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = (0o644 << 16) | 0x20
            with open(full, "rb") as handle:
                zf.writestr(info, handle.read())

    with zipfile.ZipFile(archive) as check:
        bad = [i.filename for i in check.infolist()
               if not i.filename.isascii() and not (i.flag_bits & 0x800)]
        if bad:
            print(f"entries missing the UTF-8 flag: {len(bad)}", file=sys.stderr)
            return 1
    print(f"ZIP_ENTRIES={len(entries)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
