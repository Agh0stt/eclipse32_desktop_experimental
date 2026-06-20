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
