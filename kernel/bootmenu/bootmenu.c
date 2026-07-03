// =============================================================================
// Eclipse32 - Boot Menu (splash F-key hotkeys)
//
// Drawn right after VBE init, before the GUI/initramfs path is chosen, so
// the hotkeys work no matter which boot path follows.
//
//   F1  Kernel Panic / Upload Mode  - shows the last persisted panic (if any)
//                                     and waits in an upload-mode stub
//   F2  Boot Menu (TUI)             - Diagnose / Software Reset
//   F3  Force VGA text mode         - skips VBE/GUI for this boot
//   F4  Software Update             - stub, design TBD
//   F5  Reserved                    - does nothing yet
// =============================================================================
#include "bootmenu.h"
#include "../kernel.h"
#include "../drivers/vbe/vbe.h"
#include "../drivers/vga/vga.h"
#include "../drivers/keyboard/keyboard.h"
#include "../fs/fat32/fat32.h"
#include "../arch/x86/pit.h"

// kernel-internal string/mem helpers (defined in initramfs.c, used
// implicitly across the kernel the same way shell.c / gui_desktop.c do)
size_t kstrlen(const char *s);
char  *kstrcpy(char *dst, const char *src);
char  *kstrncpy(char *dst, const char *src, size_t n);
int    kstrcmp(const char *a, const char *b);
void  *kmemset(void *s, int c, size_t n);

// ---------------------------------------------------------------------------
// Panic info persistence (survives warm reboot - BIOS keyboard-controller
// reset does not clear RAM). Fixed low-memory scratch address, well below
// the kernel load address and above the BIOS/IVT area.
// ---------------------------------------------------------------------------
#define PANIC_INFO_ADDR  0x00090000u

static panic_info_t *panic_info(void) {
    return (panic_info_t *)PANIC_INFO_ADDR;
}

void bootmenu_record_panic(const char *msg) {
    panic_info_t *pi = panic_info();
    pi->magic = PANIC_INFO_MAGIC;
    size_t n = kstrlen(msg);
    if (n >= PANIC_MSG_MAX) n = PANIC_MSG_MAX - 1;
    for (size_t i = 0; i < n; i++) pi->message[i] = msg[i];
    pi->message[n] = 0;
}

// ---------------------------------------------------------------------------
// Machine reboot - pulse the keyboard controller's reset line (port 0x64,
// command 0xFE). Standard PC reset trick, works under QEMU and real HW.
// ---------------------------------------------------------------------------
void machine_reboot(void) {
    cli();
    // Wait for the controller's input buffer to be clear first.
    for (int timeout = 0; timeout < 100000; timeout++) {
        if (!(inb(0x64) & 0x02)) break;
    }
    outb(0x64, 0xFE);
    // Fallback if the controller reset didn't take: halt forever.
    for (;;) hlt();
}

// ---------------------------------------------------------------------------
// Small helpers shared by the screens below
// ---------------------------------------------------------------------------
static void clear_screen(void) {
    vbe_clear(ECLIPSE_BG);
}

static void header(const char *title) {
    clear_screen();
    vbe_set_text_color(ECLIPSE_ACCENT, 0x00000000);
    vbe_set_cursor(2, 1);
    vbe_puts("Eclipse32 Boot Menu");
    vbe_set_text_color(ECLIPSE_FG, 0x00000000);
    vbe_set_cursor(2, 2);
    vbe_puts(title);
    vbe_draw_line(0, 3 * 16 + 4, vbe_get_width(), 3 * 16 + 4, ECLIPSE_ACCENT);
}

static void wait_keyup(void) {
    // Drain events until we see a release, so the same physical keypress
    // that opened a screen doesn't immediately re-trigger inside it.
    while (keyboard_has_event()) {
        key_event_t e = keyboard_get_event();
        if (e.released) break;
    }
}

// ---------------------------------------------------------------------------
// F1 - Kernel Panic / Upload Mode
// ---------------------------------------------------------------------------
static void mode_panic_upload(void) {
    header("F1: Kernel Panic / Upload Mode");

    vbe_set_cursor(2, 5);
    panic_info_t *pi = panic_info();
    if (pi->magic == PANIC_INFO_MAGIC && pi->message[0]) {
        vbe_set_text_color(ECLIPSE_RED, 0x00000000);
        vbe_puts("Last kernel panic:");
        vbe_set_cursor(4, 6);
        vbe_set_text_color(ECLIPSE_YELLOW, 0x00000000);
        vbe_puts(pi->message);
    } else {
        vbe_set_text_color(ECLIPSE_GREEN, 0x00000000);
        vbe_puts("No panic on record.");
    }

    vbe_set_text_color(ECLIPSE_CYAN, 0x00000000);
    vbe_set_cursor(2, 9);
    vbe_puts("[ Upload mode: waiting for firmware/image upload... ]");
    vbe_set_cursor(2, 10);
    vbe_set_text_color(ECLIPSE_FG, 0x00000000);
    vbe_puts("(stub - transport not yet implemented)");

    vbe_set_cursor(2, 22);
    vbe_set_text_color(ECLIPSE_FG, 0x00000000);
    vbe_puts("ESC: return to boot menu   R: reboot");

    for (;;) {
        if (keyboard_has_event()) {
            key_event_t e = keyboard_get_event();
            if (e.released) continue;
            if (e.keycode == KEY_ASCII && e.ascii == 27) return; // ESC
            if (e.keycode == KEY_ASCII && (e.ascii == 'r' || e.ascii == 'R')) machine_reboot();
        }
    }
}

// ---------------------------------------------------------------------------
// F2 - Boot Menu TUI: Diagnose / Software Reset
// ---------------------------------------------------------------------------
#define BOOTMENU_ITEM_COUNT 2
static const char *bootmenu_items[BOOTMENU_ITEM_COUNT] = {
    "Diagnose",
    "Software Reset",
};

static void draw_tui_menu(int selected) {
    header("F2: Boot Menu");

    uint32_t box_x = 2, box_y = 6, box_h = BOOTMENU_ITEM_COUNT + 2;
    vbe_set_text_color(ECLIPSE_ACCENT, 0x00000000);
    vbe_set_cursor(box_x, box_y);
    vbe_puts("+--------------------------------------+");
    for (uint32_t r = 1; r <= (uint32_t)BOOTMENU_ITEM_COUNT; r++) {
        vbe_set_cursor(box_x, box_y + r);
        vbe_puts("|                                        |");
    }
    vbe_set_cursor(box_x, box_y + box_h - 1);
    vbe_puts("+--------------------------------------+");

    for (int i = 0; i < BOOTMENU_ITEM_COUNT; i++) {
        vbe_set_cursor(box_x + 2, box_y + 1 + i);
        if (i == selected) {
            vbe_set_text_color(ECLIPSE_BG, ECLIPSE_ACCENT);
            vbe_printf("> %s", bootmenu_items[i]);
            vbe_set_text_color(ECLIPSE_FG, 0x00000000);
        } else {
            vbe_set_text_color(ECLIPSE_FG, 0x00000000);
            vbe_printf("  %s", bootmenu_items[i]);
        }
    }

    vbe_set_cursor(box_x, box_y + box_h + 1);
    vbe_set_text_color(ECLIPSE_CYAN, 0x00000000);
    vbe_puts("UP/DOWN: select   ENTER: choose   ESC: back");
}

static void mode_diagnose(void) {
    header("Diagnose");

    vbe_set_cursor(2, 5);
    vbe_set_text_color(ECLIPSE_FG, 0x00000000);
    vbe_puts("Filesystem : ");
    vbe_set_text_color(fs_is_mounted() ? ECLIPSE_GREEN : ECLIPSE_RED, 0x00000000);
    vbe_puts(fs_is_mounted() ? "FAT32 mounted" : "Not mounted");

    vbe_set_text_color(ECLIPSE_FG, 0x00000000);
    vbe_set_cursor(2, 6);
    vbe_puts("Display    : ");
    vbe_set_text_color(ECLIPSE_GREEN, 0x00000000);
    vbe_printf("%ux%u, VBE active", vbe_get_width(), vbe_get_height());

    vbe_set_text_color(ECLIPSE_FG, 0x00000000);
    vbe_set_cursor(2, 7);
    vbe_puts("Keyboard   : ");
    vbe_set_text_color(ECLIPSE_GREEN, 0x00000000);
    vbe_puts("OK (this screen responds to it)");

    vbe_set_cursor(2, 22);
    vbe_set_text_color(ECLIPSE_FG, 0x00000000);
    vbe_puts("Press any key to return...");

    wait_keyup();
    for (;;) {
        if (keyboard_has_event()) {
            key_event_t e = keyboard_get_event();
            if (!e.released) return;
        }
    }
}

// Recursively delete everything under `path` (files and subdirectories),
// then delete `path` itself. Best-effort: continues past individual
// failures so one bad entry can't abort the whole wipe.
#define WIPE_MAX_ENTRIES 64
static void wipe_recursive(const char *path) {
    static fat32_dir_entry_t entries[WIPE_MAX_ENTRIES];
    int count = fat32_readdir(path, entries, WIPE_MAX_ENTRIES);
    if (count < 0) return;

    char child[300];
    for (int i = 0; i < count; i++) {
        kstrcpy(child, path);
        size_t plen = kstrlen(child);
        if (plen == 0 || child[plen - 1] != '/') kstrcpy(child + plen, "/");
        kstrcpy(child + kstrlen(child), entries[i].name);

        if (entries[i].is_dir) {
            wipe_recursive(child);
        }
        fat32_delete(child);
    }
}

static void mode_software_reset(void) {
    header("Software Reset");

    vbe_set_text_color(ECLIPSE_RED, 0x00000000);
    vbe_set_cursor(2, 6);
    vbe_puts("Software reset will WIPE all user files and");
    vbe_set_cursor(2, 7);
    vbe_puts("perform a fresh install.");

    vbe_set_text_color(ECLIPSE_FG, 0x00000000);
    vbe_set_cursor(2, 9);
    vbe_puts("Do you want to continue?");
    vbe_set_cursor(2, 10);
    vbe_set_text_color(ECLIPSE_CYAN, 0x00000000);
    vbe_puts("[Y] Yes, confirm     [N] No, abort");

    wait_keyup();
    for (;;) {
        if (!keyboard_has_event()) continue;
        key_event_t e = keyboard_get_event();
        if (e.released) continue;

        if (e.ascii == 'n' || e.ascii == 'N' || (e.keycode == KEY_ASCII && e.ascii == 27)) {
            return; // abort, back to F2 menu
        }
        if (e.ascii == 'y' || e.ascii == 'Y') {
            vbe_set_cursor(2, 12);
            vbe_set_text_color(ECLIPSE_YELLOW, 0x00000000);
            vbe_puts("Wiping user files...");

            if (fs_is_mounted()) {
                wipe_recursive("/home");
                wipe_recursive("/root");
                fat32_mkdir("/home");
                fat32_mkdir("/root");
            }

            vbe_set_cursor(2, 13);
            vbe_set_text_color(ECLIPSE_GREEN, 0x00000000);
            vbe_puts("Done. Rebooting...");

            // Tiny delay so the message is actually visible before reboot.
            pit_sleep_ms(1500);

            machine_reboot();
        }
    }
}

static void mode_boot_menu(void) {
    int selected = 0;
    draw_tui_menu(selected);
    wait_keyup();

    for (;;) {
        if (!keyboard_has_event()) continue;
        key_event_t e = keyboard_get_event();
        if (e.released) continue;

        if (e.keycode == KEY_UP) {
            selected = (selected - 1 + BOOTMENU_ITEM_COUNT) % BOOTMENU_ITEM_COUNT;
            draw_tui_menu(selected);
        } else if (e.keycode == KEY_DOWN) {
            selected = (selected + 1) % BOOTMENU_ITEM_COUNT;
            draw_tui_menu(selected);
        } else if (e.keycode == KEY_ASCII && e.ascii == 27) {
            return; // ESC
        } else if (e.keycode == KEY_ASCII && (e.ascii == '\n' || e.ascii == '\r')) {
            if (selected == 0) mode_diagnose();
            else mode_software_reset(); // returns here if aborted
            draw_tui_menu(selected);
            wait_keyup();
        }
    }
}

// ---------------------------------------------------------------------------
// F4 - Software Update (stub)
// ---------------------------------------------------------------------------
static void mode_software_update(void) {
    header("F4: Software Update");

    vbe_set_cursor(2, 6);
    vbe_set_text_color(ECLIPSE_YELLOW, 0x00000000);
    vbe_puts("Not yet implemented.");
    vbe_set_cursor(2, 7);
    vbe_set_text_color(ECLIPSE_FG, 0x00000000);
    vbe_puts("Software update design is TBD.");

    vbe_set_cursor(2, 22);
    vbe_puts("Press any key to return...");

    wait_keyup();
    for (;;) {
        if (keyboard_has_event()) {
            key_event_t e = keyboard_get_event();
            if (!e.released) return;
        }
    }
}

// ---------------------------------------------------------------------------
// Entry point - called from kmain.c right after VBE init / splash draw
// ---------------------------------------------------------------------------
#define BOOTMENU_WINDOW_MS  2500

bootmenu_result_t bootmenu_check(void) {
    vbe_set_text_color(ECLIPSE_CYAN, 0x00000000);
    vbe_set_cursor(2, 24);
    vbe_puts("F1 Upload Mode  F2 Boot Menu  F3 VGA Text  F4 Update");

    uint32_t start = pit_ms();
    while (pit_ms() - start < BOOTMENU_WINDOW_MS) {
        if (!keyboard_has_event()) continue;
        key_event_t e = keyboard_get_event();
        if (e.released) continue;

        switch (e.keycode) {
            case KEY_F1:
                mode_panic_upload();
                return BOOTMENU_CONTINUE;
            case KEY_F2:
                mode_boot_menu();
                return BOOTMENU_CONTINUE;
            case KEY_F3:
                return BOOTMENU_FORCE_TEXT;
            case KEY_F4:
                mode_software_update();
                return BOOTMENU_CONTINUE;
            case KEY_F5:
                // Reserved - does nothing yet.
                break;
            default:
                break;
        }
    }
    return BOOTMENU_CONTINUE;
}
