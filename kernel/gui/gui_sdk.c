// =============================================================================
// Eclipse32 - GUI App SDK  (kernel/gui/gui_sdk.c)
//
// Implements the GUI syscall handlers (SYS_GUI_* 20-35).
// All drawing goes through the gui_desktop.c public API:
//   gui_fill_rect / gui_draw_hline / gui_draw_vline /
//   gui_draw_3d_box / gui_putc / gui_puts / gui_button
// which write into g_bb (the 800×600 backbuffer).  bb_present()
// in gui_pump() flushes that to the real framebuffer every frame.
//
// Windows are real entries in g_wins[] — we open them via the
// existing open_window() path by adding APP_SDK slots.
// For simplicity we track the app's window by index and expose the
// client-area coordinates so the app can draw into it directly.
// =============================================================================

#include "gui_sdk.h"
#include "gui_desktop.h"
#include "../kernel.h"
#include "../syscall/syscall.h"
#include "../initramfs/initramfs.h"   // kmemset
#include "../arch/x86/pit.h"
#include "../sched/sched.h"

// ---------------------------------------------------------------------------
// Access g_wins / g_nwins / g_front from gui_desktop.c
// These are static there so we use a thin accessor approach:
// gui_desktop exposes these helpers we declare extern here.
// ---------------------------------------------------------------------------
extern void        gui_fill_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t col);
extern void        gui_draw_hline(int32_t x, int32_t y, int32_t len, uint32_t col);
extern void        gui_draw_vline(int32_t x, int32_t y, int32_t len, uint32_t col);
extern void        gui_draw_3d_box(int32_t x, int32_t y, int32_t w, int32_t h, uint8_t raised);
extern void        gui_draw_rect_border(int32_t x, int32_t y, int32_t w, int32_t h,
                                        uint32_t tl, uint32_t br);
extern void        gui_putc(int32_t x, int32_t y, char c, uint32_t fg, uint32_t bg);
extern void        gui_puts(int32_t x, int32_t y, const char *s, uint32_t fg, uint32_t bg);
extern int         gui_button(int32_t x, int32_t y, int32_t w, int32_t h,
                              const char *label, int32_t mx, int32_t my, uint8_t clicked);
extern void        gui_pump(void);

// gui_desktop_sdk_open_window / gui_desktop_sdk_close_window / gui_desktop_sdk_getrect
// are thin wrappers we add to gui_desktop.c (see patch below).
extern int32_t     gui_desktop_sdk_open(const char *title, int32_t x, int32_t y,
                                        uint32_t w, uint32_t h);
extern void        gui_desktop_sdk_close(int32_t win_idx);
extern void        gui_desktop_sdk_set_title(int32_t win_idx, const char *title);
extern void        gui_desktop_sdk_getrect(int32_t win_idx, int32_t *ox, int32_t *oy,
                                           int32_t *ow, int32_t *oh);

// Mouse state lives in gui_desktop.c / gui_compat.h
extern MouseState  g_mouse;

// ---------------------------------------------------------------------------
// Pointer validation  (flat 32-bit, no MMU)
// Reject NULL and the zero page only. Apps link at 0x0 so their rodata
// lives at low addresses — a 1MB lower bound would reject valid app pointers.
// ---------------------------------------------------------------------------
static inline int ptr_ok(const void *p) {
    uint32_t v = (uint32_t)p;
    return (v > 0x00000004u && v < 0xFF000000u);
}

// ---------------------------------------------------------------------------
// Per-app window tracking
// ---------------------------------------------------------------------------
static struct {
    uint8_t  used;
    int32_t  win_id;    // stable window id (survives bring_front shuffles)
    uint8_t  close_requested; // set by desktop when X is clicked
    // one-deep event queue
    gui_event_t ev;
    uint8_t     has_ev;
} g_sdk[GUI_SDK_MAX_WINS];

void gui_sdk_init(void) {
    kmemset(g_sdk, 0, sizeof(g_sdk));
}

// ---------------------------------------------------------------------------
// Syscall implementations
// Each has signature syscall_fn_t: (uint32_t a0..a4) → int32_t written back to eax.
// Register convention (matches e32_syscall.h __syscall6):
//   eax = syscall nr  ebx ecx edx esi edi = args a0-a4
// ---------------------------------------------------------------------------

// SYS_GUI_WIN_CREATE  a0=title a1=x a2=y a3=w a4=h
static int32_t sys_gui_win_create(uint32_t a0, uint32_t a1, uint32_t a2,
                                  uint32_t a3, uint32_t a4) {
    const char *title = (const char *)syscall_translate_app_ptr(a0, 1);
    int32_t     x     = (int32_t)a1;
    int32_t     y     = (int32_t)a2;
    uint32_t    w     = a3;
    uint32_t    h     = a4;

    if (!ptr_ok(title)) title = "App";

    // Find a free SDK slot
    int slot = -1;
    for (int i = 0; i < GUI_SDK_MAX_WINS; i++) {
        if (!g_sdk[i].used) { slot = i; break; }
    }
    if (slot < 0) return -1;

    int32_t idx = gui_desktop_sdk_open(title, x, y, w, h);
    if (idx < 0) return -1;

    g_sdk[slot].used   = 1;
    g_sdk[slot].win_id = idx;   /* gui_desktop_sdk_open now returns stable id */
    g_sdk[slot].has_ev = 0;

    // Return a simple handle = slot + 100 (avoids confusion with raw indices)
    return (int32_t)(slot + 100);
}

// SYS_GUI_WIN_CLOSE  a0=handle
static int32_t sys_gui_win_close(uint32_t a0, uint32_t a1, uint32_t a2,
                                 uint32_t a3, uint32_t a4) {
    (void)a1; (void)a2; (void)a3; (void)a4;
    int32_t handle = (int32_t)a0 - 100;
    if (handle < 0 || handle >= GUI_SDK_MAX_WINS || !g_sdk[handle].used)
        return -1;
    gui_desktop_sdk_close(g_sdk[handle].win_id);
    g_sdk[handle].used = 0;
    g_sdk[handle].close_requested = 0;
    return 0;
}

// SYS_GUI_WIN_SETTITLE  a0=handle a1=title_ptr
static int32_t sys_gui_win_settitle(uint32_t a0, uint32_t a1, uint32_t a2,
                                    uint32_t a3, uint32_t a4) {
    (void)a2; (void)a3; (void)a4;
    int32_t     handle = (int32_t)a0 - 100;
    const char *title  = (const char *)syscall_translate_app_ptr(a1, 1);
    if (handle < 0 || handle >= GUI_SDK_MAX_WINS || !g_sdk[handle].used)
        return -1;
    if (!ptr_ok(title)) return -1;
    gui_desktop_sdk_set_title(g_sdk[handle].win_id, title);
    return 0;
}

// SYS_GUI_FILL_RECT  a0=x a1=y a2=w a3=h a4=color
static int32_t sys_gui_fill_rect(uint32_t a0, uint32_t a1, uint32_t a2,
                                 uint32_t a3, uint32_t a4) {
    gui_fill_rect((int32_t)a0, (int32_t)a1, (int32_t)a2, (int32_t)a3, a4);
    return 0;
}

// SYS_GUI_DRAW_HLINE  a0=x a1=y a2=len a3=color
static int32_t sys_gui_draw_hline(uint32_t a0, uint32_t a1, uint32_t a2,
                                  uint32_t a3, uint32_t a4) {
    (void)a4;
    gui_draw_hline((int32_t)a0, (int32_t)a1, (int32_t)a2, a3);
    return 0;
}

// SYS_GUI_DRAW_VLINE  a0=x a1=y a2=len a3=color
static int32_t sys_gui_draw_vline(uint32_t a0, uint32_t a1, uint32_t a2,
                                  uint32_t a3, uint32_t a4) {
    (void)a4;
    gui_draw_vline((int32_t)a0, (int32_t)a1, (int32_t)a2, a3);
    return 0;
}

// SYS_GUI_PUTC  a0=x a1=y a2=char a3=fg a4=bg
static int32_t sys_gui_putc(uint32_t a0, uint32_t a1, uint32_t a2,
                            uint32_t a3, uint32_t a4) {
    gui_putc((int32_t)a0, (int32_t)a1, (char)(a2 & 0xFF), a3, a4);
    return 0;
}

// SYS_GUI_PUTS  a0=x a1=y a2=str_ptr a3=fg a4=bg
static int32_t sys_gui_puts(uint32_t a0, uint32_t a1, uint32_t a2,
                            uint32_t a3, uint32_t a4) {
    const char *s = (const char *)syscall_translate_app_ptr(a2, 1);
    if (!ptr_ok(s)) return -1;
    gui_puts((int32_t)a0, (int32_t)a1, s, a3, a4);
    return 0;
}

// SYS_GUI_BUTTON  a0=ptr→gui_btn_args_t
static int32_t sys_gui_button(uint32_t a0, uint32_t a1, uint32_t a2,
                              uint32_t a3, uint32_t a4) {
    (void)a1; (void)a2; (void)a3; (void)a4;
    gui_btn_args_t *a = (gui_btn_args_t *)syscall_translate_app_ptr(a0, sizeof(gui_btn_args_t));
    if (!ptr_ok(a)) return 0;
    const char *label = (const char *)syscall_translate_app_ptr((uint32_t)a->label, 1);
    if (!ptr_ok(label)) label = "?";
    return (int32_t)gui_button(a->x, a->y, a->w, a->h,
                               label, a->mx, a->my, a->clicked);
}

// SYS_GUI_GET_MOUSE  a0=ptr→gui_mouse_t
static int32_t sys_gui_get_mouse(uint32_t a0, uint32_t a1, uint32_t a2,
                                 uint32_t a3, uint32_t a4) {
    (void)a1; (void)a2; (void)a3; (void)a4;
    gui_mouse_t *m = (gui_mouse_t *)syscall_translate_app_ptr(a0, sizeof(gui_mouse_t));
    if (!ptr_ok(m)) return -1;
    m->x       = g_mouse.x;
    m->y       = g_mouse.y;
    m->buttons = g_mouse.buttons;
    return 0;
}

// SYS_GUI_POLL_EVENT  a0=ptr→gui_event_t
static int32_t sys_gui_poll_event(uint32_t a0, uint32_t a1, uint32_t a2,
                                  uint32_t a3, uint32_t a4) {
    (void)a1; (void)a2; (void)a3; (void)a4;
    gui_event_t *ev = (gui_event_t *)syscall_translate_app_ptr(a0, sizeof(gui_event_t));
    if (!ptr_ok(ev)) return GUI_EVENT_NONE;

    ev->type      = GUI_EVENT_MOUSE;
    ev->mouse_x   = g_mouse.x;
    ev->mouse_y   = g_mouse.y;
    ev->mouse_btn = g_mouse.buttons;
    ev->key_ascii = 0;
    return (int32_t)GUI_EVENT_MOUSE;
}

// SYS_GUI_PUMP  (no args)
// In TASK_GUI (standalone): run a full frame.
// In TASK_APP (from terminal): yield back to TASK_GUI, which is mid-frame
// waiting for us to finish drawing. It will complete the frame (taskbar,
// cursor, bb_present) then yield back to us for the next iteration.
static int32_t sys_gui_pump(uint32_t a0, uint32_t a1, uint32_t a2,
                            uint32_t a3, uint32_t a4) {
    (void)a0; (void)a1; (void)a2; (void)a3; (void)a4;
    if (sched_current() == TASK_GUI) {
        gui_pump();
    } else {
        // Check if any SDK window has been close-requested by the X button.
        // Return -1 so the app's gui_pump() call signals it to exit cleanly,
        // exactly as if the app's own Close button was clicked.
        for (int _i = 0; _i < GUI_SDK_MAX_WINS; _i++) {
            if (g_sdk[_i].used && g_sdk[_i].close_requested) return -1;
        }
        sched_yield();
        // Re-check after yield.
        for (int _i = 0; _i < GUI_SDK_MAX_WINS; _i++) {
            if (g_sdk[_i].used && g_sdk[_i].close_requested) return -1;
        }
    }
    return 0;
}

// SYS_GUI_DRAW_RECT  a0=x a1=y a2=w a3=h a4=color (outline only)
static int32_t sys_gui_draw_rect(uint32_t a0, uint32_t a1, uint32_t a2,
                                 uint32_t a3, uint32_t a4) {
    int32_t  x = (int32_t)a0, y = (int32_t)a1;
    int32_t  w = (int32_t)a2, h = (int32_t)a3;
    uint32_t c = a4;
    gui_draw_hline(x,     y,     w, c);
    gui_draw_hline(x,     y+h-1, w, c);
    gui_draw_vline(x,     y,     h, c);
    gui_draw_vline(x+w-1, y,     h, c);
    return 0;
}

// SYS_GUI_DRAW_3DBOX  a0=x a1=y a2=w a3=h a4=raised
static int32_t sys_gui_draw_3dbox(uint32_t a0, uint32_t a1, uint32_t a2,
                                  uint32_t a3, uint32_t a4) {
    gui_draw_3d_box((int32_t)a0, (int32_t)a1, (int32_t)a2, (int32_t)a3,
                    (uint8_t)(a4 & 1));
    return 0;
}

// SYS_GUI_SCREEN_SIZE  a0=ptr→uint32_t[2]
static int32_t sys_gui_screen_size(uint32_t a0, uint32_t a1, uint32_t a2,
                                   uint32_t a3, uint32_t a4) {
    (void)a1; (void)a2; (void)a3; (void)a4;
    uint32_t *out = (uint32_t *)syscall_translate_app_ptr(a0, sizeof(uint32_t) * 2);
    if (!ptr_ok(out)) return -1;
    out[0] = SCREEN_W;
    out[1] = SCREEN_H;
    return 0;
}

// SYS_GUI_WIN_GETRECT  a0=handle a1=ptr→gui_rect_t
static int32_t sys_gui_win_getrect(uint32_t a0, uint32_t a1, uint32_t a2,
                                   uint32_t a3, uint32_t a4) {
    (void)a2; (void)a3; (void)a4;
    int32_t    handle = (int32_t)a0 - 100;
    gui_rect_t *rect  = (gui_rect_t *)syscall_translate_app_ptr(a1, sizeof(gui_rect_t));
    if (handle < 0 || handle >= GUI_SDK_MAX_WINS || !g_sdk[handle].used)
        return -1;
    if (!ptr_ok(rect)) return -1;
    // Use local int32_t temporaries to avoid taking addresses of packed struct
    // members, which can produce unaligned pointers on some targets.
    int32_t rx, ry, rw, rh;
    gui_desktop_sdk_getrect(g_sdk[handle].win_id,  &rx, &ry, &rw, &rh);
    rect->x = rx;
    rect->y = ry;
    rect->w = rw;
    rect->h = rh;
    return 0;
}

// ---------------------------------------------------------------------------
// Register all GUI syscalls into the syscall table
// ---------------------------------------------------------------------------

// Called by gui_desktop when the user clicks the X on an SDK window.
// Signals the app to exit cleanly instead of forcibly destroying the window.
void gui_sdk_request_close(int32_t win_id) {
    for (int i = 0; i < GUI_SDK_MAX_WINS; i++) {
        if (g_sdk[i].used && g_sdk[i].win_id == win_id) {
            g_sdk[i].close_requested = 1;
            return;
        }
    }
}

void gui_sdk_register_syscalls(void) {
    syscall_register(SYS_GUI_WIN_CREATE,   sys_gui_win_create);
    syscall_register(SYS_GUI_WIN_CLOSE,    sys_gui_win_close);
    syscall_register(SYS_GUI_WIN_SETTITLE, sys_gui_win_settitle);
    syscall_register(SYS_GUI_FILL_RECT,    sys_gui_fill_rect);
    syscall_register(SYS_GUI_DRAW_HLINE,   sys_gui_draw_hline);
    syscall_register(SYS_GUI_DRAW_VLINE,   sys_gui_draw_vline);
    syscall_register(SYS_GUI_PUTC,         sys_gui_putc);
    syscall_register(SYS_GUI_PUTS,         sys_gui_puts);
    syscall_register(SYS_GUI_BUTTON,       sys_gui_button);
    syscall_register(SYS_GUI_GET_MOUSE,    sys_gui_get_mouse);
    syscall_register(SYS_GUI_POLL_EVENT,   sys_gui_poll_event);
    syscall_register(SYS_GUI_PUMP,         sys_gui_pump);
    syscall_register(SYS_GUI_DRAW_RECT,    sys_gui_draw_rect);
    syscall_register(SYS_GUI_DRAW_3DBOX,   sys_gui_draw_3dbox);
    syscall_register(SYS_GUI_SCREEN_SIZE,  sys_gui_screen_size);
    syscall_register(SYS_GUI_WIN_GETRECT,  sys_gui_win_getrect);
}
