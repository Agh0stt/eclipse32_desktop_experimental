// =============================================================================
// Eclipse32 — guitest app  (apps/guitest/guitest.c)
//
// Demonstrates the GUI SDK:
//   - Creates a window
//   - Draws a gradient background, labels, a live counter, and a button
//   - Closes itself when the button is clicked
// =============================================================================
#include "../sdk/e32_syscall.h"
#include "../sdk/e32_gui.h"

// Tiny itoa so we don't need libc
static void uitoa(uint32_t v, char *buf) {
    char tmp[12]; int i = 0;
    if (!v) { buf[0]='0'; buf[1]=0; return; }
    while (v) { tmp[i++] = '0' + (int)(v % 10); v /= 10; }
    int j = 0; while (i > 0) buf[j++] = tmp[--i]; buf[j] = 0;
}

void main(void) {
    // Create a 320x220 window, auto-centered
    int win = gui_win_create("GUI Test", -1, -1, 320, 220);
    if (win < 0) { e32_exit(1); }

    uint32_t frame   = 0;
    uint8_t  running = 1;

    while (running) {
        // Refresh content-area rect every frame so drawing tracks the window
        // after it has been moved by the user dragging the title bar.
        gui_rect_t ca;
        gui_win_getrect(win, &ca);

        // Poll mouse for button logic
        gui_mouse_t m;
        gui_get_mouse(&m);
        uint8_t lclick = (m.buttons & 1);

        // --- Draw frame ---

        // Background gradient (simple horizontal bands)
        for (int32_t row = 0; row < ca.h; row++) {
            uint32_t r = (uint32_t)(30  + row / 3);
            uint32_t g = (uint32_t)(80  + row / 4);
            uint32_t b = (uint32_t)(160 + row / 5);
            if (r > 90)  r = 90;
            if (g > 140) g = 140;
            if (b > 210) b = 210;
            gui_draw_hline(ca.x, ca.y + row, ca.w, RGB(r, g, b));
        }

        // Header banner
        gui_fill_rect(ca.x, ca.y, ca.w, 32, RGB(20, 60, 140));
        gui_puts(ca.x + 10, ca.y + 6,  "Eclipse32 GUI SDK Demo", COL_WHITE, RGB(20,60,140));
        gui_puts(ca.x + 10, ca.y + 18, "Hello from an E32 app!", RGB(180,210,255), RGB(20,60,140));

        // Separator
        gui_draw_hline(ca.x, ca.y + 32, ca.w, RGB(100, 140, 220));
        gui_draw_hline(ca.x, ca.y + 33, ca.w, COL_WHITE);

        // Info rows
        int32_t ty = ca.y + 44;
        gui_puts(ca.x + 12, ty, "Status:",     RGB(80,80,80),   RGB(30,90,170));
        gui_puts(ca.x + 90, ty, "Running",     RGB(40,200,40),  RGB(30,90,170));
        ty += 14;

        // Live frame counter
        char fbuf[16];
        uitoa(frame, fbuf);
        gui_puts(ca.x + 12, ty, "Frame:",      RGB(80,80,80),   RGB(30,90,170));
        gui_puts(ca.x + 90, ty, fbuf,          RGB(255,220,60), RGB(30,90,170));
        ty += 14;

        // Mouse position
        char mxbuf[12], mybuf[12];
        uitoa((uint32_t)(m.x < 0 ? 0 : m.x), mxbuf);
        uitoa((uint32_t)(m.y < 0 ? 0 : m.y), mybuf);
        gui_puts(ca.x + 12, ty, "Mouse X:",    RGB(80,80,80),   RGB(30,90,170));
        gui_puts(ca.x + 90, ty, mxbuf,         COL_WHITE,       RGB(30,90,170));
        ty += 14;
        gui_puts(ca.x + 12, ty, "Mouse Y:",    RGB(80,80,80),   RGB(30,90,170));
        gui_puts(ca.x + 90, ty, mybuf,         COL_WHITE,       RGB(30,90,170));
        ty += 20;

        // 3D box demo
        gui_draw_3dbox(ca.x + 12, ty, 120, 20, 1);
        gui_puts(ca.x + 20, ty + 6, "Raised box", RGB(0,0,0), RGB(200,200,200));
        gui_draw_3dbox(ca.x + 144, ty, 120, 20, 0);
        gui_puts(ca.x + 152, ty + 6, "Sunken box", RGB(0,0,0), RGB(200,200,200));
        ty += 30;

        // Rect outline demo
        gui_draw_rect(ca.x + 12, ty, 80, 20, RGB(200,60,60));
        gui_puts(ca.x + 16, ty + 6, "Outline", RGB(200,60,60), RGB(30,90,170));

        // Pulsing dot
        uint32_t phase = (frame / 10) % 4;
        const char *dots[4] = { "o   ", "oo  ", "ooo ", "oooo" };
        gui_puts(ca.x + 110, ty + 6, dots[phase], RGB(255,180,0), RGB(30,90,170));

        // Close button at bottom
        int32_t btn_x = ca.x + (ca.w - 80) / 2;
        int32_t btn_y = ca.y + ca.h - 28;
        if (gui_button(btn_x, btn_y, 80, 18, "Close", m.x, m.y, lclick)) {
            running = 0;
        }

        frame++;
        // gui_pump returns -1 if the desktop closed our window (X button).
        if (gui_pump() < 0) break;
    }

    gui_win_close(win);
    e32_exit(0);
}
