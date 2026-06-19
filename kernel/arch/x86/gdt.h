// =============================================================================
// Eclipse32 - GDT (Global Descriptor Table) with TSS
// =============================================================================
#pragma once
#include "../../kernel.h"

// Segment selectors
#define GDT_NULL        0x00
#define GDT_KCODE       0x08    // Kernel code segment
#define GDT_KDATA       0x10    // Kernel data segment
#define GDT_UCODE       0x18    // User code segment (ring 3)
#define GDT_UDATA       0x20    // User data segment (ring 3)
#define GDT_TSS         0x28    // Task State Segment

// Access byte flags
#define GDT_PRESENT     0x80
#define GDT_RING0       0x00
#define GDT_RING3       0x60
#define GDT_SYSTEM      0x10
#define GDT_EXEC        0x08
#define GDT_DC          0x04
#define GDT_RW          0x02
#define GDT_ACCESSED    0x01

// Flags nibble
#define GDT_GRAN_4K     0x80
#define GDT_32BIT       0x40
#define GDT_16BIT       0x00

void gdt_init(void);
void gdt_set_tss(uint32_t esp0);
