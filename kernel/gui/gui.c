// =============================================================================
// Eclipse32 - EclipseGUI Window Manager
// Compositing WM: per-window back-buffers blitted to VBE framebuffer
// Supports: windows, title bars, drag, close, taskbar, mouse cursor
// =============================================================================
#include "gui.h"
#include "../initramfs/initramfs.h"
#include "../drivers/vbe/vbe.h"
#include "../drivers/keyboard/keyboard.h"
#include "../drivers/mouse/mouse.h"
#include "../arch/x86/pit.h"
#include "../mm/heap.h"
#include "../kernel.h"

// ---- 8x16 Font (matches vbe.c) forward-declared ----------------------------
// We re-use the font from vbe driver via a small wrapper
extern void vbe_draw_char(uint32_t col, uint32_t row, char c, uint32_t fg, uint32_t bg);

// We need pixel-level text; embed a tiny 8x8 font for GUI use
// 8x8 CP437 subset (only printable ASCII 0x20-0x7F)
static const uint8_t gui_font8[96][8] = {
    {0,0,0,0,0,0,0,0},           // 0x20 space
    {0x18,0x18,0x18,0x18,0x00,0x18,0x18,0x00}, // !
    {0x6C,0x6C,0x6C,0x00,0x00,0x00,0x00,0x00}, // "
    {0x6C,0x6C,0xFE,0x6C,0xFE,0x6C,0x6C,0x00}, // #
    {0x18,0x3E,0x60,0x3C,0x06,0x7C,0x18,0x00}, // $
    {0x00,0x66,0x6C,0x18,0x30,0x66,0x46,0x00}, // %
    {0x1C,0x36,0x1C,0x38,0x6F,0x66,0x3B,0x00}, // &
    {0x18,0x18,0x18,0x00,0x00,0x00,0x00,0x00}, // '
    {0x0E,0x18,0x30,0x30,0x30,0x18,0x0E,0x00}, // (
    {0x70,0x18,0x0C,0x0C,0x0C,0x18,0x70,0x00}, // )
    {0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00}, // *
    {0x00,0x18,0x18,0x7E,0x18,0x18,0x00,0x00}, // +
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x30}, // ,
    {0x00,0x00,0x00,0x7E,0x00,0x00,0x00,0x00}, // -
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00}, // .
    {0x06,0x06,0x0C,0x18,0x30,0x60,0x60,0x00}, // /
    {0x3C,0x66,0x6E,0x76,0x66,0x66,0x3C,0x00}, // 0
    {0x18,0x38,0x18,0x18,0x18,0x18,0x7E,0x00}, // 1
    {0x3C,0x66,0x06,0x1C,0x30,0x66,0x7E,0x00}, // 2
    {0x3C,0x66,0x06,0x1C,0x06,0x66,0x3C,0x00}, // 3
    {0x0E,0x1E,0x36,0x66,0x7F,0x06,0x0F,0x00}, // 4
    {0x7E,0x60,0x7C,0x06,0x06,0x66,0x3C,0x00}, // 5
    {0x1C,0x30,0x60,0x7C,0x66,0x66,0x3C,0x00}, // 6
    {0x7E,0x66,0x0C,0x18,0x18,0x18,0x18,0x00}, // 7
    {0x3C,0x66,0x66,0x3C,0x66,0x66,0x3C,0x00}, // 8
    {0x3C,0x66,0x66,0x3E,0x06,0x0C,0x38,0x00}, // 9
    {0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x00}, // :
    {0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x30}, // ;
    {0x0E,0x18,0x30,0x60,0x30,0x18,0x0E,0x00}, // <
    {0x00,0x00,0x7E,0x00,0x7E,0x00,0x00,0x00}, // =
    {0x70,0x18,0x0C,0x06,0x0C,0x18,0x70,0x00}, // >
    {0x3C,0x66,0x06,0x0C,0x18,0x00,0x18,0x00}, // ?
    {0x3C,0x66,0x6E,0x6E,0x60,0x62,0x3C,0x00}, // @
    {0x18,0x3C,0x66,0x7E,0x66,0x66,0x66,0x00}, // A
    {0x7C,0x66,0x66,0x7C,0x66,0x66,0x7C,0x00}, // B
    {0x3C,0x66,0x60,0x60,0x60,0x66,0x3C,0x00}, // C
    {0x78,0x6C,0x66,0x66,0x66,0x6C,0x78,0x00}, // D
    {0x7E,0x60,0x60,0x78,0x60,0x60,0x7E,0x00}, // E
    {0x7E,0x60,0x60,0x78,0x60,0x60,0x60,0x00}, // F
    {0x3C,0x66,0x60,0x6E,0x66,0x66,0x3C,0x00}, // G
    {0x66,0x66,0x66,0x7E,0x66,0x66,0x66,0x00}, // H
    {0x3C,0x18,0x18,0x18,0x18,0x18,0x3C,0x00}, // I
    {0x1E,0x0C,0x0C,0x0C,0x0C,0x6C,0x38,0x00}, // J
    {0x66,0x6C,0x78,0x70,0x78,0x6C,0x66,0x00}, // K
    {0x60,0x60,0x60,0x60,0x60,0x60,0x7E,0x00}, // L
    {0x63,0x77,0x7F,0x6B,0x63,0x63,0x63,0x00}, // M
    {0x66,0x76,0x7E,0x7E,0x6E,0x66,0x66,0x00}, // N
    {0x3C,0x66,0x66,0x66,0x66,0x66,0x3C,0x00}, // O
    {0x7C,0x66,0x66,0x7C,0x60,0x60,0x60,0x00}, // P
    {0x3C,0x66,0x66,0x66,0x66,0x3C,0x0E,0x00}, // Q
    {0x7C,0x66,0x66,0x7C,0x78,0x6C,0x66,0x00}, // R
    {0x3C,0x66,0x60,0x3C,0x06,0x66,0x3C,0x00}, // S
    {0x7E,0x18,0x18,0x18,0x18,0x18,0x18,0x00}, // T
    {0x66,0x66,0x66,0x66,0x66,0x66,0x3C,0x00}, // U
    {0x66,0x66,0x66,0x66,0x66,0x3C,0x18,0x00}, // V
    {0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00}, // W
    {0x66,0x66,0x3C,0x18,0x3C,0x66,0x66,0x00}, // X
    {0x66,0x66,0x66,0x3C,0x18,0x18,0x18,0x00}, // Y
    {0x7E,0x06,0x0C,0x18,0x30,0x60,0x7E,0x00}, // Z
    {0x3C,0x30,0x30,0x30,0x30,0x30,0x3C,0x00}, // [
    {0x60,0x60,0x30,0x18,0x0C,0x06,0x06,0x00}, // backslash
    {0x3C,0x0C,0x0C,0x0C,0x0C,0x0C,0x3C,0x00}, // ]
    {0x08,0x1C,0x36,0x63,0x00,0x00,0x00,0x00}, // ^
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF}, // _
    {0x18,0x18,0x0C,0x00,0x00,0x00,0x00,0x00}, // `
    {0x00,0x00,0x3C,0x06,0x3E,0x66,0x3E,0x00}, // a
    {0x60,0x60,0x7C,0x66,0x66,0x66,0x7C,0x00}, // b
    {0x00,0x00,0x3C,0x66,0x60,0x66,0x3C,0x00}, // c
    {0x06,0x06,0x3E,0x66,0x66,0x66,0x3E,0x00}, // d
    {0x00,0x00,0x3C,0x66,0x7E,0x60,0x3C,0x00}, // e
    {0x0E,0x18,0x7E,0x18,0x18,0x18,0x18,0x00}, // f
    {0x00,0x00,0x3E,0x66,0x66,0x3E,0x06,0x3C}, // g
    {0x60,0x60,0x7C,0x66,0x66,0x66,0x66,0x00}, // h
    {0x18,0x00,0x38,0x18,0x18,0x18,0x3C,0x00}, // i
    {0x06,0x00,0x06,0x06,0x06,0x06,0x66,0x3C}, // j
    {0x60,0x60,0x66,0x6C,0x78,0x6C,0x66,0x00}, // k
    {0x38,0x18,0x18,0x18,0x18,0x18,0x3C,0x00}, // l
    {0x00,0x00,0x66,0x7F,0x7F,0x6B,0x63,0x00}, // m
    {0x00,0x00,0x7C,0x66,0x66,0x66,0x66,0x00}, // n
    {0x00,0x00,0x3C,0x66,0x66,0x66,0x3C,0x00}, // o
    {0x00,0x00,0x7C,0x66,0x66,0x7C,0x60,0x60}, // p
    {0x00,0x00,0x3E,0x66,0x66,0x3E,0x06,0x06}, // q
    {0x00,0x00,0x7C,0x66,0x60,0x60,0x60,0x00}, // r
    {0x00,0x00,0x3C,0x60,0x3C,0x06,0x7C,0x00}, // s
    {0x18,0x18,0x7E,0x18,0x18,0x18,0x0E,0x00}, // t
    {0x00,0x00,0x66,0x66,0x66,0x66,0x3E,0x00}, // u
    {0x00,0x00,0x66,0x66,0x66,0x3C,0x18,0x00}, // v
    {0x00,0x00,0x63,0x6B,0x7F,0x3E,0x36,0x00}, // w
    {0x00,0x00,0x66,0x3C,0x18,0x3C,0x66,0x00}, // x
    {0x00,0x00,0x66,0x66,0x3E,0x06,0x3C,0x00}, // y
    {0x00,0x00,0x7E,0x0C,0x18,0x30,0x7E,0x00}, // z
    {0x0E,0x18,0x18,0x70,0x18,0x18,0x0E,0x00}, // {
    {0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x00}, // |
    {0x70,0x18,0x18,0x0E,0x18,0x18,0x70,0x00}, // }
    {0x3B,0x6E,0x00,0x00,0x00,0x00,0x00,0x00}, // ~
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // DEL
};

// ---- Mouse cursor (16x16, 1bpp arrow) --------------------------------------
static const uint16_t cursor_mask[16] = {
    0xC000, 0xF000, 0xFC00, 0xFF00,
    0xFFC0, 0xFFF0, 0xFFFC, 0xFFF0,
    0xFFC0, 0xFF00, 0xFC00, 0xF000,
    0xC000, 0x0000, 0x0000, 0x0000,
};
static const uint16_t cursor_img[16] = {
    0x8000, 0xC000, 0xD000, 0xDC00,
    0xDF00, 0xDFC0, 0xDFF0, 0xDFC0,
    0xDF00, 0xDC00, 0xD000, 0xC000,
    0x8000, 0x0000, 0x0000, 0x0000,
};

// ---- State ------------------------------------------------------------------

static gui_window_t  g_windows[GUI_WINDOW_MAX];
static uint32_t      g_next_id    = 1;
static int           g_focused    = -1;   // index into g_windows

// Taskbar button list
#define TASKBAR_MAX_BTNS 8
static uint32_t g_taskbar_wins[TASKBAR_MAX_BTNS];
static int      g_taskbar_count = 0;

// Screen back-buffer (composite everything here, then blit once)
static uint32_t *g_screenbuf = NULL;

// Previous cursor pos for restore
static int32_t g_cur_save_x = -1, g_cur_save_y = -1;

// ---- Screen buffer helpers --------------------------------------------------

static inline void sb_put(int32_t x, int32_t y, uint32_t color) {
    if ((uint32_t)x < GUI_SCREEN_W && (uint32_t)y < GUI_SCREEN_H)
        g_screenbuf[y * GUI_SCREEN_W + x] = color;
}

static inline uint32_t sb_get(int32_t x, int32_t y) {
    if ((uint32_t)x < GUI_SCREEN_W && (uint32_t)y < GUI_SCREEN_H)
        return g_screenbuf[y * GUI_SCREEN_W + x];
    return 0;
}

// ---- GUI drawing context helpers --------------------------------------------

void gui_dc_clear(gui_dc_t *dc, uint32_t color) {
    for (uint32_t y = 0; y < dc->height; y++)
        for (uint32_t x = 0; x < dc->width; x++)
            dc->fb[y * dc->width + x] = color;
}

void gui_dc_fill_rect(gui_dc_t *dc, int32_t x, int32_t y, uint32_t w, uint32_t h, uint32_t color) {
    for (uint32_t row = 0; row < h; row++) {
        int32_t py = y + (int32_t)row;
        if (py < 0 || (uint32_t)py >= dc->height) continue;
        for (uint32_t col = 0; col < w; col++) {
            int32_t px = x + (int32_t)col;
            if (px < 0 || (uint32_t)px >= dc->width) continue;
            dc->fb[(uint32_t)py * dc->width + (uint32_t)px] = color;
        }
    }
}

void gui_dc_draw_rect(gui_dc_t *dc, int32_t x, int32_t y, uint32_t w, uint32_t h, uint32_t color) {
    gui_dc_fill_rect(dc, x,       y,       w, 1, color);
    gui_dc_fill_rect(dc, x,       y+h-1,   w, 1, color);
    gui_dc_fill_rect(dc, x,       y,       1, h, color);
    gui_dc_fill_rect(dc, x+w-1,   y,       1, h, color);
}

void gui_dc_draw_char(gui_dc_t *dc, int32_t x, int32_t y, char c, uint32_t fg, uint32_t bg) {
    uint8_t ch = (uint8_t)c;
    if (ch < 0x20 || ch > 0x7F) return;
    const uint8_t *bmp = gui_font8[ch - 0x20];
    for (int row = 0; row < 8; row++) {
        uint8_t bits = bmp[row];
        for (int col = 0; col < 8; col++) {
            int32_t px = x + col, py = y + row;
            if (px < 0 || py < 0 || (uint32_t)px >= dc->width || (uint32_t)py >= dc->height) continue;
            uint32_t color = (bits & (0x80 >> col)) ? fg : bg;
            if (color != 0xFF000000) // transparent sentinel
                dc->fb[(uint32_t)py * dc->width + (uint32_t)px] = color;
        }
    }
}

void gui_dc_draw_text(gui_dc_t *dc, int32_t x, int32_t y, const char *str, uint32_t fg, uint32_t bg) {
    int32_t cx = x;
    for (; *str; str++) {
        if (*str == '\n') { cx = x; y += 9; continue; }
        gui_dc_draw_char(dc, cx, y, *str, fg, bg);
        cx += 8;
    }
}

void gui_dc_draw_line(gui_dc_t *dc, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color) {
    int32_t dx = (x1>x0)?(x1-x0):(x0-x1), sx = (x0<x1)?1:-1;
    int32_t dy = (y1>y0)?(y1-y0):(y0-y1), sy = (y0<y1)?1:-1;
    dy = -dy;
    int32_t err = dx + dy;
    for(;;) {
        if ((uint32_t)x0 < dc->width && (uint32_t)y0 < dc->height)
            dc->fb[y0 * dc->width + x0] = color;
        if (x0==x1 && y0==y1) break;
        int32_t e2 = 2*err;
        if (e2 >= dy) { if (x0==x1) break; err+=dy; x0+=sx; }
        if (e2 <= dx) { if (y0==y1) break; err+=dx; y0+=sy; }
    }
}

void gui_dc_fill_circle(gui_dc_t *dc, int32_t cx, int32_t cy, int32_t r, uint32_t color) {
    for (int32_t y = -r; y <= r; y++)
        for (int32_t x = -r; x <= r; x++)
            if (x*x + y*y <= r*r)
                gui_dc_fill_rect(dc, cx+x, cy+y, 1, 1, color);
}

void gui_dc_draw_image(gui_dc_t *dc, int32_t x, int32_t y, uint32_t iw, uint32_t ih, const uint32_t *pixels) {
    for (uint32_t row = 0; row < ih; row++) {
        int32_t py = y + (int32_t)row;
        if (py < 0 || (uint32_t)py >= dc->height) continue;
        for (uint32_t col = 0; col < iw; col++) {
            int32_t px = x + (int32_t)col;
            if (px < 0 || (uint32_t)px >= dc->width) continue;
            dc->fb[(uint32_t)py * dc->width + (uint32_t)px] = pixels[row * iw + col];
        }
    }
}

// ---- Window management ------------------------------------------------------

uint32_t gui_window_create(int32_t x, int32_t y, uint32_t w, uint32_t h,
                            const char *title, uint32_t style,
                            gui_paint_fn_t on_paint, gui_event_fn_t on_event,
                            void *userdata)
{
    for (int i = 0; i < GUI_WINDOW_MAX; i++) {
        if (!g_windows[i].used) {
            gui_window_t *win = &g_windows[i];
            kmemset(win, 0, sizeof(*win));
            win->used     = true;
            win->id       = g_next_id++;
            win->x        = x;
            win->y        = y;
            win->w        = w;
            win->h        = h;
            win->style    = style;
            win->on_paint = on_paint;
            win->on_event = on_event;
            win->userdata = userdata;
            win->dirty    = true;

            // Copy title safely
            int ti = 0;
            while (title && title[ti] && ti < 63) { win->title[ti] = title[ti]; ti++; }
            win->title[ti] = 0;

            // Allocate back buffer
            win->backbuf = (uint32_t *)kmalloc(w * h * sizeof(uint32_t));
            if (win->backbuf)
                kmemset(win->backbuf, 0xFF, w * h * sizeof(uint32_t));

            // Focus this window
            g_focused = i;

            // Add to taskbar
            if (g_taskbar_count < TASKBAR_MAX_BTNS)
                g_taskbar_wins[g_taskbar_count++] = win->id;

            return win->id;
        }
    }
    return 0;
}

void gui_window_destroy(uint32_t id) {
    for (int i = 0; i < GUI_WINDOW_MAX; i++) {
        if (g_windows[i].used && g_windows[i].id == id) {
            if (g_windows[i].backbuf) kfree(g_windows[i].backbuf);
            g_windows[i].used = false;
            // Remove from taskbar
            for (int t = 0; t < g_taskbar_count; t++) {
                if (g_taskbar_wins[t] == id) {
                    for (int k = t; k < g_taskbar_count-1; k++)
                        g_taskbar_wins[k] = g_taskbar_wins[k+1];
                    g_taskbar_count--;
                    break;
                }
            }
            // Move focus
            if (g_focused == i) {
                g_focused = -1;
                for (int j = GUI_WINDOW_MAX-1; j >= 0; j--)
                    if (g_windows[j].used) { g_focused = j; break; }
            }
            return;
        }
    }
}

void gui_window_invalidate(uint32_t id) {
    for (int i = 0; i < GUI_WINDOW_MAX; i++)
        if (g_windows[i].used && g_windows[i].id == id) { g_windows[i].dirty = true; return; }
}

void gui_window_set_title(uint32_t id, const char *title) {
    for (int i = 0; i < GUI_WINDOW_MAX; i++) {
        if (g_windows[i].used && g_windows[i].id == id) {
            int ti = 0;
            while (title && title[ti] && ti < 63) { g_windows[i].title[ti] = title[ti]; ti++; }
            g_windows[i].title[ti] = 0;
            g_windows[i].dirty = true;
            return;
        }
    }
}

gui_window_t *gui_window_get(uint32_t id) {
    for (int i = 0; i < GUI_WINDOW_MAX; i++)
        if (g_windows[i].used && g_windows[i].id == id) return &g_windows[i];
    return NULL;
}

void gui_taskbar_register(uint32_t win_id) {
    (void)win_id; // already auto-registered in create
}

// ---- Find window under point ------------------------------------------------

static int find_window_at(int32_t mx, int32_t my) {
    // Iterate top (last) to bottom (first) for hit testing
    for (int i = GUI_WINDOW_MAX - 1; i >= 0; i--) {
        gui_window_t *w = &g_windows[i];
        if (!w->used) continue;
        int32_t wl = w->x - GUI_BORDER;
        int32_t wr = w->x + (int32_t)w->w + GUI_BORDER;
        int32_t wt = w->y - GUI_TITLEBAR_H - GUI_BORDER;
        int32_t wb = w->y + (int32_t)w->h + GUI_BORDER;
        if (mx >= wl && mx < wr && my >= wt && my < wb) return i;
    }
    return -1;
}

static bool point_in_close_btn(gui_window_t *w, int32_t mx, int32_t my) {
    // Close button: top-right corner of title bar
    int32_t bx = w->x + (int32_t)w->w - GUI_CLOSE_BTN_W - 4;
    int32_t by = w->y - GUI_TITLEBAR_H + (GUI_TITLEBAR_H - GUI_CLOSE_BTN_W) / 2;
    return mx >= bx && mx < bx + GUI_CLOSE_BTN_W && my >= by && my < by + GUI_CLOSE_BTN_W;
}

// ---- Compositor: paint window chrome + content to screen buffer -------------

static void draw_window_to_screenbuf(int idx) {
    gui_window_t *w = &g_windows[idx];
    if (!w->used) return;

    bool focused = (idx == g_focused);

    // Call app paint callback into back buffer
    if (w->dirty && w->on_paint && w->backbuf) {
        gui_dc_t dc;
        dc.fb     = w->backbuf;
        dc.width  = w->w;
        dc.height = w->h;
        dc.clip_x = 0; dc.clip_y = 0;
        dc.clip_w = w->w; dc.clip_h = w->h;
        w->on_paint(&dc, w->id, w->userdata);
        w->dirty = false;
    }

    // ---- Draw window chrome to screen buffer --------------------------------

    if (w->style & GUI_STYLE_DECORATED) {
        uint32_t tb_color = focused ? GUI_COLOR_TITLEBAR_ACT : GUI_COLOR_TITLEBAR_INACT;

        // Shadow (simple 2px offset)
        for (int32_t sy = 0; sy < (int32_t)(w->h + GUI_TITLEBAR_H + GUI_BORDER*2); sy++) {
            for (int sx = 0; sx < (int32_t)(w->w + GUI_BORDER*2); sx++) {
                int32_t px = w->x - GUI_BORDER + sx + 4;
                int32_t py = w->y - GUI_TITLEBAR_H - GUI_BORDER + sy + 4;
                uint32_t old = sb_get(px, py);
                sb_put(px, py, gui_blend(0x000000, old, 60));
            }
        }

        // Border (outer)
        int32_t bx = w->x - GUI_BORDER;
        int32_t by = w->y - GUI_TITLEBAR_H - GUI_BORDER;
        uint32_t bw = w->w + GUI_BORDER*2;
        uint32_t bh = w->h + GUI_TITLEBAR_H + GUI_BORDER*2;

        for (uint32_t row = 0; row < bh; row++) {
            for (uint32_t col = 0; col < bw; col++) {
                int32_t px = bx + (int32_t)col;
                int32_t py = by + (int32_t)row;
                // border pixels only
                if (col < (uint32_t)GUI_BORDER || col >= bw-GUI_BORDER ||
                    row < (uint32_t)GUI_BORDER || row >= bh-GUI_BORDER) {
                    sb_put(px, py, GUI_COLOR_WIN_BORDER);
                }
            }
        }

        // Title bar background
        for (int32_t ty = 0; ty < GUI_TITLEBAR_H; ty++) {
            for (int32_t tx = 0; tx < (int32_t)w->w; tx++) {
                // gradient: top slightly lighter
                uint32_t blend_t = (uint32_t)(ty * 255 / GUI_TITLEBAR_H);
                uint32_t top = gui_blend(0xFFFFFF, tb_color, 20);
                uint32_t col = gui_blend(top, tb_color, (uint8_t)blend_t);
                sb_put(w->x + tx, w->y - GUI_TITLEBAR_H + ty, col);
            }
        }

        // Title text (centered vertically in bar)
        const char *title = w->title;
        int32_t tx = w->x + 8;
        int32_t ty = w->y - GUI_TITLEBAR_H + (GUI_TITLEBAR_H - 8) / 2;
        for (int ci = 0; title[ci]; ci++) {
            char c = title[ci];
            if (c < 0x20 || c > 0x7F) c = '?';
            const uint8_t *bmp = gui_font8[(uint8_t)c - 0x20];
            for (int fy = 0; fy < 8; fy++) {
                for (int fx = 0; fx < 8; fx++) {
                    if (bmp[fy] & (0x80 >> fx))
                        sb_put(tx + ci*8 + fx, ty + fy, GUI_COLOR_TEXT_LIGHT);
                }
            }
        }

        // Close button
        int32_t cbx = w->x + (int32_t)w->w - GUI_CLOSE_BTN_W - 4;
        int32_t cby = w->y - GUI_TITLEBAR_H + (GUI_TITLEBAR_H - GUI_CLOSE_BTN_W) / 2;
        // Circle close button
        for (int fy = 0; fy < GUI_CLOSE_BTN_W; fy++) {
            for (int fx = 0; fx < GUI_CLOSE_BTN_W; fx++) {
                int dx = fx - GUI_CLOSE_BTN_W/2, dy = fy - GUI_CLOSE_BTN_W/2;
                if (dx*dx + dy*dy <= (GUI_CLOSE_BTN_W/2)*(GUI_CLOSE_BTN_W/2))
                    sb_put(cbx+fx, cby+fy, GUI_COLOR_CLOSE_BTN);
            }
        }
        // X mark on close button
        for (int k = 3; k < GUI_CLOSE_BTN_W-3; k++) {
            sb_put(cbx+k,                     cby+k,                     GUI_COLOR_TEXT_LIGHT);
            sb_put(cbx+k,                     cby+GUI_CLOSE_BTN_W-1-k,   GUI_COLOR_TEXT_LIGHT);
            if (k+1 < GUI_CLOSE_BTN_W-3) {
                sb_put(cbx+k+1, cby+k,                     GUI_COLOR_TEXT_LIGHT);
                sb_put(cbx+k+1, cby+GUI_CLOSE_BTN_W-1-k,   GUI_COLOR_TEXT_LIGHT);
            }
        }
    }

    // Blit content back-buffer to screen buffer
    if (w->backbuf) {
        for (uint32_t row = 0; row < w->h; row++) {
            int32_t py = w->y + (int32_t)row;
            if (py < 0 || py >= GUI_SCREEN_H) continue;
            for (uint32_t col = 0; col < w->w; col++) {
                int32_t px = w->x + (int32_t)col;
                if (px < 0 || px >= GUI_SCREEN_W) continue;
                g_screenbuf[py * GUI_SCREEN_W + px] = w->backbuf[row * w->w + col];
            }
        }
    }
}

// ---- Draw taskbar to screen buffer ------------------------------------------

static void draw_taskbar(void) {
    int32_t ty_base = GUI_SCREEN_H - GUI_TASKBAR_H;

    // Background
    for (int32_t y = ty_base; y < GUI_SCREEN_H; y++) {
        for (int32_t x = 0; x < GUI_SCREEN_W; x++) {
            // subtle gradient
            uint32_t blend = (uint32_t)((y - ty_base) * 40 / GUI_TASKBAR_H);
            uint32_t col = gui_blend(0x1C2A3A, GUI_COLOR_TASKBAR, (uint8_t)(200+blend));
            g_screenbuf[y * GUI_SCREEN_W + x] = col;
        }
    }

    // Top border line
    for (int32_t x = 0; x < GUI_SCREEN_W; x++)
        g_screenbuf[ty_base * GUI_SCREEN_W + x] = GUI_COLOR_ACCENT;

    // "Eclipse32" logo button on left
    int32_t logo_x = 6, logo_y = ty_base + (GUI_TASKBAR_H - 8) / 2;
    const char *logo = "Eclipse32";
    for (int ci = 0; logo[ci]; ci++) {
        char c = logo[ci];
        const uint8_t *bmp = gui_font8[(uint8_t)c - 0x20];
        for (int fy = 0; fy < 8; fy++)
            for (int fx = 0; fx < 8; fx++)
                if (bmp[fy] & (0x80>>fx))
                    sb_put(logo_x + ci*8 + fx, logo_y + fy, GUI_COLOR_ACCENT);
    }

    // Taskbar window buttons
    int32_t btn_x = 90, btn_y = ty_base + 4;
    uint32_t btn_w = 120, btn_h = GUI_TASKBAR_H - 8;

    for (int t = 0; t < g_taskbar_count && t < TASKBAR_MAX_BTNS; t++) {
        uint32_t wid = g_taskbar_wins[t];
        gui_window_t *win = gui_window_get(wid);
        if (!win) continue;

        // Find if focused
        bool is_focused = false;
        for (int i = 0; i < GUI_WINDOW_MAX; i++)
            if (g_windows[i].used && g_windows[i].id == wid && i == g_focused)
                { is_focused = true; break; }

        uint32_t btn_color = is_focused ? GUI_COLOR_TASKBAR_ACT : GUI_COLOR_TASKBAR_BTN;

        // Button background
        for (uint32_t r = 0; r < btn_h; r++) {
            for (uint32_t c = 0; c < btn_w; c++) {
                int32_t px = btn_x + (int32_t)c;
                int32_t py = btn_y + (int32_t)r;
                // rounded corners (simple)
                if ((c == 0 || c == btn_w-1) && (r == 0 || r == btn_h-1)) continue;
                sb_put(px, py, btn_color);
            }
        }

        // Button border
        for (uint32_t r = 0; r < btn_h; r++) {
            sb_put(btn_x,        btn_y+(int32_t)r, GUI_COLOR_WIN_BORDER);
            sb_put(btn_x+(int32_t)btn_w-1, btn_y+(int32_t)r, GUI_COLOR_WIN_BORDER);
        }
        for (uint32_t c = 0; c < btn_w; c++) {
            sb_put(btn_x+(int32_t)c, btn_y,            GUI_COLOR_WIN_BORDER);
            sb_put(btn_x+(int32_t)c, btn_y+(int32_t)btn_h-1, GUI_COLOR_WIN_BORDER);
        }

        // Button label
        int32_t lx = btn_x + 6, ly = btn_y + (btn_h - 8) / 2;
        int max_chars = (btn_w - 12) / 8;
        for (int ci = 0; win->title[ci] && ci < max_chars; ci++) {
            char c = win->title[ci];
            if (c < 0x20 || c > 0x7F) continue;
            const uint8_t *bmp = gui_font8[(uint8_t)c - 0x20];
            for (int fy = 0; fy < 8; fy++)
                for (int fx = 0; fx < 8; fx++)
                    if (bmp[fy] & (0x80>>fx))
                        sb_put(lx + ci*8 + fx, ly + fy, GUI_COLOR_TEXT_LIGHT);
        }

        btn_x += (int32_t)(btn_w + 4);
    }

    // Clock (simple ms uptime) on right
    uint32_t ticks = pit_ticks();
    uint32_t secs  = ticks / 1000;
    uint32_t mins  = secs / 60;
    uint32_t hrs   = mins / 60;
    secs %= 60; mins %= 60; hrs %= 24;

    char timebuf[16];
    // Simple itoa for HH:MM:SS
    timebuf[0] = '0' + hrs/10;  timebuf[1] = '0' + hrs%10;  timebuf[2] = ':';
    timebuf[3] = '0' + mins/10; timebuf[4] = '0' + mins%10; timebuf[5] = ':';
    timebuf[6] = '0' + secs/10; timebuf[7] = '0' + secs%10; timebuf[8] = 0;

    int32_t clk_x = GUI_SCREEN_W - 80;
    int32_t clk_y = ty_base + (GUI_TASKBAR_H - 8) / 2;
    for (int ci = 0; timebuf[ci]; ci++) {
        char c = timebuf[ci];
        const uint8_t *bmp = gui_font8[(uint8_t)c - 0x20];
        for (int fy = 0; fy < 8; fy++)
            for (int fx = 0; fx < 8; fx++)
                if (bmp[fy] & (0x80>>fx))
                    sb_put(clk_x + ci*8 + fx, clk_y + fy, GUI_COLOR_TEXT_LIGHT);
    }
}

// ---- Draw desktop background ------------------------------------------------

static void draw_desktop(void) {
    // Gradient background: dark blue top to slightly lighter bottom
    for (int32_t y = 0; y < GUI_SCREEN_H - GUI_TASKBAR_H; y++) {
        float t = (float)y / (GUI_SCREEN_H - GUI_TASKBAR_H);
        uint32_t r = (uint32_t)(0x0D + t * 0x10);
        uint32_t g = (uint32_t)(0x11 + t * 0x08);
        uint32_t b = (uint32_t)(0x17 + t * 0x15);
        uint32_t color = (r << 16) | (g << 8) | b;
        for (int32_t x = 0; x < GUI_SCREEN_W; x++)
            g_screenbuf[y * GUI_SCREEN_W + x] = color;
    }

    // Watermark text in center
    const char *wm = "Eclipse32 GUI";
    int32_t tx = (GUI_SCREEN_W - (int32_t)(14*8)) / 2;
    int32_t ty2 = (GUI_SCREEN_H - GUI_TASKBAR_H) / 2;
    for (int ci = 0; wm[ci]; ci++) {
        char c = wm[ci];
        if (c < 0x20 || c > 0x7F) continue;
        const uint8_t *bmp = gui_font8[(uint8_t)c - 0x20];
        for (int fy = 0; fy < 8; fy++)
            for (int fx = 0; fx < 8; fx++)
                if (bmp[fy] & (0x80>>fx))
                    sb_put(tx + ci*8 + fx, ty2 + fy, 0x00203050);
    }
}

// ---- Draw mouse cursor to screen buffer ------------------------------------

static void draw_cursor(int32_t mx, int32_t my) {
    for (int row = 0; row < 16; row++) {
        for (int col = 0; col < 16; col++) {
            int bit = 15 - col;
            bool in_mask = !!(cursor_mask[row] & (1 << bit));
            if (!in_mask) continue;
            bool is_white = !!(cursor_img[row] & (1 << bit));
            uint32_t color = is_white ? 0x00FFFFFF : 0x00000000;
            sb_put(mx + col, my + row, color);
        }
    }
}

// ---- Composite everything and blit to VBE ----------------------------------

static void composite_frame(void) {
    draw_desktop();

    // Draw all windows bottom to top
    // First pass: non-focused windows
    for (int i = 0; i < GUI_WINDOW_MAX; i++) {
        if (g_windows[i].used && i != g_focused)
            draw_window_to_screenbuf(i);
    }
    // Second pass: focused window on top
    if (g_focused >= 0 && g_windows[g_focused].used)
        draw_window_to_screenbuf(g_focused);

    draw_taskbar();
    draw_cursor(mouse_x(), mouse_y());

    // Blit screen buffer to VBE framebuffer
    vbe_blit_buffer(g_screenbuf, GUI_SCREEN_W, GUI_SCREEN_H);
}

// ---- Input handling ---------------------------------------------------------

static bool g_prev_btn_left = false;

static void handle_mouse(void) {
    bool cur_left = mouse_btn_left();
    int32_t mx = mouse_x(), my = mouse_y();
    bool just_pressed  = cur_left && !g_prev_btn_left;
    bool just_released = !cur_left && g_prev_btn_left;

    if (just_pressed) {
        // Focus window
        int hit = find_window_at(mx, my);
        if (hit >= 0) {
            // Fire FOCUS_LOST on old focused
            if (g_focused >= 0 && g_focused != hit && g_windows[g_focused].used) {
                gui_event_t ev = {0};
                ev.type = GUI_EVENT_FOCUS_LOST;
                ev.window_id = g_windows[g_focused].id;
                if (g_windows[g_focused].on_event)
                    g_windows[g_focused].on_event(&ev, g_windows[g_focused].userdata);
                g_windows[g_focused].dirty = true;
            }

            g_focused = hit;
            gui_window_t *w = &g_windows[hit];

            // Check close button
            if ((w->style & GUI_STYLE_DECORATED) && point_in_close_btn(w, mx, my)) {
                gui_event_t ev = {0};
                ev.type = GUI_EVENT_CLOSE;
                ev.window_id = w->id;
                if (w->on_event) w->on_event(&ev, w->userdata);
            } else {
                // Start drag (if clicking on titlebar)
                int32_t title_top = w->y - GUI_TITLEBAR_H;
                int32_t title_bot = w->y;
                if ((w->style & GUI_STYLE_DECORATED) && my >= title_top && my < title_bot &&
                    !point_in_close_btn(w, mx, my)) {
                    w->dragging    = true;
                    w->drag_off_x  = mx - w->x;
                    w->drag_off_y  = my - w->y;
                } else {
                    // Mouse down event to app
                    gui_event_t ev = {0};
                    ev.type     = GUI_EVENT_MOUSE_DOWN;
                    ev.mouse_x  = mx - w->x;
                    ev.mouse_y  = my - w->y;
                    ev.mouse_btn = 1;
                    ev.window_id = w->id;
                    if (w->on_event) w->on_event(&ev, w->userdata);
                }

                // Focus gained
                gui_event_t fev = {0};
                fev.type = GUI_EVENT_FOCUS_GAINED;
                fev.window_id = w->id;
                if (w->on_event) w->on_event(&fev, w->userdata);
            }
        }
    }

    // Dragging
    if (cur_left && g_focused >= 0) {
        gui_window_t *w = &g_windows[g_focused];
        if (w->dragging) {
            w->x = mx - w->drag_off_x;
            w->y = my - w->drag_off_y;
            // Clamp
            if (w->x < 0) w->x = 0;
            if (w->y < GUI_TITLEBAR_H) w->y = GUI_TITLEBAR_H;
            if (w->x + (int32_t)w->w > GUI_SCREEN_W) w->x = GUI_SCREEN_W - (int32_t)w->w;
            if (w->y + (int32_t)w->h > GUI_SCREEN_H - GUI_TASKBAR_H) w->y = GUI_SCREEN_H - GUI_TASKBAR_H - (int32_t)w->h;
        }
    }

    if (just_released && g_focused >= 0) {
        gui_window_t *w = &g_windows[g_focused];
        if (w->dragging) {
            w->dragging = false;
        } else {
            gui_event_t ev = {0};
            ev.type = GUI_EVENT_MOUSE_UP;
            ev.mouse_x  = mx - w->x;
            ev.mouse_y  = my - w->y;
            ev.mouse_btn = 1;
            ev.window_id = w->id;
            if (w->on_event) w->on_event(&ev, w->userdata);
        }
    }

    // Mouse move events
    if (mouse_has_event()) {
        mouse_get_event(); // drain queue
        if (g_focused >= 0 && g_windows[g_focused].used) {
            gui_window_t *w = &g_windows[g_focused];
            gui_event_t ev = {0};
            ev.type    = GUI_EVENT_MOUSE_MOVE;
            ev.mouse_x = mx - w->x;
            ev.mouse_y = my - w->y;
            ev.mouse_btn = cur_left ? 1 : 0;
            ev.window_id = w->id;
            if (w->on_event) w->on_event(&ev, w->userdata);
        }
    }

    g_prev_btn_left = cur_left;
}

static void handle_keyboard(void) {
    while (keyboard_has_event()) {
        key_event_t ke = keyboard_get_event();
        if (ke.released) continue;
        if (g_focused < 0 || !g_windows[g_focused].used) continue;
        gui_window_t *w = &g_windows[g_focused];
        gui_event_t ev = {0};
        ev.type      = GUI_EVENT_KEY_DOWN;
        ev.key_ascii = ke.ascii;
        ev.key_code  = ke.keycode;
        ev.key_shift = ke.shift;
        ev.key_ctrl  = ke.ctrl;
        ev.window_id = w->id;
        if (w->on_event) w->on_event(&ev, w->userdata);
        w->dirty = true;
    }
}

// ---- Public: init & run loop ------------------------------------------------

void gui_init(void) {
    kmemset(g_windows, 0, sizeof(g_windows));

    // Allocate screen buffer
    g_screenbuf = (uint32_t *)kmalloc(GUI_SCREEN_W * GUI_SCREEN_H * sizeof(uint32_t));
    if (!g_screenbuf) return; // fatal

    // Init mouse
    mouse_init();
}

void gui_dispatch_once(void) {
    handle_keyboard();
    handle_mouse();
    composite_frame();
}

void gui_run(void) {
    for (;;) {
        gui_dispatch_once();
        // ~60 fps target: yield a bit
        pit_sleep_ms(16);
    }
}
