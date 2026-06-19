// =============================================================================
// Eclipse32 - VBE Framebuffer Driver Header
// =============================================================================
#pragma once
#include "../../kernel.h"
#include <stdarg.h>

// Common 32bpp ARGB color constants
#define VBE_COLOR_BLACK         0x00000000
#define VBE_COLOR_WHITE         0x00FFFFFF
#define VBE_COLOR_RED           0x00FF0000
#define VBE_COLOR_GREEN         0x0000FF00
#define VBE_COLOR_BLUE          0x000000FF
#define VBE_COLOR_CYAN          0x0000FFFF
#define VBE_COLOR_MAGENTA       0x00FF00FF
#define VBE_COLOR_YELLOW        0x00FFFF00
#define VBE_COLOR_ORANGE        0x00FF8000
#define VBE_COLOR_DARK_GREY     0x00404040
#define VBE_COLOR_LIGHT_GREY    0x00C0C0C0
#define VBE_COLOR_DARK_BLUE     0x00000080
#define VBE_COLOR_DARK_GREEN    0x00008000
#define VBE_COLOR_DARK_RED      0x00800000
#define VBE_COLOR_BRIGHT_GREEN  0x0039FF14
#define VBE_COLOR_PURPLE        0x008000FF

// Eclipse32 brand palette
#define ECLIPSE_BG      0x000D1117   // near-black blue
#define ECLIPSE_FG      0x00E6EDF3   // near-white
#define ECLIPSE_ACCENT  0x0058A6FF   // electric blue
#define ECLIPSE_GREEN   0x003FB950   // terminal green
#define ECLIPSE_RED     0x00F85149   // error red
#define ECLIPSE_YELLOW  0x00D29922   // warning yellow
#define ECLIPSE_CYAN    0x0079C0FF   // info cyan
#define ECLIPSE_PROMPT  0x0058A6FF   // prompt blue

void     vbe_init(uint32_t phys_fb, uint32_t width, uint32_t height,
                  uint32_t pitch, uint8_t bpp);
bool     vbe_active(void);

// Pixel ops
void     vbe_put_pixel(uint32_t x, uint32_t y, uint32_t color);
uint32_t vbe_get_pixel(uint32_t x, uint32_t y);
void     vbe_clear(uint32_t color);

// Primitives
void     vbe_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void     vbe_draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void     vbe_draw_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color);
void     vbe_draw_circle(int32_t cx, int32_t cy, int32_t r, uint32_t color);
void     vbe_fill_circle(int32_t cx, int32_t cy, int32_t r, uint32_t color);

// Text
uint32_t vbe_text_cols(void);
uint32_t vbe_text_rows(void);
void     vbe_set_text_color(uint32_t fg, uint32_t bg);
void     vbe_draw_char(uint32_t col, uint32_t row, char c, uint32_t fg, uint32_t bg);
void     vbe_putchar(char c);
void     vbe_puts(const char *s);
void     vbe_printf(const char *fmt, ...);
void     vbe_set_cursor(uint32_t col, uint32_t row);
void     vbe_get_cursor(uint32_t *col, uint32_t *row);
void     vbe_draw_cursor(bool visible);
void vbe_blit_buffer(const uint32_t *buf, uint32_t width, uint32_t height);

uint32_t *vbe_get_fb(void);
uint32_t  vbe_get_width(void);
uint32_t  vbe_get_height(void);
uint32_t  vbe_get_pitch(void);
uint32_t  vbe_get_bpp(void);
