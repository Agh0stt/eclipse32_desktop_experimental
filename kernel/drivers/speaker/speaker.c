#include "speaker.h"

#include "../../arch/x86/pit.h"

void speaker_on(uint32_t freq_hz) {
    if (freq_hz < 20) freq_hz = 20;
    if (freq_hz > 20000) freq_hz = 20000;

    uint32_t div = 1193180u / freq_hz;
    if (div == 0) div = 1;

    outb(0x43, 0xB6);
    outb(0x42, (uint8_t)(div & 0xFF));
    outb(0x42, (uint8_t)((div >> 8) & 0xFF));

    uint8_t val = inb(0x61);
    if ((val & 0x03) != 0x03) outb(0x61, (uint8_t)(val | 0x03));
}

void speaker_off(void) {
    outb(0x61, (uint8_t)(inb(0x61) & 0xFC));
}

void speaker_beep(uint32_t freq_hz, uint32_t duration_ms) {
    speaker_on(freq_hz);
    pit_sleep_ms(duration_ms);
    speaker_off();
}
