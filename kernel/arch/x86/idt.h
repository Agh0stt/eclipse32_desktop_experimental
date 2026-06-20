// =============================================================================
// Eclipse32 - IDT Header
// =============================================================================
#pragma once
#include "../../kernel.h"

typedef void (*irq_handler_t)(void *regs);

void idt_init(void);
void idt_register_handler(uint8_t vec, irq_handler_t handler);
