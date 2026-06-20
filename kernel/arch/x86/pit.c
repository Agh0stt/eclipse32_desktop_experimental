// =============================================================================
// Eclipse32 - PIT (Programmable Interval Timer)
// =============================================================================
#include "pit.h"
#include "pic.h"
#include "idt.h"
#include "../../kernel.h"

#define PIT_FREQ    1193180     // PIT base frequency (Hz)
#define PIT_CMD     0x43
#define PIT_CH0     0x40

static volatile uint32_t tick_count = 0;
static uint32_t ticks_per_ms = 0;

static void pit_handler(void *regs) {
    (void)regs;
    tick_count++;
}

void pit_init(uint32_t hz) {
    uint32_t divisor = PIT_FREQ / hz;
    if (divisor > 0xFFFF) divisor = 0xFFFF;

    ticks_per_ms = hz / 1000;
    if (ticks_per_ms == 0) ticks_per_ms = 1;

    // Channel 0, lobyte/hibyte, mode 3 (square wave)
    outb(PIT_CMD, 0x36);
    outb(PIT_CH0, divisor & 0xFF);
    outb(PIT_CH0, (divisor >> 8) & 0xFF);

    idt_register_handler(32, pit_handler);  // IRQ0 = INT 32
    pic_unmask_irq(0);
}

uint32_t pit_ticks(void) {
    return tick_count;
}
uint32_t pit_ms(void) {
    return (uint32_t)(tick_count / ticks_per_ms);
}

void pit_sleep_ms(uint32_t ms) {
    uint32_t target = pit_ms() + ms;
    while (pit_ms() < target) {
        asm volatile("hlt");
    }
}
