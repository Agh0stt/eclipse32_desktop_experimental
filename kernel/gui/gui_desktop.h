#ifndef GUI_DESKTOP_H
#define GUI_DESKTOP_H

#include "../kernel.h"

// ============================================================
// Color macros  (pixel format: 0x00RRGGBB, stored B G R in RAM)
// ============================================================
#define RGB(r,g,b) (((uint32_t)(r)<<16)|((uint32_t)(g)<<8)|(uint32_t)(b))

// ============================================================
// Theme IDs
// ============================================================
#define THEME_MODERN   0   // dark panel, blue gradient desktop
#define THEME_CLASSIC  1   // grey taskbar at bottom, teal desktop
#define THEME_MIDNIGHT 2   // deep navy, silver accents
#define THEME_SUNSET   3   // warm purple/orange gradient
#define THEME_COUNT    4

// ============================================================
// Runtime color variables — set by apply_theme(), used everywhere
// All code reads these variables; never the old #define values.
// ============================================================
extern uint32_t COL_DESKTOP;
extern uint32_t COL_WIN_BG;
extern uint32_t COL_WIN_TITLE_ACT;
extern uint32_t COL_WIN_TITLE_INA;
extern uint32_t COL_WIN_TITLE_TXT;
extern uint32_t COL_BORDER_LIGHT;
extern uint32_t COL_BORDER_DARK;
extern uint32_t COL_BUTTON_FACE;
extern uint32_t COL_BUTTON_TXT;
extern uint32_t COL_TEXT;
extern uint32_t COL_ICON_LABEL;
extern uint32_t COL_ICON_SEL_BG;
extern uint32_t COL_TASKBAR_BG;
extern uint32_t COL_TASKBAR_TXT;
extern uint32_t COL_MENU_BG;
extern uint32_t COL_MENU_SEL;
extern uint32_t COL_MENU_SEL_TXT;
extern uint32_t COL_MENU_TXT;
extern uint32_t COL_MENU_DIV;

// Fixed colors — never change with theme
#define COL_WHITE   RGB(255,255,255)
#define COL_BLACK   RGB(0,0,0)
#define COL_LTGREY  RGB(224,224,224)
#define COL_DKGREY  RGB(96,96,96)
#define COL_RED     RGB(200,40,40)
#define COL_GREEN   RGB(40,180,40)
#define COL_BLUE    RGB(30,100,210)
#define COL_YELLOW  RGB(210,180,0)
#define COL_TERM_BG RGB(20,20,30)
#define COL_TERM_TXT RGB(80,220,80)

// Current active theme index
extern int g_theme;

// Apply a theme — call once at startup, and any time user switches
void apply_theme(int theme_id);

// ============================================================
// Screen / layout
// ============================================================
#define SCREEN_W    800
#define SCREEN_H    600
#define TASKBAR_H    30
#define TITLE_BAR_H  20
#define BORDER_W      2
#define FONT_W        8
#define FONT_H        8

// These change with theme — read g_theme at render time
// THEME_MODERN:  taskbar at top  (TASKBAR_Y=0, desktop starts at TASKBAR_H)
// THEME_CLASSIC: taskbar at bottom (TASKBAR_Y=SCREEN_H-TASKBAR_H, desktop starts at 0)
#define TASKBAR_Y_FOR(t)   ((t)==THEME_CLASSIC ? (SCREEN_H-TASKBAR_H) : 0)
#define DESKTOP_TOP_FOR(t) ((t)==THEME_CLASSIC ? 0 : TASKBAR_H)

// ============================================================
// App enum  (32 apps)
// ============================================================
typedef enum {
    APP_NONE = 0,
    APP_NOTEPAD,       APP_WORDPAD,      APP_CALCULATOR,   APP_CALENDAR,
    APP_CLOCK,         APP_STOPWATCH,    APP_SYSINFO,      APP_TASKMGR,
    APP_REGEDIT,       APP_DEVMGR,       APP_DISKINFO,     APP_MEMMAP,
    APP_CPUMON,        APP_FILEMAN,      APP_HEXVIEW,      APP_TEXTVIEW,
    APP_NETINFO,       APP_PAINT,        APP_COLORPICKER,  APP_SCREENSAVER,
    APP_SNAKE,         APP_PONG,         APP_MINESWEEPER,  APP_BREAKOUT,
    APP_TICTACTOE,     APP_SLOTS,        APP_PIANO,        APP_TERMINAL,
    APP_LOGVIEWER,     APP_HELP,         APP_ABOUT,        APP_IPCONFIG,
    APP_TETRIS,
    APP_OS32,       /* launches hello.os32 directly from desktop */
    APP_SETTINGS,   /* theme / settings panel */
    APP_SDK,        /* SDK-managed window; userspace app draws its own content */
    APP_COUNT
} AppType;

// ============================================================
// Window flags
// ============================================================
#define WIN_FLAG_VISIBLE   (1<<0)
#define WIN_FLAG_MOVEABLE  (1<<1)
#define WIN_FLAG_CLOSEABLE (1<<2)
#define MAX_WINDOWS        16

// ============================================================
// Per-window app state  (kept small — shared fields)
// ============================================================
#define WIN_TEXT_BUF  8192

typedef struct {
    int32_t  scroll_y;
    // text/terminal
    char     text[WIN_TEXT_BUF];
    int32_t  text_len;
    // calculator
    int32_t  calc_val, calc_accum;
    char     calc_disp[20];
    char     calc_op;
    uint8_t  calc_new;
    // clock/stopwatch
    uint32_t sw_start;
    uint8_t  sw_running;
    // paint
    uint8_t  paint_col_idx;
    int16_t  paint_prev_x, paint_prev_y;  // last drawn position for line interpolation
    uint8_t  paint_was_down;              // was mouse down last frame
    // snake
    int8_t   sn_dx, sn_dy;
    uint8_t  sn_len, sn_dead;
    uint8_t  sn_x[64], sn_y[64];
    uint8_t  sn_food_x, sn_food_y;
    uint32_t sn_tick;
    // pong
    int32_t  pong_bx, pong_by, pong_bdx, pong_bdy;
    int32_t  pong_p1y, pong_p2y, pong_s1, pong_s2;
    uint32_t pong_tick;
    // minesweeper: 9x9, bits: 0=mine 1=revealed 2=flagged
    uint8_t  ms_board[81];
    uint8_t  ms_init, ms_dead, ms_won;
    // breakout
    int32_t  brk_bx, brk_by, brk_bdx, brk_bdy, brk_px, brk_score;
    uint8_t  brk_bricks[50];   // 5 rows × 10 cols
    uint8_t  brk_init, brk_dead;
    uint32_t brk_tick;
    // tic tac toe
    uint8_t  ttt_board[9], ttt_turn, ttt_done;
    // slots
    uint8_t  slot_reels[3], slot_spinning;
    uint32_t slot_tick;
    int32_t  slot_credits;
    // screensaver
    int32_t  ss_x, ss_y, ss_dx, ss_dy;
    uint32_t ss_col;
    // hex view
    uint32_t hex_addr;
    // calendar
    uint8_t  cal_month;
    // file manager per-window cache
    char     fm_names[32][64];
    uint32_t fm_sizes[32];
    int32_t  fm_count;      // -1 = not loaded
    int32_t  fm_sel;        // index of selected file (-1 = none)
    uint32_t fm_last_click; // g_tick at last click (for double-click detect)
    int32_t  fm_mode;       // 0=list 1=new 2=rename 3=edit 4=confirm-delete
    char     fm_input[64];  // text input buffer for new/rename dialogs
    // tetris
    uint8_t  tet_board[20*10];  // 20 rows x 10 cols
    int8_t   tet_px, tet_py;    // current piece position
    uint8_t  tet_piece;         // current piece type 0-6
    uint8_t  tet_rot;           // current rotation 0-3
    int32_t  tet_score;
    uint8_t  tet_dead;
    uint8_t  tet_init;
    uint32_t tet_tick;
    uint32_t tet_speed;         // ticks between drops
    // terminal shell state (real shell)
    int32_t  term_initialized;
    uint8_t  term_app_running;          // 1 if a sub-app is running
    uint8_t  term_in_head;              // circular input buffer write index
    uint8_t  term_in_tail;             // circular input buffer read index
    char     term_inbuf[256];           // keyboard input ring buffer
} AppState;

// ============================================================
// Window struct
// ============================================================
typedef struct {
    int32_t  x, y, w, h;
    uint32_t flags;
    AppType  app;
    char     title[40];
    AppState st;
    uint32_t id;   /* stable identity — survives bring_front() shuffles */
} Window;

// ============================================================
// Desktop icon
// ============================================================
typedef struct {
    int16_t   x, y;
    AppType   app;
    const char *label;
    uint8_t   selected;
} DesktopIcon;

// ============================================================
// Public API
// ============================================================
void gui_run(void);
void gui_pump(void);  // run one GUI frame; safe to call from syscall context
void gui_fill_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t col);
void gui_draw_hline(int32_t x, int32_t y, int32_t len, uint32_t col);
void gui_draw_vline(int32_t x, int32_t y, int32_t len, uint32_t col);
void gui_draw_3d_box(int32_t x, int32_t y, int32_t w, int32_t h, uint8_t raised);
void gui_draw_rect_border(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t tl, uint32_t br);
void gui_putc(int32_t x, int32_t y, char c, uint32_t fg, uint32_t bg);
void gui_puts(int32_t x, int32_t y, const char *s, uint32_t fg, uint32_t bg);
int  gui_button(int32_t x, int32_t y, int32_t w, int32_t h,
                const char *label, int32_t mx, int32_t my, uint8_t clicked);

#endif
uint8_t gui_sdk_is_active(void);
void    gui_sdk_request_close(int32_t win_id);
