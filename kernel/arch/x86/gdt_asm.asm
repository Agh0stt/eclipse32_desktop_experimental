; =============================================================================
; Eclipse32 - GDT/TSS flush routines
; =============================================================================
[BITS 32]

global gdt_flush
global tss_flush

; gdt_flush(uint32_t gdtr_ptr)
; Loads new GDT and reloads all segment registers
gdt_flush:
    mov eax, [esp + 4]
    lgdt [eax]

    ; Reload segments via far jump
    mov ax, 0x10            ; kernel data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Far jump to reload CS
    jmp 0x08:.flush
.flush:
    ret

; tss_flush - loads TSS selector into TR
tss_flush:
    mov ax, 0x28            ; TSS selector (index 5 * 8 = 40 = 0x28)
    or  ax, 3               ; ring3 RPL (not strictly needed for TSS)
    ltr ax
    ret
