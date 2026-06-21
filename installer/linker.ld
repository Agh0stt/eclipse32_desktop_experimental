OUTPUT_FORMAT(elf32-i386)
OUTPUT_ARCH(i386)
ENTRY(inst_main)
INST_BASE = 0x00010000;
SECTIONS {
    . = INST_BASE;
    inst_start = .;
    .text.entry ALIGN(0x100) : {
        *(.text.entry)
    }
    .text ALIGN(0x10) : {
        *(.text)
        *(.text.*)
    }
    .rodata ALIGN(0x100) : {
        *(.rodata*)
    }
    .data ALIGN(0x100) : {
        *(.data*)
    }
    .bss ALIGN(0x100) : {
        bss_start = .;
        *(.bss*)
        *(COMMON)
        bss_end = .;
    }
    inst_end = .;
    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
        *(.rel.eh_frame*)
    }
}
