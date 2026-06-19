#!/usr/bin/env python3
"""
Eclipse32 - format_fat32.py
Formats the FAT32 partition inside an existing disk image.
The partition starts at LBA 6144 and is 63488 sectors (31 MB).
Usage: python3 format_fat32.py <image>
"""
import struct, sys

# ---- Geometry ---------------------------------------------------------------
SECT        = 512       # bytes per sector
PART_LBA    = 6144      # partition start (must match write_mbr.py)
TSC         = 63488     # total sectors in partition
SPC         = 8         # sectors per cluster  (8 * 512 = 4 KB clusters)
RSC         = 32        # reserved sector count (VBR + FSInfo + backup)
NFATS       = 2         # number of FATs
ROOT_CLUST  = 2         # root directory cluster

# Compute sectors-per-FAT:
#   Each cluster entry is 4 bytes; round up to whole sectors.
#   Formula from Microsoft FAT spec: SPF = ceil((TSC - RSC) / (SPC + NFATS*4/SECT*SPC ... ))
#   Simplified: SPF = ceil((TSC - RSC) / (SPC * SECT/4 + NFATS))
#   We use the safe over-estimate: one FAT sector covers SECT/4 cluster entries.
DATA_CLUST_MAX  = (TSC - RSC) // SPC        # upper-bound on cluster count
SPF             = (DATA_CLUST_MAX * 4 + SECT - 1) // SECT
SPF             = max(SPF, 1)

DATA_START      = RSC + NFATS * SPF         # first data sector (partition-relative)
DATA_SECTS      = TSC - DATA_START
NCLUSTERS       = DATA_SECTS // SPC         # usable cluster count

# ---- Build VBR (Volume Boot Record) ----------------------------------------
def build_vbr():
    vbr = bytearray(SECT)

    # Jump + NOP
    vbr[0:3] = b'\xEB\x58\x90'
    # OEM name
    vbr[3:11] = b'ECLIPSE1'

    # BPB (BIOS Parameter Block)
    struct.pack_into('<H', vbr, 11, SECT)       # bytes per sector
    vbr[13] = SPC                               # sectors per cluster
    struct.pack_into('<H', vbr, 14, RSC)        # reserved sectors
    vbr[16] = NFATS                             # number of FATs
    struct.pack_into('<H', vbr, 17, 0)          # root entry count (0 = FAT32)
    struct.pack_into('<H', vbr, 19, 0)          # total sectors 16 (0 = use 32-bit)
    vbr[21] = 0xF8                              # media type (fixed disk)
    struct.pack_into('<H', vbr, 22, 0)          # sectors per FAT 16 (0 = FAT32)
    struct.pack_into('<H', vbr, 24, 63)         # sectors per track
    struct.pack_into('<H', vbr, 26, 255)        # number of heads
    struct.pack_into('<I', vbr, 28, PART_LBA)   # hidden sectors (= partition LBA start)
    struct.pack_into('<I', vbr, 32, TSC)        # total sectors 32

    # FAT32 Extended BPB
    struct.pack_into('<I', vbr, 36, SPF)        # sectors per FAT 32
    struct.pack_into('<H', vbr, 40, 0)          # ext flags
    struct.pack_into('<H', vbr, 42, 0)          # FS version 0.0
    struct.pack_into('<I', vbr, 44, ROOT_CLUST) # root cluster
    struct.pack_into('<H', vbr, 48, 1)          # FS info sector
    struct.pack_into('<H', vbr, 50, 6)          # backup boot sector

    # Drive number, boot signature, volume ID, label, FS type
    vbr[64] = 0x80
    vbr[66] = 0x29                              # extended boot signature
    struct.pack_into('<I', vbr, 67, 0xEC320001) # volume serial
    vbr[71:82] = b'ECLIPSE32  '                 # volume label (11 bytes)
    vbr[82:90] = b'FAT32   '                    # FS type string

    # Boot sector signature
    vbr[510] = 0x55
    vbr[511] = 0xAA

    return bytes(vbr)

# ---- Build initial FAT ------------------------------------------------------
def build_fat():
    fat = bytearray(SPF * SECT)
    # Entry 0: media byte
    struct.pack_into('<I', fat, 0,  0x0FFFFFF8)
    # Entry 1: EOC
    struct.pack_into('<I', fat, 4,  0x0FFFFFFF)
    # Entry 2: root directory (EOC)
    struct.pack_into('<I', fat, 8,  0x0FFFFFFF)
    return bytes(fat)

# ---- Write image ------------------------------------------------------------
def main():
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <image>", file=sys.stderr)
        sys.exit(1)

    img = sys.argv[1]
    vbr = build_vbr()
    fat = build_fat()

    with open(img, 'r+b') as f:
        base = PART_LBA * SECT

        # Write VBR at partition sector 0
        f.seek(base)
        f.write(vbr)

        # Zero reserved sectors 1..RSC-1
        for i in range(1, RSC):
            f.seek(base + i * SECT)
            f.write(bytes(SECT))

        # Write both FAT copies
        for fat_num in range(NFATS):
            f.seek(base + (RSC + fat_num * SPF) * SECT)
            f.write(fat)

        # Zero root directory cluster
        root_offset = base + DATA_START * SECT
        f.seek(root_offset)
        f.write(bytes(SPC * SECT))

    print(f"FAT32 formatted: SPF={SPF} DATA_START={DATA_START} CLUSTERS={NCLUSTERS}")

if __name__ == '__main__':
    main()
