// =============================================================================
// Eclipse32 - GUI App SDK  (kernel side)
// Bridges userspace apps to gui_desktop.c via INT 0x40 syscalls.
// =============================================================================
#pragma once
#include "../kernel.h"

// ---------------------------------------------------------------------------
// SDK limits
// ---------------------------------------------------------------------------
#define GUI_SDK_MAX_WINS  16   // max concurrent SDK-managed windows

// ---------------------------------------------------------------------------
// Syscall numbers  (mirror these in apps/sdk/e32_gui.h)
// ---------------------------------------------------------------------------
#define SYS_GUI_WIN_CREATE   20   // ebx=title ecx=x edx=y esi=w edi=h → win_id or -1
#define SYS_GUI_WIN_CLOSE    21   // ebx=win_id
#define SYS_GUI_WIN_SETTITLE 22   // ebx=win_id ecx=title_ptr
#define SYS_GUI_FILL_RECT    23   // ebx=x ecx=y edx=w esi=h edi=color
#define SYS_GUI_DRAW_HLINE   24   // ebx=x ecx=y edx=len esi=color
#define SYS_GUI_DRAW_VLINE   25   // ebx=x ecx=y edx=len esi=color
#define SYS_GUI_PUTC         26   // ebx=x ecx=y edx=char esi=fg edi=bg
#define SYS_GUI_PUTS         27   // ebx=x ecx=y edx=str_ptr esi=fg edi=bg
#define SYS_GUI_BUTTON       28   // ebx=ptr→gui_btn_args_t → 1 if clicked
#define SYS_GUI_GET_MOUSE    29   // ebx=ptr→gui_mouse_t
#define SYS_GUI_POLL_EVENT   30   // ebx=ptr→gui_event_t   → event type or 0
#define SYS_GUI_PUMP         31   // (no args) run one desktop frame
#define SYS_GUI_DRAW_RECT    32   // ebx=x ecx=y edx=w esi=h edi=color (outline)
#define SYS_GUI_DRAW_3DBOX   33   // ebx=x ecx=y edx=w esi=h edi=raised
#define SYS_GUI_SCREEN_SIZE  34   // ebx=ptr→uint32_t[2] → [w, h]
#define SYS_GUI_WIN_GETRECT  35   // ebx=win_id ecx=ptr→gui_rect_t

// ---------------------------------------------------------------------------
// Shared argument structs  (must exactly match apps/sdk/e32_gui.h)
// ---------------------------------------------------------------------------

// Passed by pointer for SYS_GUI_BUTTON (too many params for regs)
typedef struct {
    int32_t  x, y, w, h;
    const char *label;
    int32_t  mx, my;
    uint8_t  clicked;
} PACKED gui_btn_args_t;

// Filled by SYS_GUI_GET_MOUSE
typedef struct {
    int32_t  x, y;
    uint8_t  buttons;   // bit0=left bit1=right
} PACKED gui_mouse_t;

// Returned by SYS_GUI_POLL_EVENT
#define GUI_EVENT_NONE    0
#define GUI_EVENT_PAINT   1
#define GUI_EVENT_MOUSE   2
#define GUI_EVENT_KEYDOWN 3
#define GUI_EVENT_CLOSE   4

typedef struct {
    uint32_t type;
    int32_t  mouse_x, mouse_y;
    uint8_t  mouse_btn;
    uint8_t  key_ascii;
} PACKED gui_event_t;

// Returned by SYS_GUI_WIN_GETRECT
typedef struct {
    int32_t x, y, w, h;   // client area coordinates (screen-absolute)
} PACKED gui_rect_t;

// ---------------------------------------------------------------------------
// Public kernel API
// ---------------------------------------------------------------------------
void gui_sdk_init(void);             // zero state; call before gui_run()
void gui_sdk_register_syscalls(void); // plug into syscall_table[]
