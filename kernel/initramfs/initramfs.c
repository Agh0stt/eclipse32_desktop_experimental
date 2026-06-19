// =============================================================================
// Eclipse32 - InitRAMFS
// Initializes the runtime environment: env vars, virtual mounts, then
// launches the Eclipse Shell (esh)
// =============================================================================
#include "initramfs.h"
#include "../kernel.h"
#include "../drivers/vbe/vbe.h"
#include "../drivers/vga/vga.h"
#include "../drivers/keyboard/keyboard.h"
#include "../mm/heap.h"
#include "../../shell/shell.h"
// Freestanding memcpy - required by compiler for struct/array copies
void *memcpy(void *dst, const void *src, size_t n) {
    return kmemcpy(dst, src, n);
}

void *memset(void *s, int c, size_t n) {
    return kmemset(s, c, n);
}

void *memmove(void *dst, const void *src, size_t n) {
    uint8_t *d = dst;
    const uint8_t *s = src;
    if (d < s) {
        while (n--) *d++ = *s++;
    } else {
        d += n; s += n;
        while (n--) *--d = *--s;
    }
    return dst;
}
// ---- Environment variable store ----
#define MAX_ENV_VARS    64
#define ENV_KEY_LEN     64
#define ENV_VAL_LEN     256

typedef struct {
    bool used;
    char key[ENV_KEY_LEN];
    char value[ENV_VAL_LEN];
} env_var_t;

static env_var_t env_table[MAX_ENV_VARS];

#define LOGIN_INPUT_MAX 64
#define ROOT_USERNAME   "root"
#define ROOT_PASSWORD   "root"

void env_set(const char *key, const char *value) {
    // Update existing
    for (int i = 0; i < MAX_ENV_VARS; i++) {
        if (env_table[i].used) {
            if (kstrcmp(env_table[i].key, key) == 0) {
                kstrncpy(env_table[i].value, value, ENV_VAL_LEN - 1);
                return;
            }
        }
    }
    // Add new
    for (int i = 0; i < MAX_ENV_VARS; i++) {
        if (!env_table[i].used) {
            env_table[i].used = true;
            kstrncpy(env_table[i].key, key, ENV_KEY_LEN - 1);
            kstrncpy(env_table[i].value, value, ENV_VAL_LEN - 1);
            return;
        }
    }
}

const char *env_get(const char *key) {
    for (int i = 0; i < MAX_ENV_VARS; i++) {
        if (env_table[i].used && kstrcmp(env_table[i].key, key) == 0) {
            return env_table[i].value;
        }
    }
    return NULL;
}

void env_unset(const char *key) {
    for (int i = 0; i < MAX_ENV_VARS; i++) {
        if (env_table[i].used && kstrcmp(env_table[i].key, key) == 0) {
            env_table[i].used = false;
            return;
        }
    }
}

int env_list(env_var_t **out) {
    *out = env_table;
    return MAX_ENV_VARS;
}

static void console_clear(void) {
    if (vbe_active()) {
        vbe_clear(ECLIPSE_BG);
        vbe_set_text_color(ECLIPSE_FG, ECLIPSE_BG);
        vbe_set_cursor(0, 0);
    } else {
        vga_clear();
        vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    }
}

static void console_putc(char c) {
    if (vbe_active()) vbe_putchar(c);
    else vga_putchar(c);
}

static void console_puts(const char *s) {
    while (*s) console_putc(*s++);
}

static void login_readline(char *buf, int max, bool mask_input) {
    int len = 0;
    buf[0] = 0;

    while (1) {
        key_event_t ev = keyboard_wait_event();
        if (ev.released) continue;

        if (ev.keycode == KEY_ENTER || (ev.keycode == KEY_ASCII && (ev.ascii == '\n' || ev.ascii == '\r'))) {
            buf[len] = 0;
            console_putc('\n');
            return;
        }

        if (ev.keycode == KEY_BACKSPACE || (ev.keycode == KEY_ASCII && ev.ascii == '\b')) {
            if (len > 0) {
                len--;
                buf[len] = 0;
                console_putc('\b');
                console_putc(' ');
                console_putc('\b');
            }
            continue;
        }

        if (ev.keycode != KEY_ASCII || ev.ascii == 0) continue;
        if (ev.ascii < 32 || ev.ascii > 126) continue;
        if (len >= max - 1) continue;

        buf[len++] = ev.ascii;
        buf[len] = 0;
        console_putc(mask_input ? '*' : ev.ascii);
    }
}

static void draw_text_logo(void) {
    console_puts("============================================================\n");
    console_puts("                     Eclipse32 Operating System              \n");
    console_puts("============================================================\n");
    console_puts("   ______     __ _                  ____ ___                \n");
    console_puts("  / ____/____/ /(_)___  _________  / __ \\__ \\               \n");
    console_puts(" / __/ / ___/ / / __ \\/ ___/ ___/ / /_/ /_/ /               \n");
    console_puts("/ /___/ /__/ / / /_/ (__  ) /__  / ____/ __/                \n");
    console_puts("/_____/\\___/_/_/ .___/____/\\___/ /_/   /____/               \n");
    console_puts("              /_/                                            \n");
    console_puts("\n");
    console_puts("Welcome to Eclipse32.\n");
    console_puts("Login required to start the shell.\n\n");
}

static void login_screen(void) {
    char username[LOGIN_INPUT_MAX];
    char password[LOGIN_INPUT_MAX];

    while (1) {
        console_clear();
        draw_text_logo();

        console_puts("eclipse login: ");
        login_readline(username, sizeof(username), false);

        console_puts("Password: ");
        login_readline(password, sizeof(password), true);

        if (kstrcmp(username, ROOT_USERNAME) == 0 && kstrcmp(password, ROOT_PASSWORD) == 0) {
            env_set("USER", ROOT_USERNAME);
            env_set("LOGNAME", ROOT_USERNAME);
            env_set("HOME", "/root");
            env_set("PWD", "/root");
            env_set("OLDPWD", "/root");
            return;
        }

        console_puts("\nLogin incorrect.\n");
        console_puts("Valid default credentials: root / root\n");
        console_puts("Press Enter to try again...");
        login_readline(username, sizeof(username), false);
    }
}

// ---- Boot splash screen (VBE graphical) ----
static void draw_boot_splash(void) {
    if (!vbe_active()) return;

    // Background gradient: dark to blue
    for (uint32_t y = 0; y < 768; y++) {
        uint32_t r = 0;
        uint32_t g = (y * 5) / 768;
        uint32_t b = (y * 30) / 768;
        uint32_t color = (r << 16) | (g << 8) | b;
        vbe_fill_rect(0, y, 1024, 1, color);
    }

    // Draw Eclipse32 ASCII art logo
    vbe_set_text_color(ECLIPSE_ACCENT, 0x00000000);
    vbe_set_cursor(24, 8);
    vbe_puts(" ___        _ _                 ___  ____  ");
    vbe_set_cursor(24, 9);
    vbe_puts("| __| __ | (_) _ __ ___ ___ |_  )|_  /");
    vbe_set_cursor(24, 10);
    vbe_puts("| _|| / _| | | '_ (_-</ -_) / /  / / ");
    vbe_set_cursor(24, 11);
    vbe_puts("|___||_\\__|_|_| .__//__/\\___| /___| /___|");
    vbe_set_cursor(24, 12);
    vbe_puts("              |_|                         ");

    vbe_set_text_color(ECLIPSE_FG, 0x00000000);
    vbe_set_cursor(30, 14);
    vbe_puts("32-bit Protected Mode Operating System");
    vbe_set_cursor(35, 15);
    vbe_puts("Version 0.1.0 - ALPHA");

    // Decorative line
    vbe_draw_line(200, 17*16, 824, 17*16, ECLIPSE_ACCENT);

    vbe_set_text_color(ECLIPSE_GREEN, 0x00000000);
    vbe_set_cursor(32, 19);
    vbe_puts("[ Initializing environment... ]");
}

static void draw_init_progress(const char *msg, int step, int total) {
    if (!vbe_active()) return;

    // Progress bar
    uint32_t bar_x = 200, bar_y = 21 * 16, bar_w = 624, bar_h = 12;
    vbe_draw_rect(bar_x, bar_y, bar_w, bar_h, ECLIPSE_ACCENT);
    uint32_t filled = (bar_w - 2) * step / total;
    vbe_fill_rect(bar_x + 1, bar_y + 1, filled, bar_h - 2, ECLIPSE_ACCENT);

    // Clear message line and print new message
    vbe_fill_rect(0, 23 * 16, 1024, 16, 0x00000000);
    vbe_set_text_color(ECLIPSE_CYAN, 0x00000000);
    vbe_set_cursor(2, 23);
    vbe_puts("  >> ");
    vbe_puts(msg);
}

// ---- String library stubs (kernel-internal) ----
// These are declared in shell.h and defined here for the initramfs
int kstrcmp(const char *a, const char *b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

int kstrncmp(const char *a, const char *b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i]) return (unsigned char)a[i] - (unsigned char)b[i];
        if (a[i] == 0) return 0;
    }
    return 0;
}

char *kstrcpy(char *dst, const char *src) {
    char *ret = dst;
    while ((*dst++ = *src++));
    return ret;
}

char *kstrncpy(char *dst, const char *src, size_t n) {
    char *ret = dst;
    while (n > 0 && *src) { *dst++ = *src++; n--; }
    while (n > 0) { *dst++ = 0; n--; }
    return ret;
}

size_t kstrlen(const char *s) {
    const char *p = s;
    while (*p) p++;
    return p - s;
}

char *kstrcat(char *dst, const char *src) {
    char *ret = dst;
    while (*dst) dst++;
    while ((*dst++ = *src++));
    return ret;
}

char *kstrchr(const char *s, int c) {
    while (*s) {
        if (*s == (char)c) return (char *)s;
        s++;
    }
    return c == 0 ? (char *)s : NULL;
}

char *kstrrchr(const char *s, int c) {
    const char *last = NULL;
    while (*s) {
        if (*s == (char)c) last = s;
        s++;
    }
    return (char *)last;
}

void *kmemset(void *s, int c, size_t n) {
    uint8_t *p = (uint8_t *)s;
    while (n--) *p++ = (uint8_t)c;
    return s;
}

void *kmemcpy(void *dst, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    while (n--) *d++ = *s++;
    return dst;
}

int kmemcmp(const void *a, const void *b, size_t n) {
    const uint8_t *p = (const uint8_t *)a;
    const uint8_t *q = (const uint8_t *)b;
    while (n--) {
        if (*p != *q) return *p - *q;
        p++; q++;
    }
    return 0;
}

int katoi(const char *s) {
    int n = 0, sign = 1;
    while (*s == ' ') s++;
    if (*s == '-') { sign = -1; s++; }
    else if (*s == '+') s++;
    while (*s >= '0' && *s <= '9') n = n * 10 + (*s++ - '0');
    return sign * n;
}

// ---- InitRAMFS Main ----
void initramfs_start(void) {
    // Clear env table
    for (int i = 0; i < MAX_ENV_VARS; i++) env_table[i].used = false;

    // Draw graphical splash if VBE is available
    draw_boot_splash();

    // Set up default environment
    int step = 0, total = 10;

    draw_init_progress("Setting up environment variables...", ++step, total);
    env_set("PATH",    "/bin:/usr/bin:/sbin");
    env_set("HOME",    "/root");
    env_set("USER",    ROOT_USERNAME);
    env_set("LOGNAME", ROOT_USERNAME);
    env_set("SHELL",   "/bin/esh");
    env_set("TERM",    "eclipse-vt100");
    env_set("OS",      "Eclipse32");
    env_set("VERSION", "0.1.0");
    env_set("ARCH",    "x86-32");
    env_set("HOSTNAME","eclipse");
    env_set("PWD",     "/");
    env_set("OLDPWD",  "/");
    env_set("PS1",     "\\u@\\h:\\w$ ");
    env_set("IFS",     " \t\n");
    env_set("?",       "0");            // last exit code

    draw_init_progress("Mounting virtual filesystems...", ++step, total);
    // VFS mounts would go here (procfs, devfs, etc.)

    draw_init_progress("Setting up /dev entries...", ++step, total);
    // Device files setup

    draw_init_progress("Initializing process table...", ++step, total);

    draw_init_progress("Loading kernel modules...", ++step, total);

    draw_init_progress("Setting up /proc filesystem...", ++step, total);

    draw_init_progress("Running init scripts...", ++step, total);

    draw_init_progress("Starting system services...", ++step, total);

    draw_init_progress("Preparing user environment...", ++step, total);

    draw_init_progress("Launching login manager...", ++step, total);

    console_clear();
    login_screen();
    console_clear();

    // Launch the Eclipse Shell
    shell_main();

    // Shell exited - halt
    kpanic("Shell exited!");
}
