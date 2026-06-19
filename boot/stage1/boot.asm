; =============================================================================
; Eclipse32 - Stage 1 Bootloader (MBR, exactly 512 bytes)
; Loads Stage 2 from disk. BMP splash is done by Stage 2 in real mode.
; =============================================================================

[BITS 16]
[ORG 0x7C00]

%define STAGE2_LOAD_OFF     0x8000
%define STAGE2_SECTORS      16
%define STAGE2_START_LBA    1

; =============================================================================
start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    mov [boot_drive], dl

    mov si, msg_boot
    call print16

    ; Load Stage 2 at 0x0000:0x8000
    xor ax, ax
    mov es, ax
    mov bx, STAGE2_LOAD_OFF
    mov eax, STAGE2_START_LBA
    mov cx, STAGE2_SECTORS
    call load_lba

    
    mov dl, [boot_drive]
    jmp 0x0000:STAGE2_LOAD_OFF

.bad:
    mov si, msg_err
    call print16

halt:
    cli
    hlt
    jmp halt

; =============================================================================
; load_lba  EAX=LBA, CX=count, ES:BX=dest
; =============================================================================
load_lba:
    pusha
.loop:
    test cx, cx
    jz .done

    push dword 0
    push eax
    push es
    push bx
    push word 1
    push word 16

    mov si, sp
    mov ah, 0x42
    mov dl, [boot_drive]
    int 0x13
    jc .err

    add sp, 16
    add bx, 512
    jnc .ok
    mov ax, es
    add ax, 0x20
    mov es, ax
.ok:
    inc eax
    dec cx
    jmp .loop
.done:
    popa
    ret
.err:
    add sp, 16
    popa
    mov si, msg_disk
    call print16
    jmp halt

; =============================================================================
; print16  DS:SI = ASCIIZ string
; =============================================================================
print16:
    pusha
.lp:
    lodsb
    test al, al
    jz .dn
    mov ah, 0x0E
    xor bx, bx
    int 0x10
    jmp .lp
.dn:
    popa
    ret

; =============================================================================
boot_drive  db 0
msg_boot    db "Eclipse32", 0x0D, 0x0A, 0
msg_err     db "Bad Stage2!", 0x0D, 0x0A, 0
msg_disk    db "Disk error!", 0x0D, 0x0A, 0

times 510 - ($ - $$) db 0
dw 0xAA55
