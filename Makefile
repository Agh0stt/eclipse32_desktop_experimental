# =============================================================================
# Eclipse32 - Master Makefile
# Builds: Stage1, Stage2, Kernel, Shell -> disk.img
# =============================================================================

# --- Toolchain ---
NASM       := nasm
CC         := clang --target=i686-pc-none-elf
LD         := ld.lld
OBJCOPY    := llvm-objcopy

# --- Flags ---
NASMFLAGS  := -f elf32
CFLAGS     := -m32 -march=i686 -std=gnu11 \
              -ffreestanding -fno-stack-protector -fno-pie -fno-pic \
              -O2 -Wall -Wextra -Wshadow \
              -Ikernel -Ikernel/arch/x86 -Ikernel/drivers \
              -Ikernel/mm -Ikernel/fs -Ikernel/initramfs \
              -Ikernel/gui \
              -Ishell
LDFLAGS    := -m elf_i386 -nostdlib --nmagic

# --- Directories ---
BUILD      := build
BOOT_S1    := boot/stage1
BOOT_S2    := boot/stage2
KERNEL_DIR := kernel
SHELL_DIR  := shell
APPS_DIR   := apps
APP_CFLAGS := -m32 -march=i686 -std=gnu11 -ffreestanding -fno-stack-protector \
              -fPIE -fPIC -O2 -nostdlib -fno-builtin \
              -I$(APPS_DIR)/sdk -I$(APPS_DIR)/sdk/libc

# --- Output files ---
STAGE1_BIN   := $(BUILD)/stage1.bin
STAGE2_BIN   := $(BUILD)/stage2.bin
KERNEL_BIN   := $(BUILD)/kernel.bin
DISK_IMG     := $(BUILD)/eclipse32.img

# --- Boot splash (raw 800x600x24bpp BGR, blitted directly into VBE framebuffer) ---
SPLASH_RAW   := assets/splash.raw
SPLASH_LBA   := 460
HELLO_APP_BIN := $(BUILD)/apps/hello.bin
HELLO_APP_E32 := $(BUILD)/apps/HELLO.E32
CRT0_OBJ      := $(BUILD)/apps/sdk/crt0.o
HELLOC_OBJ    := $(BUILD)/apps/hello_c.o
HELLOC_ELF    := $(BUILD)/apps/helloc.elf
HELLOC_BIN    := $(BUILD)/apps/helloc.bin
HELLOC_E32    := $(BUILD)/apps/HELLOC.E32
NUMECHO_OBJ   := $(BUILD)/apps/numecho.o
NUMECHO_ELF   := $(BUILD)/apps/numecho.elf
NUMECHO_BIN   := $(BUILD)/apps/numecho.bin
NUMECHO_E32   := $(BUILD)/apps/NUMECHO.E32
WRITER_OBJ    := $(BUILD)/apps/writer.o
WRITER_ELF    := $(BUILD)/apps/writer.elf
WRITER_BIN    := $(BUILD)/apps/writer.bin
WRITER_E32    := $(BUILD)/apps/WRITER.E32

GUITEST_SRC   := $(APPS_DIR)/guitest/guitest.c
GUITEST_OBJ   := $(BUILD)/apps/guitest.o
GUITEST_ELF   := $(BUILD)/apps/guitest.elf
GUITEST_BIN   := $(BUILD)/apps/guitest.bin
GUITEST_E32   := $(BUILD)/apps/GUITEST.E32
NEWFEATURES_SRC := $(APPS_DIR)/newfeatures/newfeatures.c
NEWFEATURES_OBJ := $(BUILD)/apps/newfeatures.o
NEWFEATURES_ELF := $(BUILD)/apps/newfeatures.elf
NEWFEATURES_BIN := $(BUILD)/apps/newfeatures.bin
NEWFEATURES_E32 := $(BUILD)/apps/NEWFEATURES.E32
WRITER_INS    := $(BUILD)/WRITER.INS
LIBC_STRING_OBJ := $(BUILD)/apps/sdk/libc/string.o
LIBC_UNISTD_OBJ := $(BUILD)/apps/sdk/libc/unistd.o
LIBC_STDIO_OBJ  := $(BUILD)/apps/sdk/libc/stdio.o
LIBC_ERRNO_OBJ  := $(BUILD)/apps/sdk/libc/errno.o
LIBC_STDLIB_OBJ := $(BUILD)/apps/sdk/libc/stdlib.o
LIBC_OBJS := $(LIBC_STRING_OBJ) $(LIBC_UNISTD_OBJ) $(LIBC_STDIO_OBJ) $(LIBC_ERRNO_OBJ) $(LIBC_STDLIB_OBJ)

# --- Kernel C sources ---
KERNEL_SRCS := \
    $(KERNEL_DIR)/kmain.c \
    $(KERNEL_DIR)/arch/x86/gdt.c \
    $(KERNEL_DIR)/arch/x86/idt.c \
    $(KERNEL_DIR)/arch/x86/pic.c \
    $(KERNEL_DIR)/arch/x86/pit.c \
    $(KERNEL_DIR)/mm/pmm.c \
    $(KERNEL_DIR)/mm/vmm.c \
    $(KERNEL_DIR)/mm/heap.c \
    $(KERNEL_DIR)/drivers/vga/vga.c \
    $(KERNEL_DIR)/drivers/vbe/vbe.c \
    $(KERNEL_DIR)/drivers/keyboard/keyboard.c \
    $(KERNEL_DIR)/drivers/speaker/speaker.c \
    $(KERNEL_DIR)/drivers/disk/ata.c \
    $(KERNEL_DIR)/drivers/pci/pci.c \
    $(KERNEL_DIR)/drivers/net/rtl8139.c \
    $(KERNEL_DIR)/net/net.c \
    $(KERNEL_DIR)/net/eth.c \
    $(KERNEL_DIR)/net/arp.c \
    $(KERNEL_DIR)/net/ipv4.c \
    $(KERNEL_DIR)/net/icmp.c \
    $(KERNEL_DIR)/net/udp.c \
    $(KERNEL_DIR)/net/dns.c \
    $(KERNEL_DIR)/net/tcp.c \
    $(KERNEL_DIR)/net/http.c \
    $(KERNEL_DIR)/fs/fat32/fat32.c \
    $(KERNEL_DIR)/syscall/syscall.c \
    $(KERNEL_DIR)/exec/e32_loader.c \
    $(KERNEL_DIR)/initramfs/initramfs.c \
    $(SHELL_DIR)/shell.c \
    $(SHELL_DIR)/ins_pkg.c \
    $(KERNEL_DIR)/drivers/mouse/mouse.c \
    $(KERNEL_DIR)/gui/gui_desktop.c \
    $(KERNEL_DIR)/gui/gui_sdk.c \
    $(KERNEL_DIR)/sched/sched.c \
    $(KERNEL_DIR)/libk/kmath.c \
    $(KERNEL_DIR)/bootmenu/bootmenu.c

# --- Kernel ASM sources ---
KERNEL_ASM_SRCS := \
    $(KERNEL_DIR)/arch/x86/entry.asm \
    $(KERNEL_DIR)/arch/x86/gdt_asm.asm

KERNEL_OBJS := \
    $(patsubst %.c,   $(BUILD)/%.o, $(KERNEL_SRCS)) \
    $(patsubst %.asm, $(BUILD)/%.asm.o, $(KERNEL_ASM_SRCS))

# =============================================================================
.PHONY: all clean run run-debug run-apps

all: $(DISK_IMG)
	@echo ""
	@echo "╔════════════════════════════════════════╗"
	@echo "║     Eclipse32 build complete!          ║"
	@echo "║     Image: $(DISK_IMG)         ║"
	@echo "╚════════════════════════════════════════╝"

# =============================================================================
# Stage 1 - MBR bootloader
# =============================================================================
$(STAGE1_BIN): $(BOOT_S1)/boot.asm | $(BUILD)
	@echo "[NASM] Stage 1"
	$(NASM) -f bin -o $@ $<

# =============================================================================
# Stage 2 - System initializer
# =============================================================================
$(STAGE2_BIN): $(BOOT_S2)/stage2.asm | $(BUILD)
	@echo "[NASM] Stage 2"
	$(NASM) -f bin -o $@ $<

# =============================================================================
# Kernel C objects
# =============================================================================
$(BUILD)/%.o: %.c | $(BUILD)
	@mkdir -p $(dir $@)
	@echo "[CC  ] $<"
	$(CC) $(CFLAGS) -c -o $@ $<

# sched.c uses __builtin_setjmp/__builtin_longjmp as a repeated coroutine
# yield point (sched_yield() is called many times per shell command, from
# inside tcp.c's networking wait loops). Clang's __builtin_setjmp/longjmp
# are designed around single-shot C++-style exception unwinding; at -O2
# the optimizer can keep live values in registers across the setjmp call
# site under assumptions that don't hold for this repeated-yield pattern,
# which corrupts register state across the jump. This manifested as a
# crash to a garbage EIP (observed via GDB: an address far outside valid
# physical RAM) once render_terminal started pumping sched_yield() every
# frame instead of once per command -- a call-frequency increase that
# exposed a latent miscompilation that was never exercised before.
# Building this one file at -O0 with inlining disabled avoids the issue
# without touching the rest of the kernel's optimization level.
$(BUILD)/kernel/sched/sched.o: kernel/sched/sched.c | $(BUILD)
	@mkdir -p $(dir $@)
	@echo "[CC  ] $< (forced -O0, see Makefile comment)"
	$(CC) $(filter-out -O2,$(CFLAGS)) -O0 -fno-inline -c -o $@ $<

$(BUILD)/%.asm.o: %.asm | $(BUILD)
	@mkdir -p $(dir $@)
	@echo "[ASM ] $<"
	$(NASM) $(NASMFLAGS) -o $@ $<

# =============================================================================
# Link kernel
# =============================================================================
$(KERNEL_BIN): $(KERNEL_OBJS) kernel/linker.ld
	@echo "[LD  ] Kernel"
	$(LD) $(LDFLAGS) -T kernel/linker.ld -o $(BUILD)/kernel.elf $(KERNEL_OBJS)
	$(OBJCOPY) -O binary $(BUILD)/kernel.elf $@
	@KERN_BYTES=$$(wc -c < $@); \
	 KERN_SECTS=$$(( ($$KERN_BYTES + 511) / 512 )); \
	 LAST_CHUNK=$$(( $$KERN_SECTS - 381 )); \
	 echo "[INFO] Kernel size: $$KERN_BYTES bytes = $$KERN_SECTS sectors (last chunk: $$LAST_CHUNK)"; \
	 sed -i "s/^%define KERNEL_SECTORS.*/%define KERNEL_SECTORS    $$KERN_SECTS/" boot/stage2/stage2.asm; \
	 sed -i "/KERNEL_LBA_START + 381/{n; s/mov word  \[dap\.count\],  [0-9]*/mov word  [dap.count],  $$LAST_CHUNK/}" boot/stage2/stage2.asm; \
	 echo "[INFO] stage2.asm patched: KERNEL_SECTORS=$$KERN_SECTS last_chunk=$$LAST_CHUNK"

# =============================================================================
# Built-in apps (baked into disk image at build time)
# =============================================================================
$(HELLO_APP_BIN): $(APPS_DIR)/hello/hello.asm | $(BUILD)
	@mkdir -p $(dir $@)
	$(NASM) -f bin -o $@ $<

$(HELLO_APP_E32): $(HELLO_APP_BIN) tools/pack_e32.py | $(BUILD)
	@mkdir -p $(dir $@)
	python3 tools/pack_e32.py $(HELLO_APP_BIN) $(HELLO_APP_E32)

$(CRT0_OBJ): $(APPS_DIR)/sdk/crt0.asm | $(BUILD)
	@mkdir -p $(dir $@)
	$(NASM) -f elf32 -o $@ $<

$(LIBC_STRING_OBJ): $(APPS_DIR)/sdk/libc/string.c $(APPS_DIR)/sdk/libc/string.h | $(BUILD)
	@mkdir -p $(dir $@)
	$(CC) -m32 -march=i686 -std=gnu11 -ffreestanding -fno-stack-protector -fPIE -fPIC -O2 \
	      -nostdlib -fno-builtin -I$(APPS_DIR)/sdk/libc -c -o $@ $<

$(LIBC_UNISTD_OBJ): $(APPS_DIR)/sdk/libc/unistd.c $(APPS_DIR)/sdk/libc/unistd.h $(APPS_DIR)/sdk/e32_syscall.h | $(BUILD)
	@mkdir -p $(dir $@)
	$(CC) -m32 -march=i686 -std=gnu11 -ffreestanding -fno-stack-protector -fPIE -fPIC -O2 \
	      -nostdlib -fno-builtin -I$(APPS_DIR)/sdk -I$(APPS_DIR)/sdk/libc -c -o $@ $<

$(LIBC_STDIO_OBJ): $(APPS_DIR)/sdk/libc/stdio.c $(APPS_DIR)/sdk/libc/stdio.h $(APPS_DIR)/sdk/libc/string.h $(APPS_DIR)/sdk/libc/unistd.h | $(BUILD)
	@mkdir -p $(dir $@)
	$(CC) -m32 -march=i686 -std=gnu11 -ffreestanding -fno-stack-protector -fPIE -fPIC -O2 \
	      -nostdlib -fno-builtin -I$(APPS_DIR)/sdk/libc -c -o $@ $<

$(LIBC_ERRNO_OBJ): $(APPS_DIR)/sdk/libc/errno.c $(APPS_DIR)/sdk/libc/errno.h | $(BUILD)
	@mkdir -p $(dir $@)
	$(CC) -m32 -march=i686 -std=gnu11 -ffreestanding -fno-stack-protector -fPIE -fPIC -O2 \
	      -nostdlib -fno-builtin -I$(APPS_DIR)/sdk/libc -c -o $@ $<

$(LIBC_STDLIB_OBJ): $(APPS_DIR)/sdk/libc/stdlib.c $(APPS_DIR)/sdk/libc/stdlib.h $(APPS_DIR)/sdk/libc/errno.h $(APPS_DIR)/sdk/libc/string.h $(APPS_DIR)/sdk/libc/unistd.h | $(BUILD)
	@mkdir -p $(dir $@)
	$(CC) -m32 -march=i686 -std=gnu11 -ffreestanding -fno-stack-protector -fPIE -fPIC -O2 \
	      -nostdlib -fno-builtin -I$(APPS_DIR)/sdk/libc -c -o $@ $<

$(HELLOC_OBJ): $(APPS_DIR)/hello/hello.c $(APPS_DIR)/sdk/e32_syscall.h | $(BUILD)
	@mkdir -p $(dir $@)
	$(CC) -m32 -march=i686 -std=gnu11 -ffreestanding -fno-stack-protector -fPIE -fPIC -O2 \
	      -nostdlib -fno-builtin -I$(APPS_DIR)/sdk -I$(APPS_DIR)/sdk/libc -c -o $@ $<

$(HELLOC_ELF): $(CRT0_OBJ) $(HELLOC_OBJ) $(LIBC_OBJS) $(APPS_DIR)/sdk/app.ld | $(BUILD)
	$(LD) -m elf_i386 -nostdlib -T $(APPS_DIR)/sdk/app.ld -o $@ $(CRT0_OBJ) $(HELLOC_OBJ) $(LIBC_OBJS)

$(HELLOC_BIN): $(HELLOC_ELF) | $(BUILD)
	$(OBJCOPY) -O binary $(HELLOC_ELF) $@

$(HELLOC_E32): $(HELLOC_BIN) tools/pack_e32.py | $(BUILD)
	python3 tools/pack_e32.py $(HELLOC_BIN) $(HELLOC_E32)

$(NUMECHO_OBJ): $(APPS_DIR)/numecho/numecho.c $(APPS_DIR)/sdk/e32_syscall.h | $(BUILD)
	@mkdir -p $(dir $@)
	$(CC) -m32 -march=i686 -std=gnu11 -ffreestanding -fno-stack-protector -fPIE -fPIC -O2 \
	      -nostdlib -fno-builtin -I$(APPS_DIR)/sdk -I$(APPS_DIR)/sdk/libc -c -o $@ $<

$(NUMECHO_ELF): $(CRT0_OBJ) $(NUMECHO_OBJ) $(LIBC_OBJS) $(APPS_DIR)/sdk/app.ld | $(BUILD)
	$(LD) -m elf_i386 -nostdlib -T $(APPS_DIR)/sdk/app.ld -o $@ $(CRT0_OBJ) $(NUMECHO_OBJ) $(LIBC_OBJS)

$(NUMECHO_BIN): $(NUMECHO_ELF) | $(BUILD)
	$(OBJCOPY) -O binary $(NUMECHO_ELF) $@

$(NUMECHO_E32): $(NUMECHO_BIN) tools/pack_e32.py | $(BUILD)
	python3 tools/pack_e32.py $(NUMECHO_BIN) $(NUMECHO_E32)

$(WRITER_OBJ): $(APPS_DIR)/writer/writer.c $(APPS_DIR)/sdk/e32_syscall.h | $(BUILD)
	@mkdir -p $(dir $@)
	$(CC) $(APP_CFLAGS) -c -o $@ $<

$(WRITER_ELF): $(CRT0_OBJ) $(WRITER_OBJ) $(LIBC_OBJS) $(APPS_DIR)/sdk/app.ld | $(BUILD)
	$(LD) -m elf_i386 -nostdlib -T $(APPS_DIR)/sdk/app.ld -o $@ $(CRT0_OBJ) $(WRITER_OBJ) $(LIBC_OBJS)

$(WRITER_BIN): $(WRITER_ELF) | $(BUILD)
	$(OBJCOPY) -O binary $(WRITER_ELF) $@

$(WRITER_E32): $(WRITER_BIN) tools/pack_e32.py | $(BUILD)
	python3 tools/pack_e32.py $(WRITER_BIN) $(WRITER_E32)

$(GUITEST_OBJ): $(GUITEST_SRC) $(APPS_DIR)/sdk/e32_gui.h $(APPS_DIR)/sdk/e32_syscall.h | $(BUILD)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I$(APPS_DIR)/sdk -I$(APPS_DIR)/sdk/libc -c $< -o $@

$(GUITEST_ELF): $(CRT0_OBJ) $(GUITEST_OBJ) $(LIBC_OBJS) $(APPS_DIR)/sdk/app.ld | $(BUILD)
	$(LD) -m elf_i386 -nostdlib -T $(APPS_DIR)/sdk/app.ld -o $@ $(CRT0_OBJ) $(GUITEST_OBJ) $(LIBC_OBJS)

$(GUITEST_BIN): $(GUITEST_ELF) | $(BUILD)
	$(OBJCOPY) -O binary $(GUITEST_ELF) $@

$(GUITEST_E32): $(GUITEST_BIN) tools/pack_e32.py | $(BUILD)
	python3 tools/pack_e32.py $(GUITEST_BIN) $@

$(NEWFEATURES_OBJ): $(NEWFEATURES_SRC) $(APPS_DIR)/sdk/e32_gui.h $(APPS_DIR)/sdk/e32_syscall.h | $(BUILD)
	@mkdir -p $(dir $@)
	$(CC) $(APP_CFLAGS) -I$(APPS_DIR)/sdk -I$(APPS_DIR)/sdk/libc -c $< -o $@

$(NEWFEATURES_ELF): $(CRT0_OBJ) $(NEWFEATURES_OBJ) $(LIBC_OBJS) $(APPS_DIR)/sdk/app.ld | $(BUILD)
	$(LD) -m elf_i386 -nostdlib -T $(APPS_DIR)/sdk/app.ld -o $@ $(CRT0_OBJ) $(NEWFEATURES_OBJ) $(LIBC_OBJS)

$(NEWFEATURES_BIN): $(NEWFEATURES_ELF) | $(BUILD)
	$(OBJCOPY) -O binary $(NEWFEATURES_ELF) $@

$(NEWFEATURES_E32): $(NEWFEATURES_BIN) tools/pack_e32.py | $(BUILD)
	python3 tools/pack_e32.py $(NEWFEATURES_BIN) $@

$(WRITER_INS): $(WRITER_E32) tools/writer_pkg/install.cfg tools/pack_ins.py | $(BUILD)
	@mkdir -p $(BUILD)/writer_pkg_staging
	cp tools/writer_pkg/install.cfg $(BUILD)/writer_pkg_staging/install.cfg
	cp $(WRITER_E32) $(BUILD)/writer_pkg_staging/WRITER.E32
	python3 tools/pack_ins.py $(BUILD)/writer_pkg_staging $(WRITER_INS)

# =============================================================================
# Disk image assembly
# =============================================================================
$(DISK_IMG): $(STAGE1_BIN) $(STAGE2_BIN) $(KERNEL_BIN) $(SPLASH_RAW) $(HELLO_APP_E32) $(HELLOC_E32) $(NUMECHO_E32) $(WRITER_E32) $(WRITER_INS) $(GUITEST_E32) $(NEWFEATURES_E32) | $(BUILD)
	@echo "[IMG ] Creating disk image"
	dd if=/dev/zero      of=$@ bs=512 count=131072 status=none
	dd if=$(STAGE1_BIN)  of=$@ bs=512 seek=0  conv=notrunc status=none
	dd if=$(STAGE2_BIN)  of=$@ bs=512 seek=1  conv=notrunc status=none
	dd if=$(KERNEL_BIN)  of=$@ bs=512 seek=17 conv=notrunc status=none
	dd if=$(SPLASH_RAW)  of=$@ bs=512 seek=$(SPLASH_LBA) conv=notrunc status=none
	python3 tools/write_mbr.py $@
	python3 tools/format_fat32.py $@
	python3 tools/inject_fat32_file.py $@ $(HELLO_APP_E32) HELLO.E32
	python3 tools/inject_fat32_file.py $@ $(HELLOC_E32) HELLOC.E32
	python3 tools/inject_fat32_file.py $@ $(NUMECHO_E32) NUMECHO.E32
	python3 tools/inject_fat32_file.py $@ $(WRITER_E32) WRITER.E32
	python3 tools/inject_fat32_file.py $@ $(GUITEST_E32) GUITEST.E32
	python3 tools/inject_fat32_file.py $@ $(NEWFEATURES_E32) NEWFEATURES.E32
	python3 tools/inject_ins.py $@ $(WRITER_INS) WRITER.INS
	@echo "[IMG ] Done: $@"

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD)

# =============================================================================
# App tools — 4 separate targets
# =============================================================================

# 1. Compile only → build/apps/NAME.E32  (no injection)
# Usage: make app/ekuecho
app/%:
	@echo "[APP ] Compiling $*"
	@mkdir -p $(BUILD)/apps
	$(CC) $(APP_CFLAGS) -c -o $(BUILD)/apps/$*.o $(APPS_DIR)/$*/$*.c
	$(LD) -m elf_i386 -nostdlib -T $(APPS_DIR)/sdk/app.ld -o $(BUILD)/apps/$*.elf \
	      $(CRT0_OBJ) $(BUILD)/apps/$*.o $(LIBC_OBJS)
	$(OBJCOPY) -O binary $(BUILD)/apps/$*.elf $(BUILD)/apps/$*.bin
	python3 tools/pack_e32.py $(BUILD)/apps/$*.bin $(BUILD)/apps/$*.E32
	@echo "[DONE] $(BUILD)/apps/$*.E32 — use inject/ or ins/ to deploy"

# 2. Inject raw .E32 into disk root (quick testing only)
# Usage: make inject/ekuecho
inject/%:
	@echo "[INJECT] $*.E32 → disk root"
	python3 tools/inject_fat32_file.py $(DISK_IMG) $(BUILD)/apps/$*.E32 $*.E32
	@echo "[DONE] $*.E32 in FAT32 root"

# 2b. Inject .E32 into /bin (shell commands)
# Usage: make injectsh/hi
injectsh/%:
	@echo "[INJECTSH] $*.E32 → /bin"
	python3 tools/injectsh.py $(DISK_IMG) $(BUILD)/apps/$*.E32 $*
	@echo "[DONE] $*.E32 in /bin"

# 3. Inject any arbitrary file into disk root
# Usage: make inject-file/path/to/file.txt
inject-file/%:
	@echo "[INJECT-FILE] $* → disk root"
	python3 tools/inject_fat32_file.py $(DISK_IMG) $* $(notdir $*)
	@echo "[DONE] $(notdir $*) in FAT32 root"

# 4. Package as .INS and inject (proper install — requires apps/NAME/install.cfg)
# Usage: make ins/ekuecho
ins/%:
	@echo "[INS ] Packaging $*"
	@mkdir -p $(BUILD)/ins/$*_pkg
	cp $(BUILD)/apps/$*.E32 $(BUILD)/ins/$*_pkg/
	cp $(APPS_DIR)/$*/install.cfg $(BUILD)/ins/$*_pkg/
	python3 tools/pack_ins.py $(BUILD)/ins/$*_pkg/ $(BUILD)/ins/$*.INS
	python3 tools/inject_ins.py $(DISK_IMG) $(BUILD)/ins/$*.INS $*.INS
	@echo "[DONE] $*.INS injected into $(DISK_IMG) — run: install $*.INS"

# =============================================================================
# Run with QEMU
# =============================================================================
run: $(DISK_IMG)
	qemu-system-i386 \
		-drive file=$(DISK_IMG),format=raw,if=ide \
		-m 64M \
		-vga std \
		-boot c \
		-no-reboot \
		-netdev user,id=net0 \
		-device rtl8139,netdev=net0 \
		-object filter-dump,id=net0dump,netdev=net0,file=/tmp/eclipse32-net.pcap \
		-serial stdio

run-debug: $(DISK_IMG)
	qemu-system-i386 \
		-drive file=$(DISK_IMG),format=raw,if=ide \
		-m 64M \
		-vga std \
		-boot c \
		-no-reboot \
		-netdev user,id=net0 \
		-device rtl8139,netdev=net0 \
		-object filter-dump,id=net0dump,netdev=net0,file=/tmp/eclipse32-net.pcap \
		-serial stdio \
		-d int,cpu_reset \
		-D /tmp/eclipse32-debug.log \
		-s -S &
	gdb -ex "target remote :1234" \
	    -ex "symbol-file $(BUILD)/kernel.elf" \
	    -ex "break kmain" \
	    -ex "continue"
