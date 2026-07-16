// =============================================================================
// Eclipse32 — newfeatures app  (apps/newfeatures/newfeatures.c)
// =============================================================================
#include "e32_gui.h"
#include "e32_syscall.h"

static void uitoa(uint32_t v, char *buf) {
    char tmp[12]; int i = 0;
    if (!v) { buf[0]='0'; buf[1]=0; return; }
    while (v) { tmp[i++] = '0' + (int)(v % 10); v /= 10; }
    int j = 0; while (i > 0) buf[j++] = tmp[--i]; buf[j] = 0;
}
static int kstrlen(const char *s) { int n=0; while(s[n]) n++; return n; }

#define IMG_W 8
#define IMG_H 8
static const uint32_t test_image[IMG_W * IMG_H] = {
    0xFFFF0000,0xFFFF4000,0xFFFF8000,0xFFFFBF00,0xFFFFFF00,0xFF80FF00,0xFF00FF00,0xFF00FF80,
    0xFFFF2000,0xFFFF6000,0xFFFFA000,0xFFFFDF00,0xFFDFFF00,0xFF60FF00,0xFF00FF40,0xFF00FFC0,
    0xFF00FFFF,0xFF00BFFF,0xFF0080FF,0xFF0040FF,0xFF0000FF,0xFF4000FF,0xFF8000FF,0xFFBF00FF,
    0xFF20DFFF,0xFF20AFFF,0xFF2070FF,0xFF2030FF,0xFF2000DF,0xFF6000BF,0xFF9000BF,0xFFBF00DF,
    0xFFFF00FF,0xFFFF00BF,0xFFFF0080,0xFFFF0040,0xFFFF0000,0xFFBF0020,0xFF800040,0xFF400060,
    0xFFDF20DF,0xFFDF20AF,0xFFDF2070,0xFFDF2030,0xFFDF2000,0xFFBF2020,0xFF802040,0xFF402060,
    0xFF808080,0xFFA0A0A0,0xFFC0C0C0,0xFFE0E0E0,0xFFFFFFFF,0xFFE0E0E0,0xFFC0C0C0,0xFFA0A0A0,
    0xFF404040,0xFF606060,0xFF808080,0xFFA0A0A0,0xFFC0C0C0,0xFF808080,0xFF606060,0xFF404040,
};

static const char *pulse_str(uint32_t ticks) {
    static const char *f[4] = {"o   ","oo  ","ooo ","oooo"};
    return f[(ticks/200)%4];
}

void main(void) {
    int win = gui_win_create("New GUI Features Demo", -1, -1, 480, 420);
    if (win < 0) e32_exit(1);

    gui_input_t name_field = { {0}, 1, 0, 24 };
    gui_input_t pass_field = { {0}, 0, 1, 16 };

    uint8_t dark_mode   = 0;
    uint8_t sound_on    = 1;
    int32_t volume      = 60;
    int32_t brightness  = 80;
    uint8_t running     = 1;
    int     last_msgbox = -1;
    static char picked[64] = {0};

    uint8_t prev_btn = 0;

    while (running) {
        gui_event_t ev;
        int ev_type = gui_poll_event(&ev);

        int32_t mx  = ev.mouse_x;
        int32_t my  = ev.mouse_y;
        uint8_t btn = (uint8_t)ev.mouse_btn;

        uint8_t lclick = (uint8_t)(btn & 1) & (uint8_t)(~prev_btn & 1);
        prev_btn = btn;

        char key = (ev_type == GUI_EVENT_KEYDOWN) ? (char)ev.key_ascii : 0;

        if (key == '\t') {
            if (name_field.focused) { name_field.focused=0; pass_field.focused=1; }
            else                    { pass_field.focused=0; name_field.focused=1; }
            key = 0;
        }

        uint32_t ticks = gui_get_ticks();

        gui_rect_t ca;
        gui_win_getrect(win, &ca);

        uint32_t bg  = dark_mode ? RGB(30,30,40)  : RGB(236,233,216);
        uint32_t lbl = dark_mode ? COL_LTGREY     : COL_BLACK;
        uint32_t sec = dark_mode ? RGB(60,80,120) : RGB(180,200,230);
        gui_fill_rect(ca.x, ca.y, ca.w, ca.h, bg);

        uint32_t ban = dark_mode ? RGB(20,40,80) : RGB(20,60,140);
        gui_fill_rect(ca.x, ca.y, ca.w, 28, ban);
        gui_puts(ca.x+10, ca.y+8, "Eclipse32  Extended GUI Widget Demo", COL_WHITE, ban);

        int32_t y = ca.y + 36;

        // ---- Section 1: Text inputs ---------------------------------------
        gui_fill_rect(ca.x+4, y, ca.w-8, 13, sec);
        gui_puts(ca.x+8, y+2, "Text Input  (click to focus | Tab to switch)", COL_BLACK, sec);
        y += 15;

        gui_puts(ca.x+8, y+4,  "Name:",     lbl, bg);
        gui_puts(ca.x+8, y+22, "Password:", lbl, bg);

        gui_input_draw(ca.x+72, y,    240, 16, &name_field,
                       mx, my, lclick, name_field.focused ? key : 0);
        gui_input_draw(ca.x+72, y+18, 240, 16, &pass_field,
                       mx, my, lclick, pass_field.focused ? key : 0);

        char lenbuf[8]; uitoa((uint32_t)kstrlen(name_field.buf), lenbuf);
        gui_puts(ca.x+320, y+4,  "len:", lbl, bg);
        gui_puts(ca.x+356, y+4,  lenbuf, lbl, bg);
        gui_puts(ca.x+320, y+22,
                 name_field.focused ? "focus:name" : "focus:pass",
                 RGB(80,160,255), bg);
        y += 44;

        // ---- Section 2: Checkboxes ----------------------------------------
        gui_fill_rect(ca.x+4, y, ca.w-8, 13, sec);
        gui_puts(ca.x+8, y+2, "Checkboxes", COL_BLACK, sec);
        y += 15;

        gui_checkbox(ca.x+8,   y, "Dark mode", &dark_mode, mx, my, lclick);
        gui_checkbox(ca.x+130, y, "Sound on",  &sound_on,  mx, my, lclick);
        gui_puts(ca.x+260, y, pulse_str(ticks),
                 sound_on ? RGB(40,200,40) : RGB(180,40,40), bg);
        y += 22;

        // ---- Section 3: Sliders -------------------------------------------
        gui_fill_rect(ca.x+4, y, ca.w-8, 13, sec);
        gui_puts(ca.x+8, y+2, "Sliders", COL_BLACK, sec);
        y += 15;

        gui_puts(ca.x+8, y+2,  "Volume:",     lbl, bg);
        gui_puts(ca.x+8, y+18, "Brightness:", lbl, bg);
        gui_slider(ca.x+80, y,    240, 0, 100, &volume,     mx, my, btn);
        gui_slider(ca.x+80, y+16, 240, 0, 100, &brightness, mx, my, btn);

        char vbuf[8], bbuf[8];
        uitoa((uint32_t)volume,     vbuf);
        uitoa((uint32_t)brightness, bbuf);
        gui_puts(ca.x+328, y,    vbuf, lbl, bg);
        gui_puts(ca.x+328, y+16, bbuf, lbl, bg);
        gui_fill_rect(ca.x+352, y,    volume*80/100,     8, RGB(40,160,255));
        gui_fill_rect(ca.x+352, y+10, brightness*80/100, 8, RGB(255,200,40));
        y += 34;

        // ---- Section 4: Circles + image blit ------------------------------
        gui_fill_rect(ca.x+4, y, ca.w-8, 13, sec);
        gui_puts(ca.x+8, y+2, "Circles & Image Blit", COL_BLACK, sec);
        y += 15;

        uint32_t ph = ticks/500;
        gui_fill_circle(ca.x+24,  y+16, 14, RGB(200-(ph%3)*40, 40, 40));
        gui_fill_circle(ca.x+62,  y+16, 14, RGB(40, 200-(ph%3)*40, 40));
        gui_fill_circle(ca.x+100, y+16, 14, RGB(40, 40, 200-(ph%3)*40));
        gui_draw_circle(ca.x+24,  y+16, 15, COL_BLACK);
        gui_draw_circle(ca.x+62,  y+16, 15, COL_BLACK);
        gui_draw_circle(ca.x+100, y+16, 15, COL_BLACK);

        int32_t pr = 10 + (int32_t)((ticks/120)%6);
        gui_draw_circle(ca.x+142, y+16, pr, RGB(255,160,0));
        gui_puts(ca.x+128, y+34, "pulse", lbl, bg);

        gui_draw_image(ca.x+178, y+8, IMG_W, IMG_H, test_image);
        gui_puts(ca.x+170, y+34, "8x8 blit", lbl, bg);

        gui_draw_3dbox(ca.x+210, y+4, 90, 26, (uint8_t)((ticks/700)&1));
        gui_puts(ca.x+214, y+14, "3D box", lbl, RGB(200,200,200));
        y += 50;

        // ---- Section 5: msgbox --------------------------------------------
        gui_fill_rect(ca.x+4, y, ca.w-8, 13, sec);
        gui_puts(ca.x+8, y+2, "Modal Dialog  (gui_msgbox)", COL_BLACK, sec);
        y += 15;

        if (gui_button(ca.x+8,   y, 110, 18, "OK dialog",   mx, my, lclick))
            last_msgbox = gui_msgbox("Info", "Blocking modal dialog.\nPress OK to continue.", 0);
        if (gui_button(ca.x+128, y, 120, 18, "OK / Cancel", mx, my, lclick))
            last_msgbox = gui_msgbox("Confirm", "Proceed?", 1);
        if (last_msgbox >= 0)
            gui_puts(ca.x+260, y+3,
                     last_msgbox ? "  OK pressed" : "Cancel pressed",
                     last_msgbox ? RGB(40,200,40) : RGB(200,40,40), bg);
        y += 26;

        // ---- Section 6: File Picker ---------------------------------------
        gui_fill_rect(ca.x+4, y, ca.w-8, 13, sec);
        gui_puts(ca.x+8, y+2, "File Picker  (gui_filepick)", COL_BLACK, sec);
        y += 15;

        if (gui_button(ca.x+8, y, 110, 18, "Pick File", mx, my, lclick))
            gui_filepick(picked, sizeof(picked), NULL, "Open File");
        if (picked[0])
            gui_puts(ca.x+128, y+3, picked, RGB(255,180,0), bg);
        y += 22;

        // ---- Ticks --------------------------------------------------------
        char tbuf[12]; uitoa(ticks, tbuf);
        gui_puts(ca.x+8,  y, "Ticks:", lbl, bg);
        gui_puts(ca.x+60, y, tbuf, RGB(255,180,0), bg);

        // ---- Close --------------------------------------------------------
        if (gui_button(ca.x+(ca.w-80)/2, ca.y+ca.h-22, 80, 18,
                       "Close", mx, my, lclick))
            running = 0;

        if (gui_pump() < 0) break;
    }

    gui_win_close(win);
    e32_exit(0);
}
