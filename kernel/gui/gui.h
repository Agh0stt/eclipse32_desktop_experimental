// =============================================================================
// Eclipse32 - EclipseGUI Window Manager Header
// Lightweight compositing WM for 1024x768x32 VBE framebuffer
// =============================================================================
#pragma once
#include "../kernel.h"

// ---- GUI Event Types --------------------------------------------------------

typedef enum {
    GUI_EVENT_NONE = 0,
    GUI_EVENT_MOUSE_MOVE,
    GUI_EVENT_MOUSE_DOWN,
    GUI_EVENT_MOUSE_UP,
    GUI_EVENT_KEY_DOWN,
    GUI_EVENT_KEY_UP,
    GUI_EVENT_PAINT,
    GUI_EVENT_CLOSE,
    GUI_EVENT_RESIZE,
    GUI_EVENT_FOCUS_GAINED,
    GUI_EVENT_FOCUS_LOST,
} gui_event_type_t;

typedef struct {
    gui_event_type_t type;
    int32_t  mouse_x;
    int32_t  mouse_y;
    uint8_t  mouse_btn;      // bitmask: bit0=left, bit1=right
    char     key_ascii;
    uint32_t key_code;
    bool     key_shift;
    bool     key_ctrl;
    uint32_t window_id;
} gui_event_t;

// ---- Drawing Context --------------------------------------------------------

typedef struct {
    uint32_t *fb;            // window's back-buffer (ARGB)
    uint32_t  width;
    uint32_t  height;
    uint32_t  clip_x, clip_y, clip_w, clip_h;
} gui_dc_t;

// ---- Window -----------------------------------------------------------------

#define GUI_WINDOW_MAX         16
#define GUI_TITLEBAR_H         28
#define GUI_BORDER             2
#define GUI_CLOSE_BTN_W        20
#define GUI_MIN_WIN_W          120
#define GUI_MIN_WIN_H          80

#define GUI_STYLE_DECORATED    0x01   // has title bar + border
#define GUI_STYLE_RESIZABLE    0x02
#define GUI_STYLE_TOPMOST      0x04
#define GUI_STYLE_NO_FOCUS     0x08

typedef void (*gui_paint_fn_t)(gui_dc_t *dc, uint32_t win_id, void *userdata);
typedef void (*gui_event_fn_t)(gui_event_t *ev, void *userdata);

typedef struct {
    bool      used;
    uint32_t  id;
    int32_t   x, y;          // screen position (content area)
    uint32_t  w, h;          // content area dimensions
    char      title[64];
    uint32_t  style;

    uint32_t *backbuf;        // pixel buffer (w*h)
    bool      dirty;

    // drag state
    bool      dragging;
    int32_t   drag_off_x, drag_off_y;

    // callbacks
    gui_paint_fn_t  on_paint;
    gui_event_fn_t  on_event;
    void           *userdata;
} gui_window_t;

// ---- Desktop ----------------------------------------------------------------

typedef struct {
    uint32_t  bg_color;
    char      wallpaper_text[64];   // simple text on desktop
} gui_desktop_t;

// ---- Public API -------------------------------------------------------------

void       gui_init(void);
void       gui_run(void);              // main event loop (never returns)
void       gui_dispatch_once(void);    // process one frame (for co-op multitasking)

// Window management
uint32_t   gui_window_create(int32_t x, int32_t y, uint32_t w, uint32_t h,
                              const char *title, uint32_t style,
                              gui_paint_fn_t on_paint, gui_event_fn_t on_event,
                              void *userdata);
void       gui_window_destroy(uint32_t id);
void       gui_window_invalidate(uint32_t id);   // mark for repaint
void       gui_window_set_title(uint32_t id, const char *title);
gui_window_t *gui_window_get(uint32_t id);

// Drawing primitives (call inside on_paint callback)
void gui_dc_clear(gui_dc_t *dc, uint32_t color);
void gui_dc_fill_rect(gui_dc_t *dc, int32_t x, int32_t y, uint32_t w, uint32_t h, uint32_t color);
void gui_dc_draw_rect(gui_dc_t *dc, int32_t x, int32_t y, uint32_t w, uint32_t h, uint32_t color);
void gui_dc_draw_text(gui_dc_t *dc, int32_t x, int32_t y, const char *str, uint32_t fg, uint32_t bg);
void gui_dc_draw_char(gui_dc_t *dc, int32_t x, int32_t y, char c, uint32_t fg, uint32_t bg);
void gui_dc_draw_line(gui_dc_t *dc, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color);
void gui_dc_fill_circle(gui_dc_t *dc, int32_t cx, int32_t cy, int32_t r, uint32_t color);
void gui_dc_draw_image(gui_dc_t *dc, int32_t x, int32_t y, uint32_t w, uint32_t h, const uint32_t *pixels);

// Taskbar / system
void gui_taskbar_register(uint32_t win_id);

// Color helpers
static inline uint32_t gui_rgb(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}
static inline uint32_t gui_blend(uint32_t a, uint32_t b, uint8_t alpha) {
    uint32_t ar = (a >> 16) & 0xFF, ag = (a >> 8) & 0xFF, ab = a & 0xFF;
    uint32_t br = (b >> 16) & 0xFF, bg = (b >> 8) & 0xFF, bb = b & 0xFF;
    uint32_t t = alpha;
    uint32_t nt = 255 - t;
    return gui_rgb((ar*t + br*nt) >> 8, (ag*t + bg*nt) >> 8, (ab*t + bb*nt) >> 8);
}

// Eclipse32 GUI palette
#define GUI_COLOR_DESKTOP       0x001A2332   // dark blue-grey desktop
#define GUI_COLOR_TITLEBAR_ACT  0x0058A6FF   // active title (eclipse blue)
#define GUI_COLOR_TITLEBAR_INACT 0x00404858  // inactive title
#define GUI_COLOR_WIN_BG        0x00F0F0F0   // window background
#define GUI_COLOR_WIN_BORDER    0x00303848   // window border
#define GUI_COLOR_CLOSE_BTN     0x00F85149   // close button red
#define GUI_COLOR_CLOSE_HOV     0x00FF6B6A   // hover
#define GUI_COLOR_TEXT_DARK     0x00111111
#define GUI_COLOR_TEXT_LIGHT    0x00F0F0F0
#define GUI_COLOR_ACCENT        0x0058A6FF
#define GUI_COLOR_TASKBAR       0x00141D26
#define GUI_COLOR_TASKBAR_BTN   0x00243040
#define GUI_COLOR_TASKBAR_ACT   0x003060A0
#define GUI_COLOR_BUTTON        0x00D0D8E0
#define GUI_COLOR_BUTTON_HOV    0x00B8C8D8
#define GUI_COLOR_BUTTON_PRESS  0x008BAAD0
#define GUI_COLOR_INPUT_BG      0x00FFFFFF
#define GUI_COLOR_INPUT_BORDER  0x00A0A8B0
#define GUI_COLOR_SCROLLBAR     0x00C8D0D8
#define GUI_COLOR_SHADOW        0x22000000

#define GUI_TASKBAR_H           36
#define GUI_SCREEN_W            1024
#define GUI_SCREEN_H            768
