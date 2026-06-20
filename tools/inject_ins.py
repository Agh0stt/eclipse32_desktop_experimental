#!/usr/bin/env python3
"""
Inject a .INS (or any file) into the FAT32 root of an eclipse32 raw disk image.

This is a thin wrapper around inject_fat32_file.py so you can document app/package
drops separately from plain E32 binaries.

Usage:
  python3 tools/inject_ins.py <disk.img> <source.ins> [ROOTNAME.INS]

If ROOTNAME is omitted, the basename of source is uppercased (8.3-friendly).
"""
import os
import subprocess
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))


def main() -> None:
    if len(sys.argv) < 3:
        print(
            f"usage: {sys.argv[0]} <disk.img> <file.ins> [dest_8.3_name]",
            file=sys.stderr,
        )
        sys.exit(1)
    img = sys.argv[1]
    src = sys.argv[2]
    if len(sys.argv) >= 4:
        dst = sys.argv[3]
    else:
        dst = os.path.basename(src).upper()
    inj = os.path.join(SCRIPT_DIR, "inject_fat32_file.py")
    subprocess.check_call([sys.executable, inj, img, src, dst])
    print(f"[inject_ins] root ← {src} as {dst}")


if __name__ == "__main__":
    main()
