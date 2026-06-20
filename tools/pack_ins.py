#!/usr/bin/env python3
"""
Pack a directory into an Eclipse32 .INS install package (Eclipse32 Install Stream).

Format (little-endian):
  'EINS' (4)   magic
  u16 version  (1)
  u16 nfiles
  for each file:
    u16 name_len
    name bytes (no NUL)
    u32 data_size
    data bytes

Usage:
  python3 tools/pack_ins.py <source_dir> <output.ins>

See tools/sample_ins/ for install.cfg + payload layout.
"""
import os
import struct
import sys

INS_MAGIC = b"EINS"
VERSION = 1


def gather_files(root: str) -> list[tuple[str, bytes]]:
    out: list[tuple[str, bytes]] = []
    root = os.path.abspath(root)
    for dirpath, _dirs, files in os.walk(root):
        for fn in files:
            full = os.path.join(dirpath, fn)
            rel = os.path.relpath(full, root).replace(os.sep, "/")
            with open(full, "rb") as f:
                out.append((rel, f.read()))
    out.sort(key=lambda x: x[0].lower())
    return out


def pack(src: str, dst: str) -> None:
    files = gather_files(src)
    if not files:
        print("pack_ins: no files in", src, file=sys.stderr)
        sys.exit(1)
    with open(dst, "wb") as o:
        o.write(INS_MAGIC)
        o.write(struct.pack("<HH", VERSION, len(files)))
        for name, data in files:
            nb = name.encode("utf-8")
            if len(nb) > 255:
                print("pack_ins: path too long:", name, file=sys.stderr)
                sys.exit(1)
            o.write(struct.pack("<H", len(nb)))
            o.write(nb)
            o.write(struct.pack("<I", len(data)))
            o.write(data)
    print("pack_ins:", dst, "files=", len(files))


def main() -> None:
    if len(sys.argv) != 3:
        print("usage: pack_ins.py <source_dir> <output.ins>", file=sys.stderr)
        sys.exit(2)
    pack(sys.argv[1], sys.argv[2])


if __name__ == "__main__":
    main()
