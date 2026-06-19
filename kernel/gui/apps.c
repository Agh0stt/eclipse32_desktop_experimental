// =============================================================================
// Eclipse32 - Built-in GUI Applications
// FileManager, GuiTerminal, ClockApp, AboutDialog, PaintApp
// =============================================================================
#include "apps.h"
#include "gui.h"
#include "../initramfs/initramfs.h"
#include "../drivers/keyboard/keyboard.h"
#include "../fs/fat32/fat32.h"
#include "../arch/x86/pit.h"
#include "../mm/heap.h"
#include "../kernel.h"

// ============================================================
// Shared helpers
// ============================================================

static void draw_button(gui_dc_t *dc, int32_t x, int32_t y, uint32_t w, uint32_t h,
                        const char *label, bool hovered, bool pressed)
{
    uint32_t bg = pressed ? GUI_COLOR_BUTTON_PRESS
                : hovered ? GUI_COLOR_BUTTON_HOV
                : GUI_COLOR_BUTTON;
    gui_dc_fill_rect(dc, x, y, w, h, bg);
    gui_dc_draw_rect(dc, x, y, w, h, 0x00909090);
    // Highlight top/left
    gui_dc_draw_line(dc, x+1, y+1, x+(int32_t)w-2, y+1, 0x00FFFFFF);
    gui_dc_draw_line(dc, x+1, y+1, x+1, y+(int32_t)h-2, 0x00FFFFFF);
    // Shadow bottom/right
    gui_dc_draw_line(dc, x+1, y+(int32_t)h-2, x+(int32_t)w-2, y+(int32_t)h-2, 0x00707070);
    gui_dc_draw_line(dc, x+(int32_t)w-2, y+1, x+(int32_t)w-2, y+(int32_t)h-2, 0x00707070);

    // Center label
    int32_t lw = 0; for (const char *p = label; *p; p++) lw += 8;
    int32_t lx = x + ((int32_t)w - lw) / 2;
    int32_t ly = y + ((int32_t)h - 8) / 2;
    gui_dc_draw_text(dc, lx, ly, label, GUI_COLOR_TEXT_DARK, bg);
}

static bool point_in(int32_t px, int32_t py, int32_t x, int32_t y, uint32_t w, uint32_t h) {
    return px >= x && px < x+(int32_t)w && py >= y && py < y+(int32_t)h;
}

// ============================================================
// About Dialog
// ============================================================

typedef struct {
    uint32_t win_id;
    uint32_t ticks_created;
} about_state_t;

static void about_paint(gui_dc_t *dc, uint32_t id, void *ud) {
    (void)id; (void)ud;
    gui_dc_clear(dc, GUI_COLOR_WIN_BG);

    // Header bar
    gui_dc_fill_rect(dc, 0, 0, dc->width, 48, GUI_COLOR_ACCENT);

    // Title text
    gui_dc_draw_text(dc, 8, 8, "Eclipse32 Operating System", 0x00FFFFFF, GUI_COLOR_ACCENT);
    gui_dc_draw_text(dc, 8, 22, "Experimental Build - EclipseGUI v1.0", 0x00D0E8FF, GUI_COLOR_ACCENT);

    // Divider
    gui_dc_draw_line(dc, 0, 48, (int32_t)dc->width, 48, GUI_COLOR_WIN_BORDER);

    // Info block
    gui_dc_draw_text(dc, 16, 64,  "Kernel:   Eclipse32 v0.1 (32-bit x86 Protected Mode)", GUI_COLOR_TEXT_DARK, GUI_COLOR_WIN_BG);
    gui_dc_draw_text(dc, 16, 80,  "Display:  VBE 1024x768x32bpp Linear Framebuffer",     GUI_COLOR_TEXT_DARK, GUI_COLOR_WIN_BG);
    gui_dc_draw_text(dc, 16, 96,  "FS:       FAT32",                                      GUI_COLOR_TEXT_DARK, GUI_COLOR_WIN_BG);
    gui_dc_draw_text(dc, 16, 112, "Shell:    Eclipse Shell (esh)",                        GUI_COLOR_TEXT_DARK, GUI_COLOR_WIN_BG);
    gui_dc_draw_text(dc, 16, 128, "GUI:      EclipseGUI Window Manager",                  GUI_COLOR_TEXT_DARK, GUI_COLOR_WIN_BG);
    gui_dc_draw_text(dc, 16, 144, "Input:    PS/2 Keyboard + PS/2 Mouse",                 GUI_COLOR_TEXT_DARK, GUI_COLOR_WIN_BG);
    gui_dc_draw_text(dc, 16, 160, "Memory:   32MB, kmalloc heap at 0xC0000000",           GUI_COLOR_TEXT_DARK, GUI_COLOR_WIN_BG);

    gui_dc_draw_line(dc, 16, 178, (int32_t)dc->width-16, 178, 0x00C0C8D0);
    gui_dc_draw_text(dc, 16, 188, "Built from scratch in C + NASM Assembly.", 0x00506070, GUI_COLOR_WIN_BG);
    gui_dc_draw_text(dc, 16, 200, "Because every great OS starts from zero.", 0x00506070, GUI_COLOR_WIN_BG);

    draw_button(dc, (int32_t)(dc->width/2 - 40), (int32_t)(dc->height - 36), 80, 24, "OK", false, false);
}

static void about_event(gui_event_t *ev, void *ud) {
    uint32_t *wid = (uint32_t *)ud;
    if (ev->type == GUI_EVENT_CLOSE ||
        (ev->type == GUI_EVENT_MOUSE_DOWN && point_in(ev->mouse_x, ev->mouse_y,
            (int32_t)(320/2-40), (int32_t)(240-36), 80, 24))) {
        gui_window_destroy(*wid);
    }
}

void app_about_open(void) {
    uint32_t *ud = (uint32_t *)kmalloc(sizeof(uint32_t));
    uint32_t id = gui_window_create(300, 220, 400, 260, "About Eclipse32",
                                    GUI_STYLE_DECORATED, about_paint, about_event, ud);
    if (ud) *ud = id;
}

// ============================================================
// Clock App
// ============================================================

typedef struct {
    uint32_t win_id;
    uint32_t last_tick;
} clock_state_t;

static void clock_paint(gui_dc_t *dc, uint32_t id, void *ud) {
    (void)id;
    clock_state_t *s = (clock_state_t *)ud;

    gui_dc_clear(dc, 0x00111820);

    // Draw analog clock face
    int32_t cx = (int32_t)(dc->width / 2);
    int32_t cy = (int32_t)(dc->height / 2) - 8;
    int32_t r  = 70;

    // Face
    gui_dc_fill_circle(dc, cx, cy, r, 0x00182530);
    gui_dc_draw_line(dc, cx-r, cy, cx+r, cy, 0x00203848);  // horizontal guide
    gui_dc_draw_line(dc, cx, cy-r, cx, cy+r, 0x00203848);  // vertical guide

    // Hour marks
    for (int h = 0; h < 12; h++) {
        float angle = h * 3.14159265f * 2.0f / 12.0f - 3.14159265f / 2.0f;
        int32_t x0 = cx + (int32_t)((r-8) * __builtin_cosf(angle));
        int32_t y0 = cy + (int32_t)((r-8) * __builtin_sinf(angle));
        int32_t x1 = cx + (int32_t)((r-2) * __builtin_cosf(angle));
        int32_t y1 = cy + (int32_t)((r-2) * __builtin_sinf(angle));
        gui_dc_draw_line(dc, x0, y0, x1, y1, GUI_COLOR_ACCENT);
    }

    // Compute time from PIT ticks
    uint32_t ticks = pit_ticks();
    uint32_t secs  = ticks / 1000;
    uint32_t mins  = secs / 60;
    uint32_t hrs   = (mins / 60) % 12;
    secs %= 60; mins %= 60;

    // Angles (12 o'clock = -pi/2)
    float PI2 = 3.14159265f * 2.0f;
    float ha = ((float)hrs + (float)mins/60.0f) / 12.0f * PI2 - 3.14159265f/2.0f;
    float ma = (float)mins / 60.0f * PI2 - 3.14159265f/2.0f;
    float sa = (float)secs / 60.0f * PI2 - 3.14159265f/2.0f;

    // Hour hand
    gui_dc_draw_line(dc, cx, cy,
        cx + (int32_t)((r*0.5f) * __builtin_cosf(ha)),
        cy + (int32_t)((r*0.5f) * __builtin_sinf(ha)),
        GUI_COLOR_TEXT_LIGHT);
    gui_dc_draw_line(dc, cx+1, cy,
        cx+1 + (int32_t)((r*0.5f) * __builtin_cosf(ha)),
        cy + (int32_t)((r*0.5f) * __builtin_sinf(ha)),
        GUI_COLOR_TEXT_LIGHT);

    // Minute hand
    gui_dc_draw_line(dc, cx, cy,
        cx + (int32_t)((r*0.75f) * __builtin_cosf(ma)),
        cy + (int32_t)((r*0.75f) * __builtin_sinf(ma)),
        GUI_COLOR_TEXT_LIGHT);

    // Second hand (red)
    gui_dc_draw_line(dc, cx, cy,
        cx + (int32_t)((r*0.85f) * __builtin_cosf(sa)),
        cy + (int32_t)((r*0.85f) * __builtin_sinf(sa)),
        GUI_COLOR_CLOSE_BTN);

    // Center dot
    gui_dc_fill_circle(dc, cx, cy, 4, GUI_COLOR_ACCENT);
    gui_dc_fill_circle(dc, cx, cy, 2, GUI_COLOR_TEXT_LIGHT);

    // Digital time
    char tbuf[16];
    tbuf[0] = '0'+hrs; tbuf[1] = '0'+0; // simplified - just show sec counter
    uint32_t h2 = hrs, m2 = mins, s2 = secs;
    tbuf[0]='0'+h2/10; tbuf[1]='0'+h2%10; tbuf[2]=':';
    tbuf[3]='0'+m2/10; tbuf[4]='0'+m2%10; tbuf[5]=':';
    tbuf[6]='0'+s2/10; tbuf[7]='0'+s2%10; tbuf[8]=0;
    gui_dc_draw_text(dc, cx-32, cy+r+12, tbuf, GUI_COLOR_ACCENT, 0x00111820);

    s->last_tick = ticks;
    (void)s;
}

static void clock_event(gui_event_t *ev, void *ud) {
    clock_state_t *s = (clock_state_t *)ud;
    if (ev->type == GUI_EVENT_CLOSE) {
        kfree(s);
        gui_window_destroy(ev->window_id);
        return;
    }
    // Repaint every second
    uint32_t now = pit_ticks();
    if (now - s->last_tick >= 1000) {
        gui_window_invalidate(s->win_id);
    }
}

void app_clock_open(void) {
    clock_state_t *s = (clock_state_t *)kmalloc(sizeof(clock_state_t));
    if (!s) return;
    s->last_tick = 0;
    uint32_t id = gui_window_create(760, 60, 200, 220, "Clock",
                                    GUI_STYLE_DECORATED, clock_paint, clock_event, s);
    s->win_id = id;
}

// ============================================================
// File Manager
// ============================================================

#define FM_MAX_ENTRIES 32

typedef struct {
    uint32_t win_id;
    char     cwd[256];
    char     entries[FM_MAX_ENTRIES][256];
    bool     is_dir[FM_MAX_ENTRIES];
    int      count;
    int      scroll;
    int      selected;
    int      hover;
    int32_t  mouse_x, mouse_y;
} fm_state_t;

static void fm_load_dir(fm_state_t *s) {
    s->count = 0;
    s->selected = -1;
    fat32_dir_entry_t dirents[FM_MAX_ENTRIES];
    int n = fat32_readdir(s->cwd, dirents, FM_MAX_ENTRIES);
    for (int i = 0; i < n && s->count < FM_MAX_ENTRIES; i++) {
        // Copy name
        int ni = 0;
        while (dirents[i].name[ni] && ni < 255) { s->entries[s->count][ni] = dirents[i].name[ni]; ni++; }
        s->entries[s->count][ni] = 0;
        s->is_dir[s->count] = dirents[i].is_dir;
        s->count++;
    }
}

static void fm_paint(gui_dc_t *dc, uint32_t id, void *ud) {
    (void)id;
    fm_state_t *s = (fm_state_t *)ud;

    gui_dc_clear(dc, GUI_COLOR_WIN_BG);

    // Toolbar strip
    gui_dc_fill_rect(dc, 0, 0, dc->width, 28, 0x00E0E8F0);
    gui_dc_draw_line(dc, 0, 28, (int32_t)dc->width, 28, 0x00C0C8D0);

    // Path bar
    gui_dc_fill_rect(dc, 60, 4, dc->width - 68, 20, GUI_COLOR_INPUT_BG);
    gui_dc_draw_rect(dc, 60, 4, dc->width - 68, 20, GUI_COLOR_INPUT_BORDER);
    gui_dc_draw_text(dc, 64, 9, s->cwd, GUI_COLOR_TEXT_DARK, GUI_COLOR_INPUT_BG);
    draw_button(dc, 2, 4, 52, 20, "Up", false, false);

    // Column headers
    gui_dc_fill_rect(dc, 0, 28, (int32_t)dc->width, 18, 0x00D0D8E0);
    gui_dc_draw_text(dc, 4, 33, "Name", 0x00404040, 0x00D0D8E0);
    gui_dc_draw_text(dc, (int32_t)dc->width - 50, 33, "Type", 0x00404040, 0x00D0D8E0);
    gui_dc_draw_line(dc, 0, 46, (int32_t)dc->width, 46, 0x00C0C8D0);

    // Entries
    int32_t row_h = 20;
    int32_t list_y = 46;
    for (int i = s->scroll; i < s->count && (list_y + (i-s->scroll)*row_h) < (int32_t)dc->height - 4; i++) {
        int32_t ry = list_y + (i - s->scroll) * row_h;

        // Hover / selection highlight
        bool is_hov = (s->mouse_y >= ry && s->mouse_y < ry + row_h);
        bool is_sel = (i == s->selected);
        uint32_t row_bg = is_sel ? GUI_COLOR_ACCENT
                        : is_hov ? 0x00D8E8F8
                        : (i % 2 == 0) ? GUI_COLOR_WIN_BG : 0x00F8F8FC;

        gui_dc_fill_rect(dc, 0, ry, dc->width, (uint32_t)row_h, row_bg);

        // Icon (dir = folder glyph, file = page glyph)
        uint32_t icon_color = s->is_dir[i] ? GUI_COLOR_ACCENT : 0x00808080;
        if (s->is_dir[i]) {
            // Folder icon
            gui_dc_fill_rect(dc, 4, ry+5, 14, 10, icon_color);
            gui_dc_fill_rect(dc, 4, ry+3, 7, 4, icon_color);
        } else {
            // File icon
            gui_dc_fill_rect(dc, 4, ry+3, 12, 14, 0x00FFFFFF);
            gui_dc_draw_rect(dc, 4, ry+3, 12, 14, 0x00808080);
            gui_dc_draw_line(dc, 13, ry+3, 16, ry+6, 0x00808080);
            gui_dc_fill_rect(dc, 13, ry+3, 3, 4, icon_color);
        }

        // Name
        uint32_t text_col = is_sel ? 0x00FFFFFF : GUI_COLOR_TEXT_DARK;
        gui_dc_draw_text(dc, 22, ry+6, s->entries[i], text_col, row_bg);

        // Type label
        const char *type = s->is_dir[i] ? "DIR" : "FILE";
        gui_dc_draw_text(dc, (int32_t)dc->width - 46, ry+6, type,
            is_sel ? 0x00FFFFFF : 0x00808090, row_bg);
    }

    // Bottom status bar
    gui_dc_fill_rect(dc, 0, (int32_t)dc->height - 18, dc->width, 18, 0x00E0E8F0);
    gui_dc_draw_line(dc, 0, (int32_t)dc->height - 18, (int32_t)dc->width, (int32_t)dc->height - 18, 0x00C0C8D0);
    char status[64];
    // simple int to string
    int n = s->count;
    char ns[12]; int ni2 = 0;
    if (n == 0) { ns[0]='0'; ns[1]=0; }
    else { while(n){ns[ni2++]='0'+n%10;n/=10;} ns[ni2]=0;
           for(int a=0,b=ni2-1;a<b;a++,b--){char t=ns[a];ns[a]=ns[b];ns[b]=t;} }
    status[0]='['; int si=1;
    for(int k=0;ns[k];k++) status[si++]=ns[k];
    const char *rest = " items]";
    for(int k=0;rest[k];k++) status[si++]=rest[k];
    status[si]=0;
    gui_dc_draw_text(dc, 4, (int32_t)dc->height - 13, status, 0x00506070, 0x00E0E8F0);
}

static void fm_event(gui_event_t *ev, void *ud) {
    fm_state_t *s = (fm_state_t *)ud;

    if (ev->type == GUI_EVENT_CLOSE) {
        kfree(s);
        gui_window_destroy(ev->window_id);
        return;
    }

    if (ev->type == GUI_EVENT_MOUSE_MOVE) {
        s->mouse_x = ev->mouse_x;
        s->mouse_y = ev->mouse_y;
        gui_window_invalidate(ev->window_id);
    }

    if (ev->type == GUI_EVENT_MOUSE_DOWN) {
        int32_t row_h = 20, list_y = 46;

        // Up button
        if (point_in(ev->mouse_x, ev->mouse_y, 2, 4, 52, 20)) {
            // Navigate up
            int len = 0; while(s->cwd[len]) len++;
            while (len > 1 && s->cwd[len-1] != '/') len--;
            if (len > 1) len--;
            s->cwd[len] = 0;
            fm_load_dir(s);
            gui_window_invalidate(ev->window_id);
            return;
        }

        // Entry click
        if (ev->mouse_y >= list_y) {
            int idx = s->scroll + (ev->mouse_y - list_y) / row_h;
            if (idx >= 0 && idx < s->count) {
                if (s->selected == idx && s->is_dir[idx]) {
                    // Double-click: enter dir (treat second click as enter)
                    int clen = 0; while(s->cwd[clen]) clen++;
                    if (clen > 1 || s->cwd[0] != '/') {
                        s->cwd[clen++] = '/';
                    }
                    int ni = 0;
                    while(s->entries[idx][ni] && clen < 254)
                        s->cwd[clen++] = s->entries[idx][ni++];
                    s->cwd[clen] = 0;
                    fm_load_dir(s);
                } else {
                    s->selected = idx;
                }
                gui_window_invalidate(ev->window_id);
            }
        }
    }
}

void app_filemanager_open(void) {
    fm_state_t *s = (fm_state_t *)kmalloc(sizeof(fm_state_t));
    if (!s) return;
    kmemset(s, 0, sizeof(*s));
    s->cwd[0] = '/'; s->cwd[1] = 0;
    s->selected = -1;
    s->hover = -1;
    fm_load_dir(s);
    uint32_t id = gui_window_create(50, 80, 480, 360, "File Manager",
                                    GUI_STYLE_DECORATED, fm_paint, fm_event, s);
    s->win_id = id;
}

// ============================================================
// GUI Terminal (simple output-only terminal window)
// ============================================================

#define GTERM_COLS 60
#define GTERM_ROWS 22
#define GTERM_SCROLL 200

typedef struct {
    uint32_t win_id;
    char     lines[GTERM_SCROLL][GTERM_COLS + 1];
    int      line_count;
    int      scroll_top;
    char     input_buf[GTERM_COLS];
    int      input_len;
    bool     cursor_visible;
    uint32_t last_blink;
} gterm_state_t;

void gterm_write(gterm_state_t *s, const char *text) {
    int col = 0;
    if (s->line_count > 0) col = kstrlen(s->lines[s->line_count-1]);
    if (s->line_count == 0) { s->line_count = 1; }

    for (; *text; text++) {
        if (*text == '\n' || col >= GTERM_COLS) {
            if (s->line_count < GTERM_SCROLL) s->line_count++;
            else {
                // scroll up
                for (int i = 0; i < GTERM_SCROLL-1; i++)
                    kmemcpy(s->lines[i], s->lines[i+1], GTERM_COLS+1);
            }
            s->lines[s->line_count-1][0] = 0;
            col = 0;
            if (*text == '\n') continue;
        }
        s->lines[s->line_count-1][col++] = *text;
        s->lines[s->line_count-1][col] = 0;
    }
    s->scroll_top = s->line_count > GTERM_ROWS ? s->line_count - GTERM_ROWS : 0;
}

static void gterm_paint(gui_dc_t *dc, uint32_t id, void *ud) {
    (void)id;
    gterm_state_t *s = (gterm_state_t *)ud;

    // Dark terminal background
    gui_dc_clear(dc, 0x000D1117);

    // Lines
    for (int row = 0; row < GTERM_ROWS; row++) {
        int line_idx = s->scroll_top + row;
        if (line_idx >= s->line_count) break;
        gui_dc_draw_text(dc, 4, 4 + row*10, s->lines[line_idx], 0x0039FF14, 0x000D1117);
    }

    // Input line
    int32_t input_y = (int32_t)dc->height - 18;
    gui_dc_fill_rect(dc, 0, input_y, dc->width, 18, 0x00111820);
    gui_dc_draw_line(dc, 0, input_y, (int32_t)dc->width, input_y, 0x00203840);

    char prompt_line[GTERM_COLS + 8];
    prompt_line[0] = '>'; prompt_line[1] = ' ';
    int pi = 2;
    for (int i = 0; i < s->input_len && pi < GTERM_COLS+4; i++)
        prompt_line[pi++] = s->input_buf[i];
    if (s->cursor_visible) prompt_line[pi++] = '_';
    prompt_line[pi] = 0;
    gui_dc_draw_text(dc, 4, input_y + 4, prompt_line, GUI_COLOR_ACCENT, 0x00111820);
}

static void gterm_event(gui_event_t *ev, void *ud) {
    gterm_state_t *s = (gterm_state_t *)ud;

    if (ev->type == GUI_EVENT_CLOSE) {
        kfree(s);
        gui_window_destroy(ev->window_id);
        return;
    }

    if (ev->type == GUI_EVENT_KEY_DOWN) {
        gui_window_invalidate(s->win_id);
        if (ev->key_ascii == '\b' || ev->key_code == KEY_BACKSPACE) {
            if (s->input_len > 0) s->input_buf[--s->input_len] = 0;
        } else if (ev->key_ascii == '\r' || ev->key_ascii == '\n' || ev->key_code == KEY_ENTER) {
            s->input_buf[s->input_len] = 0;
            // Echo and simple command response
            char echo[GTERM_COLS + 4];
            echo[0]='>';echo[1]=' ';
            int ei=2; for(int i=0;s->input_buf[i]&&ei<GTERM_COLS+2;i++) echo[ei++]=s->input_buf[i];
            echo[ei]=0;
            gterm_write(s, echo);
            gterm_write(s, "\n");

            // Handle basic commands
            if (kstrncmp(s->input_buf, "help", 4) == 0) {
                gterm_write(s, "Commands: help, clear, uname, echo <text>\n");
            } else if (kstrncmp(s->input_buf, "clear", 5) == 0) {
                s->line_count = 0;
                s->scroll_top = 0;
            } else if (kstrncmp(s->input_buf, "uname", 5) == 0) {
                gterm_write(s, "Eclipse32 GUI Build - 32-bit x86\n");
            } else if (kstrncmp(s->input_buf, "echo ", 5) == 0) {
                gterm_write(s, s->input_buf + 5);
                gterm_write(s, "\n");
            } else if (s->input_len > 0) {
                gterm_write(s, "Unknown command. Type 'help'.\n");
            }

            s->input_len = 0;
            kmemset(s->input_buf, 0, sizeof(s->input_buf));
        } else if (ev->key_ascii >= 0x20 && ev->key_ascii < 0x7F) {
            if (s->input_len < GTERM_COLS - 1) {
                s->input_buf[s->input_len++] = ev->key_ascii;
                s->input_buf[s->input_len] = 0;
            }
        }
    }

    // Cursor blink via ticks
    uint32_t now = pit_ticks();
    if (now - s->last_blink >= 500) {
        s->cursor_visible = !s->cursor_visible;
        s->last_blink = now;
        gui_window_invalidate(s->win_id);
    }
}

void app_terminal_open(void) {
    gterm_state_t *s = (gterm_state_t *)kmalloc(sizeof(gterm_state_t));
    if (!s) return;
    kmemset(s, 0, sizeof(*s));
    s->cursor_visible = true;
    s->last_blink = 0;
    gterm_write(s, "Eclipse32 GUI Terminal\n");
    gterm_write(s, "Type 'help' for commands.\n\n");
    uint32_t id = gui_window_create(100, 100, 500, 260, "Terminal",
                                    GUI_STYLE_DECORATED, gterm_paint, gterm_event, s);
    s->win_id = id;
}

// ============================================================
// Desktop launcher (icons on desktop)
// ============================================================

#define LAUNCHER_ICONS 4

typedef struct {
    const char *label;
    void (*open)(void);
    int32_t x, y;
} desktop_icon_t;

static desktop_icon_t g_icons[LAUNCHER_ICONS] = {
    { "Terminal",     app_terminal_open,     20,  40 },
    { "Files",        app_filemanager_open,  20, 110 },
    { "Clock",        app_clock_open,        20, 180 },
    { "About",        app_about_open,        20, 250 },
};

// These are called from the GUI main loop's desktop overlay
// (handled in gui_desktop_paint if you add a desktop window, or
//  from the launcher window below)

// ============================================================
// Launcher / Start Menu window
// ============================================================

typedef struct {
    uint32_t win_id;
    int hover;
} launcher_state_t;

static void launcher_paint(gui_dc_t *dc, uint32_t id, void *ud) {
    (void)id;
    launcher_state_t *s = (launcher_state_t *)ud;

    gui_dc_clear(dc, 0x00141D26);

    // Header
    gui_dc_fill_rect(dc, 0, 0, dc->width, 32, GUI_COLOR_ACCENT);
    gui_dc_draw_text(dc, 8, 10, "Eclipse32  Launcher", 0x00FFFFFF, GUI_COLOR_ACCENT);
    gui_dc_draw_line(dc, 0, 32, (int32_t)dc->width, 32, 0x003070C0);

    // App buttons
    for (int i = 0; i < LAUNCHER_ICONS; i++) {
        int32_t by = 40 + i * 52;
        bool hov = (s->hover == i);
        uint32_t bg = hov ? 0x001C3050 : 0x00182838;
        uint32_t border = hov ? GUI_COLOR_ACCENT : 0x00304050;

        gui_dc_fill_rect(dc, 8, by, (int32_t)dc->width - 16, 44, bg);
        gui_dc_draw_rect(dc, 8, by, (int32_t)dc->width - 16, 44, border);

        // App icon placeholder (colored square)
        uint32_t icon_colors[4] = {GUI_COLOR_ACCENT, GUI_COLOR_ACCENT, GUI_COLOR_CLOSE_BTN, 0x00508070};
        gui_dc_fill_rect(dc, 16, by+10, 24, 24, icon_colors[i % 4]);
        gui_dc_draw_rect(dc, 16, by+10, 24, 24, 0x00000000);

        gui_dc_draw_text(dc, 48, by + 10, g_icons[i].label,
            hov ? 0x00FFFFFF : GUI_COLOR_TEXT_LIGHT, bg);
        const char *desc[] = {"Command line interface", "Browse filesystem", "Analog clock widget", "System information"};
        gui_dc_draw_text(dc, 48, by + 24, desc[i], 0x00607080, bg);
    }
}

static void launcher_event(gui_event_t *ev, void *ud) {
    launcher_state_t *s = (launcher_state_t *)ud;

    if (ev->type == GUI_EVENT_CLOSE) {
        kfree(s);
        gui_window_destroy(ev->window_id);
        return;
    }

    if (ev->type == GUI_EVENT_MOUSE_MOVE) {
        s->hover = -1;
        for (int i = 0; i < LAUNCHER_ICONS; i++) {
            int32_t by = 40 + i * 52;
            if (ev->mouse_y >= by && ev->mouse_y < by + 44)
                s->hover = i;
        }
        gui_window_invalidate(s->win_id);
    }

    if (ev->type == GUI_EVENT_MOUSE_DOWN) {
        for (int i = 0; i < LAUNCHER_ICONS; i++) {
            int32_t by = 40 + i * 52;
            if (ev->mouse_y >= by && ev->mouse_y < by + 44) {
                g_icons[i].open();
                // Close launcher after launch
                kfree(s);
                gui_window_destroy(ev->window_id);
                return;
            }
        }
    }
}

void app_launcher_open(void) {
    launcher_state_t *s = (launcher_state_t *)kmalloc(sizeof(launcher_state_t));
    if (!s) return;
    kmemset(s, 0, sizeof(*s));
    s->hover = -1;
    uint32_t id = gui_window_create(400, 200, 240, 260, "Launcher",
                                    GUI_STYLE_DECORATED, launcher_paint, launcher_event, s);
    s->win_id = id;
}
