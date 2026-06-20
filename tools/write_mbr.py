#!/usr/bin/env python3
"""
Eclipse32 - write_mbr.py
Writes a valid MBR partition table into an existing disk image.
Partition 0: FAT32 LBA (type 0x0C), starts at LBA 6144, 63488 sectors (31 MB).
Usage: python3 write_mbr.py <image>
"""
import struct, sys

def main():
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <image>", file=sys.stderr)
        sys.exit(1)

    img = sys.argv[1]
    with open(img, 'r+b') as f:
        f.seek(0x1BE)
        # Partition entry: status, CHS_start(3), type, CHS_end(3), LBA_start, LBA_size
        f.write(struct.pack('<BBBBBBBBII',
            0x80,           # bootable
            0x00, 0x02, 0x00,   # CHS start (irrelevant for LBA)
            0x0C,           # FAT32 LBA
            0xFF, 0xFF, 0xFF,   # CHS end (irrelevant for LBA)
            6144,           # LBA start
            63488           # LBA size  (63488 * 512 = 31 MB)
        ))
        # Three empty partition entries
        for _ in range(3):
            f.write(b'\x00' * 16)
        # Boot signature
        f.seek(0x1FE)
        f.write(b'\x55\xAA')

    print(f"MBR partition table written to {img}")

if __name__ == '__main__':
    main()
