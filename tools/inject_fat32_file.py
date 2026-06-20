#!/usr/bin/env python3
import math
import struct
import sys

SECTOR_SIZE = 512


def fat_entry_get(img, fat_start_lba, cluster):
    off = fat_start_lba * SECTOR_SIZE + cluster * 4
    img.seek(off)
    return struct.unpack("<I", img.read(4))[0] & 0x0FFFFFFF


def fat_entry_set(img, fat_start_lba, nfats, sectors_per_fat, cluster, value):
    raw = struct.pack("<I", value & 0x0FFFFFFF)
    for i in range(nfats):
        fat_lba = fat_start_lba + i * sectors_per_fat
        off = fat_lba * SECTOR_SIZE + cluster * 4
        img.seek(off)
        img.write(raw)


def to_8_3(name):
    upper = name.upper()
    if "." in upper:
        base, ext = upper.split(".", 1)
    else:
        base, ext = upper, ""
    base = base[:8].ljust(8, " ")
    ext = ext[:3].ljust(3, " ")
    return (base + ext).encode("ascii")


def main():
    if len(sys.argv) != 4:
        print(f"Usage: {sys.argv[0]} <disk.img> <src_file> <dest_8.3_name>", file=sys.stderr)
        sys.exit(1)

    img_path, src_path, dst_name = sys.argv[1], sys.argv[2], sys.argv[3]
    with open(src_path, "rb") as f:
        data = f.read()
    size = len(data)
    if size == 0:
        print("Refusing empty file injection", file=sys.stderr)
        sys.exit(1)

    with open(img_path, "r+b") as img:
        mbr = img.read(SECTOR_SIZE)
        part = mbr[0x1BE:0x1BE + 16]
        part_lba = struct.unpack("<I", part[8:12])[0]

        img.seek(part_lba * SECTOR_SIZE)
        bpb = img.read(SECTOR_SIZE)
        bytes_per_sector = struct.unpack("<H", bpb[11:13])[0]
        if bytes_per_sector != SECTOR_SIZE:
            raise RuntimeError("Only 512-byte sectors supported")
        spc = bpb[13]
        rsc = struct.unpack("<H", bpb[14:16])[0]
        nfats = bpb[16]
        sectors_per_fat = struct.unpack("<I", bpb[36:40])[0]
        root_cluster = struct.unpack("<I", bpb[44:48])[0]
        total_sectors = struct.unpack("<I", bpb[32:36])[0]

        fat_start_lba = part_lba + rsc
        data_start_lba = fat_start_lba + nfats * sectors_per_fat
        total_clusters = (total_sectors - (rsc + nfats * sectors_per_fat)) // spc

        cluster_bytes = spc * SECTOR_SIZE
        need_clusters = math.ceil(size / cluster_bytes)

        free_clusters = []
        for c in range(3, total_clusters + 2):
            if fat_entry_get(img, fat_start_lba, c) == 0:
                free_clusters.append(c)
                if len(free_clusters) >= need_clusters:
                    break
        if len(free_clusters) < need_clusters:
            raise RuntimeError("Not enough free clusters")

        for i, c in enumerate(free_clusters):
            nxt = free_clusters[i + 1] if i + 1 < len(free_clusters) else 0x0FFFFFFF
            fat_entry_set(img, fat_start_lba, nfats, sectors_per_fat, c, nxt)

        remain = size
        p = 0
        for c in free_clusters:
            lba = data_start_lba + (c - 2) * spc
            off = lba * SECTOR_SIZE
            img.seek(off)
            chunk = data[p:p + cluster_bytes]
            p += len(chunk)
            remain -= len(chunk)
            if len(chunk) < cluster_bytes:
                chunk += b"\x00" * (cluster_bytes - len(chunk))
            img.write(chunk)

        root_lba = data_start_lba + (root_cluster - 2) * spc
        root_off = root_lba * SECTOR_SIZE
        img.seek(root_off)
        root_data = bytearray(img.read(cluster_bytes))

        slot = None
        for i in range(0, len(root_data), 32):
            if root_data[i] in (0x00, 0xE5):
                slot = i
                break
        if slot is None:
            raise RuntimeError("No free root directory entry")

        de = bytearray(32)
        de[0:11] = to_8_3(dst_name)
        de[11] = 0x20
        first_cluster = free_clusters[0]
        de[20:22] = struct.pack("<H", (first_cluster >> 16) & 0xFFFF)
        de[26:28] = struct.pack("<H", first_cluster & 0xFFFF)
        de[28:32] = struct.pack("<I", size)
        root_data[slot:slot + 32] = de

        img.seek(root_off)
        img.write(root_data)

    print(f"Injected {src_path} as {dst_name}")


if __name__ == "__main__":
    main()
