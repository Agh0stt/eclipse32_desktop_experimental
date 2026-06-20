#!/usr/bin/env python3
# =============================================================================
# Eclipse32 - mkdisk.py
# Creates FAT32 partition at LBA 2048 inside an existing raw disk image,
# then writes KERNEL.BIN as the first file.
# Usage: mkdisk.py <disk.img> <kernel.bin>
# =============================================================================

import sys
import os
import struct
import math

SECTOR_SIZE     = 512
FAT32_START_LBA = 6144          # post-kernel + splash region
SECTORS_PER_CLUSTER = 8         # 4KB clusters
RESERVED_SECTORS = 32
FAT_COUNT        = 2
FAT32_SIG        = 0xAA55
VOLUME_LABEL     = b"ECLIPSE32  "  # 11 bytes

def write_at(f, offset, data):
    f.seek(offset)
    f.write(data)

def read_at(f, offset, size):
    f.seek(offset)
    return f.read(size)

def make_fat32_partition(img_path, kernel_path):
    kernel_data = open(kernel_path, 'rb').read()
    kernel_size = len(kernel_data)

    img_size = os.path.getsize(img_path)
    total_img_sectors = img_size // SECTOR_SIZE
    partition_sectors = total_img_sectors - FAT32_START_LBA

    # Calculate FAT size
    # data_sectors = partition_sectors - reserved - fat_count * fat_sectors
    # clusters = data_sectors / sectors_per_cluster
    # fat_entries = clusters + 2  (4 bytes each)
    # fat_sectors = ceil(fat_entries * 4 / 512)

    # Estimate
    data_sectors_estimate = partition_sectors - RESERVED_SECTORS
    clusters_estimate = data_sectors_estimate // SECTORS_PER_CLUSTER
    fat_sectors = math.ceil((clusters_estimate + 2) * 4 / SECTOR_SIZE)
    fat_sectors = max(fat_sectors, 9)  # minimum

    data_start = RESERVED_SECTORS + FAT_COUNT * fat_sectors
    data_sectors = partition_sectors - data_start
    total_clusters = data_sectors // SECTORS_PER_CLUSTER

    print(f"[mkdisk] FAT32 partition at LBA {FAT32_START_LBA}")
    print(f"[mkdisk] Partition sectors: {partition_sectors}")
    print(f"[mkdisk] FAT sectors: {fat_sectors} x{FAT_COUNT}")
    print(f"[mkdisk] Data start: sector {data_start} (relative to partition)")
    print(f"[mkdisk] Total clusters: {total_clusters}")

    with open(img_path, 'r+b') as f:
        # ---- Write MBR partition entry ----
        # Partition entry at 0x1BE (first entry)
        # Type 0x0C = FAT32 LBA
        pe = struct.pack('<BBBBBBBBII',
            0x80,           # bootable
            0xFE, 0xFF, 0xFF,  # CHS start (LBA mode, ignored)
            0x0C,           # type: FAT32 LBA
            0xFE, 0xFF, 0xFF,  # CHS end (ignored)
            FAT32_START_LBA,   # LBA start
            partition_sectors  # sector count
        )
        write_at(f, 0x1BE, pe)
        # MBR signature
        write_at(f, 510, struct.pack('<H', FAT32_SIG))

        # ---- Build FAT32 VBR (Volume Boot Record) ----
        vbr = bytearray(SECTOR_SIZE)

        # Jump + NOP
        vbr[0:3] = b'\xEB\x58\x90'
        # OEM name
        vbr[3:11] = b'ECLIPSE '

        # BPB
        struct.pack_into('<H', vbr, 11, SECTOR_SIZE)         # bytes per sector
        vbr[13] = SECTORS_PER_CLUSTER
        struct.pack_into('<H', vbr, 14, RESERVED_SECTORS)     # reserved sectors
        vbr[16] = FAT_COUNT
        struct.pack_into('<H', vbr, 17, 0)                    # root entry count (0 for FAT32)
        struct.pack_into('<H', vbr, 19, 0)                    # total sectors 16 (0 for FAT32)
        vbr[21] = 0xF8                                         # media type
        struct.pack_into('<H', vbr, 22, 0)                    # sectors per FAT 16 (0 for FAT32)
        struct.pack_into('<H', vbr, 24, 63)                   # sectors per track
        struct.pack_into('<H', vbr, 26, 255)                  # head count
        struct.pack_into('<I', vbr, 28, FAT32_START_LBA)      # hidden sectors
        struct.pack_into('<I', vbr, 32, partition_sectors)    # total sectors 32

        # FAT32 extended BPB
        struct.pack_into('<I', vbr, 36, fat_sectors)          # sectors per FAT
        struct.pack_into('<H', vbr, 40, 0)                    # ext flags
        struct.pack_into('<H', vbr, 42, 0)                    # FS version
        struct.pack_into('<I', vbr, 44, 2)                    # root cluster (cluster 2)
        struct.pack_into('<H', vbr, 48, 1)                    # FS info sector
        struct.pack_into('<H', vbr, 50, 6)                    # backup boot sector
        vbr[64] = 0x80                                         # drive number
        vbr[66] = 0x29                                         # boot signature
        struct.pack_into('<I', vbr, 67, 0xEC320001)           # volume ID
        vbr[71:82] = VOLUME_LABEL
        vbr[82:90] = b'FAT32   '

        # Signature
        struct.pack_into('<H', vbr, 510, FAT32_SIG)

        # Write VBR
        vbr_lba = FAT32_START_LBA
        write_at(f, vbr_lba * SECTOR_SIZE, bytes(vbr))

        # Write backup VBR at sector 6
        write_at(f, (vbr_lba + 6) * SECTOR_SIZE, bytes(vbr))

        # ---- Initialize FATs ----
        fat1_start = FAT32_START_LBA + RESERVED_SECTORS
        fat2_start = fat1_start + fat_sectors

        # First three FAT entries: media + EOC + EOC
        fat_header = struct.pack('<III',
            0x0FFFFFF8,   # entry 0: media byte
            0x0FFFFFFF,   # entry 1: EOC
            0x0FFFFFFF    # entry 2: root dir cluster (EOC - single cluster for now)
        )

        # Calculate how many clusters kernel needs
        kernel_clusters = math.ceil(kernel_size / (SECTORS_PER_CLUSTER * SECTOR_SIZE))
        kernel_clusters = max(1, kernel_clusters)

        print(f"[mkdisk] Kernel size: {kernel_size} bytes = {kernel_clusters} clusters")

        # Build FAT: entries 3..3+kernel_clusters-1 form a chain
        fat_data = bytearray(fat_sectors * SECTOR_SIZE)
        struct.pack_into('<I', fat_data, 0,  0x0FFFFFF8)    # entry 0
        struct.pack_into('<I', fat_data, 4,  0x0FFFFFFF)    # entry 1
        struct.pack_into('<I', fat_data, 8,  0x0FFFFFFF)    # entry 2: root dir (EOC)

        # Kernel clusters: 3, 4, 5, ... chain
        kernel_first_cluster = 3
        for i in range(kernel_clusters):
            cluster_num = kernel_first_cluster + i
            if i < kernel_clusters - 1:
                next_cluster = cluster_num + 1
            else:
                next_cluster = 0x0FFFFFFF  # EOC
            struct.pack_into('<I', fat_data, cluster_num * 4, next_cluster)

        # Write both FATs
        write_at(f, fat1_start * SECTOR_SIZE, bytes(fat_data))
        write_at(f, fat2_start * SECTOR_SIZE, bytes(fat_data))

        # ---- Build root directory ----
        # Root directory is at cluster 2
        root_dir_lba = FAT32_START_LBA + data_start + (2 - 2) * SECTORS_PER_CLUSTER

        # Create KERNEL.BIN directory entry
        de = bytearray(32)
        de[0:8]   = b'KERNEL  '      # 8.3 name
        de[8:11]  = b'BIN'
        de[11]    = 0x20               # archive bit
        de[12]    = 0                  # reserved
        de[13]    = 0                  # creation time ms
        de[14:16] = struct.pack('<H', 0x0000)  # creation time
        de[16:18] = struct.pack('<H', 0x5340)  # creation date
        de[18:20] = struct.pack('<H', 0x5340)  # last access date
        de[20:22] = struct.pack('<H', kernel_first_cluster >> 16)  # cluster hi
        de[22:24] = struct.pack('<H', 0x0000)  # modification time
        de[24:26] = struct.pack('<H', 0x5340)  # modification date
        de[26:28] = struct.pack('<H', kernel_first_cluster & 0xFFFF)  # cluster lo
        de[28:32] = struct.pack('<I', kernel_size)  # file size

        write_at(f, root_dir_lba * SECTOR_SIZE, bytes(de))

        # ---- Write kernel data ----
        kernel_data_lba = FAT32_START_LBA + data_start + (kernel_first_cluster - 2) * SECTORS_PER_CLUSTER
        # Pad kernel to cluster boundary
        padded_size = kernel_clusters * SECTORS_PER_CLUSTER * SECTOR_SIZE
        padded_kernel = kernel_data + b'\x00' * (padded_size - kernel_size)

        write_at(f, kernel_data_lba * SECTOR_SIZE, padded_kernel)

        print(f"[mkdisk] KERNEL.BIN written at LBA {kernel_data_lba} (cluster {kernel_first_cluster})")
        print(f"[mkdisk] Disk image complete!")

if __name__ == '__main__':
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <disk.img> <kernel.bin>")
        sys.exit(1)
    make_fat32_partition(sys.argv[1], sys.argv[2])
