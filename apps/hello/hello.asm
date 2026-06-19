[BITS 32]
org 0

section .text
start:
    call .get_base
.get_base:
    pop esi
    sub esi, .get_base

    mov eax, 1              ; SYS_write
    mov ebx, 1              ; stdout
    lea ecx, [esi + msg]
    mov edx, msg_len
    int 0x80

    mov eax, 8              ; SYS_exit
    mov ebx, 0
    int 0x80

    mov eax, 0
    ret

section .data
msg db "Hello from Eclipse32 app (.e32)", 10
msg_len equ $ - msg
