// Eclipse32 GUI  –  Eclipse32-style desktop
// Fixes: double-buffer (no flicker), cursor always on top,
//        proper window z-order, start menu, 32 apps, terminal window

#include "../kernel.h"
#include "gui_desktop.h"
#include "gui_sdk.h"
#include "../drivers/keyboard/keyboard.h"
#include "../drivers/mouse/mouse.h"
#include "../arch/x86/pit.h"
#include "../syscall/syscall.h"
#include "../sched/sched.h"
#include "gui_compat.h"
#include "../../net/net.h"

// ============================================================
// Theme system — runtime color variables
// ============================================================
int g_theme = THEME_MODERN;  // default: modern

uint32_t COL_DESKTOP;
uint32_t COL_WIN_BG;
uint32_t COL_WIN_TITLE_ACT;
uint32_t COL_WIN_TITLE_INA;
uint32_t COL_WIN_TITLE_TXT;
uint32_t COL_BORDER_LIGHT;
uint32_t COL_BORDER_DARK;
uint32_t COL_BUTTON_FACE;
uint32_t COL_BUTTON_TXT;
uint32_t COL_TEXT;
uint32_t COL_ICON_LABEL;
uint32_t COL_ICON_SEL_BG;
uint32_t COL_TASKBAR_BG;
uint32_t COL_TASKBAR_TXT;
uint32_t COL_MENU_BG;
uint32_t COL_MENU_SEL;
uint32_t COL_MENU_SEL_TXT;
uint32_t COL_MENU_TXT;
uint32_t COL_MENU_DIV;

void apply_theme(int theme_id) {
    g_theme = theme_id;
    if (theme_id == THEME_CLASSIC) {
        // ── Classic: Eclipse32 grey look ──
        COL_DESKTOP       = RGB(0, 128, 128);     // teal
        COL_WIN_BG        = RGB(192, 192, 192);
        COL_WIN_TITLE_ACT = RGB(0, 0, 128);       // deep blue
        COL_WIN_TITLE_INA = RGB(128, 128, 128);
        COL_WIN_TITLE_TXT = RGB(255, 255, 255);
        COL_BORDER_LIGHT  = RGB(255, 255, 255);
        COL_BORDER_DARK   = RGB(64, 64, 64);
        COL_BUTTON_FACE   = RGB(192, 192, 192);
        COL_BUTTON_TXT    = RGB(0, 0, 0);
        COL_TEXT          = RGB(0, 0, 0);
        COL_ICON_LABEL    = RGB(255, 255, 255);
        COL_ICON_SEL_BG   = RGB(0, 0, 128);
        COL_TASKBAR_BG    = RGB(192, 192, 192);   // grey bottom bar
        COL_TASKBAR_TXT   = RGB(0, 0, 0);
        COL_MENU_BG       = RGB(192, 192, 192);
        COL_MENU_SEL      = RGB(0, 0, 128);
        COL_MENU_SEL_TXT  = RGB(255, 255, 255);
        COL_MENU_TXT      = RGB(0, 0, 0);
        COL_MENU_DIV      = RGB(128, 128, 128);
    } else if (theme_id == THEME_MIDNIGHT) {
        // ── Midnight: deep navy desktop, silver/ice accents ──
        COL_DESKTOP       = RGB(8,  12,  40);
        COL_WIN_BG        = RGB(18, 22,  54);
        COL_WIN_TITLE_ACT = RGB(60, 80, 160);
        COL_WIN_TITLE_INA = RGB(35, 40,  80);
        COL_WIN_TITLE_TXT = RGB(200, 220, 255);
        COL_BORDER_LIGHT  = RGB(80, 100, 180);
        COL_BORDER_DARK   = RGB(10,  14,  40);
        COL_BUTTON_FACE   = RGB(30,  36,  80);
        COL_BUTTON_TXT    = RGB(200, 220, 255);
        COL_TEXT          = RGB(200, 220, 255);
        COL_ICON_LABEL    = RGB(200, 220, 255);
        COL_ICON_SEL_BG   = RGB(60,  80, 160);
        COL_TASKBAR_BG    = RGB(10,  14,  35);
        COL_TASKBAR_TXT   = RGB(180, 200, 255);
        COL_MENU_BG       = RGB(14,  18,  50);
        COL_MENU_SEL      = RGB(60,  80, 160);
        COL_MENU_SEL_TXT  = RGB(220, 235, 255);
        COL_MENU_TXT      = RGB(180, 200, 255);
        COL_MENU_DIV      = RGB(40,  50, 100);
    } else if (theme_id == THEME_SUNSET) {
        // ── Sunset: warm purple/orange, deep dusk feel ──
        COL_DESKTOP       = RGB(80,  20,  60);
        COL_WIN_BG        = RGB(40,  18,  35);
        COL_WIN_TITLE_ACT = RGB(200, 80,  40);
        COL_WIN_TITLE_INA = RGB(100, 40,  60);
        COL_WIN_TITLE_TXT = RGB(255, 230, 200);
        COL_BORDER_LIGHT  = RGB(220, 120,  60);
        COL_BORDER_DARK   = RGB(40,  10,  20);
        COL_BUTTON_FACE   = RGB(100, 40,  50);
        COL_BUTTON_TXT    = RGB(255, 220, 180);
        COL_TEXT          = RGB(255, 220, 180);
        COL_ICON_LABEL    = RGB(255, 230, 200);
        COL_ICON_SEL_BG   = RGB(180, 60,  30);
        COL_TASKBAR_BG    = RGB(25,  10,  20);
        COL_TASKBAR_TXT   = RGB(255, 200, 150);
        COL_MENU_BG       = RGB(35,  12,  25);
        COL_MENU_SEL      = RGB(200, 80,  40);
        COL_MENU_SEL_TXT  = RGB(255, 235, 200);
        COL_MENU_TXT      = RGB(240, 190, 140);
        COL_MENU_DIV      = RGB(100, 40,  40);
    } else {
        // ── Modern: dark panel, blue accent ──
        COL_DESKTOP       = RGB(30, 87, 153);
        COL_WIN_BG        = RGB(236, 236, 236);
        COL_WIN_TITLE_ACT = RGB(50, 120, 200);
        COL_WIN_TITLE_INA = RGB(110, 110, 120);
        COL_WIN_TITLE_TXT = RGB(255, 255, 255);
        COL_BORDER_LIGHT  = RGB(255, 255, 255);
        COL_BORDER_DARK   = RGB(80, 80, 80);
        COL_BUTTON_FACE   = RGB(220, 220, 220);
        COL_BUTTON_TXT    = RGB(30, 30, 30);
        COL_TEXT          = RGB(20, 20, 20);
        COL_ICON_LABEL    = RGB(255, 255, 255);
        COL_ICON_SEL_BG   = RGB(50, 120, 200);
        COL_TASKBAR_BG    = RGB(42, 42, 42);      // dark top panel
        COL_TASKBAR_TXT   = RGB(230, 230, 230);
        COL_MENU_BG       = RGB(48, 48, 48);
        COL_MENU_SEL      = RGB(70, 140, 220);
        COL_MENU_SEL_TXT  = RGB(255, 255, 255);
        COL_MENU_TXT      = RGB(220, 220, 220);
        COL_MENU_DIV      = RGB(80, 80, 80);
    }
}

// ============================================================
// Wallpaper system
// ============================================================
#define WALLPAPER_COUNT  8
#define WALLPAPER_GRADIENT 0   // special: draw gradient
static int g_wallpaper = WALLPAPER_GRADIENT;  // default: gradient

// Wallpaper color options (solid colors + gradient)
static const uint32_t g_wallpaper_colors[WALLPAPER_COUNT] = {
    0,                          // 0 = gradient (placeholder)
    RGB(0,  128, 128),          // 1 teal
    RGB(30,  60, 120),          // 2 navy
    RGB(60,  20,  80),          // 3 purple
    RGB(20,  80,  30),          // 4 forest green
    RGB(100, 40,  10),          // 5 brown/rust
    RGB(10,  10,  10),          // 6 near-black
    RGB(80,  80,  80),          // 7 grey
};
static const char *g_wallpaper_names[WALLPAPER_COUNT] = {
    "Gradient",
    "Teal",
    "Navy",
    "Purple",
    "Forest",
    "Rust",
    "Black",
    "Grey",
};

// ---- Persist theme + wallpaper to disk ----
static void settings_save(void) {
    char buf[8];
    buf[0] = (char)('0' + g_theme);
    buf[1] = ' ';
    buf[2] = (char)('0' + g_wallpaper);
    buf[3] = '\0';
    fs_write_file("settings.cfg", buf, 3);
}

static void settings_load(void) {
    char buf[16];
    int r = fs_read_file("settings.cfg", buf, sizeof(buf));
    if (r >= 3) {
        int t = buf[0] - '0';
        int w = buf[2] - '0';
        if (t >= 0 && t < THEME_COUNT)      { apply_theme(t); }
        if (w >= 0 && w < WALLPAPER_COUNT)  { g_wallpaper = w; }
    }
}

// ---- Draw desktop background using current wallpaper ----
static void draw_desktop_bg(int32_t desk_top, int32_t desk_h) {
    if (g_wallpaper != WALLPAPER_GRADIENT) {
        gui_fill_rect(0, desk_top, SCREEN_W, desk_h, g_wallpaper_colors[g_wallpaper]);
        return;
    }
    // Gradient — style depends on theme
    for (int32_t row = 0; row < desk_h; row++) {
        uint32_t t = (uint32_t)row * 255 / (uint32_t)desk_h;
        uint32_t r2, g2, b2;
        if (g_theme == THEME_MIDNIGHT) {
            // Deep navy → slightly lighter navy
            r2 = 4  + t *  8 / 255;
            g2 = 8  + t * 18 / 255;
            b2 = 30 + t * 50 / 255;
        } else if (g_theme == THEME_SUNSET) {
            // Deep purple → warm orange at horizon
            r2 = 40  + t * 120 / 255;
            g2 = 5   + t *  40 / 255;
            b2 = 50  - t *  40 / 255; if ((int32_t)b2 < 5) b2 = 5;
        } else {
            // Modern / Classic default blue gradient
            r2 = 20 + t * 10 / 255; if (r2 > 50) r2 = 50;
            g2 = 60 + t * 30 / 255; if (g2 > 100) g2 = 100;
            b2 = 120 + t * 40 / 255; if (b2 > 180) b2 = 180;
        }
        gui_draw_hline(0, desk_top + row, SCREEN_W, RGB(r2, g2, b2));
    }
}

// ============================================================
// Right-click context menu
// ============================================================
// Forward declaration (defined later in file)
static inline uint8_t pt_in(int32_t px, int32_t py, int32_t rx, int32_t ry, int32_t rw, int32_t rh);
#define CTX_NONE     0
#define CTX_DESKTOP  1
static int   g_ctx_open    = 0;   // which context menu (CTX_*)
static int32_t g_ctx_x     = 0;
static int32_t g_ctx_y     = 0;

// Context menu items for desktop
#define CTX_ITEM_WALLPAPER  0
#define CTX_ITEM_THEME      1
#define CTX_ITEM_COUNT      2
static const char *g_ctx_labels[CTX_ITEM_COUNT] = {
    "Change Wallpaper",
    "Change Theme",
};
#define CTX_MENU_W  140
#define CTX_ITEM_H  16

static void draw_context_menu(int32_t mx, int32_t my) {
    if (!g_ctx_open) return;
    int32_t mh = CTX_ITEM_COUNT * CTX_ITEM_H + 4;
    // keep menu on screen
    int32_t cx = g_ctx_x;
    int32_t cy = g_ctx_y;
    if (cx + CTX_MENU_W > SCREEN_W) cx = SCREEN_W - CTX_MENU_W;
    if (cy + mh > SCREEN_H) cy = SCREEN_H - mh;
    // shadow
    gui_fill_rect(cx+3, cy+3, CTX_MENU_W, mh, RGB(0,0,0));
    // background
    gui_fill_rect(cx, cy, CTX_MENU_W, mh, COL_MENU_BG);
    gui_draw_rect_border(cx, cy, CTX_MENU_W, mh, RGB(90,90,90), RGB(30,30,30));
    for (int i = 0; i < CTX_ITEM_COUNT; i++) {
        int32_t iy = cy + 2 + i * CTX_ITEM_H;
        uint8_t hover = pt_in(mx, my, cx, iy, CTX_MENU_W, CTX_ITEM_H);
        if (hover) {
            gui_fill_rect(cx+1, iy, CTX_MENU_W-2, CTX_ITEM_H, COL_MENU_SEL);
            gui_puts(cx+8, iy+4, g_ctx_labels[i], COL_MENU_SEL_TXT, COL_MENU_SEL);
        } else {
            gui_puts(cx+8, iy+4, g_ctx_labels[i], COL_MENU_TXT, COL_MENU_BG);
        }
    }
}

// Returns clicked item index (0..CTX_ITEM_COUNT-1) or -1 if not clicked/closed
static int handle_context_menu_click(int32_t mx, int32_t my) {
    if (!g_ctx_open) return -1;
    int32_t mh = CTX_ITEM_COUNT * CTX_ITEM_H + 4;
    int32_t cx = g_ctx_x;
    int32_t cy = g_ctx_y;
    if (cx + CTX_MENU_W > SCREEN_W) cx = SCREEN_W - CTX_MENU_W;
    if (cy + mh > SCREEN_H) cy = SCREEN_H - mh;
    if (!pt_in(mx, my, cx, cy, CTX_MENU_W, mh)) {
        g_ctx_open = 0;
        return -1;
    }
    for (int i = 0; i < CTX_ITEM_COUNT; i++) {
        int32_t iy = cy + 2 + i * CTX_ITEM_H;
        if (pt_in(mx, my, cx, iy, CTX_MENU_W, CTX_ITEM_H)) {
            g_ctx_open = 0;
            return i;
        }
    }
    return -1;
}

// ============================================================
// Compatibility shims — bridge old GUI to new driver APIs
// ============================================================

// kb_getchar_nowait: return 0 if no printable key press, else ASCII char.
// Drains and discards all release events and non-ASCII press events so they
// don't clog the queue and starve subsequent calls.
static char kb_getchar_nowait(void) {
    while (keyboard_has_event()) {
        key_event_t e = keyboard_get_event();
        if (e.released) continue;          // discard key-up events
        if (e.keycode != KEY_ASCII) continue; // discard non-printable (arrows etc)
        if (e.ascii == 0) continue;        // safety: ignore null chars
        return e.ascii;
    }
    return 0;
}

Framebuffer g_fb = {0};


MouseState g_mouse = {0};

// shell_exec_line stub (terminal window)
__attribute__((weak))
void shell_exec_line(const char *cmd, void (*cb)(const char*, void*), void *ud) {
    (void)cmd; (void)cb; (void)ud;
}

// pit_ticks from new driver
// (already included via pit.h)

// Globals defined in kmain.c / gui_desktop.c
KernelState kstate = {
    .mem_total    = 32 * 1024 * 1024,
    .mem_free     = 16 * 1024 * 1024,
    .disk_sectors = 0,
    .fb_active    = 1,
};


// Sync new mouse driver → old g_mouse struct each frame
static void sync_mouse(void) {
    while (mouse_has_event()) mouse_get_event();
    g_mouse.x       = mouse_x();
    g_mouse.y       = mouse_y();
    g_mouse.buttons = (mouse_btn_left()  ? 1 : 0)
                    | (mouse_btn_right() ? 2 : 0);
}



// ============================================================
// BACK BUFFER  – eliminates flicker entirely
// Each pixel = 3 bytes (24bpp) or 4 bytes (32bpp).
// We keep a 32-bit (XRGB) back buffer and blit to FB once/frame.
// 800×600×4 = 1,920,000 bytes ≈ 1.83 MB  — fits in our 5 MB heap.
// ============================================================
#define BB_W  SCREEN_W
#define BB_H  SCREEN_H
static uint32_t g_bb[BB_W * BB_H];   // back buffer (XRGB32)

extern Framebuffer g_fb;

// Blit back buffer → real framebuffer
static void bb_present(void) {
    if (!g_fb.active) return;
    if (g_fb.bpp == 32) {
        // Direct copy, 4 bytes per pixel
        uint8_t *dst = (uint8_t *)g_fb.addr;
        uint32_t *src = g_bb;
        for (int32_t y = 0; y < BB_H; y++) {
            uint8_t *row = dst + (uint32_t)y * g_fb.pitch;
            for (int32_t x = 0; x < BB_W; x++) {
                uint32_t c = *src++;
                row[0] = c & 0xFF;
                row[1] = (c >> 8) & 0xFF;
                row[2] = (c >> 16) & 0xFF;
                row[3] = 0;
                row += 4;
            }
        }
    } else {
        // 24bpp
        uint8_t *dst = (uint8_t *)g_fb.addr;
        uint32_t *src = g_bb;
        for (int32_t y = 0; y < BB_H; y++) {
            uint8_t *row = dst + (uint32_t)y * g_fb.pitch;
            for (int32_t x = 0; x < BB_W; x++) {
                uint32_t c = *src++;
                row[0] = c & 0xFF;
                row[1] = (c >> 8) & 0xFF;
                row[2] = (c >> 16) & 0xFF;
                row += 3;
            }
        }
    }
}

// ============================================================
// Drawing into back buffer
// ============================================================
static inline void bb_pix(int32_t x, int32_t y, uint32_t col) {
    if ((uint32_t)x < BB_W && (uint32_t)y < BB_H)
        g_bb[(uint32_t)y * BB_W + (uint32_t)x] = col;
}

void gui_fill_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t col) {
    int32_t x2 = x + w, y2 = y + h;
    if (x  < 0) x  = 0;
    if (y  < 0) y  = 0;
    if (x2 > BB_W) x2 = BB_W;
    if (y2 > BB_H) y2 = BB_H;
    for (int32_t row = y; row < y2; row++) {
        uint32_t *p = g_bb + (uint32_t)row * BB_W + (uint32_t)x;
        for (int32_t n = x2 - x; n > 0; n--) *p++ = col;
    }
}

void gui_draw_hline(int32_t x, int32_t y, int32_t len, uint32_t col) {
    gui_fill_rect(x, y, len, 1, col);
}
void gui_draw_vline(int32_t x, int32_t y, int32_t len, uint32_t col) {
    gui_fill_rect(x, y, 1, len, col);
}

void gui_draw_3d_box(int32_t x, int32_t y, int32_t w, int32_t h, uint8_t raised) {
    uint32_t tl  = raised ? COL_BORDER_LIGHT  : COL_BORDER_DARK;
    uint32_t br  = raised ? COL_BORDER_DARK   : COL_BORDER_LIGHT;
    uint32_t tl2 = raised ? RGB(255,255,255)  : RGB(32,32,32);
    uint32_t br2 = raised ? RGB(32,32,32)     : RGB(255,255,255);
    gui_draw_hline(x,     y,     w,   tl);
    gui_draw_vline(x,     y,     h,   tl);
    gui_draw_hline(x,     y+h-1, w,   br);
    gui_draw_vline(x+w-1, y,     h,   br);
    if (w > 2 && h > 2) {
        gui_draw_hline(x+1,   y+1,   w-2, tl2);
        gui_draw_vline(x+1,   y+1,   h-2, tl2);
        gui_draw_hline(x+1,   y+h-2, w-2, br2);
        gui_draw_vline(x+w-2, y+1,   h-2, br2);
    }
}

void gui_draw_rect_border(int32_t x, int32_t y, int32_t w, int32_t h,
                           uint32_t tl, uint32_t br) {
    gui_draw_hline(x,     y,     w,   tl);
    gui_draw_vline(x,     y,     h,   tl);
    gui_draw_hline(x,     y+h-1, w,   br);
    gui_draw_vline(x+w-1, y,     h,   br);
}

// ============================================================
// 8×8 font rendered into back buffer
// ============================================================
extern void fb_draw_char(uint32_t x, uint32_t y, char c, uint32_t fg, uint32_t bg);

// We can't use fb_draw_char because it writes to the real FB, not our back buffer.
// Inline the font data here so we can draw to g_bb directly.

static const uint8_t g_font[128][8] = {
    [' ']={0,0,0,0,0,0,0,0},
    ['!']={0x18,0x18,0x18,0x18,0,0x18,0,0},
    ['"']={0x66,0x66,0x22,0,0,0,0,0},
    ['#']={0x6C,0x6C,0xFF,0x6C,0xFF,0x6C,0x6C,0},
    ['$']={0x18,0x3E,0x60,0x3C,0x06,0x7C,0x18,0},
    ['%']={0x62,0x66,0x0C,0x18,0x30,0x66,0x46,0},
    ['&']={0x38,0x6C,0x38,0x70,0x6F,0x66,0x3F,0},
    ['\'']={0x30,0x30,0x60,0,0,0,0,0},
    ['(']={0x0C,0x18,0x30,0x30,0x30,0x18,0x0C,0},
    [')']={0x30,0x18,0x0C,0x0C,0x0C,0x18,0x30,0},
    ['*']={0,0x66,0x3C,0xFF,0x3C,0x66,0,0},
    ['+']={0,0x18,0x18,0x7E,0x18,0x18,0,0},
    [',']={0,0,0,0,0,0x18,0x18,0x30},
    ['-']={0,0,0,0x7E,0,0,0,0},
    ['.']={0,0,0,0,0,0x18,0x18,0},
    ['/']={0x02,0x06,0x0C,0x18,0x30,0x60,0x40,0},
    ['0']={0x3C,0x66,0x6E,0x76,0x66,0x66,0x3C,0},
    ['1']={0x18,0x38,0x18,0x18,0x18,0x18,0x3C,0},
    ['2']={0x3C,0x66,0x06,0x0C,0x18,0x30,0x7E,0},
    ['3']={0x3C,0x66,0x06,0x1C,0x06,0x66,0x3C,0},
    ['4']={0x06,0x0E,0x1E,0x66,0x7F,0x06,0x06,0},
    ['5']={0x7E,0x60,0x7C,0x06,0x06,0x66,0x3C,0},
    ['6']={0x3C,0x66,0x60,0x7C,0x66,0x66,0x3C,0},
    ['7']={0x7E,0x66,0x0C,0x18,0x18,0x18,0x18,0},
    ['8']={0x3C,0x66,0x66,0x3C,0x66,0x66,0x3C,0},
    ['9']={0x3C,0x66,0x66,0x3E,0x06,0x66,0x3C,0},
    [':']={0,0x18,0x18,0,0x18,0x18,0,0},
    [';']={0,0x18,0x18,0,0x18,0x18,0x30,0},
    ['<']={0x02,0x04,0x08,0x10,0x08,0x04,0x02,0},
    ['=']={0,0,0x7E,0,0x7E,0,0,0},
    ['>']={0x40,0x20,0x10,0x08,0x10,0x20,0x40,0},
    ['?']={0x3C,0x66,0x06,0x0C,0x18,0,0x18,0},
    ['@']={0x3C,0x66,0x6E,0x6E,0x60,0x62,0x3C,0},
    ['A']={0x18,0x3C,0x66,0x7E,0x66,0x66,0x66,0},
    ['B']={0x7C,0x66,0x66,0x7C,0x66,0x66,0x7C,0},
    ['C']={0x3C,0x66,0x60,0x60,0x60,0x66,0x3C,0},
    ['D']={0x78,0x6C,0x66,0x66,0x66,0x6C,0x78,0},
    ['E']={0x7E,0x60,0x60,0x78,0x60,0x60,0x7E,0},
    ['F']={0x7E,0x60,0x60,0x78,0x60,0x60,0x60,0},
    ['G']={0x3C,0x66,0x60,0x6E,0x66,0x66,0x3C,0},
    ['H']={0x66,0x66,0x66,0x7E,0x66,0x66,0x66,0},
    ['I']={0x3C,0x18,0x18,0x18,0x18,0x18,0x3C,0},
    ['J']={0x1E,0x06,0x06,0x06,0x66,0x66,0x3C,0},
    ['K']={0x66,0x6C,0x78,0x70,0x78,0x6C,0x66,0},
    ['L']={0x60,0x60,0x60,0x60,0x60,0x60,0x7E,0},
    ['M']={0x63,0x77,0x7F,0x6B,0x63,0x63,0x63,0},
    ['N']={0x66,0x76,0x7E,0x7E,0x6E,0x66,0x66,0},
    ['O']={0x3C,0x66,0x66,0x66,0x66,0x66,0x3C,0},
    ['P']={0x7C,0x66,0x66,0x7C,0x60,0x60,0x60,0},
    ['Q']={0x3C,0x66,0x66,0x66,0x6E,0x3C,0x0E,0},
    ['R']={0x7C,0x66,0x66,0x7C,0x6C,0x66,0x66,0},
    ['S']={0x3C,0x66,0x60,0x3C,0x06,0x66,0x3C,0},
    ['T']={0x7E,0x18,0x18,0x18,0x18,0x18,0x18,0},
    ['U']={0x66,0x66,0x66,0x66,0x66,0x66,0x3C,0},
    ['V']={0x66,0x66,0x66,0x66,0x66,0x3C,0x18,0},
    ['W']={0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0},
    ['X']={0x66,0x66,0x3C,0x18,0x3C,0x66,0x66,0},
    ['Y']={0x66,0x66,0x3C,0x18,0x18,0x18,0x18,0},
    ['Z']={0x7E,0x06,0x0C,0x18,0x30,0x60,0x7E,0},
    ['[']={0x3C,0x30,0x30,0x30,0x30,0x30,0x3C,0},
    ['\\']={0x40,0x60,0x30,0x18,0x0C,0x06,0x02,0},
    [']']={0x3C,0x0C,0x0C,0x0C,0x0C,0x0C,0x3C,0},
    ['^']={0x18,0x3C,0x66,0,0,0,0,0},
    ['_']={0,0,0,0,0,0,0,0x7E},
    ['`']={0x30,0x18,0x0C,0,0,0,0,0},
    ['a']={0,0,0x3C,0x06,0x3E,0x66,0x3E,0},
    ['b']={0x60,0x60,0x7C,0x66,0x66,0x66,0x7C,0},
    ['c']={0,0,0x3C,0x66,0x60,0x66,0x3C,0},
    ['d']={0x06,0x06,0x3E,0x66,0x66,0x66,0x3E,0},
    ['e']={0,0,0x3C,0x66,0x7E,0x60,0x3C,0},
    ['f']={0x1C,0x30,0x7C,0x30,0x30,0x30,0x30,0},
    ['g']={0,0,0x3E,0x66,0x66,0x3E,0x06,0x3C},
    ['h']={0x60,0x60,0x7C,0x66,0x66,0x66,0x66,0},
    ['i']={0x18,0,0x38,0x18,0x18,0x18,0x3C,0},
    ['j']={0x06,0,0x06,0x06,0x06,0x66,0x3C,0},
    ['k']={0x60,0x60,0x66,0x6C,0x78,0x6C,0x66,0},
    ['l']={0x38,0x18,0x18,0x18,0x18,0x18,0x3C,0},
    ['m']={0,0,0x63,0x77,0x7F,0x6B,0x63,0},
    ['n']={0,0,0x7C,0x66,0x66,0x66,0x66,0},
    ['o']={0,0,0x3C,0x66,0x66,0x66,0x3C,0},
    ['p']={0,0,0x7C,0x66,0x66,0x7C,0x60,0x60},
    ['q']={0,0,0x3E,0x66,0x66,0x3E,0x06,0x06},
    ['r']={0,0,0x6C,0x76,0x60,0x60,0x60,0},
    ['s']={0,0,0x3C,0x60,0x3C,0x06,0x7C,0},
    ['t']={0x18,0x18,0x7E,0x18,0x18,0x18,0x0E,0},
    ['u']={0,0,0x66,0x66,0x66,0x66,0x3E,0},
    ['v']={0,0,0x66,0x66,0x66,0x3C,0x18,0},
    ['w']={0,0,0x63,0x6B,0x7F,0x36,0x22,0},
    ['x']={0,0,0x66,0x3C,0x18,0x3C,0x66,0},
    ['y']={0,0,0x66,0x66,0x3E,0x06,0x3C,0},
    ['z']={0,0,0x7E,0x0C,0x18,0x30,0x7E,0},
    ['{']={0x0E,0x18,0x18,0x70,0x18,0x18,0x0E,0},
    ['|']={0x18,0x18,0x18,0x00,0x18,0x18,0x18,0},
    ['}']={0x70,0x18,0x18,0x0E,0x18,0x18,0x70,0},
    ['~']={0x70,0x92,0x0E,0,0,0,0,0},
};

void gui_putc(int32_t x, int32_t y, char c, uint32_t fg, uint32_t bg) {
    uint8_t ch = (uint8_t)c;
    if (ch >= 128) ch = '?';
    const uint8_t *bm = g_font[ch];
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            uint32_t color = (bm[row] & (0x80 >> col)) ? fg : bg;
            bb_pix(x + col, y + row, color);
        }
    }
}

void gui_puts(int32_t x, int32_t y, const char *s, uint32_t fg, uint32_t bg) {
    while (*s) { gui_putc(x, y, *s++, fg, bg); x += FONT_W; }
}

// gui_puts with clipping rectangle
static void gui_puts_clip(int32_t x, int32_t y, const char *s,
                           uint32_t fg, uint32_t bg,
                           int32_t cx, int32_t cy, int32_t cw, int32_t ch) {
    while (*s) {
        if (x + FONT_W > cx && x < cx + cw && y + FONT_H > cy && y < cy + ch) {
            uint8_t idx = (uint8_t)*s;
            if (idx >= 128) idx = '?';
            const uint8_t *bm = g_font[idx];
            for (int row = 0; row < 8; row++) {
                int py = y + row;
                if (py < cy || py >= cy + ch) continue;
                for (int col = 0; col < 8; col++) {
                    int px = x + col;
                    if (px < cx || px >= cx + cw) continue;
                    bb_pix(px, py, (bm[row] & (0x80>>col)) ? fg : bg);
                }
            }
        }
        x += FONT_W;
        s++;
        if (x >= cx + cw) break;
    }
}

// ============================================================
// Button widget
// ============================================================
int gui_button(int32_t x, int32_t y, int32_t w, int32_t h,
               const char *label, int32_t mx, int32_t my, uint8_t clicked) {
    uint8_t hover   = (mx >= x && mx < x+w && my >= y && my < y+h);
    uint8_t pressed = hover && clicked;
    gui_fill_rect(x, y, w, h, COL_BUTTON_FACE);
    gui_draw_3d_box(x, y, w, h, !pressed);
    int32_t lw = (int32_t)kstrlen(label) * FONT_W;
    gui_puts(x + (w-lw)/2 + (pressed?1:0),
             y + (h-FONT_H)/2 + (pressed?1:0),
             label, COL_BUTTON_TXT, COL_BUTTON_FACE);
    return pressed;
}

// ============================================================
// Window manager state
// ============================================================
static Window      g_wins[MAX_WINDOWS];
static int32_t     g_nwins  = 0;
static int32_t     g_front  = -1;   // index of active window
static uint32_t    g_next_win_id = 1; // monotonic; 0 = uninitialised

/* Look up g_wins[] index by stable window id. Returns -1 if not found. */
static int32_t win_idx_by_id(uint32_t id) {
    for (int32_t i = 0; i < g_nwins; i++)
        if (g_wins[i].id == id) return i;
    return -1;
}

// Set to 1 while a SDK app is running in TASK_APP via the terminal.
// gui_pump_sdk() uses this to yield to the app between chrome and bb_present.
static uint8_t     g_sdk_app_active = 0;
uint8_t gui_sdk_is_active(void) { return g_sdk_app_active; }
// Set to 1 while the user is dragging a window title bar.
static uint8_t     g_dragging = 0;
// Snapshot of each SDK window's content-area origin taken BEFORE the
// drag-position update each frame.  After sched_yield() the app will
// have drawn at these (old) coords; we blit those pixels to the new
// position so the window content follows the chrome with no white flash.
static int32_t     g_sdk_prev_cx[GUI_SDK_MAX_WINS];
static int32_t     g_sdk_prev_cy[GUI_SDK_MAX_WINS];

// Start menu
static uint8_t     g_startmenu = 0; // 1 = open

// Start menu items
typedef struct { const char *label; AppType app; } MenuItem;
static const MenuItem g_menu_items[] = {
    { "Notepad",        APP_NOTEPAD     },
    { "WordPad",        APP_WORDPAD     },
    { "Calculator",     APP_CALCULATOR  },
    { "Calendar",       APP_CALENDAR    },
    { "Clock",          APP_CLOCK       },
    { "Stopwatch",      APP_STOPWATCH   },
    { "---",            APP_NONE        },
    { "System Info",    APP_SYSINFO     },
    { "Task Manager",   APP_TASKMGR     },
    { "Disk Info",      APP_DISKINFO    },
    { "Memory Map",     APP_MEMMAP      },
    { "CPU Monitor",    APP_CPUMON      },
    { "Device Mgr",     APP_DEVMGR      },
    { "Reg Editor",     APP_REGEDIT     },
    { "---",            APP_NONE        },
    { "File Manager",   APP_FILEMAN     },
    { "Hex Viewer",     APP_HEXVIEW     },
    { "Text Viewer",    APP_TEXTVIEW    },
    { "---",            APP_NONE        },
    { "Paint",          APP_PAINT       },
    { "Color Picker",   APP_COLORPICKER },
    { "Screen Saver",   APP_SCREENSAVER },
    { "---",            APP_NONE        },
    { "Snake",          APP_SNAKE       },
    { "Tetris",         APP_TETRIS      },
    { "Pong",           APP_PONG        },
    { "Minesweeper",    APP_MINESWEEPER },
    { "Breakout",       APP_BREAKOUT    },
    { "Tic Tac Toe",    APP_TICTACTOE   },
    { "Slots",          APP_SLOTS       },
    { "Piano",          APP_PIANO       },
    { "---",            APP_NONE        },
    { "Terminal",       APP_TERMINAL    },
    { "Log Viewer",     APP_LOGVIEWER   },
    { "Network Info",   APP_NETINFO     },
    { "IP Config",      APP_IPCONFIG    },
    { "Help",           APP_HELP        },
    { "Settings",       APP_SETTINGS    },
    { "About",          APP_ABOUT       },
};
#define N_MENU_ITEMS  ((int32_t)(sizeof(g_menu_items)/sizeof(g_menu_items[0])))
#define MENU_ITEM_H   12
#define MENU_W        140
// Menu is split into two columns when tall
#define MENU_COL_ROWS 18   // rows per column

static int32_t     g_menu_hover = -1;

// Desktop icons (left column, single-click selects, double-click opens)
static DesktopIcon g_icons[] = {
    {  8,  TASKBAR_H+10, APP_TERMINAL,   "Terminal",  0 },
    {  8,  TASKBAR_H+65, APP_FILEMAN,    "Files",     0 },
    {  8, TASKBAR_H+120, APP_NOTEPAD,    "Notepad",   0 },
    {  8, TASKBAR_H+175, APP_CALCULATOR, "Calc",      0 },
    {  8, TASKBAR_H+230, APP_SYSINFO,    "Sys Info",  0 },
    {  8, TASKBAR_H+285, APP_PAINT,      "Paint",     0 },
    {  8, TASKBAR_H+340, APP_SNAKE,      "Snake",     0 },
    {  8, TASKBAR_H+395, APP_SETTINGS,   "Settings",  0 },
};
#define N_ICONS  ((int32_t)(sizeof(g_icons)/sizeof(g_icons[0])))

static uint32_t g_last_click_tick = 0;
static int32_t  g_last_click_icon = -1;
#define DBLCLICK_MS  500   // 500ms double-click window (pit_ticks = ms at 1000Hz)

// ============================================================
// Window helpers
// ============================================================
static inline int32_t win_ca_x(Window *w){ return w->x + BORDER_W + 2; }
static inline int32_t win_ca_y(Window *w){ return w->y + BORDER_W + TITLE_BAR_H + 3; }
static inline int32_t win_ca_w(Window *w){ return w->w - 2*BORDER_W - 4; }
static inline int32_t win_ca_h(Window *w){ return w->h - (2*BORDER_W + TITLE_BAR_H + 5); }

static inline uint8_t pt_in(int32_t px, int32_t py,
                              int32_t rx, int32_t ry, int32_t rw, int32_t rh) {
    return (px>=rx && px<rx+rw && py>=ry && py<ry+rh);
}
static inline uint8_t hit_win(Window *w, int32_t px, int32_t py) {
    return pt_in(px, py, w->x, w->y, w->w, w->h);
}
static inline uint8_t hit_title(Window *w, int32_t px, int32_t py) {
    return pt_in(px, py, w->x+BORDER_W, w->y+BORDER_W,
                 w->w-2*BORDER_W-TITLE_BAR_H, TITLE_BAR_H);
}
static inline uint8_t hit_close(Window *w, int32_t px, int32_t py) {
    return pt_in(px, py,
        w->x+w->w-BORDER_W-TITLE_BAR_H, w->y+BORDER_W,
        TITLE_BAR_H, TITLE_BAR_H);
}

static void bring_front(int32_t idx) {
    if (idx < 0 || idx >= g_nwins || idx == g_nwins-1) {
        if (g_nwins > 0) g_front = g_nwins-1;
        return;
    }
    Window tmp = g_wins[idx];
    for (int32_t i = idx; i < g_nwins-1; i++) g_wins[i] = g_wins[i+1];
    g_wins[g_nwins-1] = tmp;
    g_front = g_nwins-1;
}

static int32_t open_window(AppType app) {
    // If already open, bring to front
    for (int32_t i = 0; i < g_nwins; i++) {
        if (g_wins[i].app == app && (g_wins[i].flags & WIN_FLAG_VISIBLE)) {
            bring_front(i);
            return g_front;
        }
    }
    if (g_nwins >= MAX_WINDOWS) return -1;

    Window *w = &g_wins[g_nwins];
    kmemset(w, 0, sizeof(Window));
    w->flags = WIN_FLAG_VISIBLE | WIN_FLAG_MOVEABLE | WIN_FLAG_CLOSEABLE;
    w->app   = app;

    // Default size/title per app
    switch (app) {
    case APP_NOTEPAD:     kstrcpy(w->title,"Notepad");       w->w=420;w->h=300;
        kstrcpy(w->st.text,"Eclipse32 Notepad\n\nType here...\n");
        w->st.text_len=(int32_t)kstrlen(w->st.text); break;
    case APP_WORDPAD:     kstrcpy(w->title,"WordPad");       w->w=460;w->h=320;
        kstrcpy(w->st.text,"WordPad - rich text viewer\n\nEclipse32 OS v1.0\nBuilt with zig cc + NASM\n");
        w->st.text_len=(int32_t)kstrlen(w->st.text); break;
    case APP_CALCULATOR:  kstrcpy(w->title,"Calculator");    w->w=200;w->h=240;
        kstrcpy(w->st.calc_disp,"0"); w->st.calc_new=1; break;
    case APP_CALENDAR:    kstrcpy(w->title,"Calendar");      w->w=260;w->h=200; break;
    case APP_CLOCK:       kstrcpy(w->title,"Clock");         w->w=200;w->h=180; break;
    case APP_STOPWATCH:   kstrcpy(w->title,"Stopwatch");     w->w=220;w->h=150; break;
    case APP_SYSINFO:     kstrcpy(w->title,"System Info");   w->w=380;w->h=270; break;
    case APP_TASKMGR:     kstrcpy(w->title,"Task Manager");  w->w=340;w->h=240; break;
    case APP_REGEDIT:     kstrcpy(w->title,"Registry Edit"); w->w=360;w->h=260; break;
    case APP_DEVMGR:      kstrcpy(w->title,"Device Mgr");    w->w=340;w->h=240; break;
    case APP_DISKINFO:    kstrcpy(w->title,"Disk Info");     w->w=340;w->h=230; break;
    case APP_MEMMAP:      kstrcpy(w->title,"Memory Map");    w->w=360;w->h=260; break;
    case APP_CPUMON:      kstrcpy(w->title,"CPU Monitor");   w->w=300;w->h=220; break;
    case APP_FILEMAN:     kstrcpy(w->title,"File Manager");  w->w=400;w->h=280;
        w->st.fm_count=-1; break;
    case APP_HEXVIEW:     kstrcpy(w->title,"Hex Viewer");    w->w=420;w->h=280;
        w->st.hex_addr = 0x100000; break;
    case APP_TEXTVIEW:    kstrcpy(w->title,"Text Viewer");   w->w=400;w->h=280; break;
    case APP_NETINFO:     kstrcpy(w->title,"Network Info");  w->w=340;w->h=220; break;
    case APP_IPCONFIG:    kstrcpy(w->title,"IP Config");     w->w=300;w->h=200; break;
    case APP_PAINT:       kstrcpy(w->title,"Paint");         w->w=440;w->h=340; break;
    case APP_COLORPICKER: kstrcpy(w->title,"Color Picker");  w->w=280;w->h=200; break;
    case APP_SCREENSAVER: kstrcpy(w->title,"Screen Saver");  w->w=360;w->h=240;
        w->st.ss_x=50;w->st.ss_y=50;w->st.ss_dx=2;w->st.ss_dy=2;
        w->st.ss_col=RGB(255,200,0); break;
    case APP_SNAKE:       kstrcpy(w->title,"Snake");         w->w=220;w->h=240;
        w->st.sn_x[0]=10;w->st.sn_y[0]=10;w->st.sn_len=3;
        w->st.sn_dx=1;w->st.sn_food_x=15;w->st.sn_food_y=8; break;
    case APP_PONG:        kstrcpy(w->title,"Pong");          w->w=320;w->h=240;
        w->st.pong_bx=100;w->st.pong_by=80;
        w->st.pong_bdx=2;w->st.pong_bdy=2;
        w->st.pong_p1y=60;w->st.pong_p2y=60; break;
    case APP_MINESWEEPER: kstrcpy(w->title,"Minesweeper");   w->w=220;w->h=240; break;
    case APP_BREAKOUT:    kstrcpy(w->title,"Breakout");      w->w=260;w->h=280; break;
    case APP_TICTACTOE:   kstrcpy(w->title,"Tic Tac Toe");   w->w=200;w->h=220;
        w->st.ttt_turn=1; break;
    case APP_SLOTS:       kstrcpy(w->title,"Slots");         w->w=220;w->h=180;
        w->st.slot_credits=100; break;
    case APP_PIANO:       kstrcpy(w->title,"Piano");         w->w=380;w->h=160; break;
    case APP_TETRIS:      kstrcpy(w->title,"Tetris");        w->w=260;w->h=300;
        w->st.tet_speed=20; break;
    case APP_TERMINAL:    kstrcpy(w->title,"Terminal");      w->w=460;w->h=300;
        kstrcpy(w->st.text,"Eclipse32 Terminal\nType commands below:\n> ");
        w->st.text_len=(int32_t)kstrlen(w->st.text); break;
    case APP_LOGVIEWER:   kstrcpy(w->title,"Log Viewer");    w->w=400;w->h=280; break;
    case APP_HELP:        kstrcpy(w->title,"Help");          w->w=380;w->h=300;
        kstrcpy(w->st.text,
            "Eclipse32 Help\n"
            "==============\n\n"
            "Desktop:\n"
            "  Single click = select icon\n"
            "  Double click = open app\n"
            "  Start button = app menu\n\n"
            "App windows:\n"
            "  Drag title bar = move window\n"
            "  [x] button = close\n"
            "  Taskbar = switch windows\n\n"
            "Apps:\n"
            "  30+ built-in applications\n"
            "  Games, tools, utilities\n");
        w->st.text_len=(int32_t)kstrlen(w->st.text); break;
    case APP_ABOUT:       kstrcpy(w->title,"About Eclipse32"); w->w=300;w->h=220; break;
    case APP_SETTINGS:    kstrcpy(w->title,"Settings");        w->w=380;w->h=560; break;
    default:              kstrcpy(w->title,"Window");        w->w=320;w->h=240; break;
    }

    // Cascade position (respect theme desktop area)
    int32_t desk_top = DESKTOP_TOP_FOR(g_theme);
    int32_t off = g_nwins * 22;
    w->x = 60 + (off % 300);
    w->y = desk_top + 10 + (off % 180);
    if (w->x + w->w > SCREEN_W - 4)  w->x = SCREEN_W - w->w - 4;
    if (w->y + w->h > SCREEN_H - 4)  w->y = desk_top + 4;
    if (w->x < 0) w->x = 0;
    if (w->y < desk_top) w->y = desk_top;

    uint32_t new_id = w->id;     /* capture before bring_front can shuffle */
    g_nwins++;
    bring_front(g_nwins - 1);
    return (int32_t)new_id;
}

static void close_window(int32_t idx) {
    if (idx < 0 || idx >= g_nwins) return;
    Window *cw = &g_wins[idx];

    // Erase the window's pixels (chrome + content + shadow) from g_bb so
    // nothing lingers on screen after the window is gone.  We repaint the
    // desktop background rows that were covered, then let the next frame
    // redraw any other windows on top.
    {
        int32_t _ex = cw->x,  _ey = cw->y;
        int32_t _ew = cw->w + 4, _eh = cw->h + 4; // +4 for shadow
        int32_t _dt = DESKTOP_TOP_FOR(g_theme);
        int32_t _dh = SCREEN_H - _dt - ((g_theme==THEME_CLASSIC)?TASKBAR_H:0);
        for (int32_t _r = _ey; _r < _ey + _eh; _r++) {
            if (_r < 0 || _r >= BB_H) continue;
            uint32_t _bg;
            if (g_wallpaper != WALLPAPER_GRADIENT) {
                _bg = g_wallpaper_colors[g_wallpaper];
            } else {
                int32_t _rel = _r - _dt;
                if (_rel < 0) _rel = 0;
                if (_rel >= _dh) _rel = _dh > 0 ? _dh - 1 : 0;
                uint32_t _t = (uint32_t)_rel * 255 / (_dh > 0 ? (uint32_t)_dh : 1);
                uint32_t _r2, _g2, _b2;
                if (g_theme == THEME_MIDNIGHT) {
                    _r2=4+_t*8/255; _g2=8+_t*18/255; _b2=30+_t*50/255;
                } else if (g_theme == THEME_SUNSET) {
                    _r2=40+_t*120/255; _g2=5+_t*40/255;
                    _b2=(50>_t*40/255)?(50-_t*40/255):5;
                } else {
                    _r2=20+_t*10/255; if(_r2>50)_r2=50;
                    _g2=60+_t*30/255; if(_g2>100)_g2=100;
                    _b2=120+_t*40/255; if(_b2>180)_b2=180;
                }
                _bg = RGB(_r2, _g2, _b2);
            }
            int32_t _x0 = _ex < 0 ? 0 : _ex;
            int32_t _x1 = _ex + _ew > BB_W ? BB_W : _ex + _ew;
            for (int32_t _c = _x0; _c < _x1; _c++)
                g_bb[_r * BB_W + _c] = _bg;
        }
    }

    // If this was an SDK window, clear the active flag so gui_pump stops
    // yielding to the (now-exited) app task.
    if (cw->app == APP_SDK) {
        g_sdk_app_active = 0;
    }

    for (int32_t i = idx; i < g_nwins-1; i++) g_wins[i] = g_wins[i+1];
    g_nwins--;
    g_front = (g_nwins > 0) ? g_nwins-1 : -1;
}

// ============================================================
// Window chrome
// ============================================================
static void draw_chrome(Window *w, uint8_t active) {
    int32_t x=w->x, y=w->y, ww=w->w, wh=w->h;
    // Window shadow
    gui_fill_rect(x+4, y+4, ww, wh, RGB(0,0,0));
    // Fill + outer border
    gui_fill_rect(x, y, ww, wh, COL_WIN_BG);
    gui_draw_rect_border(x, y, ww, wh, RGB(130,130,140), RGB(90,90,100));
    // Title bar gradient (simple two-tone)
    uint32_t tc1 = active ? RGB(50,120,200) : RGB(110,110,120);
    uint32_t tc2 = active ? RGB(30,90,170)  : RGB(90,90,100);
    int32_t tbar_w = ww - 2*BORDER_W;
    int32_t tbar_x = x + BORDER_W;
    int32_t tbar_y = y + BORDER_W;
    for(int32_t row=0; row<TITLE_BAR_H; row++){
        uint32_t c = (row < TITLE_BAR_H/2) ? tc1 : tc2;
        gui_draw_hline(tbar_x, tbar_y+row, tbar_w-TITLE_BAR_H-1, c);
    }
    // Title text
    gui_puts_clip(tbar_x+5, tbar_y+(TITLE_BAR_H-FONT_H)/2,
                  w->title, RGB(255,255,255), tc1,
                  tbar_x+3, tbar_y, tbar_w-TITLE_BAR_H-6, TITLE_BAR_H);
    // Close button [x] — red on hover otherwise dark
    int32_t bx = x+ww-BORDER_W-TITLE_BAR_H;
    int32_t by = y+BORDER_W;
    gui_fill_rect(bx, by, TITLE_BAR_H, TITLE_BAR_H, RGB(180,50,50));
    gui_draw_rect_border(bx, by, TITLE_BAR_H, TITLE_BAR_H, RGB(220,80,80), RGB(130,30,30));
    gui_puts(bx+5, by+(TITLE_BAR_H-FONT_H)/2, "x", RGB(255,255,255), RGB(180,50,50));
    // Client sunken border
    int32_t cax=x+BORDER_W, cay=y+BORDER_W+TITLE_BAR_H+1;
    int32_t caw=ww-2*BORDER_W, cah=wh-(2*BORDER_W+TITLE_BAR_H+1);
    gui_draw_rect_border(cax, cay, caw, cah, RGB(140,140,150), RGB(200,200,210));
}

// ============================================================
// Icon glyphs (32×32 pixel art drawn to back buffer)
// ============================================================
static void draw_icon_glyph(int32_t x, int32_t y, AppType app, uint8_t sel) {
    uint32_t bg = sel ? COL_ICON_SEL_BG : COL_DESKTOP;
    gui_fill_rect(x, y, 32, 32, bg);
    switch (app) {
    case APP_NOTEPAD: case APP_WORDPAD: case APP_TEXTVIEW: case APP_HELP: {
        gui_fill_rect(x+5,y+2,22,27,RGB(255,255,255));
        gui_draw_3d_box(x+5,y+2,22,27,1);
        for(int i=0;i<5;i++) gui_draw_hline(x+8,y+7+i*4,14,RGB(0,0,180));
        gui_fill_rect(x+21,y+2,6,6,bg);
        gui_draw_hline(x+21,y+7,6,RGB(128,128,128));
        gui_draw_vline(x+21,y+2,6,RGB(128,128,128));
        break; }
    case APP_CALCULATOR: {
        gui_fill_rect(x+4,y+2,24,28,RGB(220,220,220));
        gui_draw_3d_box(x+4,y+2,24,28,1);
        gui_fill_rect(x+7,y+5,18,7,RGB(0,200,0));
        for(int r=0;r<3;r++) for(int c=0;c<3;c++){
            gui_fill_rect(x+7+c*6,y+14+r*5,5,4,COL_BUTTON_FACE);
            gui_draw_3d_box(x+7+c*6,y+14+r*5,5,4,1); }
        break; }
    case APP_SYSINFO: case APP_TASKMGR: case APP_CPUMON: {
        gui_fill_rect(x+2,y+2,28,22,RGB(0,0,0));
        gui_fill_rect(x+4,y+4,24,18,RGB(0,0,180));
        gui_draw_3d_box(x+2,y+2,28,22,0);
        gui_fill_rect(x+13,y+24,6,4,RGB(160,160,160));
        gui_fill_rect(x+9,y+27,14,3,RGB(160,160,160));
        gui_puts(x+5,y+9,"SYS",RGB(0,255,0),RGB(0,0,180));
        break; }
    case APP_FILEMAN: {
        gui_fill_rect(x+3,y+10,26,18,RGB(255,200,0));
        gui_fill_rect(x+3,y+7,12,5,RGB(255,200,0));
        gui_draw_3d_box(x+3,y+10,26,18,1);
        for(int f=0;f<3;f++){
            gui_fill_rect(x+6+f*6,y+13,5,11,RGB(255,255,255));
            gui_draw_rect_border(x+6+f*6,y+13,5,11,RGB(128,128,128),RGB(128,128,128)); }
        break; }
    case APP_TERMINAL: {
        gui_fill_rect(x+2,y+2,28,24,RGB(0,0,0));
        gui_draw_3d_box(x+2,y+2,28,24,0);
        gui_fill_rect(x+2,y+26,28,4,RGB(64,64,64));
        gui_puts(x+4,y+6,">_",RGB(0,220,0),RGB(0,0,0));
        gui_draw_hline(x+4,y+16,20,RGB(0,180,0));
        break; }
    case APP_ABOUT: {
        gui_fill_rect(x+6,y+3,20,26,RGB(0,0,128));
        gui_draw_3d_box(x+6,y+3,20,26,1);
        gui_puts(x+8,y+9,"E32",RGB(255,255,0),RGB(0,0,128));
        break; }
    case APP_PAINT: {
        gui_fill_rect(x+2,y+2,28,22,RGB(255,255,255));
        gui_draw_3d_box(x+2,y+2,28,22,0);
        static const uint32_t pc[]={RGB(255,0,0),RGB(0,200,0),RGB(0,0,255),RGB(255,200,0)};
        for(int i=0;i<4;i++) gui_fill_rect(x+4+i*7,y+25,6,5,pc[i]);
        gui_fill_rect(x+10,y+8,12,10,RGB(255,200,200));
        break; }
    case APP_SNAKE: {
        gui_fill_rect(x+2,y+2,28,28,RGB(0,80,0));
        for(int i=0;i<4;i++) gui_fill_rect(x+4+i*5,y+13,4,4,RGB(0,200,0));
        gui_fill_rect(x+24,y+9,4,4,RGB(255,0,0));
        break; }
    case APP_PONG: {
        gui_fill_rect(x+2,y+2,28,28,RGB(0,0,0));
        gui_draw_vline(x+16,y+5,22,RGB(255,255,255));
        gui_fill_rect(x+3,y+9,3,10,RGB(255,255,255));
        gui_fill_rect(x+26,y+12,3,10,RGB(255,255,255));
        gui_fill_rect(x+14,y+14,4,4,RGB(255,255,255));
        break; }
    case APP_MINESWEEPER: {
        gui_fill_rect(x+2,y+2,28,28,RGB(192,192,192));
        gui_draw_3d_box(x+2,y+2,28,28,0);
        for(int r=0;r<4;r++) for(int c=0;c<4;c++){
            uint32_t cc=(r+c)%3==0?RGB(255,0,0):RGB(128,128,128);
            gui_fill_rect(x+4+c*6,y+4+r*6,5,5,cc); }
        break; }
    case APP_CLOCK: case APP_STOPWATCH: case APP_CALENDAR: {
        gui_fill_rect(x+4,y+2,24,28,RGB(255,255,240));
        gui_draw_3d_box(x+4,y+2,24,28,0);
        gui_draw_hline(x+4,y+12,24,RGB(200,200,200));
        gui_puts(x+7,y+5,"12",RGB(0,0,0),RGB(255,255,240));
        gui_puts(x+7,y+14,"3",RGB(0,0,0),RGB(255,255,240));
        break; }
    default: {
        gui_fill_rect(x+4,y+4,24,24,RGB(128,128,200));
        gui_draw_3d_box(x+4,y+4,24,24,1);
        break; }
    }
}

static void draw_icon(DesktopIcon *ic) {
    draw_icon_glyph(ic->x, ic->y, ic->app, ic->selected);
    int32_t lw = (int32_t)kstrlen(ic->label) * FONT_W;
    int32_t lx = ic->x + (32 - lw) / 2;
    if (lx < ic->x - 4) lx = ic->x - 4;
    uint32_t lbg = ic->selected ? COL_ICON_SEL_BG : COL_DESKTOP;
    gui_fill_rect(ic->x - 4, ic->y+33, 40, FONT_H+2, lbg);
    gui_puts(lx, ic->y+34, ic->label, COL_ICON_LABEL, lbg);
}

// ============================================================
// App renderers
// ============================================================

// --- Generic text display (Notepad, WordPad, Help, TextViewer) ---
static void render_text_app(Window *w, int32_t mx, int32_t my, uint8_t click,
                              uint8_t editable) {
    int32_t cx=win_ca_x(w),cy=win_ca_y(w),cw=win_ca_w(w),ch=win_ca_h(w);

    // Toolbar for editable apps (Notepad)
    int32_t txt_y=cy;
    if(editable){
        int32_t tb_h=18;
        gui_fill_rect(cx,cy,cw,tb_h,RGB(230,230,235));
        gui_draw_hline(cx,cy+tb_h-1,cw,RGB(180,180,190));
        if(gui_button(cx+2,cy+2,38,14,"New",mx,my,click)){
            kmemset(w->st.text,0,WIN_TEXT_BUF);
            w->st.text_len=0;
        }
        if(gui_button(cx+44,cy+2,38,14,"Save",mx,my,click)){
            // Save to a default notepad.txt on disk
            fs_write_file("notepad.txt",w->st.text,(uint32_t)w->st.text_len);
        }
        if(gui_button(cx+86,cy+2,42,14,"Load",mx,my,click)){
            int r=fs_read_file("notepad.txt",w->st.text,WIN_TEXT_BUF);
            if(r>0){w->st.text_len=r; w->st.text[r]=0;}
        }
        txt_y=cy+tb_h+1;
    }

    int32_t txt_h=ch-(txt_y-cy);
    gui_fill_rect(cx,txt_y,cw,txt_h,RGB(255,255,255));
    gui_draw_rect_border(cx,txt_y,cw,txt_h,RGB(150,150,160),RGB(200,200,210));
    // Render text
    const char *p=w->st.text;
    int32_t tx=cx+3, ty=txt_y+3-w->st.scroll_y;
    int32_t lx=cx+3;
    while(*p){
        if(*p=='\n'){tx=lx;ty+=FONT_H+1;}
        else {
            if(ty>=txt_y-FONT_H && ty<txt_y+txt_h && tx<cx+cw)
                gui_putc(tx,ty,*p,COL_TEXT,RGB(255,255,255));
            tx+=FONT_W;
        }
        p++;
    }
    // Blinking cursor for editable
    if(editable && (get_ticks()/50)%2==0)
        gui_fill_rect(tx,ty,FONT_W,FONT_H,RGB(50,100,200));

    if(editable){
        char kc=kb_getchar_nowait();
        if(kc){
            if(kc=='\b'){if(w->st.text_len>0){w->st.text_len--;w->st.text[w->st.text_len]=0;}}
            else if(w->st.text_len<WIN_TEXT_BUF-2){
                w->st.text[w->st.text_len++]=kc;
                w->st.text[w->st.text_len]=0;
            }
        }
    }
    (void)mx;(void)my;(void)click;
}

// --- Calculator ---
static const char *g_calc_lbl[4][4]={{"7","8","9","/"}, {"4","5","6","*"},
                                      {"1","2","3","-"}, {"0","C","=","+"}};
#define CBW 35
#define CBH 20

static void calc_press(Window *w, const char *lbl){
    if(lbl[0]>='0'&&lbl[0]<='9'){
        int d=lbl[0]-'0';
        if(w->st.calc_new){w->st.calc_val=d;w->st.calc_new=0;}
        else w->st.calc_val=w->st.calc_val*10+d;
        kitoa(w->st.calc_val,w->st.calc_disp,10);
    }else if(lbl[0]=='C'){
        w->st.calc_val=0;w->st.calc_accum=0;w->st.calc_op=0;w->st.calc_new=1;
        kstrcpy(w->st.calc_disp,"0");
    }else if(lbl[0]=='='){
        if(w->st.calc_op=='+')w->st.calc_accum+=w->st.calc_val;
        else if(w->st.calc_op=='-')w->st.calc_accum-=w->st.calc_val;
        else if(w->st.calc_op=='*')w->st.calc_accum*=w->st.calc_val;
        else if(w->st.calc_op=='/'){if(w->st.calc_val)w->st.calc_accum/=w->st.calc_val;else kstrcpy(w->st.calc_disp,"ERR");}
        else w->st.calc_accum=w->st.calc_val;
        kitoa(w->st.calc_accum,w->st.calc_disp,10);
        w->st.calc_val=w->st.calc_accum;w->st.calc_new=1;w->st.calc_op=0;
    }else{
        if(!w->st.calc_op)w->st.calc_accum=w->st.calc_val;
        else{
            if(w->st.calc_op=='+')w->st.calc_accum+=w->st.calc_val;
            else if(w->st.calc_op=='-')w->st.calc_accum-=w->st.calc_val;
            else if(w->st.calc_op=='*')w->st.calc_accum*=w->st.calc_val;
            else if(w->st.calc_op=='/')if(w->st.calc_val)w->st.calc_accum/=w->st.calc_val;
            kitoa(w->st.calc_accum,w->st.calc_disp,10);
        }
        w->st.calc_op=lbl[0];w->st.calc_new=1;
    }
}

static void render_calculator(Window *w, int32_t mx, int32_t my, uint8_t click){
    int32_t cx=win_ca_x(w),cy=win_ca_y(w),cw=win_ca_w(w),ch=win_ca_h(w);
    gui_fill_rect(cx,cy,cw,ch,COL_WIN_BG);
    int32_t dw=cw-8;
    gui_fill_rect(cx+4,cy+4,dw,18,RGB(200,230,200));
    gui_draw_3d_box(cx+4,cy+4,dw,18,0);
    int32_t dl=(int32_t)kstrlen(w->st.calc_disp);
    gui_puts(cx+4+dw-dl*FONT_W-3,cy+9,w->st.calc_disp,COL_TEXT,RGB(200,230,200));
    int32_t bsx=cx+4,bsy=cy+26;
    for(int r=0;r<4;r++) for(int c=0;c<4;c++){
        int32_t bx=bsx+c*(CBW+3),by=bsy+r*(CBH+3);
        if(gui_button(bx,by,CBW,CBH,g_calc_lbl[r][c],mx,my,click))
            calc_press(w,g_calc_lbl[r][c]);
    }
    (void)ch;
}

// --- Clock ---
static void render_clock(Window *w, int32_t mx, int32_t my, uint8_t click){
    (void)mx;(void)my;(void)click;
    int32_t cx=win_ca_x(w),cy=win_ca_y(w),cw=win_ca_w(w),ch=win_ca_h(w);
    gui_fill_rect(cx,cy,cw,ch,COL_WIN_BG);
    uint32_t ticks=get_ticks();
    uint32_t secs=ticks/100, mins=(secs/60)%60, hrs=(secs/3600)%24;
    char buf[16];
    buf[0]='0'+(hrs/10); buf[1]='0'+(hrs%10); buf[2]=':';
    buf[3]='0'+(mins/10); buf[4]='0'+(mins%10); buf[5]=':';
    uint32_t s=secs%60; buf[6]='0'+(s/10); buf[7]='0'+(s%10); buf[8]=0;
    // Large digital display
    gui_fill_rect(cx+8,cy+20,cw-16,30,RGB(0,0,0));
    gui_draw_3d_box(cx+8,cy+20,cw-16,30,0);
    int32_t tw=(int32_t)kstrlen(buf)*FONT_W;
    gui_puts(cx+8+(cw-16-tw)/2, cy+29, buf, RGB(0,255,0), RGB(0,0,0));
    gui_puts(cx+(cw-8*FONT_W)/2, cy+60, "Eclipse Clock", COL_TEXT, COL_WIN_BG);
    (void)ch;
}

// --- Stopwatch ---
static void render_stopwatch(Window *w, int32_t mx, int32_t my, uint8_t click){
    int32_t cx=win_ca_x(w),cy=win_ca_y(w),cw=win_ca_w(w),ch=win_ca_h(w);
    gui_fill_rect(cx,cy,cw,ch,COL_WIN_BG);
    uint32_t elapsed=0;
    if(w->st.sw_running) elapsed=get_ticks()-w->st.sw_start;
    uint32_t s=(elapsed/100)%60, ms=(elapsed%100);
    char buf[12];
    buf[0]='0'+(s/10);buf[1]='0'+(s%10);buf[2]='.';buf[3]='0'+(ms/10);buf[4]='0'+(ms%10);buf[5]=0;
    gui_fill_rect(cx+8,cy+8,cw-16,24,RGB(0,0,0));
    gui_draw_3d_box(cx+8,cy+8,cw-16,24,0);
    int32_t tw=(int32_t)kstrlen(buf)*FONT_W;
    gui_puts(cx+8+(cw-16-tw)/2,cy+14,buf,RGB(0,255,0),RGB(0,0,0));
    if(gui_button(cx+10,cy+44,60,20,w->st.sw_running?"Stop":"Start",mx,my,click)){
        if(!w->st.sw_running){w->st.sw_start=get_ticks();w->st.sw_running=1;}
        else w->st.sw_running=0;
    }
    if(gui_button(cx+76,cy+44,50,20,"Reset",mx,my,click)){
        w->st.sw_running=0;w->st.sw_start=0;
    }
    (void)ch;
}

// --- Sysinfo ---
static void render_sysinfo(Window *w, int32_t mx, int32_t my, uint8_t click){
    (void)mx;(void)my;(void)click;
    int32_t cx=win_ca_x(w),cy=win_ca_y(w),cw=win_ca_w(w),ch=win_ca_h(w);
    gui_fill_rect(cx,cy,cw,ch,COL_WIN_BG);
    int32_t tx=cx+6,ty=cy+6;
    char nb[32];
    #define SR(lbl,val) gui_puts(tx,ty,lbl,COL_TEXT,COL_WIN_BG); \
        gui_puts(tx+130,ty,val,COL_BLUE,COL_WIN_BG); ty+=FONT_H+4;
    gui_puts(tx,ty,"Eclipse32 System Information",RGB(0,0,128),COL_WIN_BG);
    ty+=FONT_H+6; gui_draw_hline(cx+4,ty,cw-8,COL_BORDER_DARK); ty+=4;
    SR("OS:","Eclipse32 v1.0")  SR("Arch:","x86 (32-bit)")
    SR("CPU:","i686 Compatible")
    kutoa(kstate.mem_total>>10,nb,10); kstrcat(nb," KB"); SR("Total RAM:",nb)
    kutoa(kstate.mem_free>>10,nb,10);  kstrcat(nb," KB"); SR("Free RAM:",nb)
    kutoa(kstate.disk_sectors,nb,10);  kstrcat(nb," sectors"); SR("Disk:",nb)
    SR("FB:", kstate.fb_active?"VESA 800x600x24":"VGA text")
    SR("FS:","EclipseFS")
    SR("Kernel:","0x100000")
    kutoa(get_ticks()/100,nb,10); kstrcat(nb,"s"); SR("Uptime:",nb)
    #undef SR
}

// --- Task Manager ---
static void render_taskmgr(Window *w, int32_t mx, int32_t my, uint8_t click){
    (void)mx;(void)my;(void)click;
    int32_t cx=win_ca_x(w),cy=win_ca_y(w),cw=win_ca_w(w),ch=win_ca_h(w);
    gui_fill_rect(cx,cy,cw,ch,COL_WIN_BG);
    gui_fill_rect(cx,cy,cw,FONT_H+4,COL_WIN_TITLE_ACT);
    gui_puts(cx+4,cy+2,"PID  Name            Status",RGB(255,255,255),COL_WIN_TITLE_ACT);
    gui_draw_hline(cx,cy+FONT_H+4,cw,COL_BORDER_DARK);
    int32_t ty=cy+FONT_H+7;
    // List open windows as "processes"
    gui_puts(cx+4,ty,"  1  kernel          Running",COL_TEXT,COL_WIN_BG); ty+=FONT_H+3;
    gui_puts(cx+4,ty,"  2  gui_run         Running",COL_TEXT,COL_WIN_BG); ty+=FONT_H+3;
    for(int32_t i=0;i<g_nwins&&ty<cy+ch-4;i++){
        char pbuf[40]; pbuf[0]=' '; pbuf[1]=' ';
        kitoa(i+3,pbuf+2,10);
        int32_t l=(int32_t)kstrlen(pbuf);
        pbuf[l]=' '; pbuf[l+1]=' '; pbuf[l+2]=0;
        kstrncat(pbuf,g_wins[i].title,16);
        l=(int32_t)kstrlen(pbuf);
        while(l<28){pbuf[l++]=' ';}
        pbuf[28]=0; kstrcat(pbuf,"Running");
        gui_puts(cx+4,ty,pbuf,COL_TEXT,COL_WIN_BG);
        ty+=FONT_H+3;
    }
    (void)ch;
}

// --- Device Manager ---
static void render_devmgr(Window *w, int32_t mx, int32_t my, uint8_t click){
    (void)mx;(void)my;(void)click;
    int32_t cx=win_ca_x(w),cy=win_ca_y(w),cw=win_ca_w(w),ch=win_ca_h(w);
    gui_fill_rect(cx,cy,cw,ch,COL_WIN_BG);
    int32_t ty=cy+4;
    #define DEV(name,status,col) \
        gui_putc(cx+4,ty,'+',COL_TEXT,COL_WIN_BG); \
        gui_puts(cx+14,ty,name,COL_TEXT,COL_WIN_BG); \
        gui_puts(cx+200,ty,status,col,COL_WIN_BG); \
        ty+=FONT_H+4;
    DEV("CPU: i686",         "OK",     COL_GREEN)
    DEV("RAM: 5120 KB",      "OK",     COL_GREEN)
    DEV("ATA Disk",          "OK",     COL_GREEN)
    DEV("PS/2 Keyboard",     "OK",     COL_GREEN)
    DEV("PS/2 Mouse",        "OK",     COL_GREEN)
    DEV("VESA Framebuffer",  kstate.fb_active?"OK":"N/A", kstate.fb_active?COL_GREEN:COL_RED)
    DEV("PIT Timer 100Hz",   "OK",     COL_GREEN)
    DEV("PIC 8259",          "OK",     COL_GREEN)
    DEV("EclipseFS",         "OK",     COL_GREEN)
    DEV("VGA Text Mode",     "standby",COL_YELLOW)
    #undef DEV
    (void)ch;
}

// --- Disk Info ---
static void render_diskinfo(Window *w, int32_t mx, int32_t my, uint8_t click){
    (void)mx;(void)my;(void)click;
    int32_t cx=win_ca_x(w),cy=win_ca_y(w),cw=win_ca_w(w),ch=win_ca_h(w);
    gui_fill_rect(cx,cy,cw,ch,COL_WIN_BG);
    int32_t ty=cy+6;
    char nb[32];
    #define DR(lbl,val) gui_puts(cx+6,ty,lbl,COL_TEXT,COL_WIN_BG); \
        gui_puts(cx+160,ty,val,COL_BLUE,COL_WIN_BG); ty+=FONT_H+5;
    gui_puts(cx+6,ty,"Disk Information",RGB(0,0,128),COL_WIN_BG); ty+=FONT_H+8;
    gui_draw_hline(cx+4,ty,cw-8,COL_BORDER_DARK); ty+=5;
    DR("Controller:","ATA PIO Primary")
    kutoa(kstate.disk_sectors,nb,10); kstrcat(nb," sectors"); DR("Total:",nb)
    kutoa(kstate.disk_sectors/2,nb,10); kstrcat(nb," KB"); DR("Capacity:",nb)
    DR("FS Start LBA:","256")
    DR("Kernel LBA:","17")
    DR("InitRAMFS LBA:","227")
    DR("Format:","EclipseFS")
    DR("Status:","Mounted")
    #undef DR
}

// --- Memory Map ---
static void render_memmap(Window *w, int32_t mx, int32_t my, uint8_t click){
    (void)mx;(void)my;(void)click;
    int32_t cx=win_ca_x(w),cy=win_ca_y(w),cw=win_ca_w(w),ch=win_ca_h(w);
    gui_fill_rect(cx,cy,cw,ch,COL_WIN_BG);
    gui_fill_rect(cx,cy,cw,FONT_H+4,COL_WIN_TITLE_ACT);
    gui_puts(cx+4,cy+2,"Start      End        Use",RGB(255,255,255),COL_WIN_TITLE_ACT);
    int32_t ty=cy+FONT_H+7;
    static const char *mmap[]={
        "0x00000000 0x0009FFFF  Conventional RAM",
        "0x000A0000 0x000BFFFF  VGA Memory",
        "0x000C0000 0x000FFFFF  BIOS/ROM",
        "0x00100000 0x004FFFFF  Kernel+Heap",
        "0x00500000 0x007FFFFF  Free RAM",
        "0xFD000000 0xFE000000  VESA Framebuffer",
    };
    for(int i=0;i<6&&ty<cy+ch-4;i++){
        gui_puts(cx+4,ty,mmap[i],COL_TEXT,COL_WIN_BG);
        ty+=FONT_H+3;
    }
    (void)ch;
}

// --- CPU Monitor ---
static void render_cpumon(Window *w, int32_t mx, int32_t my, uint8_t click){
    (void)mx;(void)my;(void)click;
    int32_t cx=win_ca_x(w),cy=win_ca_y(w),cw=win_ca_w(w),ch=win_ca_h(w);
    gui_fill_rect(cx,cy,cw,ch,COL_WIN_BG);
    gui_puts(cx+4,cy+4,"CPU Activity",RGB(0,0,128),COL_WIN_BG);
    // Bar graph (pseudo-random bars based on ticks for visual effect)
    uint32_t t=get_ticks();
    int32_t bx=cx+4, by=cy+50;
    int32_t bar_w=20, bar_max=80;
    for(int i=0;i<10;i++){
        uint32_t h=(((t>>(i*3))^(t<<i))*6271)%bar_max;
        if(h<4)h=4;
        gui_fill_rect(bx,by+bar_max-h,bar_w,h,COL_GREEN);
        gui_fill_rect(bx,by,bar_w,bar_max-h,RGB(0,40,0));
        gui_draw_rect_border(bx,by,bar_w,bar_max,RGB(0,100,0),RGB(0,100,0));
        bx+=bar_w+3;
    }
    gui_puts(cx+4,by+bar_max+4,"CPU cores  (simulated)",RGB(80,80,80),COL_WIN_BG);
    (void)ch;
}

// --- Reg Editor ---
static void render_regedit(Window *w, int32_t mx, int32_t my, uint8_t click){
    (void)mx;(void)my;(void)click;
    int32_t cx=win_ca_x(w),cy=win_ca_y(w),cw=win_ca_w(w),ch=win_ca_h(w);
    gui_fill_rect(cx,cy,cw,ch,RGB(255,255,255));
    int32_t ty=cy+4;
    static const char *keys[]={
        "[HKEY_ECLIPSE]",
        "  Version = Eclipse32 v1.0",
        "  Build   = 2025-01-01",
        "  Arch    = x86-32",
        "",
        "[HKEY_ECLIPSE\\Hardware]",
        "  CPU     = i686",
        "  RAM     = 5120 KB",
        "  Screen  = 800x600",
        "",
        "[HKEY_ECLIPSE\\Software]",
        "  FS      = EclipseFS",
        "  GUI     = EclipseGUI",
        "  Shell   = EclipseShell",
    };
    for(int i=0;i<14&&ty<cy+ch-4;i++){
        uint32_t col= (keys[i][0]=='[') ? RGB(0,0,128) : COL_TEXT;
        gui_puts(cx+4,ty,keys[i],col,RGB(255,255,255));
        ty+=FONT_H+2;
    }
    (void)ch;
}

// --- File Manager ---
// Modes for the file manager panel
#define FM_MODE_LIST    0   // normal file listing
#define FM_MODE_NEW     1   // typing new filename
#define FM_MODE_RENAME  2   // typing rename target
#define FM_MODE_EDIT    3   // editing file content (simple text editor)
#define FM_MODE_CONFIRM 4   // confirm delete dialog

#define FM_MAX 32

// Returns 1 if filename ends with ".os32"
static int fm_is_executable(const char *name){
    int32_t n=(int32_t)kstrlen(name);
    if(n<6) return 0;
    return kstrcmp(name+n-5,".os32")==0;
}

// Double-click threshold: reuse DBLCLICK_MS defined near desktop icons
#define FM_DBLCLICK_MS DBLCLICK_MS

static void render_fileman(Window *w, int32_t mx, int32_t my, uint8_t click){
    int32_t cx=win_ca_x(w),cy=win_ca_y(w),cw=win_ca_w(w),ch=win_ca_h(w);
    AppState *st=&w->st;

    // ---- Mode: edit file content ----
    if(st->fm_mode==FM_MODE_EDIT){
        gui_fill_rect(cx,cy,cw,ch,RGB(255,255,255));
        // Title bar
        gui_fill_rect(cx,cy,cw,FONT_H+6,RGB(50,100,160));
        char etitle[80]; kstrcpy(etitle,"Editing: ");
        if(st->fm_sel>=0&&st->fm_sel<st->fm_count) kstrncat(etitle,st->fm_names[st->fm_sel],40);
        gui_puts(cx+4,cy+3,etitle,RGB(255,255,255),RGB(50,100,160));
        // Editable text area
        int32_t ta_y=cy+FONT_H+8;
        int32_t ta_h=ch-FONT_H-30;
        gui_fill_rect(cx,ta_y,cw,ta_h,RGB(255,255,255));
        gui_draw_rect_border(cx,ta_y,cw,ta_h,RGB(150,150,150),RGB(200,200,200));
        // Render text
        const char *p=st->text;
        int32_t tx=cx+3, ty2=ta_y+3-st->scroll_y;
        while(*p){
            if(*p=='\n'){tx=cx+3;ty2+=FONT_H+1;}
            else{
                if(ty2>=ta_y&&ty2<ta_y+ta_h&&tx<cx+cw)
                    gui_putc(tx,ty2,*p,COL_TEXT,RGB(255,255,255));
                tx+=FONT_W;
            }
            p++;
        }
        // Blinking cursor at end
        if((get_ticks()/50)%2==0) gui_fill_rect(tx,ty2,FONT_W,FONT_H,RGB(50,100,200));
        // Keyboard input
        char kc=kb_getchar_nowait();
        if(kc){
            if(kc=='\b'){if(st->text_len>0){st->text_len--;st->text[st->text_len]=0;}}
            else if(st->text_len<WIN_TEXT_BUF-2){st->text[st->text_len++]=kc;st->text[st->text_len]=0;}
        }
        // Bottom toolbar: Save / Cancel
        int32_t by2=cy+ch-24;
        gui_fill_rect(cx,by2,cw,24,RGB(230,230,230));
        gui_draw_hline(cx,by2,cw,RGB(160,160,160));
        if(gui_button(cx+4,by2+4,70,16,"Save",mx,my,click)){
            if(st->fm_sel>=0&&st->fm_sel<st->fm_count){
                fs_write_file(st->fm_names[st->fm_sel],st->text,(uint32_t)st->text_len);
            }
            st->fm_mode=FM_MODE_LIST; st->fm_count=-1;
        }
        if(gui_button(cx+80,by2+4,70,16,"Cancel",mx,my,click)){
            st->fm_mode=FM_MODE_LIST;
        }
        if(gui_button(cx+156,by2+4,100,16,"Save As New",mx,my,click)){
            st->fm_mode=FM_MODE_NEW; // repurpose new-file mode to save as
        }
        return;
    }

    // ---- Mode: confirm delete ----
    if(st->fm_mode==FM_MODE_CONFIRM){
        gui_fill_rect(cx,cy,cw,ch,COL_WIN_BG);
        gui_fill_rect(cx,cy,cw,FONT_H+6,RGB(180,40,40));
        gui_puts(cx+4,cy+3,"Confirm Delete",RGB(255,255,255),RGB(180,40,40));
        char msg[80]; kstrcpy(msg,"Delete: ");
        if(st->fm_sel>=0&&st->fm_sel<st->fm_count) kstrncat(msg,st->fm_names[st->fm_sel],40);
        kstrcat(msg," ?");
        gui_puts(cx+8,cy+30,msg,COL_TEXT,COL_WIN_BG);
        if(gui_button(cx+10,cy+55,80,20,"Yes, Delete",mx,my,click)){
            if(st->fm_sel>=0&&st->fm_sel<st->fm_count)
                fs_delete(st->fm_names[st->fm_sel]);
            st->fm_mode=FM_MODE_LIST; st->fm_count=-1; st->fm_sel=-1;
        }
        if(gui_button(cx+100,cy+55,60,20,"Cancel",mx,my,click)){
            st->fm_mode=FM_MODE_LIST;
        }
        return;
    }

    // ---- Mode: new file / rename: show input dialog ----
    if(st->fm_mode==FM_MODE_NEW||st->fm_mode==FM_MODE_RENAME){
        gui_fill_rect(cx,cy,cw,ch,COL_WIN_BG);
        const char *dlg_title=(st->fm_mode==FM_MODE_NEW)?"New File Name:":"Rename to:";
        gui_fill_rect(cx,cy,cw,FONT_H+6,RGB(50,100,160));
        gui_puts(cx+4,cy+3,dlg_title,RGB(255,255,255),RGB(50,100,160));
        // Input box
        gui_fill_rect(cx+4,cy+24,cw-8,18,RGB(255,255,255));
        gui_draw_3d_box(cx+4,cy+24,cw-8,18,0);
        gui_puts(cx+7,cy+28,st->fm_input,COL_TEXT,RGB(255,255,255));
        // Blinking cursor
        int32_t il=(int32_t)kstrlen(st->fm_input);
        if((get_ticks()/50)%2==0)
            gui_fill_rect(cx+7+il*FONT_W,cy+28,FONT_W,FONT_H,RGB(50,100,200));
        // Input handling
        char kc=kb_getchar_nowait();
        if(kc){
            if(kc=='\r'||kc=='\n'){
                if(st->fm_mode==FM_MODE_NEW){
                    // Create empty file
                    if(kstrlen(st->fm_input)>0)
                        fs_write_file(st->fm_input,"",0);
                } else {
                    // Rename selected file
                    if(st->fm_sel>=0&&st->fm_sel<st->fm_count&&kstrlen(st->fm_input)>0)
                        fs_rename(st->fm_names[st->fm_sel],st->fm_input);
                }
                st->fm_mode=FM_MODE_LIST; st->fm_count=-1; st->fm_sel=-1;
                kmemset(st->fm_input,0,sizeof(st->fm_input));
            } else if(kc==27){ // ESC
                st->fm_mode=FM_MODE_LIST;
                kmemset(st->fm_input,0,sizeof(st->fm_input));
            } else if(kc=='\b'){
                int32_t l=(int32_t)kstrlen(st->fm_input);
                if(l>0) st->fm_input[l-1]=0;
            } else if(kstrlen(st->fm_input)<(uint32_t)EFS_FILENAME_MAX-2){
                int32_t l=(int32_t)kstrlen(st->fm_input);
                st->fm_input[l]=kc; st->fm_input[l+1]=0;
            }
        }
        if(gui_button(cx+4,cy+50,60,16,"OK",mx,my,click)){
            if(st->fm_mode==FM_MODE_NEW){
                if(kstrlen(st->fm_input)>0) fs_write_file(st->fm_input,"",0);
            } else {
                if(st->fm_sel>=0&&st->fm_sel<st->fm_count&&kstrlen(st->fm_input)>0)
                    fs_rename(st->fm_names[st->fm_sel],st->fm_input);
            }
            st->fm_mode=FM_MODE_LIST; st->fm_count=-1; st->fm_sel=-1;
            kmemset(st->fm_input,0,sizeof(st->fm_input));
        }
        if(gui_button(cx+70,cy+50,60,16,"Cancel",mx,my,click)){
            st->fm_mode=FM_MODE_LIST;
            kmemset(st->fm_input,0,sizeof(st->fm_input));
        }
        return;
    }

    // ---- Normal list mode ----
    gui_fill_rect(cx,cy,cw,ch,RGB(245,245,250));

    // === Toolbar ===
    int32_t tb_h=22;
    gui_fill_rect(cx,cy,cw,tb_h,RGB(230,230,235));
    gui_draw_hline(cx,cy+tb_h-1,cw,RGB(180,180,190));
    // Toolbar buttons
    if(gui_button(cx+2,cy+3,40,16,"New",mx,my,click)){
        kmemset(st->fm_input,0,sizeof(st->fm_input));
        st->fm_mode=FM_MODE_NEW;
    }
    if(gui_button(cx+46,cy+3,50,16,"Open",mx,my,click)){
        if(st->fm_sel>=0&&st->fm_sel<st->fm_count){
            st->text_len=fs_read_file(st->fm_names[st->fm_sel],st->text,WIN_TEXT_BUF);
            if(st->text_len<0) st->text_len=0;
            st->text[st->text_len]=0;
            st->fm_mode=FM_MODE_EDIT;
        }
    }
    if(gui_button(cx+100,cy+3,52,16,"Rename",mx,my,click)){
        if(st->fm_sel>=0&&st->fm_sel<st->fm_count){
            kmemset(st->fm_input,0,sizeof(st->fm_input));
            kstrcpy(st->fm_input,st->fm_names[st->fm_sel]);
            st->fm_mode=FM_MODE_RENAME;
        }
    }
    if(gui_button(cx+156,cy+3,48,16,"Delete",mx,my,click)){
        if(st->fm_sel>=0&&st->fm_sel<st->fm_count)
            st->fm_mode=FM_MODE_CONFIRM;
    }
    // Refresh button right-aligned
    if(gui_button(cx+cw-52,cy+3,50,16,"Refresh",mx,my,click)){
        st->fm_count=-1; st->fm_sel=-1;
    }

    // === Column headers ===
    int32_t hdr_y=cy+tb_h;
    int32_t hdr_h=FONT_H+4;
    gui_fill_rect(cx,hdr_y,cw,hdr_h,RGB(210,210,220));
    gui_draw_hline(cx,hdr_y+hdr_h-1,cw,RGB(160,160,170));
    gui_puts(cx+16,hdr_y+2,"Name",RGB(40,40,80),RGB(210,210,220));
    gui_puts(cx+cw-52,hdr_y+2,"Size",RGB(40,40,80),RGB(210,210,220));

    // Load directory if needed
    if(st->fm_count<0){
        st->fm_count=fs_list(st->fm_names,FM_MAX);
        for(int32_t i=0;i<st->fm_count;i++)
            st->fm_sizes[i]=fs_size(st->fm_names[i]);
        st->fm_sel=-1;
    }

    int32_t list_y=hdr_y+hdr_h;
    int32_t list_h=ch-tb_h-hdr_h-FONT_H-6;
    int32_t ty=list_y+2-st->scroll_y;
    int32_t row_h=FONT_H+5;
    char sb[16];

    for(int32_t i=0;i<st->fm_count;i++,ty+=row_h){
        if(ty+row_h<list_y) continue;
        if(ty>list_y+list_h) break;

        uint8_t is_sel=(st->fm_sel==i);
        int is_exec=fm_is_executable(st->fm_names[i]);

        uint32_t rbg = is_sel ? RGB(50,120,200) :
                       (i%2)  ? RGB(248,248,252) : RGB(255,255,255);
        uint32_t tfg = is_sel ? RGB(255,255,255) : COL_TEXT;
        uint32_t sfg = is_sel ? RGB(200,230,255) : RGB(100,100,120);

        gui_fill_rect(cx, ty, cw, row_h-1, rbg);

        // File type icon char: green > for .os32, folder @ for dirs, doc char otherwise
        uint32_t ifg = is_sel ? RGB(255,255,180) :
                       is_exec ? RGB(0,160,40) : RGB(60,120,200);
        gui_putc(cx+3, ty+1, is_exec ? '>' : '*', ifg, rbg);
        gui_puts(cx+14, ty+1, st->fm_names[i], tfg, rbg);

        // File size right-aligned
        kutoa(st->fm_sizes[i],sb,10); kstrcat(sb," B");
        int32_t sw=(int32_t)kstrlen(sb)*FONT_W;
        gui_puts(cx+cw-sw-4, ty+1, sb, sfg, rbg);

        // Hit test for click/double-click
        if(click && mx>=cx && mx<cx+cw && my>=ty && my<ty+row_h){
            uint32_t now=get_ticks();
            if(st->fm_sel==i && (now - st->fm_last_click) < FM_DBLCLICK_MS){
                // Double-click: open/run
                if(is_exec){
                    kernel_exec_os32(st->fm_names[i]);
                    st->fm_count=-1;
                } else {
                    // Open in text editor
                    st->text_len=fs_read_file(st->fm_names[i],st->text,WIN_TEXT_BUF);
                    if(st->text_len<0) st->text_len=0;
                    st->text[st->text_len]=0;
                    st->fm_mode=FM_MODE_EDIT;
                }
                st->fm_sel=-1; st->fm_last_click=0;
            } else {
                st->fm_sel=i; st->fm_last_click=now;
            }
        }
    }

    if(st->fm_count==0)
        gui_puts(cx+8,list_y+8,"(empty — no files on disk)",RGB(128,128,140),RGB(245,245,250));

    // === Status bar ===
    int32_t sb_y=cy+ch-FONT_H-5;
    gui_fill_rect(cx, sb_y, cw, FONT_H+5, RGB(230,230,235));
    gui_draw_hline(cx, sb_y, cw, RGB(180,180,190));
    char sbuf[80]; kitoa(st->fm_count,sbuf,10); kstrcat(sbuf," files");
    if(st->fm_sel>=0&&st->fm_sel<st->fm_count){
        kstrcat(sbuf,"   Selected: "); kstrncat(sbuf,st->fm_names[st->fm_sel],30);
    }
    gui_puts(cx+4, sb_y+2, sbuf, RGB(60,60,80), RGB(230,230,235));
    (void)list_h;
}

// --- Hex Viewer ---
static void render_hexview(Window *w, int32_t mx, int32_t my, uint8_t click){
    int32_t cx=win_ca_x(w),cy=win_ca_y(w),cw=win_ca_w(w),ch=win_ca_h(w);
    gui_fill_rect(cx,cy,cw,ch,RGB(0,0,0));
    uint32_t addr=w->st.hex_addr;
    int32_t ty=cy+2;
    char hb[3]; hb[2]=0;
    static const char hex[]="0123456789ABCDEF";
    int32_t rows=(ch-4)/(FONT_H+1);
    for(int r=0;r<rows&&ty<cy+ch-FONT_H;r++){
        char lbuf[10];
        kutoa(addr,lbuf,16);
        // pad to 8
        int32_t ll=(int32_t)kstrlen(lbuf);
        char lb2[12]; lb2[0]='0';lb2[1]='x';
        for(int q=0;q<6-ll;q++) lb2[2+q]='0';
        kstrcpy(lb2+2+(6-ll<0?0:6-ll),lbuf);
        lb2[8]=':'; lb2[9]=0;
        gui_puts(cx+2,ty,lb2,RGB(0,160,0),RGB(0,0,0));
        int32_t hx=cx+70;
        for(int b=0;b<8&&hx<cx+cw-8;b++){
            uint8_t v=*(uint8_t*)(addr+b);
            hb[0]=hex[v>>4]; hb[1]=hex[v&0xF];
            gui_puts(hx,ty,hb,RGB(0,220,180),RGB(0,0,0));
            hx+=22;
        }
        addr+=8; ty+=FONT_H+1;
    }
    if(gui_button(cx+4,cy+ch-20,60,16,"-4KB",mx,my,click)) w->st.hex_addr-=4096;
    if(gui_button(cx+68,cy+ch-20,60,16,"+4KB",mx,my,click)) w->st.hex_addr+=4096;
}

// --- Network Info ---
static void render_netinfo(Window *w, int32_t mx, int32_t my, uint8_t click){
    (void)mx;(void)my;(void)click;
    int32_t cx=win_ca_x(w),cy=win_ca_y(w),cw=win_ca_w(w),ch=win_ca_h(w);
    gui_fill_rect(cx,cy,cw,ch,COL_WIN_BG);
    int32_t ty=cy+6;
    #define NR(l,v) gui_puts(cx+6,ty,l,COL_TEXT,COL_WIN_BG); \
        gui_puts(cx+140,ty,v,COL_BLUE,COL_WIN_BG); ty+=FONT_H+5;
    gui_puts(cx+6,ty,"Network Information",RGB(0,0,128),COL_WIN_BG); ty+=FONT_H+8;
    gui_draw_hline(cx+4,ty,cw-8,COL_BORDER_DARK); ty+=5;
    NR("Interface:","N/A (no NIC)") NR("IP Address:","0.0.0.0")
    NR("Subnet:","255.0.0.0") NR("Gateway:","0.0.0.0")
    NR("DNS:","0.0.0.0") NR("MAC:","00:00:00:00:00:00")
    NR("Status:","No network hardware")
    #undef NR
    (void)ch;
}

// --- IP Config ---
static void render_ipconfig(Window *w, int32_t mx, int32_t my, uint8_t click){
    (void)mx;(void)my;(void)click;
    int32_t cx=win_ca_x(w),cy=win_ca_y(w),cw=win_ca_w(w),ch=win_ca_h(w);
    gui_fill_rect(cx,cy,cw,ch,COL_WIN_BG);
    int32_t ty=cy+6;
    gui_puts(cx+6,ty,"EclipseNET v1.0",RGB(0,0,128),COL_WIN_BG); ty+=FONT_H+10;
    gui_puts(cx+6,ty,"lo0  127.0.0.1  UP LOOPBACK",COL_TEXT,COL_WIN_BG); ty+=FONT_H+4;
    gui_puts(cx+6,ty,"eth0 (not present)",RGB(128,128,128),COL_WIN_BG); ty+=FONT_H+4;
    (void)ch;
}

// --- Paint (simple pixel canvas) ---
static const uint32_t paint_palette[]={
    RGB(0,0,0),RGB(255,255,255),RGB(200,0,0),RGB(0,180,0),
    RGB(0,0,200),RGB(200,200,0),RGB(200,0,200),RGB(0,200,200)
};
#define PAINT_COLS 8
#define PAINT_CELL 14
#define PAINT_CANVAS_X 4
#define PAINT_CANVAS_Y 20
#define PAINT_STRIDE   100   // canvas width = height = 100 pixels
// Canvas is stored in window's text buffer as raw pixel data (1 byte per pixel = color index)
// Paint a single pixel (2x2 brush) on the canvas buffer
static inline void paint_dot(AppState *st, int32_t px, int32_t py) {
    if(px<0||px>=100||py<0||py>=100) return;
    st->text[py*PAINT_STRIDE+px]=(char)st->paint_col_idx;
    if(px+1<100) st->text[py*PAINT_STRIDE+px+1]=(char)st->paint_col_idx;
    if(py+1<100) st->text[(py+1)*PAINT_STRIDE+px]=(char)st->paint_col_idx;
    if(px+1<100&&py+1<100) st->text[(py+1)*PAINT_STRIDE+px+1]=(char)st->paint_col_idx;
}

// Bresenham line from (x0,y0) to (x1,y1)
static void paint_line(AppState *st, int32_t x0, int32_t y0, int32_t x1, int32_t y1){
    int32_t dx=x1-x0; if(dx<0)dx=-dx;
    int32_t dy=y1-y0; if(dy<0)dy=-dy;
    int32_t sx=(x0<x1)?1:-1, sy=(y0<y1)?1:-1;
    int32_t err=dx-dy;
    for(;;){
        paint_dot(st,x0,y0);
        if(x0==x1&&y0==y1) break;
        int32_t e2=err*2;
        if(e2>-dy){ err-=dy; x0+=sx; }
        if(e2< dx){ err+=dx; y0+=sy; }
    }
}

static void render_paint(Window *w, int32_t mx, int32_t my, uint8_t click){
    int32_t cx=win_ca_x(w),cy=win_ca_y(w),cw=win_ca_w(w),ch=win_ca_h(w);
    int32_t cax=cx+PAINT_CANVAS_X, cay=cy+PAINT_CANVAS_Y;
    // init canvas to white (index 1)
    if(w->st.text_len==0){
        kmemset(w->st.text,1,PAINT_STRIDE*PAINT_STRIDE);
        w->st.text_len=PAINT_STRIDE*PAINT_STRIDE;
    }
    // Draw canvas
    int32_t pw=100, ph=100;
    // clamp display to available client area
    int32_t disp_w = cw-8; if(disp_w>pw) disp_w=pw;
    int32_t disp_h = ch-PAINT_CANVAS_Y-4; if(disp_h>ph) disp_h=ph;
    for(int32_t row=0;row<disp_h;row++){
        for(int32_t col=0;col<disp_w;col++){
            uint8_t idx=(uint8_t)w->st.text[row*PAINT_STRIDE+col];
            if(idx>=PAINT_COLS)idx=1;
            bb_pix(cax+col,cay+row,paint_palette[idx]);
        }
    }
    gui_draw_rect_border(cax-1,cay-1,disp_w+2,disp_h+2,COL_BORDER_DARK,COL_BORDER_LIGHT);
    // Palette row at top
    for(int i=0;i<PAINT_COLS;i++){
        int32_t px=cx+4+i*PAINT_CELL;
        gui_fill_rect(px,cy+2,PAINT_CELL-2,PAINT_CELL-2,paint_palette[i]);
        gui_draw_rect_border(px,cy+2,PAINT_CELL-2,PAINT_CELL-2,
            (i==(int)w->st.paint_col_idx)?COL_BORDER_DARK:RGB(128,128,128),
            (i==(int)w->st.paint_col_idx)?COL_BORDER_LIGHT:RGB(128,128,128));
    }
    // Select palette color on click in palette row
    if(click&&my>=cy+2&&my<cy+2+PAINT_CELL){
        for(int i=0;i<PAINT_COLS;i++){
            int32_t px=cx+4+i*PAINT_CELL;
            if(mx>=px&&mx<px+PAINT_CELL) w->st.paint_col_idx=(uint8_t)i;
        }
    }
    // Paint on canvas — Bresenham from last position for smooth strokes
    uint8_t is_down = (g_mouse.buttons&1) &&
                      mx>=cax && mx<cax+disp_w &&
                      my>=cay && my<cay+disp_h;
    if(is_down){
        int32_t px=mx-cax, py=my-cay;
        if(w->st.paint_was_down){
            // interpolate line from previous position
            paint_line(&w->st, (int32_t)w->st.paint_prev_x, (int32_t)w->st.paint_prev_y, px, py);
        } else {
            paint_dot(&w->st, px, py);
        }
        w->st.paint_prev_x=(int16_t)px;
        w->st.paint_prev_y=(int16_t)py;
    }
    w->st.paint_was_down=is_down;
}

// --- Color Picker ---
static void render_colorpicker(Window *w, int32_t mx, int32_t my, uint8_t click){
    (void)click;
    int32_t cx=win_ca_x(w),cy=win_ca_y(w),cw=win_ca_w(w),ch=win_ca_h(w);
    gui_fill_rect(cx,cy,cw,ch,COL_WIN_BG);
    // 8x8 swatch grid
    int32_t sw=18, rows=8, cols=12;
    for(int r=0;r<rows;r++) for(int c=0;c<cols;c++){
        uint32_t col=RGB((r*32)&0xFF,(c*20)&0xFF,((r+c)*15)&0xFF);
        int32_t px=cx+4+c*sw, py=cy+4+r*sw;
        gui_fill_rect(px,py,sw-2,sw-2,col);
        if(mx>=px&&mx<px+sw&&my>=py&&my<py+sw){
            gui_draw_rect_border(px,py,sw-2,sw-2,COL_WHITE,COL_BLACK);
            gui_fill_rect(cx+4,cy+ch-20,sw*3,18,col);
            char cb[20];
            ksprintf(cb,"R=%d G=%d B=%d",(int)((col>>16)&0xFF),(int)((col>>8)&0xFF),(int)(col&0xFF));
            gui_puts(cx+4+sw*3+4,cy+ch-16,cb,COL_TEXT,COL_WIN_BG);
        }
    }
    (void)ch;
}

// --- Screen Saver ---
static void render_screensaver(Window *w, int32_t mx, int32_t my, uint8_t click){
    (void)mx;(void)my;(void)click;
    int32_t cx=win_ca_x(w),cy=win_ca_y(w),cw=win_ca_w(w),ch=win_ca_h(w);
    gui_fill_rect(cx,cy,cw,ch,RGB(0,0,0));
    // Bounce logo
    int32_t lw=8*FONT_W, lh=FONT_H;
    w->st.ss_x+=w->st.ss_dx; w->st.ss_y+=w->st.ss_dy;
    if(w->st.ss_x<0){w->st.ss_x=0;w->st.ss_dx=-w->st.ss_dx;w->st.ss_col=RGB(0,200,255);}
    if(w->st.ss_y<0){w->st.ss_y=0;w->st.ss_dy=-w->st.ss_dy;w->st.ss_col=RGB(255,200,0);}
    if(w->st.ss_x+lw>cw){w->st.ss_x=cw-lw;w->st.ss_dx=-w->st.ss_dx;w->st.ss_col=RGB(200,0,255);}
    if(w->st.ss_y+lh>ch){w->st.ss_y=ch-lh;w->st.ss_dy=-w->st.ss_dy;w->st.ss_col=RGB(0,255,100);}
    gui_puts(cx+w->st.ss_x,cy+w->st.ss_y,"Eclipse32",w->st.ss_col,RGB(0,0,0));
}

// ============================================================
// Games
// ============================================================

// --- Snake ---
#define SN_COLS 18
#define SN_ROWS 18
#define SN_CS   10  // cell size px

static void snake_init(Window *w){
    kmemset(w->st.sn_x,0,64); kmemset(w->st.sn_y,0,64);
    w->st.sn_x[0]=9;w->st.sn_y[0]=9;
    w->st.sn_x[1]=8;w->st.sn_y[1]=9;
    w->st.sn_x[2]=7;w->st.sn_y[2]=9;
    w->st.sn_len=3;w->st.sn_dx=1;w->st.sn_dy=0;w->st.sn_dead=0;
    w->st.sn_food_x=14;w->st.sn_food_y=5;
}

static void render_snake(Window *w, int32_t mx, int32_t my, uint8_t click, uint8_t active){
    (void)mx;(void)my;
    int32_t cx=win_ca_x(w),cy=win_ca_y(w),cw=win_ca_w(w),ch=win_ca_h(w);
    int32_t ox=cx+(cw-SN_COLS*SN_CS)/2;
    int32_t oy=cy+20;
    if(w->st.sn_len==0) snake_init(w);
    // Input: only consume keys when this window is focused
    if(active){
        char kc=kb_getchar_nowait();
        if(kc=='w'&&w->st.sn_dy==0){w->st.sn_dx=0;w->st.sn_dy=-1;}
        if(kc=='s'&&w->st.sn_dy==0){w->st.sn_dx=0;w->st.sn_dy=1;}
        if(kc=='a'&&w->st.sn_dx==0){w->st.sn_dx=-1;w->st.sn_dy=0;}
        if(kc=='d'&&w->st.sn_dx==0){w->st.sn_dx=1;w->st.sn_dy=0;}
    }
    if(click&&w->st.sn_dead) snake_init(w);
    // Advance snake every 10 ticks
    uint32_t now=get_ticks();
    if(!w->st.sn_dead && now-w->st.sn_tick>=10){
        w->st.sn_tick=now;
        int8_t nx=w->st.sn_x[0]+w->st.sn_dx;
        int8_t ny=w->st.sn_y[0]+w->st.sn_dy;
        if(nx<0||nx>=SN_COLS||ny<0||ny>=SN_ROWS) {w->st.sn_dead=1; goto sn_draw;}
        for(int i=0;i<w->st.sn_len;i++)
            if(w->st.sn_x[i]==nx&&w->st.sn_y[i]==ny){w->st.sn_dead=1;goto sn_draw;}
        // Move tail
        for(int i=w->st.sn_len-1;i>0;i--){w->st.sn_x[i]=w->st.sn_x[i-1];w->st.sn_y[i]=w->st.sn_y[i-1];}
        w->st.sn_x[0]=nx;w->st.sn_y[0]=ny;
        if(nx==w->st.sn_food_x&&ny==w->st.sn_food_y){
            if(w->st.sn_len<62) w->st.sn_len++;
            w->st.sn_food_x=(uint8_t)((now*7+13)%SN_COLS);
            w->st.sn_food_y=(uint8_t)((now*11+5)%SN_ROWS);
        }
    }
    sn_draw:;
    gui_fill_rect(cx,cy,cw,ch,RGB(0,40,0));
    gui_fill_rect(ox,oy,SN_COLS*SN_CS,SN_ROWS*SN_CS,RGB(0,60,0));
    gui_draw_rect_border(ox,oy,SN_COLS*SN_CS,SN_ROWS*SN_CS,RGB(0,180,0),RGB(0,180,0));
    // Food
    gui_fill_rect(ox+w->st.sn_food_x*SN_CS+1,oy+w->st.sn_food_y*SN_CS+1,SN_CS-2,SN_CS-2,RGB(255,50,50));
    // Snake
    for(int i=0;i<w->st.sn_len;i++){
        uint32_t sc=(i==0)?RGB(0,220,0):RGB(0,160,0);
        gui_fill_rect(ox+w->st.sn_x[i]*SN_CS+1,oy+w->st.sn_y[i]*SN_CS+1,SN_CS-2,SN_CS-2,sc);
    }
    if(w->st.sn_dead){
        gui_puts(cx+4,cy+4,"GAME OVER - click to restart",RGB(255,50,50),RGB(0,40,0));
    } else {
        char lb[20]; kstrcpy(lb,"Len:"); kitoa(w->st.sn_len,lb+4,10);
        gui_puts(cx+4,cy+4,lb,RGB(0,220,0),RGB(0,40,0));
        gui_puts(cx+4,cy+ch-10,"WASD to move",RGB(0,150,0),RGB(0,40,0));
    }
    (void)ch;
}

// --- Pong ---
#define PONG_PAD_H 30
#define PONG_PAD_W  6
#define PONG_BALL   5

static void render_pong(Window *w, int32_t mx, int32_t my, uint8_t click){
    (void)click;
    int32_t cx=win_ca_x(w),cy=win_ca_y(w),cw=win_ca_w(w),ch=win_ca_h(w);
    gui_fill_rect(cx,cy,cw,ch,RGB(0,0,0));
    // AI for p2
    uint32_t now=get_ticks();
    if(now-w->st.pong_tick>=3){
        w->st.pong_tick=now;
        // p1 follows mouse y clamped
        int32_t rmy=my-cy;
        w->st.pong_p1y=rmy-PONG_PAD_H/2;
        if(w->st.pong_p1y<0)w->st.pong_p1y=0;
        if(w->st.pong_p1y+PONG_PAD_H>ch)w->st.pong_p1y=ch-PONG_PAD_H;
        // ai
        if(w->st.pong_by>w->st.pong_p2y+PONG_PAD_H/2) w->st.pong_p2y+=2;
        else if(w->st.pong_by<w->st.pong_p2y+PONG_PAD_H/2) w->st.pong_p2y-=2;
        if(w->st.pong_p2y<0)w->st.pong_p2y=0;
        if(w->st.pong_p2y+PONG_PAD_H>ch)w->st.pong_p2y=ch-PONG_PAD_H;
        // move ball
        w->st.pong_bx+=w->st.pong_bdx; w->st.pong_by+=w->st.pong_bdy;
        if(w->st.pong_by<=0){w->st.pong_by=0;w->st.pong_bdy=-w->st.pong_bdy;}
        if(w->st.pong_by+PONG_BALL>=ch){w->st.pong_by=ch-PONG_BALL;w->st.pong_bdy=-w->st.pong_bdy;}
        // p1 paddle
        if(w->st.pong_bx<=PONG_PAD_W+4 &&
           w->st.pong_by+PONG_BALL>=w->st.pong_p1y &&
           w->st.pong_by<=w->st.pong_p1y+PONG_PAD_H){
            w->st.pong_bx=PONG_PAD_W+4;w->st.pong_bdx=-w->st.pong_bdx;
        }
        // p2 paddle
        if(w->st.pong_bx+PONG_BALL>=cw-PONG_PAD_W-4 &&
           w->st.pong_by+PONG_BALL>=w->st.pong_p2y &&
           w->st.pong_by<=w->st.pong_p2y+PONG_PAD_H){
            w->st.pong_bx=cw-PONG_PAD_W-4-PONG_BALL;w->st.pong_bdx=-w->st.pong_bdx;
        }
        // score
        if(w->st.pong_bx<0){w->st.pong_s2++;w->st.pong_bx=cw/2;w->st.pong_by=ch/2;w->st.pong_bdx=2;}
        if(w->st.pong_bx>cw){w->st.pong_s1++;w->st.pong_bx=cw/2;w->st.pong_by=ch/2;w->st.pong_bdx=-2;}
    }
    // Center line
    for(int y2=0;y2<ch;y2+=8) bb_pix(cx+cw/2,cy+y2,RGB(64,64,64));
    // Paddles
    gui_fill_rect(cx+4,cy+w->st.pong_p1y,PONG_PAD_W,PONG_PAD_H,RGB(255,255,255));
    gui_fill_rect(cx+cw-4-PONG_PAD_W,cy+w->st.pong_p2y,PONG_PAD_W,PONG_PAD_H,RGB(255,255,255));
    // Ball
    gui_fill_rect(cx+w->st.pong_bx,cy+w->st.pong_by,PONG_BALL,PONG_BALL,RGB(255,255,255));
    // Scores
    char sb[8]; kitoa(w->st.pong_s1,sb,10);
    gui_puts(cx+cw/2-30,cy+4,sb,RGB(200,200,200),RGB(0,0,0));
    kitoa(w->st.pong_s2,sb,10);
    gui_puts(cx+cw/2+20,cy+4,sb,RGB(200,200,200),RGB(0,0,0));
    gui_puts(cx+4,cy+ch-10,"You=L  AI=R  Mouse=paddle",RGB(64,64,64),RGB(0,0,0));
    (void)mx;
}

// --- Minesweeper ---
#define MS_COLS 9
#define MS_ROWS 9
#define MS_MINES 10
#define MS_CS    20

static void ms_init(Window *w){
    kmemset(w->st.ms_board,0,81);
    w->st.ms_dead=0;w->st.ms_won=0;
    // place 10 mines pseudo-randomly
    uint32_t seed=get_ticks()+1;
    int placed=0;
    while(placed<MS_MINES){
        seed=seed*1664525+1013904223;
        int idx=(int)(seed%(MS_ROWS*MS_COLS));
        if(!(w->st.ms_board[idx]&1)){w->st.ms_board[idx]|=1;placed++;}
    }
    w->st.ms_init=1;
}

static int ms_count(Window *w, int r, int c){
    int n=0;
    for(int dr=-1;dr<=1;dr++) for(int dc=-1;dc<=1;dc++){
        int nr=r+dr,nc=c+dc;
        if(nr>=0&&nr<MS_ROWS&&nc>=0&&nc<MS_COLS&&(w->st.ms_board[nr*MS_COLS+nc]&1)) n++;
    }
    return n;
}

static void render_minesweeper(Window *w, int32_t mx, int32_t my, uint8_t click){
    int32_t cx=win_ca_x(w),cy=win_ca_y(w),cw=win_ca_w(w),ch=win_ca_h(w);
    if(!w->st.ms_init) ms_init(w);
    int32_t ox=cx+(cw-MS_COLS*MS_CS)/2, oy=cy+24;
    gui_fill_rect(cx,cy,cw,ch,COL_WIN_BG);
    // Status bar
    char sb[20]; kstrcpy(sb,"Mines: "); kitoa(MS_MINES,sb+7,10);
    gui_puts(cx+4,cy+6,sb,COL_TEXT,COL_WIN_BG);
    if(w->st.ms_dead) gui_puts(cx+cw/2-20,cy+6,"BOOM!",COL_RED,COL_WIN_BG);
    if(w->st.ms_won)  gui_puts(cx+cw/2-20,cy+6,"WIN! ",COL_GREEN,COL_WIN_BG);
    // Click
    if(click && !w->st.ms_dead && !w->st.ms_won){
        int32_t rc=(mx-ox)/MS_CS, rr=(my-oy)/MS_CS;
        if(rc>=0&&rc<MS_COLS&&rr>=0&&rr<MS_ROWS){
            int idx=rr*MS_COLS+rc;
            if(!(w->st.ms_board[idx]&2)){
                w->st.ms_board[idx]|=2;
                if(w->st.ms_board[idx]&1) w->st.ms_dead=1;
                else {
                    int revealed=0;
                    for(int i=0;i<MS_ROWS*MS_COLS;i++) if((w->st.ms_board[i]&2)&&!(w->st.ms_board[i]&1)) revealed++;
                    if(revealed==MS_ROWS*MS_COLS-MS_MINES) w->st.ms_won=1;
                }
            }
        }
    }
    if(click&&(w->st.ms_dead||w->st.ms_won)) ms_init(w);
    // Draw grid
    for(int r=0;r<MS_ROWS;r++) for(int c=0;c<MS_COLS;c++){
        int32_t px=ox+c*MS_CS, py=oy+r*MS_CS;
        uint8_t cell=w->st.ms_board[r*MS_COLS+c];
        uint8_t revealed=(cell&2), mine=(cell&1);
        if(revealed){
            gui_fill_rect(px,py,MS_CS,MS_CS,RGB(200,200,200));
            gui_draw_3d_box(px,py,MS_CS,MS_CS,0);
            if(mine){
                gui_fill_rect(px+4,py+4,MS_CS-8,MS_CS-8,RGB(255,0,0));
            } else {
                int n=ms_count(w,r,c);
                if(n>0){
                    char nc[2]; nc[0]='0'+n; nc[1]=0;
                    static const uint32_t nc_cols[]={COL_BLUE,COL_GREEN,COL_RED,
                        RGB(0,0,128),RGB(128,0,0),RGB(0,128,128),COL_BLACK,RGB(128,128,128)};
                    gui_puts(px+6,py+6,nc,nc_cols[n-1],RGB(200,200,200));
                }
            }
        } else {
            gui_fill_rect(px,py,MS_CS,MS_CS,COL_WIN_BG);
            gui_draw_3d_box(px,py,MS_CS,MS_CS,1);
        }
    }
    gui_puts(cx+4,cy+ch-12,"Click=reveal, click anywhere when dead to restart",
             RGB(80,80,80),COL_WIN_BG);
    (void)ch;
}

// --- Breakout ---
#define BRK_ROWS 5
#define BRK_COLS 10
#define BRK_BW   22
#define BRK_BH   8
#define BRK_PAD_W 40
#define BRK_PAD_H  6
#define BRK_BALL_S  6

static void brk_init(Window *w){
    int32_t cx=win_ca_x(w), cw=win_ca_w(w), ch=win_ca_h(w);
    w->st.brk_px=(cw-BRK_PAD_W)/2;
    w->st.brk_bx=cw/2; w->st.brk_by=ch-50;
    w->st.brk_bdx=2; w->st.brk_bdy=-2;
    for(int i=0;i<BRK_ROWS*BRK_COLS;i++) w->st.brk_bricks[i]=1;
    w->st.brk_init=1; w->st.brk_dead=0; w->st.brk_score=0;
    (void)cx;
}

static void render_breakout(Window *w, int32_t mx, int32_t my, uint8_t click){
    (void)my;
    int32_t cx=win_ca_x(w),cy=win_ca_y(w),cw=win_ca_w(w),ch=win_ca_h(w);
    if(!w->st.brk_init) brk_init(w);
    gui_fill_rect(cx,cy,cw,ch,RGB(0,0,0));
    int32_t ox=cx+2, oy=cy+24;
    // Paddle follows mouse x
    w->st.brk_px=mx-cx-BRK_PAD_W/2;
    if(w->st.brk_px<0)w->st.brk_px=0;
    if(w->st.brk_px+BRK_PAD_W>cw-4)w->st.brk_px=cw-4-BRK_PAD_W;
    uint32_t now=get_ticks();
    if(!w->st.brk_dead&&now-w->st.brk_tick>=2){
        w->st.brk_tick=now;
        w->st.brk_bx+=w->st.brk_bdx; w->st.brk_by+=w->st.brk_bdy;
        // walls
        if(w->st.brk_bx<=0){w->st.brk_bx=0;w->st.brk_bdx=-w->st.brk_bdx;}
        if(w->st.brk_bx+BRK_BALL_S>=cw-4){w->st.brk_bx=cw-4-BRK_BALL_S;w->st.brk_bdx=-w->st.brk_bdx;}
        if(w->st.brk_by<=0){w->st.brk_by=0;w->st.brk_bdy=-w->st.brk_bdy;}
        // paddle
        int32_t pad_y=ch-20;
        if(w->st.brk_by+BRK_BALL_S>=pad_y&&w->st.brk_by<pad_y+BRK_PAD_H&&
           w->st.brk_bx+BRK_BALL_S>=w->st.brk_px&&w->st.brk_bx<=w->st.brk_px+BRK_PAD_W){
            w->st.brk_by=pad_y-BRK_BALL_S;w->st.brk_bdy=-w->st.brk_bdy;
        }
        // bottom
        if(w->st.brk_by+BRK_BALL_S>=ch) w->st.brk_dead=1;
        // bricks
        int32_t bsy=12;
        for(int r=0;r<BRK_ROWS;r++) for(int c=0;c<BRK_COLS;c++){
            if(!w->st.brk_bricks[r*BRK_COLS+c]) continue;
            int32_t bx=(cw/2-BRK_COLS*(BRK_BW+2)/2)+c*(BRK_BW+2);
            int32_t by2=bsy+r*(BRK_BH+2);
            if(w->st.brk_bx+BRK_BALL_S>=bx&&w->st.brk_bx<=bx+BRK_BW&&
               w->st.brk_by+BRK_BALL_S>=by2&&w->st.brk_by<=by2+BRK_BH){
                w->st.brk_bricks[r*BRK_COLS+c]=0;
                w->st.brk_bdy=-w->st.brk_bdy;
                w->st.brk_score+=10;
            }
        }
    }
    if(click&&w->st.brk_dead) brk_init(w);
    // Draw bricks
    static const uint32_t brc[]={RGB(200,0,0),RGB(200,120,0),RGB(200,200,0),RGB(0,180,0),RGB(0,0,200)};
    int32_t bsy=cy+12;
    for(int r=0;r<BRK_ROWS;r++) for(int c=0;c<BRK_COLS;c++){
        if(!w->st.brk_bricks[r*BRK_COLS+c]) continue;
        int32_t bx=ox+(cw/2-BRK_COLS*(BRK_BW+2)/2)+c*(BRK_BW+2);
        int32_t by2=bsy+r*(BRK_BH+2);
        gui_fill_rect(bx,by2,BRK_BW,BRK_BH,brc[r]);
        gui_draw_3d_box(bx,by2,BRK_BW,BRK_BH,1);
    }
    // Paddle
    gui_fill_rect(cx+w->st.brk_px,cy+ch-20,BRK_PAD_W,BRK_PAD_H,RGB(200,200,200));
    gui_draw_3d_box(cx+w->st.brk_px,cy+ch-20,BRK_PAD_W,BRK_PAD_H,1);
    // Ball
    gui_fill_rect(cx+w->st.brk_bx,cy+w->st.brk_by,BRK_BALL_S,BRK_BALL_S,RGB(255,255,255));
    // Score
    char sc[20]; kstrcpy(sc,"Score: "); kitoa(w->st.brk_score,sc+7,10);
    gui_puts(cx+4,cy+4,sc,RGB(200,200,200),RGB(0,0,0));
    if(w->st.brk_dead) gui_puts(cx+cw/2-40,cy+ch/2,"GAME OVER - click",RGB(255,0,0),RGB(0,0,0));
    (void)oy;(void)ch;
}

// --- Tetris ---
// Piece definitions: each piece has 4 rotations, each rotation is 4 (row,col) pairs
// relative to a 4x4 bounding box origin
#define TET_CELL 12
#define TET_ROWS 20
#define TET_COLS 10

static const int8_t tet_shapes[7][4][4][2] = {
    // I
    {{{0,0},{0,1},{0,2},{0,3}}, {{0,2},{1,2},{2,2},{3,2}}, {{2,0},{2,1},{2,2},{2,3}}, {{0,1},{1,1},{2,1},{3,1}}},
    // O
    {{{0,1},{0,2},{1,1},{1,2}}, {{0,1},{0,2},{1,1},{1,2}}, {{0,1},{0,2},{1,1},{1,2}}, {{0,1},{0,2},{1,1},{1,2}}},
    // T
    {{{0,1},{1,0},{1,1},{1,2}}, {{0,1},{1,1},{1,2},{2,1}}, {{1,0},{1,1},{1,2},{2,1}}, {{0,1},{1,0},{1,1},{2,1}}},
    // S
    {{{0,1},{0,2},{1,0},{1,1}}, {{0,1},{1,1},{1,2},{2,2}}, {{1,1},{1,2},{2,0},{2,1}}, {{0,0},{1,0},{1,1},{2,1}}},
    // Z
    {{{0,0},{0,1},{1,1},{1,2}}, {{0,2},{1,1},{1,2},{2,1}}, {{1,0},{1,1},{2,1},{2,2}}, {{0,1},{1,0},{1,1},{2,0}}},
    // J
    {{{0,0},{1,0},{1,1},{1,2}}, {{0,1},{0,2},{1,1},{2,1}}, {{1,0},{1,1},{1,2},{2,2}}, {{0,1},{1,1},{2,0},{2,1}}},
    // L
    {{{0,2},{1,0},{1,1},{1,2}}, {{0,1},{1,1},{2,1},{2,2}}, {{1,0},{1,1},{1,2},{2,0}}, {{0,0},{0,1},{1,1},{2,1}}}
};

static const uint32_t tet_colors[7] = {
    RGB(0,240,240), RGB(240,240,0), RGB(160,0,240),
    RGB(0,240,0),   RGB(240,0,0),   RGB(0,0,240), RGB(240,160,0)
};

static uint8_t tet_valid(AppState *st, int8_t px, int8_t py, uint8_t piece, uint8_t rot){
    for(int i=0;i<4;i++){
        int8_t r=(int8_t)(py+tet_shapes[piece][rot][i][0]);
        int8_t c=(int8_t)(px+tet_shapes[piece][rot][i][1]);
        if(c<0||c>=TET_COLS||r>=TET_ROWS) return 0;
        if(r>=0 && st->tet_board[r*TET_COLS+c]) return 0;
    }
    return 1;
}

static void tet_lock(AppState *st){
    for(int i=0;i<4;i++){
        int8_t r=(int8_t)(st->tet_py+tet_shapes[st->tet_piece][st->tet_rot][i][0]);
        int8_t c=(int8_t)(st->tet_px+tet_shapes[st->tet_piece][st->tet_rot][i][1]);
        if(r>=0&&r<TET_ROWS&&c>=0&&c<TET_COLS)
            st->tet_board[r*TET_COLS+c]=st->tet_piece+1;
    }
    // Clear complete rows
    for(int r=TET_ROWS-1;r>=0;r--){
        int full=1;
        for(int c=0;c<TET_COLS;c++) if(!st->tet_board[r*TET_COLS+c]){full=0;break;}
        if(full){
            st->tet_score+=100;
            for(int rr=r;rr>0;rr--)
                for(int c=0;c<TET_COLS;c++)
                    st->tet_board[rr*TET_COLS+c]=st->tet_board[(rr-1)*TET_COLS+c];
            for(int c=0;c<TET_COLS;c++) st->tet_board[c]=0;
            r++; // recheck this row
        }
    }
}

static void render_tetris(Window *w, int32_t mx, int32_t my, uint8_t click, uint8_t active){
    (void)mx;(void)my;
    int32_t cx=win_ca_x(w),cy=win_ca_y(w),cw=win_ca_w(w),ch=win_ca_h(w);
    AppState *st=&w->st;
    gui_fill_rect(cx,cy,cw,ch,RGB(20,20,30));

    // Board origin (centred)
    int32_t bw=TET_COLS*TET_CELL;
    int32_t bh=TET_ROWS*TET_CELL;
    int32_t ox=cx+(cw-bw)/2;
    int32_t oy=cy+4;

    // Init
    if(!st->tet_init){
        st->tet_init=1;
        kmemset(st->tet_board,0,sizeof(st->tet_board));
        st->tet_piece=0; st->tet_rot=0;
        st->tet_px=3; st->tet_py=-1;
        st->tet_score=0; st->tet_dead=0;
        st->tet_tick=get_ticks(); st->tet_speed=30;
    }

    // Restart on click when dead
    if(click&&st->tet_dead){
        st->tet_init=0;
        return;
    }

    if(!st->tet_dead){
        // Keyboard: left/right/down/rotate — only when this window is active
        if(active){
            char kc=kb_getchar_nowait();
            if(kc=='a'||kc=='4'){if(tet_valid(st,st->tet_px-1,st->tet_py,st->tet_piece,st->tet_rot)) st->tet_px--;}
            if(kc=='d'||kc=='6'){if(tet_valid(st,st->tet_px+1,st->tet_py,st->tet_piece,st->tet_rot)) st->tet_px++;}
            if(kc=='s'||kc=='2'){if(tet_valid(st,st->tet_px,st->tet_py+1,st->tet_piece,st->tet_rot)) st->tet_py++;}
            if(kc=='w'||kc==' '){
                uint8_t nr=(st->tet_rot+1)%4;
                if(tet_valid(st,st->tet_px,st->tet_py,st->tet_piece,nr)) st->tet_rot=nr;
            }
        }
        // Gravity
        uint32_t now=get_ticks();
        if(now-st->tet_tick>=st->tet_speed){
            st->tet_tick=now;
            if(tet_valid(st,st->tet_px,st->tet_py+1,st->tet_piece,st->tet_rot)){
                st->tet_py++;
            } else {
                tet_lock(st);
                // New piece
                static uint32_t seed=0xABCD1234;
                seed^=get_ticks(); seed=seed*1664525+1013904223;
                st->tet_piece=(uint8_t)(seed%7);
                st->tet_rot=0; st->tet_px=3; st->tet_py=-1;
                if(!tet_valid(st,st->tet_px,st->tet_py,st->tet_piece,st->tet_rot))
                    st->tet_dead=1;
            }
        }
    }

    // Draw board border
    gui_fill_rect(ox-1,oy-1,bw+2,bh+2,RGB(80,80,100));
    gui_fill_rect(ox,oy,bw,bh,RGB(10,10,20));

    // Draw locked cells
    for(int r=0;r<TET_ROWS;r++) for(int c=0;c<TET_COLS;c++){
        uint8_t v=st->tet_board[r*TET_COLS+c];
        if(!v) continue;
        int32_t px2=ox+c*TET_CELL, py2=oy+r*TET_CELL;
        gui_fill_rect(px2+1,py2+1,TET_CELL-2,TET_CELL-2,tet_colors[v-1]);
        gui_draw_hline(px2+1,py2+1,TET_CELL-2,RGB(255,255,255));
        gui_draw_vline(px2+1,py2+1,TET_CELL-2,RGB(255,255,255));
    }

    // Draw active piece
    if(!st->tet_dead){
        uint32_t pc=tet_colors[st->tet_piece];
        for(int i=0;i<4;i++){
            int8_t r=(int8_t)(st->tet_py+tet_shapes[st->tet_piece][st->tet_rot][i][0]);
            int8_t c=(int8_t)(st->tet_px+tet_shapes[st->tet_piece][st->tet_rot][i][1]);
            if(r<0||r>=TET_ROWS||c<0||c>=TET_COLS) continue;
            int32_t px2=ox+c*TET_CELL, py2=oy+r*TET_CELL;
            gui_fill_rect(px2+1,py2+1,TET_CELL-2,TET_CELL-2,pc);
            gui_draw_hline(px2+1,py2+1,TET_CELL-2,RGB(255,255,255));
            gui_draw_vline(px2+1,py2+1,TET_CELL-2,RGB(255,255,255));
        }
    }

    // Sidebar: score
    int32_t sx=ox+bw+8;
    char sb[20]; kstrcpy(sb,"Score:");
    gui_puts(sx,oy,sb,RGB(200,200,200),RGB(20,20,30));
    kutoa(st->tet_score,sb,10);
    gui_puts(sx,oy+FONT_H+2,sb,RGB(255,255,0),RGB(20,20,30));
    gui_puts(sx,oy+FONT_H*3,"WASD",RGB(180,180,180),RGB(20,20,30));
    gui_puts(sx,oy+FONT_H*4,"move",RGB(130,130,130),RGB(20,20,30));
    gui_puts(sx,oy+FONT_H*5,"W=rot",RGB(130,130,130),RGB(20,20,30));

    if(st->tet_dead){
        int32_t gx=ox+bw/2-32, gy=oy+bh/2-8;
        gui_fill_rect(gx-4,gy-4,90,FONT_H+12,RGB(0,0,0));
        gui_puts(gx,gy,"GAME OVER",RGB(255,0,0),RGB(0,0,0));
        gui_puts(gx-6,gy+FONT_H+2,"click restart",RGB(200,200,200),RGB(0,0,0));
    }
    (void)ch;(void)cw;
}

// --- Tic Tac Toe ---
#define TTT_CS 40
static int ttt_check(uint8_t *b, uint8_t p){
    static const int wins[8][3]={{0,1,2},{3,4,5},{6,7,8},{0,3,6},{1,4,7},{2,5,8},{0,4,8},{2,4,6}};
    for(int i=0;i<8;i++) if(b[wins[i][0]]==p&&b[wins[i][1]]==p&&b[wins[i][2]]==p) return 1;
    return 0;
}
static void ttt_ai(Window *w){
    // try to win or block
    for(int p=2;p>=1;p--){
        for(int i=0;i<9;i++){
            if(w->st.ttt_board[i]) continue;
            w->st.ttt_board[i]=p;
            if(ttt_check(w->st.ttt_board,(uint8_t)p)){
                if(p==2){w->st.ttt_done=2;return;}
                w->st.ttt_board[i]=0; continue;
            }
            if(p==2){return;}
            w->st.ttt_board[i]=0;
        }
    }
    // center
    if(!w->st.ttt_board[4]){w->st.ttt_board[4]=2;return;}
    // any free
    for(int i=0;i<9;i++) if(!w->st.ttt_board[i]){w->st.ttt_board[i]=2;return;}
}

static void render_tictactoe(Window *w, int32_t mx, int32_t my, uint8_t click){
    int32_t cx=win_ca_x(w),cy=win_ca_y(w),cw=win_ca_w(w),ch=win_ca_h(w);
    gui_fill_rect(cx,cy,cw,ch,COL_WIN_BG);
    int32_t ox=cx+(cw-3*TTT_CS)/2, oy=cy+30;
    // Input
    if(click&&w->st.ttt_turn==1&&!w->st.ttt_done){
        int32_t rc=(mx-ox)/TTT_CS, rr=(my-oy)/TTT_CS;
        if(rc>=0&&rc<3&&rr>=0&&rr<3&&!w->st.ttt_board[rr*3+rc]){
            w->st.ttt_board[rr*3+rc]=1;
            if(ttt_check(w->st.ttt_board,1)) w->st.ttt_done=1;
            else { ttt_ai(w); if(!w->st.ttt_done&&ttt_check(w->st.ttt_board,2)) w->st.ttt_done=2; }
            // check draw
            if(!w->st.ttt_done){int f=1;for(int i=0;i<9;i++)if(!w->st.ttt_board[i])f=0;if(f)w->st.ttt_done=3;}
        }
    }
    if(click&&w->st.ttt_done){kmemset(w->st.ttt_board,0,9);w->st.ttt_done=0;w->st.ttt_turn=1;}
    // Grid lines
    for(int i=1;i<3;i++){
        gui_draw_vline(ox+i*TTT_CS,oy,3*TTT_CS,COL_BORDER_DARK);
        gui_draw_hline(ox,oy+i*TTT_CS,3*TTT_CS,COL_BORDER_DARK);
    }
    // Cells
    for(int r=0;r<3;r++) for(int c=0;c<3;c++){
        int32_t px=ox+c*TTT_CS+4, py=oy+r*TTT_CS+4;
        uint8_t v=w->st.ttt_board[r*3+c];
        if(v==1) gui_puts(px,py,"X",RGB(200,0,0),COL_WIN_BG);
        else if(v==2) gui_puts(px,py,"O",RGB(0,0,200),COL_WIN_BG);
    }
    // Status
    const char *msg="You=X  AI=O  Click cell";
    if(w->st.ttt_done==1) msg="You win! Click to reset";
    else if(w->st.ttt_done==2) msg="AI wins! Click to reset";
    else if(w->st.ttt_done==3) msg="Draw!    Click to reset";
    gui_puts(cx+4,cy+8,msg,COL_TEXT,COL_WIN_BG);
    (void)cw;(void)ch;
}

// --- Slots ---
static const char *g_slot_syms[]={"7","$","@","*","+"};
#define N_SLOT_SYMS 5

static void render_slots(Window *w, int32_t mx, int32_t my, uint8_t click){
    int32_t cx=win_ca_x(w),cy=win_ca_y(w),cw=win_ca_w(w),ch=win_ca_h(w);
    gui_fill_rect(cx,cy,cw,ch,COL_WIN_BG);
    uint32_t now=get_ticks();
    if(w->st.slot_spinning && now-w->st.slot_tick>40){
        // settle reels
        for(int i=0;i<3;i++) w->st.slot_reels[i]=(uint8_t)((now*(i+3)+i*17)%N_SLOT_SYMS);
        w->st.slot_spinning=0;
        if(w->st.slot_reels[0]==w->st.slot_reels[1]&&w->st.slot_reels[1]==w->st.slot_reels[2])
            w->st.slot_credits+=50;
        else if(w->st.slot_reels[0]==w->st.slot_reels[1]||w->st.slot_reels[1]==w->st.slot_reels[2])
            w->st.slot_credits+=5;
        else w->st.slot_credits-=2;
    }
    if(w->st.slot_spinning){
        for(int i=0;i<3;i++) w->st.slot_reels[i]=(uint8_t)((now+i*13)%N_SLOT_SYMS);
    }
    // Draw reels
    int32_t rox=cx+20;
    for(int i=0;i<3;i++){
        gui_fill_rect(rox+i*50,cy+20,40,40,RGB(255,255,240));
        gui_draw_3d_box(rox+i*50,cy+20,40,40,0);
        const char *sym=w->st.slot_spinning?"?":g_slot_syms[w->st.slot_reels[i]];
        int32_t sw=(int32_t)kstrlen(sym)*FONT_W;
        gui_puts(rox+i*50+(40-sw)/2,cy+34,sym,
                 w->st.slot_reels[i]==0?RGB(255,180,0):COL_TEXT,RGB(255,255,240));
    }
    // Credits
    char cb[20]; kstrcpy(cb,"Credits: "); kitoa(w->st.slot_credits,cb+9,10);
    gui_puts(cx+4,cy+70,cb,COL_TEXT,COL_WIN_BG);
    if(gui_button(cx+(cw-60)/2,cy+88,60,18,"SPIN",mx,my,click)){
        if(!w->st.slot_spinning&&w->st.slot_credits>=2){
            w->st.slot_spinning=1;w->st.slot_tick=now;
        }
    }
    gui_puts(cx+4,cy+ch-12,"7 7 7 = +50  Pair = +5  Miss = -2",RGB(80,80,80),COL_WIN_BG);
    (void)ch;
}

// --- Piano (visual only, no PC speaker in QEMU by default) ---
static void render_piano(Window *w, int32_t mx, int32_t my, uint8_t click){
    (void)click;
    int32_t cx=win_ca_x(w),cy=win_ca_y(w),cw=win_ca_w(w),ch=win_ca_h(w);
    gui_fill_rect(cx,cy,cw,ch,COL_WIN_BG);
    static const char *notes[]={"C","D","E","F","G","A","B","C2"};
    int32_t kw=(cw-8)/8, kh=ch-20;
    for(int i=0;i<8;i++){
        int32_t px=cx+4+i*kw;
        int32_t hover=(mx>=px&&mx<px+kw-2&&my>=cy&&my<cy+kh);
        uint32_t kc=hover?RGB(220,220,220):RGB(255,255,255);
        if(hover&&(g_mouse.buttons&1)) kc=RGB(180,180,255);
        gui_fill_rect(px,cy+4,kw-2,kh,kc);
        gui_draw_3d_box(px,cy+4,kw-2,kh,1);
        gui_puts(px+(kw-FONT_W)/2,cy+kh-10,notes[i],COL_BLACK,kc);
    }
    // Black keys (visual only)
    static const int bk_pos[]={0,1,3,4,5,-1,-1,-1};
    for(int i=0;i<5;i++){
        int32_t px=cx+4+(bk_pos[i]+1)*kw-kw/3;
        gui_fill_rect(px,cy+4,kw*2/3,kh*2/3,RGB(0,0,0));
    }
    gui_puts(cx+4,cy+ch-12,"Mouse over keys to play (visual)",RGB(80,80,80),COL_WIN_BG);
    (void)ch;
}

// --- Terminal ---
// Command buffer: copied before launching app task (avoids stack lifetime issues)
static char     g_term_cmd_buf[256];
static Window  *g_term_win = NULL; /* window running the current SDK app */

// Forward declaration (defined below)
static void term_out_cb(const char *c, void *ud);

// App task runner — executed in TASK_APP context by the scheduler
static void term_app_runner(void *ud) {
    Window *w = (Window *)ud;
    shell_exec_line(g_term_cmd_buf, term_out_cb, (void*)w);
    // sched_app_done() is called by the scheduler trampoline after we return
}

// Terminal output redirect: appends chars into the window text buffer
static void term_out_cb(const char *c, void *ud){
    Window *w=(Window*)ud;
    while (*c) {
        char ch = *c++;
        if (ch == '\b') {
            // Backspace: remove last character from buffer
            if (w->st.text_len > 0) {
                w->st.text_len--;
                w->st.text[w->st.text_len] = 0;
            }
            continue;
        }
        // If buffer almost full, trim oldest half (keep last 4KB)
        if(w->st.text_len >= WIN_TEXT_BUF-4){
            int32_t keep = WIN_TEXT_BUF/2;
            int32_t cut = w->st.text_len - keep;
            while(cut < w->st.text_len && w->st.text[cut] != '\n') cut++;
            if(cut < w->st.text_len) cut++;
            int32_t newlen = w->st.text_len - cut;
            if(newlen > 0) kmemcpy(w->st.text, w->st.text+cut, (uint32_t)newlen);
            w->st.text_len = newlen;
            w->st.text[w->st.text_len] = 0;
        }
        w->st.text[w->st.text_len++] = ch;
        w->st.text[w->st.text_len] = 0;
    }
}

// Terminal input redirect: pop one char from the window's ring buffer (non-blocking)
static int term_in_cb(void *ud) {
    Window *w = (Window *)ud;
    if (w->st.term_in_head == w->st.term_in_tail) return 0; // empty
    char c = w->st.term_inbuf[w->st.term_in_tail];
    w->st.term_in_tail = (w->st.term_in_tail + 1) % (uint32_t)sizeof(w->st.term_inbuf);
    return (int)(unsigned char)c;
}

// Push one char into the ring buffer (called from keyboard handler)
static void term_inbuf_push(Window *w, char c) {
    uint8_t next = (w->st.term_in_head + 1) % (uint32_t)sizeof(w->st.term_inbuf);
    if (next == w->st.term_in_tail) return; // buffer full, drop
    w->st.term_inbuf[w->st.term_in_head] = c;
    w->st.term_in_head = next;
}

// Extract the command typed after the last '> ' prompt
static int32_t term_get_cmd(Window *w, char *out, int32_t max){
    // find last newline
    int32_t i=w->st.text_len-1;
    while(i>0&&w->st.text[i-1]!='\n') i--;
    // skip "> " prefix
    if(w->st.text[i]=='>'&&w->st.text[i+1]==' ') i+=2;
    int32_t l=0;
    while(i+l<w->st.text_len&&l<max-1){out[l]=w->st.text[i+l];l++;}
    out[l]=0;
    return l;
}

static void render_terminal(Window *w, int32_t mx, int32_t my, uint8_t click, uint8_t active){
    (void)mx;(void)my;(void)click;

    // Deferred teardown: the app launch in render_terminal no longer spins a
    // blocking gui_pump() loop.  Instead we detect here, each frame, whether
    // the previously-launched app has finished and clean up if so.
    if (w->st.term_app_running && !sched_app_running()) {
        g_sdk_app_active = 0;
        syscall_set_output_cb(NULL, NULL);
        syscall_set_input_cb(NULL, NULL);
        w->st.term_app_running = 0;
        // Append the prompt for the next command
        if (w->st.text_len < WIN_TEXT_BUF - 4) {
            w->st.text[w->st.text_len++] = '>';
            w->st.text[w->st.text_len++] = ' ';
            w->st.text[w->st.text_len]   = 0;
        }
    }

    int32_t cx=win_ca_x(w),cy=win_ca_y(w),cw=win_ca_w(w),ch=win_ca_h(w);
    gui_fill_rect(cx,cy,cw,ch,COL_TERM_BG);

    // First time: print banner and initial prompt
    if(!w->st.term_initialized){
        w->st.term_initialized=1;
        kstrcpy(w->st.text,"Eclipse32 Terminal\nType 'help' for commands.\n> ");
        w->st.text_len=(int32_t)kstrlen(w->st.text);
    }

    // Render text buffer (scroll so last lines are visible)
    // Count lines total
    int32_t total_lines=1;
    for(int32_t i=0;i<w->st.text_len;i++) if(w->st.text[i]=='\n') total_lines++;
    int32_t vis_lines=(ch-4)/(FONT_H+1);
    int32_t skip_lines=total_lines-vis_lines;
    if(skip_lines<0) skip_lines=0;

    const char *p=w->st.text;
    int32_t skipped=0;
    // skip past first skip_lines newlines
    while(*p&&skipped<skip_lines){ if(*p=='\n') skipped++; p++; }

    int32_t tx=cx+3, ty=cy+3;
    while(*p){
        if(*p=='\n'){tx=cx+3;ty+=FONT_H+1;}
        else{
            if(ty>=cy&&ty+FONT_H<=cy+ch&&tx<cx+cw)
                gui_putc(tx,ty,*p,COL_TERM_TXT,COL_TERM_BG);
            tx+=FONT_W;
            if(tx+FONT_W>cx+cw){tx=cx+3;ty+=FONT_H+1;}
        }
        p++;
    }
    // Blinking cursor
    if((get_ticks()/50)%2==0) gui_fill_rect(tx,ty,FONT_W,FONT_H,COL_TERM_TXT);

    // If an app is running, pump it forward every frame regardless of
    // window focus -- a backgrounded `get` shouldn't freeze just because
    // the user clicked another window. Keyboard input below still
    // requires focus (active), but the scheduler resume must not.
    //
    // Bug history: previously the only sched_yield() into TASK_APP happened
    // once, inside the Enter-key handler below, via sched_run_app(). That
    // call returns to here as soon as the app itself calls sched_yield()
    // for ANY reason -- e.g. every wait loop in tcp_send()/tcp_recv()/
    // poll_until() in tcp.c calls "if (sched_app_running()) sched_yield();"
    // on every iteration. The first time a shell builtin like `get` hit one
    // of those loops, control returned here mid-frame with the app still
    // runnable (not finished) -- and nothing ever called sched_yield()
    // again, since that only happened at the top of the Enter-key handler
    // (a new command) or in the g_sdk_app_active branch (APP_SDK windows
    // only, not plain terminal builtins). So the app was parked forever:
    // no crash, no spin, the GUI kept rendering fine every frame (which is
    // why pit_ms()/networking IRQs kept working), but the command's own
    // execution never resumed. From the user's perspective this looked
    // exactly like a freeze right after "[HTTP] Connected", since that was
    // the last output printed before the first yield deep in tcp_send().
    if (w->st.term_app_running && sched_app_running()) {
        sched_yield();
    }

    // Keyboard input: only read keys when this terminal window is active/front
    if(!active) return;

    if(w->st.term_app_running){
        char kc=kb_getchar_nowait();
        if(kc) term_inbuf_push(w, kc);
        return;
    }

    char kc=kb_getchar_nowait();
    if(kc){
        if(kc=='\r'||kc=='\n'){
            char cmd[256];
            term_get_cmd(w, cmd, 256);
            if(w->st.text_len<WIN_TEXT_BUF-2){
                w->st.text[w->st.text_len++]='\n';
                w->st.text[w->st.text_len]=0;
            }
            if(kstrcmp(cmd,"clear")==0||kstrcmp(cmd,"cls")==0){
                kstrcpy(w->st.text,"");
                w->st.text_len=0;
            } else if(cmd[0]!=0){
                // Set up I/O callbacks for the app task
                w->st.term_app_running = 1;
                w->st.term_in_head = w->st.term_in_tail = 0;
                syscall_set_output_cb(term_out_cb, (void*)w);
                syscall_set_input_cb(term_in_cb, (void*)w);
                kstrcpy(g_term_cmd_buf, cmd);

                // Launch app in TASK_APP.  Set g_sdk_app_active BEFORE the
                // first sched_run_app so gui_pump's yield-to-app guard fires
                // correctly on every frame, including the very first.
                // We do NOT loop here — render_terminal runs inside gui_pump's
                // render pass, so calling gui_pump() recursively would corrupt
                // the stack.  Instead we return immediately; the top-level
                // gui_pump loop re-enters each frame, hits the g_sdk_app_active
                // branch, yields to the app, and the app draws its content.
                // Teardown is handled at the top of render_terminal on the
                // first frame after sched_app_running() becomes false.
                g_sdk_app_active = 1;
                g_term_win = w;
                sched_run_app(term_app_runner, (void*)w);
                // sched_run_app does the first yield; if the app finished
                // synchronously (no sys_read), sched_app_running() is already
                // false here and we fall through to teardown below.
                if (!sched_app_running()) {
                    // App finished synchronously (no sys_read calls).
                    g_sdk_app_active = 0;
                    g_term_win = NULL;
                    syscall_set_output_cb(NULL, NULL);
                    syscall_set_input_cb(NULL, NULL);
                    w->st.term_app_running = 0;
                    if (w->st.text_len < WIN_TEXT_BUF - 4) {
                        w->st.text[w->st.text_len++] = '>';
                        w->st.text[w->st.text_len++] = ' ';
                        w->st.text[w->st.text_len]   = 0;
                    }
                }
                // If still running: deferred teardown fires in render_terminal
                // on the frame after the app calls sched_app_done().
            }
            /* prompt appended by deferred teardown above */
        }else if(kc=='\b'||kc==127){
            if(w->st.text_len>=3){
                char prev=w->st.text[w->st.text_len-1];
                char prev2=w->st.text[w->st.text_len-2];
                if(!(prev==' '&&prev2=='>')){
                    w->st.text_len--;
                    w->st.text[w->st.text_len]=0;
                }
            }
        }else if(kc==3){
            if(w->st.text_len<WIN_TEXT_BUF-6){
                const char *ctc="^C\n> ";
                kstrcpy(w->st.text+w->st.text_len,ctc);
                w->st.text_len+=(int32_t)kstrlen(ctc);
            }
        }else if(kc>=32&&w->st.text_len<WIN_TEXT_BUF-2){
            w->st.text[w->st.text_len++]=kc;
            w->st.text[w->st.text_len]=0;
        }
    }
    (void)cw;(void)ch;
}

// --- Log Viewer ---
static void render_logviewer(Window *w, int32_t mx, int32_t my, uint8_t click){
    (void)mx;(void)my;(void)click;
    int32_t cx=win_ca_x(w),cy=win_ca_y(w),cw=win_ca_w(w),ch=win_ca_h(w);
    gui_fill_rect(cx,cy,cw,ch,RGB(0,0,0));
    // Show static boot log
    static const char *log[]={
        "[KERNEL] Eclipse32 starting...",
        "[MEM]    Heap: 5120 KB",
        "[GDT]    GDT loaded",
        "[IDT]    IDT loaded",
        "[PIC]    PIC initialized",
        "[PIT]    Timer: 100 Hz",
        "[KB]     PS/2 keyboard OK",
        "[MOUSE]  PS/2 mouse OK",
        "[DISK]   ATA drive detected",
        "[FB]     VESA 800x600x24",
        "[FS]     EclipseFS mounted",
        "[GUI]    EclipseGUI starting",
        "[GUI]    Desktop ready",
    };
    int32_t ty=cy+3;
    for(int i=0;i<13&&ty<cy+ch-FONT_H;i++){
        gui_puts(cx+3,ty,log[i],RGB(0,200,0),RGB(0,0,0)); ty+=FONT_H+2;
    }
    char ut[20]; kstrcpy(ut,"[UP]     "); kutoa(get_ticks()/100,ut+9,10); kstrcat(ut,"s");
    gui_puts(cx+3,ty,ut,RGB(0,200,0),RGB(0,0,0));
    (void)cw;(void)ch;
}

// --- About ---
// --- Hello OS32 App Window ---
static void render_os32app(Window *w, int32_t mx, int32_t my, uint8_t click){
    int32_t cx=win_ca_x(w), cy=win_ca_y(w), cw=win_ca_w(w), ch=win_ca_h(w);

    // Background
    gui_fill_rect(cx, cy, cw, ch, RGB(240,240,248));

    // Gradient-style header banner
    for(int32_t i=0; i<44; i++){
        uint32_t r=(uint32_t)(20 + i*2);
        uint32_t g=(uint32_t)(60 + i*2);
        uint32_t b=(uint32_t)(160 + i);
        if(r>80)  r=80;
        if(g>148) g=148;
        if(b>204) b=204;
        gui_draw_hline(cx, cy+i, cw, (r<<16)|(g<<8)|b);
    }

    // App icon box
    gui_fill_rect(cx+12, cy+8, 28, 28, RGB(255,200,0));
    gui_draw_3d_box(cx+12, cy+8, 28, 28, 1);
    gui_puts(cx+18, cy+17, "32", RGB(0,0,80), RGB(255,200,0));

    // Title text in banner
    gui_puts(cx+48, cy+12, "Hello OS32!", RGB(255,255,255), RGB(20,80,180));
    gui_puts(cx+48, cy+24, "Eclipse32 Sample App", RGB(200,220,255), RGB(20,80,180));

    // Separator line
    gui_draw_hline(cx, cy+44, cw, RGB(100,120,200));
    gui_draw_hline(cx, cy+45, cw, RGB(255,255,255));

    // Main content area
    int32_t ty = cy+56;

    // Big centred greeting
    const char *greet = "Hello from OS32!";
    int32_t gw = (int32_t)kstrlen(greet) * FONT_W;
    gui_puts(cx+(cw-gw)/2, ty, greet, RGB(0,0,128), RGB(240,240,248));
    ty += FONT_H + 6;

    // Divider
    gui_draw_hline(cx+8, ty, cw-16, RGB(180,180,210));
    ty += 8;

    // Info rows
    #define ROW(label, val) \
        gui_puts(cx+12, ty, label, RGB(80,80,80),  RGB(240,240,248)); \
        gui_puts(cx+110, ty, val,  RGB(0,80,160),  RGB(240,240,248)); \
        ty += FONT_H+5;

    ROW("Format:",   "OS32 v1")
    ROW("Entry:",    "appMain()")
    ROW("Syscall:",  "INT 0x40")
    ROW("Exit:",     "syscall -1")

    #undef ROW

    // Live uptime counter
    char tbuf[32];
    uint32_t secs = g_tick / 100;
    gui_puts(cx+12, ty, "Uptime:", RGB(80,80,80), RGB(240,240,248));
    kutoa(secs, tbuf, 10); kstrcat(tbuf, "s");
    gui_puts(cx+110, ty, tbuf, RGB(0,140,0), RGB(240,240,248));
    ty += FONT_H+5;

    // Pulsing dot animation using tick
    uint32_t phase = (g_tick / 25) % 4;
    const char *dots[] = { "●   ", "●●  ", "●●● ", "●●●●" };
    gui_puts(cx+12, ty, dots[phase], RGB(0,120,200), RGB(240,240,248));

    // Close button
    if(gui_button(cx+(cw-60)/2, cy+ch-26, 60, 18, "Close", mx, my, click)){
        for(int32_t i=0;i<g_nwins;i++)
            if(&g_wins[i]==w){ close_window(i); return; }
    }
}

static void render_about(Window *w, int32_t mx, int32_t my, uint8_t click){
    int32_t cx=win_ca_x(w),cy=win_ca_y(w),cw=win_ca_w(w),ch=win_ca_h(w);
    gui_fill_rect(cx,cy,cw,ch,COL_WIN_BG);
    gui_fill_rect(cx,cy,cw,50,RGB(0,0,128));
    int32_t tw=9*FONT_W;
    gui_puts(cx+(cw-tw)/2,cy+10,"Eclipse32",RGB(255,255,0),RGB(0,0,128));
    tw=10*FONT_W;
    gui_puts(cx+(cw-tw)/2,cy+22,"OS  v1.0.0",RGB(200,200,255),RGB(0,0,128));
    int32_t ty=cy+58;
    #define AR(s) gui_puts(cx+8,ty,s,COL_TEXT,COL_WIN_BG);ty+=FONT_H+4;
    AR("32-bit x86 OS from scratch")
    AR("Built: C + NASM Assembly")
    AR("Toolchain: zig cc + ld")
    AR("Kernel:  Eclipse32 v1.0")
    AR("FS:      EclipseFS")
    AR("GUI:     EclipseGUI v2")
    AR("Apps:    32 built-in")
    #undef AR
    int32_t bx=cx+(cw-60)/2;
    if(gui_button(bx,cy+ch-26,60,18,"OK",mx,my,click)){
        for(int32_t i=0;i<g_nwins;i++)
            if(&g_wins[i]==w){close_window(i);return;}
    }
    (void)ch;
}

// --- Settings ---
// Theme preview swatch colors
static const uint32_t g_theme_preview_desktop[THEME_COUNT] = {
    RGB(30,87,153),     // Modern: blue
    RGB(0,128,128),     // Classic: teal
    RGB(8,12,40),       // Midnight: deep navy
    RGB(80,20,60),      // Sunset: deep purple
};
static const uint32_t g_theme_preview_taskbar[THEME_COUNT] = {
    RGB(42,42,42),      // Modern: dark
    RGB(192,192,192),   // Classic: grey
    RGB(10,14,35),      // Midnight: near-black navy
    RGB(25,10,20),      // Sunset: near-black purple
};
static const uint32_t g_theme_preview_title[THEME_COUNT] = {
    RGB(50,120,200),    // Modern: blue
    RGB(0,0,128),       // Classic: navy
    RGB(60,80,160),     // Midnight: ice blue
    RGB(200,80,40),     // Sunset: orange
};
static const char *g_theme_names[THEME_COUNT] = {
    "Modern   (Dark Panel)",
    "Classic  (Grey Panel)",
    "Midnight (Deep Navy)",
    "Sunset   (Warm Dusk)",
};

static void render_settings(Window *w, int32_t mx, int32_t my, uint8_t click) {
    int32_t cx=win_ca_x(w), cy=win_ca_y(w), cw=win_ca_w(w), ch=win_ca_h(w);
    gui_fill_rect(cx, cy, cw, ch, COL_WIN_BG);

    // Header
    gui_fill_rect(cx, cy, cw, FONT_H+8, COL_WIN_TITLE_ACT);
    gui_puts(cx+6, cy+4, "Eclipse32  Settings", COL_WIN_TITLE_TXT, COL_WIN_TITLE_ACT);

    int32_t ty = cy + FONT_H + 14;
    gui_puts(cx+6, ty, "Theme", COL_TEXT, COL_WIN_BG);
    ty += FONT_H + 4;
    gui_draw_hline(cx+4, ty, cw-8, COL_BORDER_DARK);
    ty += 6;

    // Theme cards — one per theme
    for (int t = 0; t < THEME_COUNT; t++) {
        int32_t card_x = cx + 8;
        int32_t card_w = cw - 16;
        int32_t card_h = 64;
        uint8_t is_sel = (g_theme == t);

        // Card border (highlight if selected)
        uint32_t border_col = is_sel ? COL_WIN_TITLE_ACT : COL_BORDER_DARK;
        gui_fill_rect(card_x-1, ty-1, card_w+2, card_h+2, border_col);
        gui_fill_rect(card_x, ty, card_w, card_h, COL_WIN_BG);

        // ── Mini desktop preview ──
        int32_t prev_x = card_x + 6;
        int32_t prev_y = ty + 6;
        int32_t prev_w = 90;
        int32_t prev_h = 52;

        // Desktop area
        uint32_t pdt = g_theme_preview_desktop[t];
        gui_fill_rect(prev_x, prev_y, prev_w, prev_h, pdt);
        gui_draw_rect_border(prev_x, prev_y, prev_w, prev_h, RGB(60,60,60), RGB(60,60,60));

        // Taskbar position: top for Modern, bottom for Classic
        int32_t tb_y2 = (t == THEME_CLASSIC)
                      ? (prev_y + prev_h - 7)
                      : (prev_y);
        gui_fill_rect(prev_x, tb_y2, prev_w, 7, g_theme_preview_taskbar[t]);

        // Mini window inside preview
        int32_t mw_x = prev_x + 20;
        int32_t mw_y = (t == THEME_CLASSIC) ? (prev_y + 10) : (prev_y + 10);
        int32_t mw_w = 48, mw_h = 28;
        gui_fill_rect(mw_x, mw_y, mw_w, mw_h, RGB(210,210,210));
        gui_fill_rect(mw_x, mw_y, mw_w, 6, g_theme_preview_title[t]);
        gui_draw_rect_border(mw_x, mw_y, mw_w, mw_h, RGB(80,80,80), RGB(80,80,80));

        // ── Labels ──
        int32_t lx = prev_x + prev_w + 10;
        gui_puts(lx, ty+8,  g_theme_names[t], COL_TEXT, COL_WIN_BG);
        if (is_sel) {
            gui_fill_rect(lx, ty+20, 50, FONT_H+2, RGB(50,180,50));
            gui_puts(lx+4, ty+21, "Active", RGB(255,255,255), RGB(50,180,50));
        } else {
            if (gui_button(lx, ty+20, 80, FONT_H+4, "Apply", mx, my, click)) {
                apply_theme(t);
                settings_save();
            }
        }

        ty += card_h + 8;
    }

    // ── Wallpaper section ──
    ty += 8;
    gui_draw_hline(cx+4, ty, cw-8, COL_BORDER_DARK);
    ty += 6;
    gui_puts(cx+6, ty, "Wallpaper", COL_TEXT, COL_WIN_BG);
    ty += FONT_H + 4;

    // 2-row grid of wallpaper swatches
    int32_t sw_size = 28;
    int32_t sw_cols = WALLPAPER_COUNT / 2;
    for (int w2 = 0; w2 < WALLPAPER_COUNT; w2++) {
        int32_t col2 = w2 % sw_cols;
        int32_t row2 = w2 / sw_cols;
        int32_t sx = cx + 8 + col2 * (sw_size + 4);
        int32_t sy = ty + row2 * (sw_size + 4);
        uint32_t swcol = (w2 == WALLPAPER_GRADIENT)
            ? RGB(40, 90, 160)  // represent gradient
            : g_wallpaper_colors[w2];
        gui_fill_rect(sx, sy, sw_size, sw_size, swcol);
        // draw "G" label on gradient swatch
        if (w2 == WALLPAPER_GRADIENT) {
            gui_puts(sx+10, sy+10, "G", COL_WHITE, swcol);
        }
        // selection ring
        uint32_t ring = (g_wallpaper == w2) ? COL_WIN_TITLE_ACT : COL_BORDER_DARK;
        gui_draw_rect_border(sx-1, sy-1, sw_size+2, sw_size+2, ring, ring);
        // tooltip name
        gui_puts(sx+2, sy+sw_size-FONT_H-2, g_wallpaper_names[w2], COL_WHITE, swcol);
        // click to select
        if (click && pt_in(mx, my, sx, sy, sw_size, sw_size)) {
            g_wallpaper = w2;
            settings_save();
        }
    }
    ty += 2 * (sw_size + 4) + 4;

    // Save button
    gui_draw_hline(cx+4, ty, cw-8, COL_BORDER_DARK);
    ty += 6;
    if (gui_button(cx+6, ty, 90, 18, "Save Settings", mx, my, click)) {
        settings_save();
    }
    gui_puts(cx+102, ty+5, "Persists on reboot", RGB(100,100,100), COL_WIN_BG);
    (void)ch;
}

// --- Calendar ---
static void render_calendar(Window *w, int32_t mx, int32_t my, uint8_t click){
    int32_t cx=win_ca_x(w),cy=win_ca_y(w),cw=win_ca_w(w),ch=win_ca_h(w);
    gui_fill_rect(cx,cy,cw,ch,COL_WIN_BG);
    uint8_t m=w->st.cal_month%12;
    static const char *months[]={"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
    static const int days_in[]={31,28,31,30,31,30,31,31,30,31,30,31};
    char hdr[20]; kstrcpy(hdr,months[m]); kstrcat(hdr," 2025");
    int32_t hw=(int32_t)kstrlen(hdr)*FONT_W;
    gui_puts(cx+(cw-hw)/2,cy+4,hdr,RGB(0,0,128),COL_WIN_BG);
    if(gui_button(cx+4,cy+2,20,14,"<",mx,my,click)) w->st.cal_month=(w->st.cal_month+11)%12;
    if(gui_button(cx+cw-24,cy+2,20,14,">",mx,my,click)) w->st.cal_month=(w->st.cal_month+1)%12;
    static const char *dnames[]={"Su","Mo","Tu","We","Th","Fr","Sa"};
    int32_t ox=cx+4, oy=cy+22;
    for(int d=0;d<7;d++) gui_puts(ox+d*26,oy,dnames[d],RGB(0,0,128),COL_WIN_BG);
    oy+=FONT_H+4;
    int days=days_in[m]; int col=m%7; // rough start day
    int32_t row=0;
    for(int d=1;d<=days;d++){
        char dn[4]; kitoa(d,dn,10);
        gui_puts(ox+col*26,oy+row*(FONT_H+4),dn,COL_TEXT,COL_WIN_BG);
        col=(col+1)%7; if(col==0) row++;
    }
    (void)ch;
}

// ============================================================
// App dispatcher
// ============================================================
static void render_app(Window *w, int32_t mx, int32_t my, uint8_t click, uint8_t active) {
    switch(w->app) {
    case APP_NOTEPAD:     render_text_app(w,mx,my,click,1); break;
    case APP_WORDPAD:     render_text_app(w,mx,my,click,0); break;
    case APP_TEXTVIEW:    render_text_app(w,mx,my,click,0); break;
    case APP_HELP:        render_text_app(w,mx,my,click,0); break;
    case APP_CALCULATOR:  render_calculator(w,mx,my,click); break;
    case APP_CALENDAR:    render_calendar(w,mx,my,click);   break;
    case APP_CLOCK:       render_clock(w,mx,my,click);      break;
    case APP_STOPWATCH:   render_stopwatch(w,mx,my,click);  break;
    case APP_SYSINFO:     render_sysinfo(w,mx,my,click);    break;
    case APP_TASKMGR:     render_taskmgr(w,mx,my,click);    break;
    case APP_REGEDIT:     render_regedit(w,mx,my,click);    break;
    case APP_DEVMGR:      render_devmgr(w,mx,my,click);     break;
    case APP_DISKINFO:    render_diskinfo(w,mx,my,click);   break;
    case APP_MEMMAP:      render_memmap(w,mx,my,click);     break;
    case APP_CPUMON:      render_cpumon(w,mx,my,click);     break;
    case APP_FILEMAN:     render_fileman(w,mx,my,click);    break;
    case APP_HEXVIEW:     render_hexview(w,mx,my,click);    break;
    case APP_NETINFO:     render_netinfo(w,mx,my,click);    break;
    case APP_IPCONFIG:    render_ipconfig(w,mx,my,click);   break;
    case APP_PAINT:       render_paint(w,mx,my,click);      break;
    case APP_COLORPICKER: render_colorpicker(w,mx,my,click);break;
    case APP_SCREENSAVER: render_screensaver(w,mx,my,click);break;
    case APP_SNAKE:       render_snake(w,mx,my,click,active); break;
    case APP_PONG:        render_pong(w,mx,my,click);       break;
    case APP_MINESWEEPER: render_minesweeper(w,mx,my,click);break;
    case APP_BREAKOUT:    render_breakout(w,mx,my,click);   break;
    case APP_TICTACTOE:   render_tictactoe(w,mx,my,click);  break;
    case APP_SLOTS:       render_slots(w,mx,my,click);      break;
    case APP_PIANO:       render_piano(w,mx,my,click);      break;
    case APP_TETRIS:      render_tetris(w,mx,my,click,active); break;
    case APP_TERMINAL:    render_terminal(w,mx,my,click,active); break;
    case APP_LOGVIEWER:   render_logviewer(w,mx,my,click);  break;
    case APP_ABOUT:       render_about(w,mx,my,click);      break;
    case APP_SETTINGS:    render_settings(w,mx,my,click);   break;
    case APP_SDK:
        // SDK-managed window: the app draws its own content via syscalls
        // after gui_pump() yields to it (step 3b in gui_pump).
        // Don't clear or overwrite anything here.
        break;
    default: {
        int32_t cx=win_ca_x(w),cy=win_ca_y(w),cw=win_ca_w(w),ch=win_ca_h(w);
        gui_fill_rect(cx,cy,cw,ch,COL_WIN_BG);
        gui_puts(cx+8,cy+8,"(no content)",RGB(128,128,128),COL_WIN_BG);
        break; }
    }
}

// ============================================================
// Start Menu
// ============================================================
// ============================================================
// Start Menu — drops DOWN from top panel (Modern) or UP from bottom bar (Classic)
// ============================================================
#define SM_X        2
#define SM_COL_ROWS  18
#define SM_COL_W     MENU_W
#define SM_TOT_W     (SM_COL_W*2+4)
#define SM_H         (SM_COL_ROWS*MENU_ITEM_H+6)
// Position computed at draw time based on g_theme

static void draw_startmenu(int32_t mx, int32_t my) {
    // Classic: pop UP from bottom bar. Modern: drop DOWN from top panel.
    int32_t sm_y = (g_theme == THEME_CLASSIC)
                 ? (SCREEN_H - TASKBAR_H - SM_H)
                 : TASKBAR_H;

    // Shadow
    gui_fill_rect(SM_X+4, sm_y+4, SM_TOT_W, SM_H, RGB(0,0,0));
    // Menu background
    gui_fill_rect(SM_X, sm_y, SM_TOT_W, SM_H, COL_MENU_BG);
    gui_draw_rect_border(SM_X, sm_y, SM_TOT_W, SM_H, RGB(90,90,90), RGB(30,30,30));
    // Left accent stripe
    uint32_t stripe_col = (g_theme == THEME_CLASSIC)  ? RGB(0,0,128)   :
                          (g_theme == THEME_MIDNIGHT) ? RGB(60,80,160) :
                          (g_theme == THEME_SUNSET)   ? RGB(200,80,40) :
                                                        RGB(50,120,200);
    gui_fill_rect(SM_X, sm_y, 4, SM_H, stripe_col);

    g_menu_hover = -1;
    for (int32_t i = 0; i < N_MENU_ITEMS; i++) {
        int32_t col = i / SM_COL_ROWS;
        int32_t row = i % SM_COL_ROWS;
        int32_t ix = SM_X + 6 + col * SM_COL_W;
        int32_t iy = sm_y + 3 + row * MENU_ITEM_H;

        if (kstrcmp(g_menu_items[i].label, "---") == 0) {
            gui_draw_hline(ix+2, iy+5, SM_COL_W-6, COL_MENU_DIV);
            continue;
        }
        uint8_t hover = pt_in(mx, my, ix, iy, SM_COL_W-2, MENU_ITEM_H);
        if (hover) {
            gui_fill_rect(ix, iy, SM_COL_W-2, MENU_ITEM_H, COL_MENU_SEL);
            gui_puts(ix+6, iy+2, g_menu_items[i].label, COL_MENU_SEL_TXT, COL_MENU_SEL);
            g_menu_hover = i;
        } else {
            gui_puts(ix+6, iy+2, g_menu_items[i].label, COL_MENU_TXT, COL_MENU_BG);
        }
    }
}

// ============================================================
// Top Panel / Taskbar  — position and style depend on g_theme
// ============================================================
static void draw_taskbar(int32_t mx, int32_t my, uint8_t click) {
    int32_t ty = TASKBAR_Y_FOR(g_theme);  // top or bottom depending on theme

    gui_fill_rect(0, ty, SCREEN_W, TASKBAR_H, COL_TASKBAR_BG);

    if (g_theme == THEME_CLASSIC) {
        // Classic: raised 3D border
        gui_draw_3d_box(0, ty, SCREEN_W, TASKBAR_H, 1);
    } else {
        // Modern: thin separator line
        int32_t sep_y = (ty == 0) ? ty + TASKBAR_H - 1 : ty;
        gui_draw_hline(0, sep_y, SCREEN_W, RGB(60,60,60));
    }

    // === Applications / Start button ===
    uint8_t sm_hover = pt_in(mx, my, 2, ty+2, 70, TASKBAR_H-4);
    if (g_theme == THEME_CLASSIC) {
        // Classic: raised Start button
        uint32_t sbg = COL_BUTTON_FACE;
        gui_fill_rect(2, ty+2, 54, TASKBAR_H-4, sbg);
        gui_draw_3d_box(2, ty+2, 54, TASKBAR_H-4, !g_startmenu);
        gui_puts(8, ty+(TASKBAR_H-FONT_H)/2, "Start", COL_BUTTON_TXT, sbg);
    } else {
        // Modern: flat button with "E Eclipse" logo
        uint32_t sbg = g_startmenu ? RGB(70,140,220)
                                   : (sm_hover ? RGB(65,65,65) : RGB(50,50,50));
        gui_fill_rect(2, ty+2, 70, TASKBAR_H-4, sbg);
        gui_draw_rect_border(2, ty+2, 70, TASKBAR_H-4, RGB(80,80,80), RGB(35,35,35));
        gui_fill_rect(5, ty+5, 12, TASKBAR_H-10, RGB(50,120,200));
        gui_puts(7, ty+7, "E", COL_WHITE, RGB(50,120,200));
        gui_puts(21, ty+9, "Eclipse", RGB(220,220,220), sbg);
    }
    if (click && sm_hover) g_startmenu = !g_startmenu;

    // === Window buttons ===
    int32_t bx = (g_theme == THEME_CLASSIC) ? 60 : 78;
    for (int32_t i = 0; i < g_nwins && bx < SCREEN_W - 100; i++) {
        Window *w = &g_wins[i];
        if (!(w->flags & WIN_FLAG_VISIBLE)) continue;
        int32_t bw = 110;
        uint8_t active = (i == g_front);
        uint8_t bhover = pt_in(mx,my,bx,ty+3,bw,TASKBAR_H-6);
        uint32_t wbg;
        if (g_theme == THEME_CLASSIC) {
            wbg = COL_BUTTON_FACE;
            gui_fill_rect(bx, ty+3, bw, TASKBAR_H-6, wbg);
            gui_draw_3d_box(bx, ty+3, bw, TASKBAR_H-6, !active);
        } else {
            wbg = active ? RGB(70,140,220) : (bhover ? RGB(65,65,65) : RGB(50,50,50));
            gui_fill_rect(bx, ty+3, bw, TASKBAR_H-6, wbg);
            if (active) gui_draw_hline(bx, ty+TASKBAR_H-4, bw, RGB(120,200,255));
        }
        if(click && bhover) { bring_front(i); g_startmenu=0; }
        char tb[14]; kstrncpy(tb, w->title, 13); tb[13]=0;
        uint32_t txt_col = (g_theme == THEME_CLASSIC) ? COL_BUTTON_TXT
                         : (active ? COL_WHITE : RGB(200,200,200));
        gui_puts_clip(bx+4, ty+(TASKBAR_H-FONT_H)/2, tb, txt_col, wbg,
                      bx+2, ty+3, bw-4, TASKBAR_H-6);
        bx += bw + 2;
    }

    // === Clock ===
    uint32_t ticks=get_ticks(), secs=ticks/100;
    uint32_t mins=(secs/60)%60, hrs=(secs/3600)%24;
    char tc[9];
    tc[0]='0'+(hrs/10); tc[1]='0'+(hrs%10); tc[2]=':';
    tc[3]='0'+(mins/10); tc[4]='0'+(mins%10); tc[5]=':';
    uint32_t s2=secs%60; tc[6]='0'+(s2/10); tc[7]='0'+(s2%10); tc[8]=0;
    int32_t clk_x = SCREEN_W - 72;
    if (g_theme == THEME_CLASSIC) {
        gui_fill_rect(clk_x, ty+3, 70, TASKBAR_H-6, COL_BUTTON_FACE);
        gui_draw_3d_box(clk_x, ty+3, 70, TASKBAR_H-6, 0);
        gui_puts(clk_x+4, ty+(TASKBAR_H-FONT_H)/2, tc, COL_BUTTON_TXT, COL_BUTTON_FACE);
    } else {
        gui_fill_rect(clk_x, ty+3, 70, TASKBAR_H-6, RGB(38,38,38));
        gui_draw_rect_border(clk_x, ty+3, 70, TASKBAR_H-6, RGB(60,60,60), RGB(30,30,30));
        gui_puts(clk_x+4, ty+(TASKBAR_H-FONT_H)/2, tc, RGB(200,220,255), RGB(38,38,38));
    }
}

// ============================================================
// Mouse cursor (saved/restored to eliminate cursor artifacts)
// ============================================================
static const uint8_t g_cur_shape[12][12] = {
    {1,0,0,0,0,0,0,0,0,0,0,0},
    {1,1,0,0,0,0,0,0,0,0,0,0},
    {1,2,1,0,0,0,0,0,0,0,0,0},
    {1,2,2,1,0,0,0,0,0,0,0,0},
    {1,2,2,2,1,0,0,0,0,0,0,0},
    {1,2,2,2,2,1,0,0,0,0,0,0},
    {1,2,2,2,2,2,1,0,0,0,0,0},
    {1,2,2,2,2,2,2,1,0,0,0,0},
    {1,2,2,2,1,1,0,0,0,0,0,0},
    {1,2,2,1,0,0,0,0,0,0,0,0},
    {1,2,1,0,0,0,0,0,0,0,0,0},
    {1,0,0,0,0,0,0,0,0,0,0,0},
};

static void draw_cursor(int32_t x, int32_t y) {
    for (int r=0;r<12;r++) for(int c=0;c<12;c++) {
        if      (g_cur_shape[r][c]==1) bb_pix(x+c,y+r,COL_BLACK);
        else if (g_cur_shape[r][c]==2) bb_pix(x+c,y+r,COL_WHITE);
    }
}

// ============================================================
// Main GUI loop  – FIXED: no flicker, cursor always on top
// ============================================================
void gui_run(void) {
    // Init g_fb from new VBE driver
    extern uint32_t *vbe_get_fb(void);
    extern uint32_t vbe_get_width(void);
    extern uint32_t vbe_get_height(void);
    extern uint32_t vbe_get_pitch(void);
    extern uint32_t vbe_get_bpp(void);
    g_fb.addr   = (uint32_t)vbe_get_fb();
    g_fb.width  = (uint16_t)vbe_get_width();
    g_fb.height = (uint16_t)vbe_get_height();
    g_fb.pitch  = (uint16_t)vbe_get_pitch();
    g_fb.bpp    = (uint8_t)vbe_get_bpp();
    g_fb.active = (g_fb.addr != 0) ? 1 : 0;
    kstate.fb_active = g_fb.active;

    if (!g_fb.active) {
        vga_puts("GUI: no framebuffer\n");
        return;
    }

    // Apply default theme before first frame, then load persisted settings
    apply_theme(THEME_MODERN);
    settings_load();

    // Open terminal on startup
    open_window(APP_TERMINAL);

    for (;;) {
        gui_pump();
    }
}

// ============================================================
// gui_pump — run one GUI frame; callable from sys_read while
// an app is blocking on stdin so the desktop stays responsive.
// ============================================================
void gui_pump(void) {
    static uint8_t prev_btn  = 0;
    /* g_dragging: now file-scope, see declaration above gui_pump */
    static int32_t drag_idx  = -1;
    static int32_t drag_ox   = 0;
    static int32_t drag_oy   = 0;
    static uint8_t prev_rbtn = 0;
    // gui_pump runs in TASK_GUI only — no re-entrancy issues with the scheduler.

        // Drain any received network frames / retry pending ARP requests.
        // Cheap no-op if no NIC was found at boot.
        net_poll();

        // Sync mouse FIRST so mx/my reflect the current frame's position,
        // not last frame's.  This eliminates the 32ms click-position lag that
        // caused close/title hits to register at a stale cursor location.
        sync_mouse();

        int32_t mx = g_mouse.x;
        int32_t my = g_mouse.y;
        uint8_t btn  = g_mouse.buttons & 0x01;
        uint8_t rbtn = (g_mouse.buttons >> 1) & 0x01;
        uint8_t click   = (btn && !prev_btn);
        uint8_t release = (!btn && prev_btn);
        uint8_t rclick  = (rbtn && !prev_rbtn);

        // ---- Drag update ----
        // Snapshot SDK window content-area origins BEFORE moving them.
        // After sched_yield() the app will have drawn at these coords;
        // we blit those pixels to the new position after the yield.
        if (g_dragging) {
            int32_t _snap_i = 0;
            for (int32_t _k = 0; _k < g_nwins && _snap_i < GUI_SDK_MAX_WINS; _k++) {
                Window *_wk = &g_wins[_k];
                if (!(_wk->flags & WIN_FLAG_VISIBLE)) continue;
                if (_wk->app != APP_SDK) continue;
                g_sdk_prev_cx[_snap_i] = win_ca_x(_wk);
                g_sdk_prev_cy[_snap_i] = win_ca_y(_wk);
                _snap_i++;
            }
        }
        if (g_dragging && btn && drag_idx >= 0 && drag_idx < g_nwins) {
            Window *dw = &g_wins[drag_idx];
            dw->x = mx - drag_ox;
            dw->y = my - drag_oy;
            if(dw->x<0)dw->x=0;
            if(dw->y < DESKTOP_TOP_FOR(g_theme)) dw->y = DESKTOP_TOP_FOR(g_theme);
            if(dw->x+dw->w>SCREEN_W)   dw->x=SCREEN_W-dw->w;
            if(dw->y+dw->h>SCREEN_H) dw->y=SCREEN_H-dw->h;
        }
        if (release) { g_dragging=0; drag_idx=-1; }

        // ---- Click handling ----
        if (click) {
            // Close start menu if click outside it
            if (g_startmenu) {
                int32_t sm_y_rt = (g_theme == THEME_CLASSIC)
                                ? (SCREEN_H - TASKBAR_H - SM_H)
                                : TASKBAR_H;
                if (g_menu_hover >= 0 && g_menu_items[g_menu_hover].app != APP_NONE) {
                    open_window(g_menu_items[g_menu_hover].app);
                    g_startmenu = 0;
                } else if (!pt_in(mx,my,SM_X,sm_y_rt,SM_TOT_W,SM_H) &&
                           !pt_in(mx,my,2,TASKBAR_Y_FOR(g_theme)+2,70,TASKBAR_H-4)) {
                    g_startmenu = 0;
                }
                goto after_click;
            }

            // Taskbar handled inside draw_taskbar above (we pass click through)

            // App windows (front to back)
            for (int32_t i = g_nwins-1; i >= 0; i--) {
                Window *w = &g_wins[i];
                if (!(w->flags & WIN_FLAG_VISIBLE)) continue;
                if (!hit_win(w,mx,my)) continue;

                // Save index before bring_front reorders
                bring_front(i);
                Window *fw = &g_wins[g_front];

                if (hit_close(fw,mx,my)) {
                    // For SDK windows, signal the app to exit cleanly (same
                    // as its own Close button) instead of forcibly destroying
                    // the window — that would leave TASK_APP frozen forever.
                    if (fw->app == APP_SDK)
                        gui_sdk_request_close((int32_t)fw->id);
                    else
                        close_window(g_front);
                    goto after_click;
                }
                if (hit_title(fw,mx,my)) {
                    g_dragging=1; drag_idx=g_front;
                    drag_ox=mx-fw->x; drag_oy=my-fw->y;
                    goto after_click;
                }
                goto after_click;
            }

            // Desktop icons
            for (int32_t i=0;i<N_ICONS;i++) {
                DesktopIcon *ic=&g_icons[i];
                if(pt_in(mx,my,ic->x,ic->y,32,44)){
                    for(int j=0;j<N_ICONS;j++) g_icons[j].selected=0;
                    ic->selected=1;
                    uint32_t now=get_ticks();
                    if(g_last_click_icon==i && now-g_last_click_tick<DBLCLICK_MS){
                        open_window(ic->app);
                        ic->selected=0; g_last_click_icon=-1;
                    } else { g_last_click_icon=i; g_last_click_tick=now; }
                    goto after_click;
                }
            }
            for(int32_t i=0;i<N_ICONS;i++) g_icons[i].selected=0;
            after_click:;
        }

        // ---- Right-click: open context menu on desktop ----
        if (rclick) {
            // Only open on desktop (not on a window or taskbar)
            int32_t desk_top2 = DESKTOP_TOP_FOR(g_theme);
            int32_t taskbar_y2 = TASKBAR_Y_FOR(g_theme);
            uint8_t on_win = 0;
            for (int32_t i = g_nwins-1; i >= 0; i--) {
                if ((g_wins[i].flags & WIN_FLAG_VISIBLE) && hit_win(&g_wins[i],mx,my)) {
                    on_win = 1; break;
                }
            }
            uint8_t on_taskbar = pt_in(mx,my,0,taskbar_y2,SCREEN_W,TASKBAR_H);
            if (!on_win && !on_taskbar && my >= desk_top2) {
                g_ctx_open = CTX_DESKTOP;
                g_ctx_x = mx;
                g_ctx_y = my;
            }
        }

        // ---- Handle left-click on open context menu ----
        if (click && g_ctx_open) {
            int item = handle_context_menu_click(mx, my);
            if (item == CTX_ITEM_WALLPAPER) {
                g_wallpaper = (g_wallpaper + 1) % WALLPAPER_COUNT;
                settings_save();
            } else if (item == CTX_ITEM_THEME) {
                apply_theme((g_theme + 1) % THEME_COUNT);
                settings_save();
            }
            goto after_rclick;
        }
        after_rclick:;

        // ============================================================
        // RENDER  – draw to back buffer, then blit once (no flicker)
        // ============================================================

        // 1. Desktop background — theme + wallpaper aware
        int32_t desk_top = DESKTOP_TOP_FOR(g_theme);
        int32_t desk_h   = SCREEN_H - TASKBAR_H;
        draw_desktop_bg(desk_top, desk_h);

        // 2. Desktop icons
        for (int32_t i=0;i<N_ICONS;i++) draw_icon(&g_icons[i]);

        // 3. App windows back-to-front
        for (int32_t i=0; i<g_nwins; i++) {
            Window *w=&g_wins[i];
            if(!(w->flags & WIN_FLAG_VISIBLE)) continue;
            uint8_t active=(i==g_front);
            draw_chrome(w, active);
            // Only pass click to frontmost window
            render_app(w, mx, my, click && active, active);
        }

        // 3b. If a SDK app is running in TASK_APP, yield to it here so it can
        //     draw its content into g_bb on top of the chrome we just drew.
        //     It will sched_yield() back to us when done (via SYS_GUI_PUMP).
        //     We ALWAYS yield, even while dragging — skipping the yield would
        //     leave TASK_APP with no valid TASK_GUI context to return to when
        //     the app calls its own gui_pump()→sched_yield(), causing a crash.
        //     Instead we let the app draw freely, then blit its pixels from the
        //     old coords to the new coords within g_bb, so content tracks the
        //     chrome exactly with no white flash.
        if (g_sdk_app_active) {
            // Update prev_btn BEFORE yielding so the next frame computes
            // click/release correctly even when we exit mid-frame here.
            prev_btn  = btn;
            prev_rbtn = rbtn;
            sched_yield();
            // If we were dragging, the app drew at its pre-drag coords (old).
            // Blit those pixels from old position → new position within g_bb
            // so the content follows the chrome exactly, with no white flash.
            if (g_dragging) {
                int32_t _si = 0;
                for (int32_t _k = 0; _k < g_nwins && _si < GUI_SDK_MAX_WINS; _k++) {
                    Window *_sw = &g_wins[_k];
                    if (!(_sw->flags & WIN_FLAG_VISIBLE)) { continue; }
                    if (_sw->app != APP_SDK) continue;
                    int32_t _new_cx = win_ca_x(_sw), _new_cy = win_ca_y(_sw);
                    int32_t _old_cx = g_sdk_prev_cx[_si], _old_cy = g_sdk_prev_cy[_si];
                    int32_t _cw     = win_ca_w(_sw),       _ch    = win_ca_h(_sw);
                    _si++;
                    if (_new_cx == _old_cx && _new_cy == _old_cy) continue; // no move
                    // Blit scanline by scanline: old row → new row inside g_bb.
                    // Work bottom-up when moving down to avoid overlap corruption.
                    int32_t _dy = _new_cy - _old_cy;
                    int32_t _dx = _new_cx - _old_cx;
                    if (_dy >= 0) {
                        for (int32_t _row = _ch-1; _row >= 0; _row--) {
                            int32_t _src_y = _old_cy + _row;
                            int32_t _dst_y = _new_cy + _row;
                            if (_src_y < 0 || _src_y >= BB_H) continue;
                            if (_dst_y < 0 || _dst_y >= BB_H) continue;
                            uint32_t *_src = &g_bb[_src_y * BB_W + _old_cx];
                            uint32_t *_dst = &g_bb[_dst_y * BB_W + _new_cx];
                            int32_t _cols = _cw;
                            if (_old_cx < 0) { _src -= _old_cx; _dst -= _old_cx; _cols += _old_cx; }
                            if (_new_cx + _cols > BB_W) _cols = BB_W - _new_cx;
                            if (_new_cx < 0 || _old_cx < 0) continue;
                            if (_cols <= 0) continue;
                            kmemcpy(_dst, _src, (uint32_t)_cols * 4);
                        }
                    } else {
                        for (int32_t _row = 0; _row < _ch; _row++) {
                            int32_t _src_y = _old_cy + _row;
                            int32_t _dst_y = _new_cy + _row;
                            if (_src_y < 0 || _src_y >= BB_H) continue;
                            if (_dst_y < 0 || _dst_y >= BB_H) continue;
                            uint32_t *_src = &g_bb[_src_y * BB_W + _old_cx];
                            uint32_t *_dst = &g_bb[_dst_y * BB_W + _new_cx];
                            int32_t _cols = _cw;
                            if (_new_cx + _cols > BB_W) _cols = BB_W - _new_cx;
                            if (_new_cx < 0 || _old_cx < 0) continue;
                            if (_cols <= 0) continue;
                            kmemcpy(_dst, _src, (uint32_t)_cols * 4);
                        }
                    }
                    (void)_dx; // used implicitly via _old_cx offset above
                    // Erase the ghost at the old position by re-painting those
                    // desktop background rows directly into g_bb.
                    {
                        int32_t _dt = DESKTOP_TOP_FOR(g_theme);
                        int32_t _dh = SCREEN_H - _dt - ((g_theme==THEME_CLASSIC)?TASKBAR_H:0);
                        for (int32_t _gr = _old_cy; _gr < _old_cy + _ch; _gr++) {
                            if (_gr < 0 || _gr >= BB_H) continue;
                            uint32_t _bg;
                            if (g_wallpaper != WALLPAPER_GRADIENT) {
                                _bg = g_wallpaper_colors[g_wallpaper];
                            } else {
                                int32_t _rel = _gr - _dt;
                                if (_rel < 0) _rel = 0;
                                if (_rel >= _dh) _rel = _dh - 1;
                                uint32_t _t = (uint32_t)_rel * 255 / (_dh > 0 ? (uint32_t)_dh : 1);
                                uint32_t _r2, _g2, _b2;
                                if (g_theme == THEME_MIDNIGHT) {
                                    _r2=4+_t*8/255; _g2=8+_t*18/255; _b2=30+_t*50/255;
                                } else if (g_theme == THEME_SUNSET) {
                                    _r2=40+_t*120/255; _g2=5+_t*40/255;
                                    _b2=(int32_t)(50-_t*40/255)<5?5:(50-_t*40/255);
                                } else {
                                    _r2=20+_t*10/255; if(_r2>50)_r2=50;
                                    _g2=60+_t*30/255; if(_g2>100)_g2=100;
                                    _b2=120+_t*40/255; if(_b2>180)_b2=180;
                                }
                                _bg = RGB(_r2, _g2, _b2);
                            }
                            int32_t _x0 = _old_cx, _x1 = _old_cx + _cw;
                            if (_x0 < 0) _x0 = 0;
                            if (_x1 > BB_W) _x1 = BB_W;
                            for (int32_t _gx = _x0; _gx < _x1; _gx++)
                                g_bb[_gr * BB_W + _gx] = _bg;
                        }
                    }
                }
            }
        }

        // 4. Taskbar (drawn after windows so it's always on top)
        draw_taskbar(mx, my, click);

        // 5. Start menu (on top of taskbar)
        if (g_startmenu) draw_startmenu(mx, my);

        // 5b. Right-click context menu
        if (g_ctx_open) draw_context_menu(mx, my);

        // 6. Cursor LAST – always on top of everything
        draw_cursor(mx, my);

        // 7. Blit back buffer → real framebuffer  (single write, zero flicker)
    bb_present();

        prev_btn  = btn;
        prev_rbtn = rbtn;

        // ~30fps (32ms). Faster hurts older hardware, slower feels laggy.
        sleep_ms(32);
}

// =============================================================================
// GUI SDK accessors — append these to the bottom of gui_desktop.c
// =============================================================================

// Open a new SDK window with an arbitrary title and size.
// Returns the index in g_wins[] or -1 on failure.
int32_t gui_desktop_sdk_open(const char *title, int32_t x, int32_t y,
                              uint32_t w, uint32_t h) {
    if (g_nwins >= MAX_WINDOWS) return -1;

    Window *win = &g_wins[g_nwins];
    kmemset(win, 0, sizeof(Window));
    win->flags = WIN_FLAG_VISIBLE | WIN_FLAG_MOVEABLE | WIN_FLAG_CLOSEABLE;
    win->app   = APP_SDK;
    win->id    = g_next_win_id++;

    // Copy title (max 39 chars)
    int ti = 0;
    while (title && title[ti] && ti < 39) { win->title[ti] = title[ti]; ti++; }
    win->title[ti] = 0;

    // Size
    win->w = (int32_t)(w ? w : 320);
    win->h = (int32_t)(h ? h : 240);

    // Position: use requested coords or auto-cascade
    int32_t desk_top = DESKTOP_TOP_FOR(g_theme);
    if (x < 0) {
        int32_t off = g_nwins * 22;
        win->x = 60 + (off % 300);
        win->y = desk_top + 10 + (off % 180);
    } else {
        win->x = x;
        win->y = (y < desk_top) ? desk_top : y;
    }
    // Clamp to screen
    if (win->x + win->w > SCREEN_W - 4) win->x = SCREEN_W - win->w - 4;
    if (win->y + win->h > SCREEN_H - 4) win->y = desk_top + 4;
    if (win->x < 0) win->x = 0;

    uint32_t new_id = win->id;   /* capture before bring_front can shuffle */
    g_nwins++;
    bring_front(g_nwins - 1);
    return (int32_t)new_id;
}

// Close (remove) the SDK window at index win_idx.
void gui_desktop_sdk_close(int32_t win_id) {
    int32_t idx = win_idx_by_id((uint32_t)win_id);
    if (idx >= 0) close_window(idx);
}

// Update the title of the window at win_idx.
void gui_desktop_sdk_set_title(int32_t win_id, const char *title) {
    int32_t win_idx = win_idx_by_id((uint32_t)win_id);
    if (win_idx < 0) return;
    int ti = 0;
    while (title && title[ti] && ti < 39) { g_wins[win_idx].title[ti] = title[ti]; ti++; }
    g_wins[win_idx].title[ti] = 0;
}

// Return the client area (inside chrome) of the window at win_idx.
void gui_desktop_sdk_getrect(int32_t win_id,
                              int32_t *ox, int32_t *oy,
                              int32_t *ow, int32_t *oh) {
    int32_t win_idx = win_idx_by_id((uint32_t)win_id);
    if (win_idx < 0) { *ox = *oy = 0; *ow = *oh = 0; return; }
    Window *w = &g_wins[win_idx];
    *ox = win_ca_x(w);
    *oy = win_ca_y(w);
    *ow = win_ca_w(w);
    *oh = win_ca_h(w);
}
