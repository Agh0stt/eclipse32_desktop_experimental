// =============================================================================
// Eclipse32 - GDT Implementation
// =============================================================================
#include "gdt.h"
#include "../../kernel.h"

// GDT entry structure
typedef struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  flags_limit_high;  // upper 4 bits = flags, lower 4 = limit[19:16]
    uint8_t  base_high;
} PACKED gdt_entry_t;

// GDTR register value
typedef struct {
    uint16_t limit;
    uint32_t base;
} PACKED gdtr_t;

// Task State Segment (32-bit)
typedef struct {
    uint32_t prev_tss;
    uint32_t esp0;      // kernel stack pointer
    uint32_t ss0;       // kernel stack segment
    uint32_t esp1;
    uint32_t ss1;
    uint32_t esp2;
    uint32_t ss2;
    uint32_t cr3;
    uint32_t eip;
    uint32_t eflags;
    uint32_t eax, ecx, edx, ebx;
    uint32_t esp, ebp, esi, edi;
    uint32_t es, cs, ss, ds, fs, gs;
    uint32_t ldt;
    uint16_t trap;
    uint16_t iomap_base;
} PACKED tss_t;

#define GDT_ENTRIES 7

static gdt_entry_t gdt[GDT_ENTRIES];
static gdtr_t gdtr;
static tss_t tss;

extern void gdt_flush(uint32_t gdtr_ptr);  // in gdt_asm.asm
extern void tss_flush(void);

static void gdt_set_entry(int idx, uint32_t base, uint32_t limit,
                           uint8_t access, uint8_t flags) {
    gdt[idx].base_low         = (base & 0xFFFF);
    gdt[idx].base_mid         = (base >> 16) & 0xFF;
    gdt[idx].base_high        = (base >> 24) & 0xFF;
    gdt[idx].limit_low        = (limit & 0xFFFF);
    gdt[idx].flags_limit_high = ((limit >> 16) & 0x0F) | (flags & 0xF0);
    gdt[idx].access           = access;
}

void gdt_init(void) {
    // Null descriptor
    gdt_set_entry(0, 0, 0, 0, 0);

    // Kernel Code: base=0, limit=4GB, ring0, 32-bit, 4K granularity
    gdt_set_entry(1, 0, 0xFFFFF,
        GDT_PRESENT | GDT_SYSTEM | GDT_EXEC | GDT_RW,
        GDT_GRAN_4K | GDT_32BIT);

    // Kernel Data: base=0, limit=4GB, ring0
    gdt_set_entry(2, 0, 0xFFFFF,
        GDT_PRESENT | GDT_SYSTEM | GDT_RW,
        GDT_GRAN_4K | GDT_32BIT);

    // User Code: ring3
    gdt_set_entry(3, 0, 0xFFFFF,
        GDT_PRESENT | GDT_RING3 | GDT_SYSTEM | GDT_EXEC | GDT_RW,
        GDT_GRAN_4K | GDT_32BIT);

    // User Data: ring3
    gdt_set_entry(4, 0, 0xFFFFF,
        GDT_PRESENT | GDT_RING3 | GDT_SYSTEM | GDT_RW,
        GDT_GRAN_4K | GDT_32BIT);

    // TSS descriptor
    uint32_t tss_base  = (uint32_t)&tss;
    uint32_t tss_limit = sizeof(tss_t) - 1;
    gdt_set_entry(5, tss_base, tss_limit,
        0x89,   // present, ring0, 32-bit available TSS
        0x00);

    // Init TSS
    tss.ss0    = GDT_KDATA;
    tss.esp0   = 0;             // set per-process at context switch
    tss.cs     = GDT_KCODE | 3;
    tss.ss     = GDT_KDATA | 3;
    tss.ds     = GDT_KDATA | 3;
    tss.es     = GDT_KDATA | 3;
    tss.fs     = GDT_KDATA | 3;
    tss.gs     = GDT_KDATA | 3;
    tss.iomap_base = sizeof(tss_t);

    gdtr.base  = (uint32_t)&gdt;
    gdtr.limit = sizeof(gdt) - 1;

    gdt_flush((uint32_t)&gdtr);
    tss_flush();
}

void gdt_set_tss(uint32_t esp0) {
    tss.esp0 = esp0;
    tss.ss0  = GDT_KDATA;
}
