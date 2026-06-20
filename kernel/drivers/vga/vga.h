// =============================================================================
// Eclipse32 - VGA Text Mode Driver Header
// =============================================================================
#pragma once
#include "../../kernel.h"

// Standard VGA colors
typedef enum {
    VGA_COLOR_BLACK          = 0,
    VGA_COLOR_BLUE           = 1,
    VGA_COLOR_GREEN          = 2,
    VGA_COLOR_CYAN           = 3,
    VGA_COLOR_RED            = 4,
    VGA_COLOR_MAGENTA        = 5,
    VGA_COLOR_BROWN          = 6,
    VGA_COLOR_LIGHT_GREY     = 7,
    VGA_COLOR_DARK_GREY      = 8,
    VGA_COLOR_LIGHT_BLUE     = 9,
    VGA_COLOR_LIGHT_GREEN    = 10,
    VGA_COLOR_LIGHT_CYAN     = 11,
    VGA_COLOR_LIGHT_RED      = 12,
    VGA_COLOR_LIGHT_MAGENTA  = 13,
    VGA_COLOR_YELLOW         = 14,
    VGA_COLOR_WHITE          = 15,
} vga_color_t;

void vga_init(void);
void vga_clear(void);
void vga_set_color(uint8_t fg, uint8_t bg);
void vga_get_color(uint8_t *fg, uint8_t *bg);
void vga_putchar(char c);
void vga_puts(const char *s);
void vga_printf(const char *fmt, ...);
void vga_put_at(uint16_t x, uint16_t y, char c, uint8_t color);
void vga_cursor_enable(uint8_t start, uint8_t end);
void vga_cursor_disable(void);
void vga_cursor_move(uint16_t x, uint16_t y);
void vga_get_cursor(uint16_t *x, uint16_t *y);
