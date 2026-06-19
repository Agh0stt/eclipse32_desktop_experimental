; =============================================================================
; Eclipse32 Kernel Entry Point (kernel/arch/x86/entry.asm)
; Called by stage2 bootloader with EAX = boot_info pointer
; =============================================================================

[BITS 32]

global kernel_entry_asm
global kernel_main
global isr_stub_table
extern kmain
%define KERNEL_STACK_SIZE   0x4000      ; 16KB kernel stack
%define TSS_ADDR            0x5000

; kernel_entry_asm MUST be the very first bytes of the binary so that
; stage2's "jmp 0x100000" lands here. Linker script places .text._start first.
section .text._start

kernel_entry_asm:
    ; EAX = boot_info* (passed by stage2)
    ; Set up kernel stack FIRST before touching anything
    mov esp, kernel_stack_top

    ; Reload segment registers
    mov cx, 0x10
    mov ds, cx
    mov es, cx
    mov fs, cx
    mov gs, cx
    mov ss, cx

    ; Enable SSE
    mov ecx, cr0
    and ecx, ~0x4
    or  ecx, 0x2
    mov cr0, ecx
    mov ecx, cr4
    or  ecx, 0x600
    mov cr4, ecx

    ; Push boot_info* as argument to kmain, then call
    push eax
    call kmain

    cli
.halt:
    hlt
    jmp .halt

; All remaining code goes in the normal .text section
section .text

; =============================================================================
; ISR Stubs - 256 handlers with error code normalization
; =============================================================================

%macro ISR_NOERR 1
isr_stub_%1:
    push dword 0                ; dummy error code
    push dword %1               ; interrupt number
    jmp isr_common_handler
%endmacro

%macro ISR_ERR 1
isr_stub_%1:
    push dword %1               ; interrupt number (error code already pushed)
    jmp isr_common_handler
%endmacro

; CPU exceptions
ISR_NOERR 0     ; Division by zero
ISR_NOERR 1     ; Debug
ISR_NOERR 2     ; NMI
ISR_NOERR 3     ; Breakpoint
ISR_NOERR 4     ; Overflow
ISR_NOERR 5     ; Bound range exceeded
ISR_NOERR 6     ; Invalid opcode
ISR_NOERR 7     ; Device not available
ISR_ERR   8     ; Double fault
ISR_NOERR 9     ; Coprocessor segment overrun
ISR_ERR   10    ; Invalid TSS
ISR_ERR   11    ; Segment not present
ISR_ERR   12    ; Stack segment fault
ISR_ERR   13    ; General protection fault
ISR_ERR   14    ; Page fault
ISR_NOERR 15    ; Reserved
ISR_NOERR 16    ; x87 FP exception
ISR_ERR   17    ; Alignment check
ISR_NOERR 18    ; Machine check
ISR_NOERR 19    ; SIMD FP exception
ISR_NOERR 20    ; Virtualization exception
%assign i 21
%rep 11
ISR_NOERR i
%assign i i+1
%endrep

; IRQs (32-47)
%assign i 32
%rep 16
ISR_NOERR i
%assign i i+1
%endrep

; Syscall (128)
isr_stub_128:
    push dword 0
    push dword 128
    jmp isr_common_handler

; Fill remaining
%assign i 48
%rep 80
ISR_NOERR i
%assign i i+1
%endrep

; =============================================================================
; Common ISR handler - saves all registers, calls C handler
; =============================================================================
isr_common_handler:
    pusha
    push ds
    push es
    push fs
    push gs

    ; Load kernel data segment
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Call C interrupt handler
    ; Stack layout now: gs, fs, es, ds, edi, esi, ebp, esp_dummy,
    ;                   ebx, edx, ecx, eax, int_num, err_code, eip, cs, eflags
    push esp                    ; pointer to registers struct
    extern interrupt_dispatch
    call interrupt_dispatch
    add esp, 4

    pop gs
    pop fs
    pop es
    pop ds
    popa
    add esp, 8                  ; pop err_code and int_num
    iret

; =============================================================================
; ISR stub pointer table (used by C to set up IDT)
; =============================================================================
section .data

global isr_stub_table
isr_stub_table:
%assign i 0
%rep 256
    ; Stubs 0-128 are all generated above; 129-255 are not, so fall back to stub_0
    %if i <= 128
        dd isr_stub_%+i
    %else
        dd isr_stub_0
    %endif
%assign i i+1
%endrep

; =============================================================================
; Kernel Stack
; =============================================================================
section .bss
align 16
kernel_stack:
    resb KERNEL_STACK_SIZE
kernel_stack_top:
