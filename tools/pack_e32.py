#!/usr/bin/env python3
import struct
import sys

E32_MAGIC = 0x32454345
E32_VERSION = 1
HEADER_SIZE = 40


def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <payload.bin> <out.e32>", file=sys.stderr)
        sys.exit(1)

    payload_path = sys.argv[1]
    out_path = sys.argv[2]

    with open(payload_path, "rb") as f:
        payload = f.read()

    image_size = HEADER_SIZE + len(payload)
    header = struct.pack(
        "<IHHIIIIIIII",
        E32_MAGIC,
        E32_VERSION,
        HEADER_SIZE,
        0,
        HEADER_SIZE,         # entry offset
        HEADER_SIZE,         # code offset
        len(payload),        # code size
        HEADER_SIZE,         # data offset
        0,                   # data size
        0,                   # bss size
        image_size,
    )

    with open(out_path, "wb") as f:
        f.write(header)
        f.write(payload)

    print(f"Wrote {out_path} ({image_size} bytes)")


if __name__ == "__main__":
    main()
