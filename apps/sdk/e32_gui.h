// =============================================================================
// Eclipse32 GUI SDK — apps/sdk/e32_gui.h
// Include in your .E32 app to draw GUI windows.
// =============================================================================
#pragma once
#include <stdint.h>
#include "e32_syscall.h"

#define SYS_GUI_WIN_CREATE   20
#define SYS_GUI_WIN_CLOSE    21
#define SYS_GUI_WIN_SETTITLE 22
#define SYS_GUI_FILL_RECT    23
#define SYS_GUI_DRAW_HLINE   24
#define SYS_GUI_DRAW_VLINE   25
#define SYS_GUI_PUTC         26
#define SYS_GUI_PUTS         27
#define SYS_GUI_BUTTON       28
#define SYS_GUI_GET_MOUSE    29
#define SYS_GUI_POLL_EVENT   30
#define SYS_GUI_PUMP         31
#define SYS_GUI_DRAW_RECT    32
#define SYS_GUI_DRAW_3DBOX   33
#define SYS_GUI_SCREEN_SIZE  34
#define SYS_GUI_WIN_GETRECT  35

#define RGB(r,g,b) (((uint32_t)(r)<<16)|((uint32_t)(g)<<8)|(uint32_t)(b))
#define COL_BLACK   RGB(0,0,0)
#define COL_WHITE   RGB(255,255,255)
#define COL_LTGREY  RGB(224,224,224)
#define COL_DKGREY  RGB(96,96,96)
#define COL_RED     RGB(200,40,40)
#define COL_GREEN   RGB(40,180,40)
#define COL_BLUE    RGB(30,100,210)
#define COL_YELLOW  RGB(210,180,0)
#define COL_WIN_BG  RGB(236,233,216)

typedef struct { int32_t x,y,w,h; const char *label; int32_t mx,my; uint8_t clicked; } gui_btn_args_t;
typedef struct { int32_t x,y; uint8_t buttons; } gui_mouse_t;
typedef struct { uint32_t type; int32_t mouse_x,mouse_y; uint8_t mouse_btn,key_ascii; } gui_event_t;
typedef struct { int32_t x,y,w,h; } gui_rect_t;

#define GUI_EVENT_NONE    0
#define GUI_EVENT_PAINT   1
#define GUI_EVENT_MOUSE   2
#define GUI_EVENT_KEYDOWN 3
#define GUI_EVENT_CLOSE   4

// Convenience macro for 6-register syscall
#define _GUI_SYSCALL6(nr,a,b,c,d,e) ({ int _r; \
    __asm__ volatile("int $0x80" : "=a"(_r) \
    : "a"(nr),"b"(a),"c"(b),"d"(c),"S"(d),"D"(e) : "memory"); _r; })

static inline int gui_win_create(const char *title, int32_t x, int32_t y, uint32_t w, uint32_t h) {
    return _GUI_SYSCALL6(SYS_GUI_WIN_CREATE,(int)title,(int)x,(int)y,(int)w,(int)h);
}
static inline void gui_win_close(int h) {
    e32_syscall5(SYS_GUI_WIN_CLOSE,h,0,0,0,0);
}
static inline void gui_win_set_title(int h, const char *t) {
    e32_syscall5(SYS_GUI_WIN_SETTITLE,h,(int)t,0,0,0);
}
static inline void gui_win_getrect(int h, gui_rect_t *r) {
    e32_syscall5(SYS_GUI_WIN_GETRECT,h,(int)r,0,0,0);
}
static inline void gui_fill_rect(int32_t x,int32_t y,int32_t w,int32_t h,uint32_t col) {
    _GUI_SYSCALL6(SYS_GUI_FILL_RECT,(int)x,(int)y,(int)w,(int)h,(int)col);
}
static inline void gui_draw_hline(int32_t x,int32_t y,int32_t len,uint32_t col) {
    _GUI_SYSCALL6(SYS_GUI_DRAW_HLINE,(int)x,(int)y,(int)len,(int)col,0);
}
static inline void gui_draw_vline(int32_t x,int32_t y,int32_t len,uint32_t col) {
    _GUI_SYSCALL6(SYS_GUI_DRAW_VLINE,(int)x,(int)y,(int)len,(int)col,0);
}
static inline void gui_draw_rect(int32_t x,int32_t y,int32_t w,int32_t h,uint32_t col) {
    _GUI_SYSCALL6(SYS_GUI_DRAW_RECT,(int)x,(int)y,(int)w,(int)h,(int)col);
}
static inline void gui_draw_3dbox(int32_t x,int32_t y,int32_t w,int32_t h,uint8_t raised) {
    _GUI_SYSCALL6(SYS_GUI_DRAW_3DBOX,(int)x,(int)y,(int)w,(int)h,(int)raised);
}
static inline void gui_putc(int32_t x,int32_t y,char c,uint32_t fg,uint32_t bg) {
    _GUI_SYSCALL6(SYS_GUI_PUTC,(int)x,(int)y,(int)(uint8_t)c,(int)fg,(int)bg);
}
static inline void gui_puts(int32_t x,int32_t y,const char *s,uint32_t fg,uint32_t bg) {
    _GUI_SYSCALL6(SYS_GUI_PUTS,(int)x,(int)y,(int)s,(int)fg,(int)bg);
}
static inline int gui_button(int32_t x,int32_t y,int32_t w,int32_t h,
                              const char *label,int32_t mx,int32_t my,uint8_t clicked) {
    gui_btn_args_t a={x,y,w,h,label,mx,my,clicked};
    return e32_syscall5(SYS_GUI_BUTTON,(int)&a,0,0,0,0);
}
static inline void gui_get_mouse(gui_mouse_t *m) {
    e32_syscall5(SYS_GUI_GET_MOUSE,(int)m,0,0,0,0);
}
static inline int gui_poll_event(gui_event_t *ev) {
    return e32_syscall5(SYS_GUI_POLL_EVENT,(int)ev,0,0,0,0);
}
static inline int gui_pump(void) {
    return e32_syscall5(SYS_GUI_PUMP,0,0,0,0,0);
}
static inline void gui_screen_size(uint32_t *w,uint32_t *h) {
    uint32_t buf[2]={0,0};
    e32_syscall5(SYS_GUI_SCREEN_SIZE,(int)buf,0,0,0,0);
    if(w)*w=buf[0]; if(h)*h=buf[1];
}
static inline void gui_clear(const gui_rect_t *ca,uint32_t col) {
    _GUI_SYSCALL6(SYS_GUI_FILL_RECT,(int)ca->x,(int)ca->y,(int)ca->w,(int)ca->h,(int)col);
}

// =============================================================================
// Extended GUI SDK — Widgets: input, checkbox, slider, msgbox, ticks
// =============================================================================

// New syscall numbers (36-45)
#define SYS_GUI_INPUT        36   // gui_input_t widget draw+feed
#define SYS_GUI_CHECKBOX     37   // gui_checkbox widget
#define SYS_GUI_SLIDER       38   // gui_slider widget
#define SYS_GUI_MSGBOX       39   // blocking modal dialog
#define SYS_GUI_GET_TICKS    40   // get system tick count
#define SYS_GUI_FILL_CIRCLE  41   // filled circle
#define SYS_GUI_DRAW_CIRCLE  42   // circle outline
#define SYS_GUI_DRAW_IMAGE   43   // blit raw pixel buffer
#define SYS_GUI_FILEPICK     44   // blocking file picker dialog

// ---------------------------------------------------------------------------
// gui_input_t  — text input field widget state
//   Allocate one per field in your app. Zero-initialise before first use.
//   Call gui_input_draw() every frame, pass key from gui_get_last_key().
// ---------------------------------------------------------------------------
#define GUI_INPUT_MAXLEN  127

typedef struct {
    char     buf[GUI_INPUT_MAXLEN + 1]; // text content
    uint8_t  focused;                   // 1 = has keyboard focus
    uint8_t  masked;                    // 1 = show * instead of chars
    int32_t  maxlen;                    // max chars (0 = GUI_INPUT_MAXLEN)
} gui_input_t;

// Argument struct passed by pointer for SYS_GUI_INPUT
typedef struct {
    int32_t      x, y, w, h;
    gui_input_t *inp;
    int32_t      mx, my;
    uint8_t      clicked;
    char         key;      // ascii key to feed (0 = none)
} gui_input_args_t;

// ---------------------------------------------------------------------------
// gui_checkbox  — simple toggle widget
// ---------------------------------------------------------------------------
typedef struct {
    int32_t      x, y;
    const char  *label;
    uint8_t     *checked;   // pointer to your state variable
    int32_t      mx, my;
    uint8_t      clicked;
} gui_checkbox_args_t;

// ---------------------------------------------------------------------------
// gui_slider  — horizontal slider
// ---------------------------------------------------------------------------
typedef struct {
    int32_t  x, y, w;
    int32_t  min_val, max_val;
    int32_t *value;          // pointer to your state variable
    int32_t  mx, my;
    uint8_t  buttons;        // mouse buttons bitmask
} gui_slider_args_t;

// ---------------------------------------------------------------------------
// gui_msgbox  — modal OK/Cancel dialog
// Returns: 1 = OK, 0 = Cancel
// ---------------------------------------------------------------------------
typedef struct {
    const char *title;
    const char *message;
    uint8_t     has_cancel;  // 1 = show Cancel button too
} gui_msgbox_args_t;

// ---------------------------------------------------------------------------
// gui_draw_image  — blit a raw ARGB pixel buffer
// ---------------------------------------------------------------------------
typedef struct {
    int32_t        x, y, w, h;
    const uint32_t *pixels;   // w*h ARGB pixels
} gui_image_args_t;

// ---------------------------------------------------------------------------
// Inline API
// ---------------------------------------------------------------------------

// Draw + handle a text input field. Pass key=0 if no key this frame.
// Returns 1 if Enter was pressed.
static inline int gui_input_draw(int32_t x, int32_t y, int32_t w, int32_t h,
                                  gui_input_t *inp,
                                  int32_t mx, int32_t my, uint8_t clicked, char key) {
    gui_input_args_t a = {x, y, w, h, inp, mx, my, clicked, key};
    return e32_syscall5(SYS_GUI_INPUT, (int)&a, 0, 0, 0, 0);
}

// Draw a checkbox. Returns 1 if state was toggled this click.
static inline int gui_checkbox(int32_t x, int32_t y, const char *label,
                                uint8_t *checked,
                                int32_t mx, int32_t my, uint8_t clicked) {
    gui_checkbox_args_t a = {x, y, label, checked, mx, my, clicked};
    return e32_syscall5(SYS_GUI_CHECKBOX, (int)&a, 0, 0, 0, 0);
}

// Draw a horizontal slider. Returns 1 if value changed.
static inline int gui_slider(int32_t x, int32_t y, int32_t w,
                              int32_t min_val, int32_t max_val, int32_t *value,
                              int32_t mx, int32_t my, uint8_t buttons) {
    gui_slider_args_t a = {x, y, w, min_val, max_val, value, mx, my, buttons};
    return e32_syscall5(SYS_GUI_SLIDER, (int)&a, 0, 0, 0, 0);
}

// Show a modal message box. Blocks until user clicks OK or Cancel.
// Returns 1 for OK, 0 for Cancel.
static inline int gui_msgbox(const char *title, const char *message, uint8_t has_cancel) {
    gui_msgbox_args_t a = {title, message, has_cancel};
    return e32_syscall5(SYS_GUI_MSGBOX, (int)&a, 0, 0, 0, 0);
}

// Get the system tick counter (milliseconds at 1000 Hz).
static inline uint32_t gui_get_ticks(void) {
    return (uint32_t)e32_syscall5(SYS_GUI_GET_TICKS, 0, 0, 0, 0, 0);
}

// Draw a filled circle.
static inline void gui_fill_circle(int32_t cx, int32_t cy, int32_t r, uint32_t col) {
    _GUI_SYSCALL6(SYS_GUI_FILL_CIRCLE, (int)cx, (int)cy, (int)r, (int)col, 0);
}

// Draw a circle outline.
static inline void gui_draw_circle(int32_t cx, int32_t cy, int32_t r, uint32_t col) {
    _GUI_SYSCALL6(SYS_GUI_DRAW_CIRCLE, (int)cx, (int)cy, (int)r, (int)col, 0);
}

// Blit a raw ARGB pixel buffer at (x,y).
static inline void gui_draw_image(int32_t x, int32_t y, int32_t w, int32_t h,
                                   const uint32_t *pixels) {
    gui_image_args_t a = {x, y, w, h, pixels};
    e32_syscall5(SYS_GUI_DRAW_IMAGE, (int)&a, 0, 0, 0, 0);
}

// Convenience: get last typed key without blocking (0 = nothing).
// Uses the mouse event struct trick — just poll_event for KEYDOWN.
static inline char gui_get_last_key(void) {
    gui_event_t ev;
    if (gui_poll_event(&ev) == GUI_EVENT_KEYDOWN) return (char)ev.key_ascii;
    return 0;
}

// ---------------------------------------------------------------------------
// gui_filepick — blocking file picker modal dialog
//
//   filter : extension to show, e.g. ".E32" or ".TXT" (NULL = all files)
//   title  : dialog title (NULL = "Open File")
//   out_buf: buffer to receive selected filename
//   buf_len: size of out_buf
//   Returns 1 if user picked a file, 0 if cancelled.
//
// Example:
//   char path[64];
//   if (gui_filepick(path, sizeof(path), ".E32", "Pick an App"))
//       // path now contains "MYPROG.E32"
// ---------------------------------------------------------------------------
typedef struct {
    char    *out_buf;
    int32_t  buf_len;
    char    *filter;
    char    *title;
} gui_filepick_args_t;

static inline int32_t gui_filepick(char *out_buf, int32_t buf_len,
                                    const char *filter, const char *title) {
    gui_filepick_args_t a;
    a.out_buf = out_buf;
    a.buf_len = buf_len;
    a.filter  = (char *)filter;
    a.title   = (char *)title;
    return e32_syscall5(SYS_GUI_FILEPICK, (int)&a, 0, 0, 0, 0);
}
