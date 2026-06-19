// =============================================================================
// Eclipse32 - VGA Text Mode Driver (80x25)
// =============================================================================
#include "vga.h"
#include "../../kernel.h"
#include <stdarg.h>

#define VGA_WIDTH   80
#define VGA_HEIGHT  25
#define VGA_MEMORY  ((volatile uint16_t *)0xB8000)

static uint8_t  vga_fg    = VGA_COLOR_LIGHT_GREY;
static uint8_t  vga_bg    = VGA_COLOR_BLACK;
static uint16_t vga_col   = 0;
static uint16_t vga_row   = 0;

static inline uint16_t vga_entry(uint8_t ch, uint8_t color) {
    return (uint16_t)ch | ((uint16_t)color << 8);
}

void vga_init(void) {
    vga_col = 0;
    vga_row = 0;
    vga_fg  = VGA_COLOR_LIGHT_GREY;
    vga_bg  = VGA_COLOR_BLACK;
    vga_clear();
    vga_cursor_enable(14, 15);
}

void vga_clear(void) {
    uint16_t blank = vga_entry(' ', (vga_bg << 4) | vga_fg);
    for (uint16_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        VGA_MEMORY[i] = blank;
    }
    vga_col = 0;
    vga_row = 0;
    vga_cursor_move(0, 0);
}

void vga_set_color(uint8_t fg, uint8_t bg) {
    vga_fg = fg;
    vga_bg = bg;
}

void vga_get_color(uint8_t *fg, uint8_t *bg) {
    if (fg) *fg = vga_fg;
    if (bg) *bg = vga_bg;
}

static void vga_scroll(void) {
    uint16_t blank = vga_entry(' ', (vga_bg << 4) | vga_fg);
    // Move all rows up by one
    for (uint16_t y = 0; y < VGA_HEIGHT - 1; y++) {
        for (uint16_t x = 0; x < VGA_WIDTH; x++) {
            VGA_MEMORY[y * VGA_WIDTH + x] = VGA_MEMORY[(y+1) * VGA_WIDTH + x];
        }
    }
    // Clear last row
    for (uint16_t x = 0; x < VGA_WIDTH; x++) {
        VGA_MEMORY[(VGA_HEIGHT-1) * VGA_WIDTH + x] = blank;
    }
    vga_row = VGA_HEIGHT - 1;
}

void vga_putchar(char c) {
    if (c == '\n') {
        vga_col = 0;
        vga_row++;
    } else if (c == '\r') {
        vga_col = 0;
    } else if (c == '\t') {
        vga_col = (vga_col + 8) & ~7;
        if (vga_col >= VGA_WIDTH) {
            vga_col = 0;
            vga_row++;
        }
    } else if (c == '\b') {
        if (vga_col > 0) {
            vga_col--;
            VGA_MEMORY[vga_row * VGA_WIDTH + vga_col] =
                vga_entry(' ', (vga_bg << 4) | vga_fg);
        }
    } else {
        VGA_MEMORY[vga_row * VGA_WIDTH + vga_col] =
            vga_entry((uint8_t)c, (vga_bg << 4) | vga_fg);
        vga_col++;
        if (vga_col >= VGA_WIDTH) {
            vga_col = 0;
            vga_row++;
        }
    }

    if (vga_row >= VGA_HEIGHT) {
        vga_scroll();
    }

    vga_cursor_move(vga_col, vga_row);
}

void vga_puts(const char *s) {
    while (*s) vga_putchar(*s++);
}

void vga_put_at(uint16_t x, uint16_t y, char c, uint8_t color) {
    if (x < VGA_WIDTH && y < VGA_HEIGHT) {
        VGA_MEMORY[y * VGA_WIDTH + x] = vga_entry((uint8_t)c, color);
    }
}

void vga_cursor_enable(uint8_t start, uint8_t end) {
    outb(0x3D4, 0x0A);
    outb(0x3D5, (inb(0x3D5) & 0xC0) | start);
    outb(0x3D4, 0x0B);
    outb(0x3D5, (inb(0x3D5) & 0xE0) | end);
}

void vga_cursor_disable(void) {
    outb(0x3D4, 0x0A);
    outb(0x3D5, 0x20);
}

void vga_cursor_move(uint16_t x, uint16_t y) {
    uint16_t pos = y * VGA_WIDTH + x;
    outb(0x3D4, 0x0F);
    outb(0x3D5, pos & 0xFF);
    outb(0x3D4, 0x0E);
    outb(0x3D5, (pos >> 8) & 0xFF);
}

void vga_get_cursor(uint16_t *x, uint16_t *y) {
    if (x) *x = vga_col;
    if (y) *y = vga_row;
}

// -------------------------------------------------------------------------
// Minimal printf implementation for kernel debugging
// -------------------------------------------------------------------------
static void printf_putchar(char c) {
    vga_putchar(c);
}

static void printf_puts(const char *s) {
    if (!s) s = "(null)";
    while (*s) printf_putchar(*s++);
}

static void printf_uint(uint32_t n, uint8_t base, bool upper, int width, char pad) {
    char buf[32];
    int len = 0;

    if (n == 0) {
        buf[len++] = '0';
    } else {
        while (n > 0) {
            uint8_t d = n % base;
            buf[len++] = d < 10 ? '0' + d : (upper ? 'A' : 'a') + d - 10;
            n /= base;
        }
    }

    // Padding
    while (len < width) {
        buf[len++] = pad;
    }

    // Reverse
    for (int i = 0; i < len / 2; i++) {
        char tmp = buf[i]; buf[i] = buf[len-1-i]; buf[len-1-i] = tmp;
    }

    buf[len] = 0;
    printf_puts(buf);
}

void vga_printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    while (*fmt) {
        if (*fmt != '%') {
            printf_putchar(*fmt++);
            continue;
        }
        fmt++;  // skip '%'

        int width = 0;
        char pad = ' ';
        if (*fmt == '0') { pad = '0'; fmt++; }
        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }

        switch (*fmt) {
        case 'd': {
            int32_t n = va_arg(args, int32_t);
            if (n < 0) { printf_putchar('-'); n = -n; }
            printf_uint((uint32_t)n, 10, false, width, pad);
            break;
        }
        case 'u':
            printf_uint(va_arg(args, uint32_t), 10, false, width, pad);
            break;
        case 'x':
            printf_uint(va_arg(args, uint32_t), 16, false, width, pad);
            break;
        case 'X':
            printf_uint(va_arg(args, uint32_t), 16, true, width, pad);
            break;
        case 'p':
            printf_puts("0x");
            printf_uint(va_arg(args, uint32_t), 16, false, 8, '0');
            break;
        case 's':
            printf_puts(va_arg(args, const char *));
            break;
        case 'c':
            printf_putchar((char)va_arg(args, int));
            break;
        case '%':
            printf_putchar('%');
            break;
        default:
            printf_putchar('%');
            printf_putchar(*fmt);
            break;
        }
        fmt++;
    }

    va_end(args);
}
