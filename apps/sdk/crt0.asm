[BITS 32]

global _start
extern main

section .text
_start:
    ; _start(argc, argv) is called by the loader using cdecl
    mov eax, [esp + 4]    ; argc
    mov edx, [esp + 8]    ; argv
    push edx
    push eax
    call main
    add esp, 8
    mov ebx, eax
    mov eax, 8          ; SYS_exit
    int 0x80
    mov eax, ebx
    ret
