#!/usr/bin/env python3
"""
injectsh.py  —  Eclipse32 tool
Injects an .E32 binary into the /bin directory of the FAT32 partition.
The shell resolves bare command names (e.g. "sayhello") by appending ".E32"
and looking in /bin, so this is the right place to deploy shell commands.

Usage:
    python3 tools/injectsh.py <disk.img> <src.E32> [dest_name]

    src.E32    — path to the compiled .E32 file on the host
    dest_name  — optional FAT 8.3 name to use inside /bin
                 • if omitted, the source filename (uppercased) is used
                 • ".E32" is appended automatically if not already present
                 • only the base part may be up to 8 chars (FAT limit)

Examples:
    python3 tools/injectsh.py build/eclipse32.img build/apps/SAYHELLO.E32
        → injects as /bin/SAYHELLO.E32

    python3 tools/injectsh.py build/eclipse32.img build/apps/SAYHELLO.E32 sayhello
        → injects as /bin/SAYHELLO.E32  (extension added, name uppercased)

    python3 tools/injectsh.py build/eclipse32.img build/apps/MYPROG.E32 myprog.e32
        → injects as /bin/MYPROG.E32
"""

import math
import os
import struct
import sys

SECTOR_SIZE = 512


# ---------------------------------------------------------------------------
# Low-level FAT32 helpers
# ---------------------------------------------------------------------------

def read_bpb(img, part_lba):
    img.seek(part_lba * SECTOR_SIZE)
    bpb = img.read(SECTOR_SIZE)
    bps = struct.unpack_from("<H", bpb, 11)[0]
    if bps != SECTOR_SIZE:
        raise RuntimeError("Only 512-byte sectors supported")
    spc          = bpb[13]
    rsc          = struct.unpack_from("<H", bpb, 14)[0]
    nfats        = bpb[16]
    spf          = struct.unpack_from("<I", bpb, 36)[0]
    root_cluster = struct.unpack_from("<I", bpb, 44)[0]
    fat_start    = part_lba + rsc
    data_start   = fat_start + nfats * spf
    cluster_bytes = spc * SECTOR_SIZE
    return dict(
        spc=spc, rsc=rsc, nfats=nfats, spf=spf,
        fat_start=fat_start, data_start=data_start,
        root_cluster=root_cluster, cluster_bytes=cluster_bytes,
        part_lba=part_lba,
    )


def fat_get(img, bpb, cluster):
    off = bpb["fat_start"] * SECTOR_SIZE + cluster * 4
    img.seek(off)
    return struct.unpack("<I", img.read(4))[0] & 0x0FFFFFFF


def fat_set(img, bpb, cluster, value):
    raw = struct.pack("<I", value & 0x0FFFFFFF)
    for i in range(bpb["nfats"]):
        off = (bpb["fat_start"] + i * bpb["spf"]) * SECTOR_SIZE + cluster * 4
        img.seek(off)
        img.write(raw)


def cluster_offset(bpb, cluster):
    return (bpb["data_start"] + (cluster - 2) * bpb["spc"]) * SECTOR_SIZE


def read_cluster(img, bpb, cluster):
    img.seek(cluster_offset(bpb, cluster))
    return bytearray(img.read(bpb["cluster_bytes"]))


def write_cluster(img, bpb, cluster, data):
    img.seek(cluster_offset(bpb, cluster))
    img.write(data)


def alloc_cluster(img, bpb):
    """Find the first free cluster (starting at 3) and mark it EOC."""
    # Scan FAT for a free entry
    fat_bytes = bpb["nfats"] * bpb["spf"] * SECTOR_SIZE  # upper bound
    total_clusters = (fat_bytes // 4)  # generous upper bound
    for c in range(3, total_clusters):
        if fat_get(img, bpb, c) == 0x00000000:
            fat_set(img, bpb, c, 0x0FFFFFFF)  # EOC
            return c
    raise RuntimeError("FAT32 volume is full — no free clusters")


# ---------------------------------------------------------------------------
# Directory helpers
# ---------------------------------------------------------------------------

def to_8_3(name):
    """Convert a filename to a FAT 8.3 directory-entry name (11 bytes)."""
    upper = name.upper()
    if "." in upper:
        base, ext = upper.rsplit(".", 1)
    else:
        base, ext = upper, ""
    if len(base) > 8:
        raise ValueError(f"Base name '{base}' is longer than 8 characters (FAT limit)")
    base = base[:8].ljust(8)
    ext  = ext[:3].ljust(3)
    return (base + ext).encode("ascii")


def dir_entries(data):
    """Yield (offset, entry_bytes) for every 32-byte slot in a directory cluster."""
    for i in range(0, len(data), 32):
        yield i, data[i:i + 32]


def find_subdir_cluster(img, bpb, parent_cluster, name_upper):
    """
    Walk the directory chain rooted at parent_cluster looking for a
    subdirectory whose short name matches name_upper (e.g. 'BIN').
    Returns the first cluster of that subdir, or None if not found.
    """
    cluster = parent_cluster
    while cluster < 0x0FFFFFF8:
        data = read_cluster(img, bpb, cluster)
        for off, entry in dir_entries(data):
            if entry[0] in (0x00, 0xE5):
                continue
            attr = entry[11]
            if not (attr & 0x10):          # not a directory
                continue
            short = entry[0:11].decode("ascii", errors="replace")
            entry_name = short[0:8].rstrip() + ("." + short[8:11].rstrip() if short[8:11].strip() else "")
            entry_name8 = short[0:8].rstrip()   # dirs typically have no extension
            if entry_name8 == name_upper or entry_name == name_upper:
                hi = struct.unpack_from("<H", entry, 20)[0]
                lo = struct.unpack_from("<H", entry, 26)[0]
                return (hi << 16) | lo
        cluster = fat_get(img, bpb, cluster)
    return None


def create_subdir(img, bpb, parent_cluster, name_upper):
    """
    Create a subdirectory named name_upper inside the directory whose
    first cluster is parent_cluster.  Returns the new subdir's first cluster.
    """
    # Allocate a cluster for the new directory
    new_cluster = alloc_cluster(img, bpb)

    # Zero the cluster and write . and .. entries
    dir_data = bytearray(bpb["cluster_bytes"])

    def make_dot_entry(name11, first_cluster, attr=0x10):
        de = bytearray(32)
        de[0:11] = name11.encode("ascii")
        de[11] = attr
        de[20:22] = struct.pack("<H", (first_cluster >> 16) & 0xFFFF)
        de[26:28] = struct.pack("<H", first_cluster & 0xFFFF)
        return de

    dir_data[0:32]  = make_dot_entry(".          ", new_cluster)
    dir_data[32:64] = make_dot_entry("..         ", parent_cluster)
    write_cluster(img, bpb, new_cluster, dir_data)

    # Add a directory entry in the parent
    _add_dir_entry(img, bpb, parent_cluster,
                   name11=name_upper[:8].ljust(8) + "   ",
                   first_cluster=new_cluster,
                   size=0,
                   attr=0x10)   # ATTR_DIRECTORY

    print(f"  Created directory /{name_upper} (cluster {new_cluster})")
    return new_cluster


def _add_dir_entry(img, bpb, dir_cluster, name11, first_cluster, size, attr=0x20):
    """
    Write a 32-byte directory entry into the directory chain rooted at
    dir_cluster.  Extends the chain with a new cluster if needed.
    name11 must be exactly 11 ASCII characters (8.3 padded).
    """
    cluster = dir_cluster
    prev_cluster = None
    while True:
        data = read_cluster(img, bpb, cluster)
        for off, entry in dir_entries(data):
            if entry[0] in (0x00, 0xE5):
                # Free slot — write the entry here
                de = bytearray(32)
                de[0:11] = name11.encode("ascii") if isinstance(name11, str) else name11
                de[11] = attr
                de[20:22] = struct.pack("<H", (first_cluster >> 16) & 0xFFFF)
                de[26:28] = struct.pack("<H", first_cluster & 0xFFFF)
                de[28:32] = struct.pack("<I", size)
                data[off:off + 32] = de
                write_cluster(img, bpb, cluster, data)
                return
        # No free slot — extend the cluster chain
        next_c = fat_get(img, bpb, cluster)
        if next_c >= 0x0FFFFFF8:
            # Allocate and link a new cluster
            new_c = alloc_cluster(img, bpb)
            fat_set(img, bpb, cluster, new_c)   # link old → new
            # fat_set already set new_c → EOC via alloc_cluster
            new_data = bytearray(bpb["cluster_bytes"])
            write_cluster(img, bpb, new_c, new_data)
            cluster = new_c
        else:
            cluster = next_c


# ---------------------------------------------------------------------------
# File injection
# ---------------------------------------------------------------------------

def inject_file(img, bpb, dir_cluster, fat32_name, data):
    """Inject data bytes into dir_cluster under the FAT 8.3 name fat32_name."""
    size = len(data)
    cluster_bytes = bpb["cluster_bytes"]
    need = math.ceil(size / cluster_bytes) if size else 1

    # Allocate a chain of clusters
    chain = []
    for _ in range(need):
        c = alloc_cluster(img, bpb)
        if chain:
            fat_set(img, bpb, chain[-1], c)  # link previous → this
        chain.append(c)
    # Last cluster is already EOC from alloc_cluster

    # Write file data
    for i, c in enumerate(chain):
        chunk = data[i * cluster_bytes:(i + 1) * cluster_bytes]
        chunk = chunk.ljust(cluster_bytes, b"\x00")   # zero-pad last cluster
        img.seek(cluster_offset(bpb, c))
        img.write(chunk)

    # Add directory entry
    name11_bytes = to_8_3(fat32_name)
    name11_str   = name11_bytes.decode("ascii")
    _add_dir_entry(img, bpb, dir_cluster,
                   name11=name11_str,
                   first_cluster=chain[0],
                   size=size,
                   attr=0x20)   # ATTR_ARCHIVE


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    if len(sys.argv) < 3 or len(sys.argv) > 4:
        print(__doc__, file=sys.stderr)
        sys.exit(1)

    img_path  = sys.argv[1]
    src_path  = sys.argv[2]
    dest_name = sys.argv[3] if len(sys.argv) == 4 else os.path.basename(src_path)

    # Normalise dest_name: uppercase, ensure .E32 extension
    dest_name = dest_name.upper()
    if not dest_name.endswith(".E32"):
        dest_name += ".E32"

    with open(src_path, "rb") as f:
        file_data = f.read()

    if not file_data:
        print("ERROR: source file is empty", file=sys.stderr)
        sys.exit(1)

    with open(img_path, "r+b") as img:
        # Read MBR to find partition LBA
        img.seek(0)
        mbr = img.read(SECTOR_SIZE)
        part_lba = struct.unpack_from("<I", mbr, 0x1BE + 8)[0]

        bpb = read_bpb(img, part_lba)
        root_cluster = bpb["root_cluster"]

        # Find or create /bin
        bin_cluster = find_subdir_cluster(img, bpb, root_cluster, "BIN")
        if bin_cluster is None:
            print("  /bin not found — creating it now")
            bin_cluster = create_subdir(img, bpb, root_cluster, "BIN")

        # Inject the file
        inject_file(img, bpb, bin_cluster, dest_name, file_data)

    print(f"Injected {src_path} → /bin/{dest_name} on {img_path}")


if __name__ == "__main__":
    main()
