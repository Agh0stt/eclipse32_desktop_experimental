#!/bin/bash
# Two-pass build: compile once with placeholder LBAs, measure real LBAs
# from the built ISO, patch the source files, rebuild, and verify the
# LBAs didn't shift (they won't, since patching a fixed-width integer
# literal doesn't change file size).
set -e
cd "$(dirname "$0")"

CC="clang --target=i686-pc-none-elf"
CFLAGS="-m32 -march=i686 -std=gnu11 -ffreestanding -fno-stack-protector -fno-pie -fno-pic -O2 -Wall -Wextra -fno-builtin"

build_pass() {
    nasm -f bin -o build/boot.bin boot.asm
    $CC $CFLAGS -c -o build/kernel.o kernel.c 2>/dev/null
    $CC $CFLAGS -c -o build/shell.o shell.c 2>/dev/null
    ld.lld -m elf_i386 -nostdlib --nmagic -T linker.ld -o build/installer.elf build/kernel.o build/shell.o
    llvm-objcopy -O binary build/installer.elf build/kernel.bin
    cp build/boot.bin isoroot/boot/boot.bin
    cp build/kernel.bin isoroot/boot/kernel.bin
    xorriso -as mkisofs -o installer.iso -V "ECLIPSE32_INST" -b boot/boot.bin -no-emul-boot -boot-load-size 4 isoroot >/dev/null 2>&1
}

get_lba() {
    xorriso -indev installer.iso -find "$1" -exec report_lba -- 2>/dev/null \
        | tail -1 \
        | awk -F',' '{gsub(/[ \t]/,"",$2); print $2}'
}

echo "[1/3] First build pass..."
build_pass

KLBA1=$(get_lba /boot/kernel.bin)
ELBA1=$(get_lba /ECLIPSE32.img)
echo "      kernel.bin LBA = $KLBA1, ECLIPSE32.img LBA = $ELBA1"

echo "[2/3] Patching LBA constants and rebuilding..."
sed -i -E "s/%define INST_KERNEL_LBA[ \t]+[0-9]+/%define INST_KERNEL_LBA     $KLBA1/" boot.asm
sed -i -E "s/#define ECLIPSE_IMG_CD_LBA[ \t]+[0-9]+/#define ECLIPSE_IMG_CD_LBA   $ELBA1/" kernel.c
build_pass

KLBA2=$(get_lba /boot/kernel.bin)
ELBA2=$(get_lba /ECLIPSE32.img)
echo "      kernel.bin LBA = $KLBA2, ECLIPSE32.img LBA = $ELBA2"

echo "[3/3] Verifying stability..."
if [ "$KLBA1" != "$KLBA2" ] || [ "$ELBA1" != "$ELBA2" ]; then
    echo "ERROR: LBAs shifted between passes ($KLBA1->$KLBA2, $ELBA1->$ELBA2)."
    echo "This shouldn't happen since patching a fixed-width integer literal"
    echo "doesn't change file size -- something else about the build is"
    echo "non-deterministic. Investigate before trusting this ISO."
    exit 1
fi

echo "OK: installer.iso built with self-consistent LBAs (kernel=$KLBA2, eclipse=$ELBA2)."
