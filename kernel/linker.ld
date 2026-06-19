/* =============================================================================
 * Eclipse32 - Kernel Linker Script
 * Kernel loads at 1MB (0x100000)
 * ============================================================================= */

OUTPUT_FORMAT(elf32-i386)
OUTPUT_ARCH(i386)
ENTRY(kernel_entry_asm)

KERNEL_BASE = 0x00100000;

SECTIONS {
    . = KERNEL_BASE;

    kernel_start = .;

    /* Text (code) section */
    .text ALIGN(0x1000) : {
        *(.text._start)
        *(.text*)
    }

    /* Read-only data */
    .rodata ALIGN(0x1000) : {
        *(.rodata*)
    }

    /* Initialized data */
    .data ALIGN(0x1000) : {
        *(.data*)
    }

    /* BSS - uninitialized data (zeroed at startup) */
    .bss ALIGN(0x1000) : {
        bss_start = .;
        *(.bss*)
        *(COMMON)
        bss_end = .;
    }

    kernel_end = .;

    /* Discard debugging info to keep kernel small */
    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
        *(.rel.eh_frame*)
    }
}
