Eclipse32 Installer — Build & Usage Guide
==========================================

OVERVIEW
--------
The installer is a minimal bootable ISO that copies Eclipse32 onto a target
hard disk. It runs entirely in protected mode with no dependencies on any OS.


FILES
-----
  installer/
  ├── kernel.c        — Installer kernel (disk scan, ATAPI read, ATA write, UI)
  ├── shell.c         — Stub (placeholder, not the real shell source)
  ├── boot.asm        — Stage-1 bootloader, loads kernel into protected mode
  ├── linker.ld       — Linker script (load address, section layout)
  ├── build_iso.sh    — Two-pass build script, produces installer.iso
  └── isoroot/
      ├── boot/
      │   ├── boot.bin    — Assembled bootloader (built by build_iso.sh)
      │   └── kernel.bin  — Compiled installer kernel (built by build_iso.sh)
      └── ECLIPSE32.img   — The OS disk image to install (copied in before build)


DEPENDENCIES
------------
  - nasm              (assembles boot.asm)
  - clang             (cross-compiles kernel.c, target: i686-pc-none-elf)
  - lld / ld.lld      (links the kernel ELF)
  - llvm-objcopy      (strips ELF to flat binary)
  - xorriso           (builds the ISO 9660 image)
  - python3           (used by build_iso.sh for LBA verification)

  On Ubuntu/Debian:
    sudo apt install nasm clang lld llvm xorriso python3


HOW TO BUILD
------------
1. Build the main Eclipse32 OS image first (from the repo root):

     make

   This produces:  build/eclipse32.img

2. Copy the OS image into the installer's isoroot:

     cp build/eclipse32.img installer/isoroot/ECLIPSE32.img

3. Run the two-pass build script:

     cd installer
     mkdir -p build
     bash build_iso.sh

   What build_iso.sh does:
     Pass 1 — Assembles boot.asm, compiles kernel.c, links, strips to binary,
               builds a first-pass ISO to get LBA positions.
     Pass 2 — Patches ECLIPSE_IMG_CD_LBA in kernel.c with the real LBA from
               the first-pass ISO, recompiles, rebuilds the final ISO.
     Pass 3 — Verifies both passes agree on the LBA (sanity check).

   Expected output:
     [1/3] First build pass...
           kernel.bin LBA = 35, ECLIPSE32.img LBA = 40
     [2/3] Patching LBA constants and rebuilding...
           kernel.bin LBA = 35, ECLIPSE32.img LBA = 40
     [3/3] Verifying stability...
     OK: installer.iso built with self-consistent LBAs (kernel=35, eclipse=40)

   Output file:  installer/installer.iso


HOW TO RUN (QEMU)
-----------------
Step 1 — Create a blank target disk (only needed once):

     dd if=/dev/zero of=installer/build/target.img bs=1M count=256

Step 2 — Boot the installer ISO:

     qemu-system-i386 \
       -cdrom installer/installer.iso \
       -drive file=installer/build/target.img,format=raw,if=ide,index=1 \
       -m 64M -vga std -boot d -no-reboot -serial stdio

   In the installer:
     - Default UI:  press the disk number (e.g. '0') to install
     - Press F2:    switches to the Advanced Installer UI (cleaner layout,
                    as shown in the Eclipse32 installation manual)
     - Press 'q':   abort

   Wait for the progress bar [########...] to fill completely.

Step 3 — Boot the installed system (no CD attached):

     qemu-system-i386 \
       -drive file=installer/build/target.img,format=raw,if=ide,index=0 \
       -m 64M -vga std -boot c -no-reboot -serial stdio

   IMPORTANT: Do NOT attach -cdrom when booting the installed system.
   If the CD is attached, it takes drive slot 0 and the hard disk moves
   to slot 1, causing the filesystem mount to fail ("fs not mounted").


TROUBLESHOOTING
---------------
"ECLIPSE_IMG_CD_LBA = 0 still in kernel.c after build"
  → The sed patch in build_iso.sh failed. Check the regex matches exactly:
      grep "ECLIPSE_IMG_CD_LBA" installer/kernel.c
    Should show a number (e.g. 40), not 0 or Startlba.

"[FATAL] CD read failed at sector N"
  → ECLIPSE_IMG_CD_LBA is wrong. Run build_iso.sh again and check Pass 3
    says OK. Do not hand-edit the LBA value.

"fs not mounted" in the booted OS
  → You booted with -cdrom still attached. Remove it — see Step 3 above.
    Or: you are running an old kernel build before the drive-scan fix.
    Rebuild with make and reinstall.

"build/boot.bin: No such file or directory"
  → Run:  mkdir -p installer/build  before build_iso.sh.

"nasm: command not found" / "clang: command not found"
  → Install dependencies listed above.


NOTES
-----
- The LBA value (40) is not a constant — it depends on the ISO layout and
  will change if files are added/removed from isoroot/. Always use
  build_iso.sh to rebuild; never hardcode the LBA manually.

- ECLIPSE32.img is 64 MB. The target disk must be at least 64 MB.
  The QEMU target.img above is 256 MB which is fine.

- The installer writes raw sectors (no partition table manipulation beyond
  what ECLIPSE32.img already contains). The OS image includes its own MBR
  and FAT32 partition starting at LBA 6144.

- The installer does NOT support HTTPS/network install. It reads only from
  the attached CD-ROM (ATAPI).
