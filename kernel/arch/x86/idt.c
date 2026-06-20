// =============================================================================
// Eclipse32 - IDT (Interrupt Descriptor Table) & Interrupt Dispatch
// =============================================================================
#include "idt.h"
#include "../../kernel.h"
#include "../../drivers/vga/vga.h"

typedef struct {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  zero;
    uint8_t  type_attr;
    uint16_t offset_high;
} PACKED idt_entry_t;

typedef struct {
    uint16_t limit;
    uint32_t base;
} PACKED idtr_t;

// CPU register state passed to interrupt handlers
typedef struct {
    uint32_t gs, fs, es, ds;
    uint32_t edi, esi, ebp, esp_dummy;
    uint32_t ebx, edx, ecx, eax;
    uint32_t int_num, err_code;
    uint32_t eip, cs, eflags;
    // Only present for privilege change:
    uint32_t user_esp, user_ss;
} PACKED regs_t;

#define IDT_ENTRIES 256

static idt_entry_t idt[IDT_ENTRIES];
static idtr_t idtr;

// Handler table (set by drivers/subsystems)
static irq_handler_t irq_handlers[IDT_ENTRIES];

// ISR stub table from entry.asm
extern uint32_t isr_stub_table[256];

static void idt_set_gate(int idx, uint32_t handler,
                          uint16_t sel, uint8_t flags) {
    idt[idx].offset_low  = handler & 0xFFFF;
    idt[idx].offset_high = (handler >> 16) & 0xFFFF;
    idt[idx].selector    = sel;
    idt[idx].zero        = 0;
    idt[idx].type_attr   = flags;
}

void idt_init(void) {
    // Set all 256 gates to the corresponding ASM stub
    for (int i = 0; i < IDT_ENTRIES; i++) {
        idt_set_gate(i, isr_stub_table[i], 0x08, 0x8E);
    }

    // Syscall gate: ring 3 callable trap gate.
    // Use 0xEF (type=0xF trap gate) so IF is preserved during syscalls.
    // If this is an interrupt gate (0xEE), IF is cleared and blocking
    // keyboard reads inside sys_read(fd=0) will deadlock on hlt.
    idt_set_gate(0x80, isr_stub_table[128], 0x08, 0xEF);

    idtr.base  = (uint32_t)&idt;
    idtr.limit = sizeof(idt) - 1;

    asm volatile("lidt %0" : : "m"(idtr));
}

void idt_register_handler(uint8_t vec, irq_handler_t handler) {
    irq_handlers[vec] = handler;
}

// Exception names for CPU faults
static const char *exception_names[] = {
    "Division By Zero",       "Debug",
    "Non-Maskable Interrupt", "Breakpoint",
    "Overflow",               "Bound Range Exceeded",
    "Invalid Opcode",         "Device Not Available",
    "Double Fault",           "Coprocessor Segment Overrun",
    "Invalid TSS",            "Segment Not Present",
    "Stack Segment Fault",    "General Protection Fault",
    "Page Fault",             "Reserved",
    "x87 FP Exception",       "Alignment Check",
    "Machine Check",          "SIMD FP Exception",
    "Virtualization",
};

// =============================================================================
// Main interrupt dispatcher (called from ASM common handler)
// =============================================================================
void interrupt_dispatch(regs_t *regs) {
    uint32_t vec = regs->int_num;

    // Call registered handler if any
    if (irq_handlers[vec]) {
        irq_handlers[vec](regs);
        goto eoi;
    }

    // Unhandled CPU exception (0-31)
    if (vec < 32) {
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_RED);
        vga_puts("\n\n=== UNHANDLED CPU EXCEPTION ===\n");

        const char *name = (vec < ARRAY_SIZE(exception_names))
                           ? exception_names[vec] : "Unknown";
        vga_printf("Exception %u: %s\n", vec, name);
        vga_printf("Error code: 0x%08X\n", regs->err_code);
        vga_printf("EIP: 0x%08X  CS:  0x%04X\n", regs->eip, regs->cs);
        vga_printf("EAX: 0x%08X  EBX: 0x%08X\n", regs->eax, regs->ebx);
        vga_printf("ECX: 0x%08X  EDX: 0x%08X\n", regs->ecx, regs->edx);
        vga_printf("ESI: 0x%08X  EDI: 0x%08X\n", regs->esi, regs->edi);
        vga_printf("EBP: 0x%08X  ESP: 0x%08X\n", regs->ebp, regs->esp_dummy);
        vga_printf("EFLAGS: 0x%08X\n", regs->eflags);

        if (vec == 14) {
            uint32_t cr2;
            asm volatile("mov %%cr2, %0" : "=r"(cr2));
            vga_printf("CR2 (fault addr): 0x%08X\n", cr2);
        }

        asm volatile("cli; hlt");
        for(;;);
    }

eoi:
    // Send EOI to PIC for hardware IRQs (32-47)
    if (vec >= 32 && vec < 48) {
        if (vec >= 40) outb(0xA0, 0x20);   // slave EOI
        outb(0x20, 0x20);                   // master EOI
    }
}
