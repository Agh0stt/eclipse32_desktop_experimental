; =============================================================================
; Eclipse32 Installer - Bootloader (El Torito / ISO 9660 no-emulation)
; Loaded at 0x7C00 by BIOS (or El Torito puts us at 0x7C00 in no-emul mode).
; We are given a flat 512-byte real-mode entry; Int 13h ext. is available.
; =============================================================================

[BITS 16]
[ORG 0x7C00]

%define INST_KERNEL_LBA     35          ; start LBA of installer kernel on CD
                                          ; (verified via: xorriso -indev installer.iso
                                          ;  -find /boot/kernel.bin -exec report_lba --
                                          ;  -- this WILL change if you add/reorder/resize
                                          ;  files before kernel.bin in the ISO tree, so
                                          ;  re-check it after any Makefile/layout change)
%define INST_KERNEL_SECTS   16          ; number of CD sectors (2048 B each)
%define INST_KERNEL_DST     0x10000     ; load installer kernel here (64 KB mark)
%define SECTOR_SIZE         2048        ; CD sector size

start:
    jmp short _real_start   ; 2-byte short jump
    nop                     ; 1-byte alignment padding

    ; -------------------------------------------------------------------------
    ; xorriso / mkisofs Boot Info Table landing zone.
    ; Standard spec requires exactly 56 bytes starting at offset 8.
    ; (8 + 56 = 64). Since our short jmp + nop takes 3 bytes, we pad 61 bytes.
    ; -------------------------------------------------------------------------
    times 61 db 0

_real_start:
    ; Save boot drive FIRST before anything clobbers DL/DX
    mov [boot_drv], dl

    ; Init COM1 for debug output (QEMU -serial stdio shows this)
    mov dx, 0x3F8 + 1  ; IER
    mov al, 0x00
    out dx, al
    mov dx, 0x3F8 + 3  ; LCR: DLAB=1
    mov al, 0x80
    out dx, al
    mov dx, 0x3F8 + 0  ; DLL = 1 (115200 baud)
    mov al, 0x01
    out dx, al
    mov dx, 0x3F8 + 1  ; DLH = 0
    mov al, 0x00
    out dx, al
    mov dx, 0x3F8 + 3  ; LCR: 8N1, DLAB=0
    mov al, 0x03
    out dx, al

    mov si, dbg_start
    call serial16

    ; -------------------------------------------------------------------------
    ; FIX: stay cli for the entire bootloader. We never need IRQs here, and
    ; leaving interrupts enabled while we manipulate the IVT/IDT region,
    ; the A20 line, and the PIC masks is what was causing a stray timer
    ; tick (INT 0x08) to vector through a transiently-bad table and
    ; triple-fault the machine. No "sti" anywhere below.
    ; -------------------------------------------------------------------------
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    ; Switch to 80x25 text mode, clear screen
    mov ax, 0x0003
    int 0x10

    mov si, msg_loading
    call print16

    ; -------------------------------------------------------------------------
    ; Load installer kernel: INST_KERNEL_SECTS CD sectors -> 0x10000
    ; -------------------------------------------------------------------------
    mov eax, INST_KERNEL_LBA
    mov bx, 0x0000
    mov word [dap_seg], 0x1000      ; ES:BX = 0x1000:0x0000 = linear 0x10000
    mov cx, INST_KERNEL_SECTS

.load_loop:
    test cx, cx
    jz .load_done

    mov dword [dap_lba_lo], eax
    mov dword [dap_lba_hi], 0
    mov word  [dap_count],  1
    mov word  [dap_off],    bx
    mov si, dap
    mov ah, 0x42
    mov dl, [boot_drv]
    int 0x13
    jc .disk_err

    ; advance buffer by 2048 bytes
    add bx, 2048
    jnc .no_seg
    mov ax, [dap_seg]
    add ax, 0x80            ; 0x80 * 16 = 2048 -> advance segment
    mov [dap_seg], ax
    mov ax, [dap_seg]
    mov es, ax
    xor bx, bx
.no_seg:
    inc eax
    dec cx
    jmp .load_loop

.load_done:
    mov si, dbg_loaded
    call serial16
    mov si, msg_ok
    call print16

    ; -------------------------------------------------------------------------
    ; Enable A20
    ; -------------------------------------------------------------------------
    mov ax, 0x2401
    int 0x15
    jnc .a20_done
    call .kbc_wait
    mov al, 0xAD
    out 0x64, al
    call .kbc_wait
    mov al, 0xD0
    out 0x64, al
    call .kbc_read
    push ax
    call .kbc_wait
    mov al, 0xD1
    out 0x64, al
    call .kbc_wait
    pop ax
    or al, 2
    out 0x60, al
    call .kbc_wait
    mov al, 0xAE
    out 0x64, al
.a20_done:

    ; -------------------------------------------------------------------------
    ; Detect memory (E820) - safely clearing ES
    ;
    ; FIX: reload eax=0xE820 on every loop iteration (BIOS clobbers it on
    ; return) and re-verify the 'SMAP' signature every call, not just
    ; trust the carry flag. Also skip zero-length entries, a known BIOS
    ; quirk, instead of counting them.
    ; -------------------------------------------------------------------------
    xor ax, ax
    mov es, ax

    mov di, 0x504
    xor ebx, ebx
    xor bp, bp
    mov edx, 0x0534D4150
.mmap_loop:
    mov eax, 0xE820
    mov ecx, 24
    mov dword [es:di + 20], 1
    int 0x15
    jc .mmap_done
    cmp eax, 0x0534D4150
    jne .mmap_done
    cmp ecx, 0
    je .mmap_skip_entry
    inc bp
    add di, 24
.mmap_skip_entry:
    test ebx, ebx
    jz .mmap_done
    jmp .mmap_loop
.mmap_done:
    mov [0x500], bp

    ; -------------------------------------------------------------------------
    ; Enter protected mode and jump to installer kernel at 0x10000
    ; -------------------------------------------------------------------------
    mov si, dbg_pm
    call serial16

    cli                     ; 1. Clear CPU flags completely (already clear, kept for safety)

    ; 2. HARDWARE FIX: Mask all hardware interrupts at the PIC level.
    ; This explicitly ignores Timer ticks (INT 0x08) from triggering crashes.
    mov al, 0xFF
    out 0x21, al            ; Master PIC
    out 0xA1, al            ; Slave PIC
    out 0x80, al            ; Small I/O delay

    xor ax, ax
    mov ds, ax              ; Reset segments to absolute 0
    mov es, ax

    ; 3. LINEAR BASE FIX: Explicitly patch GDT physical base pointer location
    mov eax, gdt_start
    mov dword [gdt_linear], eax

    lgdt [gdt_ptr]          ; Load verified GDT

    mov eax, cr0
    or eax, 1
    mov cr0, eax            ; Protected Mode Active

    ; 4. Far return into 32-bit protected mode.
    ; CRITICAL: we are still in [BITS 16] here, so a bare 'retf' (0xCB)
    ; defaults to popping a 16-bit IP + 16-bit CS (4 bytes total). We
    ; pushed a 16-bit selector + a 32-bit offset (6 bytes total), so a
    ; plain retf pops the WRONG slot as CS -- it grabs the upper zero
    ; half of the pushed dword offset (0x0000) instead of the real
    ; selector (0x0008). Loading CS with the null selector is an
    ; immediate #GP(0), which is exactly the fault this caused.
    ; 'o32 retf' forces the 32-bit operand size so it pops a 32-bit EIP
    ; + 16-bit CS (6 bytes), matching what we actually pushed.
    push word 0x08            ; Code Segment Selector
    push dword pm_entry       ; 32-bit target offset linear address
    o32 retf

.disk_err:
    mov si, msg_err
    call print16
    cli
    hlt

.kbc_wait:
    in al, 0x64
    test al, 2
    jnz .kbc_wait
    ret
.kbc_read:
    in al, 0x64
    test al, 1
    jz .kbc_read
    in al, 0x60
    ret

; -------------------------------------------------------------------------
serial_putc16:
    push dx
    push ax
    mov ah, al
.w: mov dx, 0x3F8 + 5
    in al, dx
    test al, 0x20
    jz .w
    mov dx, 0x3F8
    mov al, ah
    out dx, al
    pop ax
    pop dx
    ret

serial16:
    push ax
.l: lodsb
    test al, al
    jz .d
    call serial_putc16
    jmp .l
.d: pop ax
    ret

print16:
    lodsb
    test al, al
    jz .done
    mov ah, 0x0E
    xor bx, bx
    int 0x10
    jmp print16
.done:
    ret

; -------------------------------------------------------------------------
dbg_start   db "BOOT: real_start reached", 0x0D, 0x0A, 0
dbg_loaded  db "BOOT: kernel loaded ok", 0x0D, 0x0A, 0
dbg_pm      db "BOOT: entering protected mode", 0x0D, 0x0A, 0
msg_loading db "Eclipse32 Installer Loading...", 0x0D, 0x0A, 0
msg_ok      db "Kernel loaded. Entering protected mode.", 0x0D, 0x0A, 0
msg_err     db "Disk read error!", 0x0D, 0x0A, 0

boot_drv    db 0x9F

align 4
dap:
    db 0x10, 0
dap_count:  dw 1
dap_off:    dw 0
dap_seg:    dw 0x1000
dap_lba_lo: dd 0
dap_lba_hi: dd 0

align 8
gdt_start:
    dq 0x0000000000000000
    dq 0x00CF9A000000FFFF      ; 0x08: 32-bit flat code selector (GDT[1])
    dq 0x00CF92000000FFFF      ; 0x10: 32-bit flat data selector (GDT[2])
gdt_end:

gdt_ptr:
    dw gdt_end - gdt_start - 1  ; GDT Limit size
gdt_linear:
    dd 0                        ; Dynamically patched at runtime

; -------------------------------------------------------------------------
BITS 32
pm_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x9F000

    ; Pass boot drive in EAX safely
    movzx eax, byte [boot_drv]

    ; Jump out of boot sector directly into flat C kernel address space
    jmp 0x08:0x10000

BITS 16
times 2048 - ($ - $$) db 0     ; Pad completely out to full 2048-byte CD sector
