#include "syscall.h"
#include "../gui/gui_sdk.h"

#include "../arch/x86/idt.h"
#include "../arch/x86/pit.h"
#include "../drivers/keyboard/keyboard.h"
#include "../drivers/vga/vga.h"
#include "../fs/fat32/fat32.h"
#include "../initramfs/initramfs.h"
#include "../sched/sched.h"

#define SYSCALL_TABLE_SIZE 64
#define READDIR_SCRATCH_MAX 64

static volatile bool g_app_exit_requested = false;
static volatile int g_app_exit_code = 0;
static const uint8_t *g_app_image_base = NULL;
static uint32_t g_app_image_size = 0;
static uint32_t g_brk_base = 0;
static uint32_t g_brk_current = 0;
static uint32_t g_brk_limit = 0;
static fat32_dir_entry_t readdir_scratch[READDIR_SCRATCH_MAX];

/* Output redirect: when an E32 runs inside the GUI terminal, stdout/stderr
   route through this callback instead of going directly to vga_putchar. */
static void (*g_out_cb)(const char *, void *) = NULL;
static void *g_out_ud = NULL;

/* Input redirect: when an E32 runs inside the GUI terminal, stdin reads
   use this non-blocking callback instead of keyboard_wait_event().
   Returns 0 if no char is available yet (caller should yield). */
static int (*g_in_cb)(void *) = NULL;
static void *g_in_ud = NULL;

static bool ptr_ok(const void *ptr) {
    uint32_t p = (uint32_t)ptr;
    return ptr != NULL && p >= 0x00100000 && p < 0xFFF00000;
}

static void *translate_app_ptr(uint32_t raw_ptr, uint32_t len) {
    if (g_app_image_base && raw_ptr < g_app_image_size && len <= (g_app_image_size - raw_ptr)) {
        return (void *)(g_app_image_base + raw_ptr);
    }
    return (void *)raw_ptr;
}

/* Public wrapper used by gui_sdk.c to translate app pointers. */
void *syscall_translate_app_ptr(uint32_t raw_ptr, uint32_t len) {
    return translate_app_ptr(raw_ptr, len);
}

static int32_t sys_write_impl(uint32_t fd, uint32_t buf_ptr, uint32_t len, uint32_t a3, uint32_t a4) {
    (void)a3;
    (void)a4;
    if (len == 0) return 0;
    const char *buf = (const char *)translate_app_ptr(buf_ptr, len);
    if (!ptr_ok(buf)) return -1;
    if (fd == 1 || fd == 2) {
        if (g_out_cb) {
            char tmp[2] = {0, 0};
            for (uint32_t i = 0; i < len; i++) {
                tmp[0] = buf[i];
                g_out_cb(tmp, g_out_ud);
            }
        } else {
            for (uint32_t i = 0; i < len; i++) vga_putchar(buf[i]);
        }
        return (int32_t)len;
    }
    return fat32_write((int)fd, buf, len);
}

static int32_t sys_read_impl(uint32_t fd, uint32_t buf_ptr, uint32_t len, uint32_t a3, uint32_t a4) {
    (void)a3;
    (void)a4;
    if (len == 0) return 0;
    char *buf = (char *)translate_app_ptr(buf_ptr, len);
    if (!ptr_ok(buf)) return -1;
    if (fd == 0) {
        char kbuf[512];
        uint32_t max = (len < sizeof(kbuf)) ? len : (uint32_t)sizeof(kbuf);
        uint32_t n = 0;

        if (g_in_cb) {
            /* Scheduler mode: yield to GUI task while waiting for keyboard.
               The GUI task runs gui_pump() which feeds keys into the ring
               buffer via term_inbuf_push(). g_in_cb drains them here.     */
            while (n < max) {
                int c = g_in_cb(g_in_ud);
                if (c <= 0) {
                    if (n > 0 && kbuf[n-1] == '\n') break;
                    sched_yield();  /* give GUI a frame to collect keypresses */
                    continue;
                }
                char ch = (char)c;
                if (ch == '\r') ch = '\n';

                /* Backspace: remove last char from local buffer */
                if (ch == '\b' || ch == 127) {
                    if (n > 0) {
                        n--;
                        if (g_out_cb) {
                            g_out_cb("\b", g_out_ud);
                            g_out_cb(" ", g_out_ud);
                            g_out_cb("\b", g_out_ud);
                        }
                    }
                    continue;
                }

                kbuf[n++] = ch;
                /* Echo the character so user sees what they typed */
                if (g_out_cb) {
                    char echo[2] = { ch, 0 };
                    g_out_cb(echo, g_out_ud);
                } else {
                    vga_putchar(ch);
                }
                if (ch == '\n') break;
            }
        } else {
            /* VGA text-mode: blocking wait as before */
            while (n < max) {
                key_event_t ev = keyboard_wait_event();
                if (ev.released) continue;
                if (ev.keycode != KEY_ASCII) continue;

                char c = ev.ascii;
                if (c == '\n' || c == '\r') {
                    vga_putchar('\n');
                    kbuf[n++] = '\n';
                    break;
                }
                if (c == '\b' || c == 127) {
                    if (n > 0) { n--; vga_putchar('\b'); vga_putchar(' '); vga_putchar('\b'); }
                    continue;
                }
                if (c >= 32) { kbuf[n++] = c; vga_putchar(c); }
            }
        }

        for (uint32_t i = 0; i < n; i++) buf[i] = kbuf[i];
        return (int32_t)n;
    }
    return fat32_read((int)fd, buf, len);
}

static int32_t sys_open_impl(uint32_t path_ptr, uint32_t flags, uint32_t a2, uint32_t a3, uint32_t a4) {
    (void)a2;
    (void)a3;
    (void)a4;
    const char *path = (const char *)translate_app_ptr(path_ptr, 1);
    if (!ptr_ok(path)) return -1;
    return fat32_open(path, (int)flags);
}

static int32_t sys_close_impl(uint32_t fd, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
    (void)a1;
    (void)a2;
    (void)a3;
    (void)a4;
    return fat32_close((int)fd);
}

static int32_t sys_seek_impl(uint32_t fd, uint32_t off, uint32_t whence, uint32_t a3, uint32_t a4) {
    (void)a3;
    (void)a4;
    return fat32_seek((int)fd, (int32_t)off, (int)whence);
}

static int32_t sys_fstat_impl(uint32_t path_ptr, uint32_t st_ptr, uint32_t a2, uint32_t a3, uint32_t a4) {
    (void)a2;
    (void)a3;
    (void)a4;
    const char *path = (const char *)translate_app_ptr(path_ptr, 1);
    sys_stat_t *st = (sys_stat_t *)translate_app_ptr(st_ptr, sizeof(sys_stat_t));
    if (!ptr_ok(path) || !ptr_ok(st)) return -1;
    fat32_stat_t kst;
    if (fat32_stat(path, &kst) != 0) return -1;
    st->size = kst.size;
    st->is_dir = kst.is_dir ? 1u : 0u;
    return 0;
}

static int32_t sys_readfile_impl(uint32_t path_ptr, uint32_t buf_ptr, uint32_t len, uint32_t a3, uint32_t a4) {
    (void)a3;
    (void)a4;
    const char *path = (const char *)translate_app_ptr(path_ptr, 1);
    char *buf = (char *)translate_app_ptr(buf_ptr, len);
    if (!ptr_ok(path) || !ptr_ok(buf)) return -1;
    int fd = fat32_open(path, FAT32_O_RDONLY);
    if (fd < 0) return -1;
    int n = fat32_read(fd, buf, len);
    fat32_close(fd);
    return n;
}

static int32_t sys_exit_impl(uint32_t code, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
    (void)a1;
    (void)a2;
    (void)a3;
    (void)a4;
    g_app_exit_requested = true;
    g_app_exit_code = (int)code;
    return (int32_t)code;
}

static int32_t sys_mkdir_impl(uint32_t path_ptr, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
    (void)a1;
    (void)a2;
    (void)a3;
    (void)a4;
    const char *path = (const char *)translate_app_ptr(path_ptr, 1);
    if (!ptr_ok(path)) return -1;
    return fat32_mkdir(path);
}

static int32_t sys_unlink_impl(uint32_t path_ptr, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
    (void)a1;
    (void)a2;
    (void)a3;
    (void)a4;
    const char *path = (const char *)translate_app_ptr(path_ptr, 1);
    if (!ptr_ok(path)) return -1;
    return fat32_delete(path);
}

static int32_t sys_rename_impl(uint32_t old_ptr, uint32_t new_ptr, uint32_t a2, uint32_t a3, uint32_t a4) {
    (void)a2;
    (void)a3;
    (void)a4;
    const char *oldp = (const char *)translate_app_ptr(old_ptr, 1);
    const char *newp = (const char *)translate_app_ptr(new_ptr, 1);
    if (!ptr_ok(oldp) || !ptr_ok(newp)) return -1;
    return fat32_rename(oldp, newp);
}

static int32_t sys_readdir_impl(uint32_t path_ptr, uint32_t ent_ptr, uint32_t max_ent, uint32_t a3, uint32_t a4) {
    (void)a3;
    (void)a4;
    if (max_ent == 0) return 0;
    int cap = (int)max_ent;
    if (cap > READDIR_SCRATCH_MAX) cap = READDIR_SCRATCH_MAX;
    const char *path = (const char *)translate_app_ptr(path_ptr, 1);
    size_t out_bytes = (size_t)cap * sizeof(sys_dirent_t);
    sys_dirent_t *out = (sys_dirent_t *)translate_app_ptr(ent_ptr, out_bytes);
    if (!ptr_ok(path) || !ptr_ok(out)) return -1;
    int n = fat32_readdir(path, readdir_scratch, cap);
    if (n < 0) return n;
    for (int i = 0; i < n; i++) {
        kstrncpy(out[i].name, readdir_scratch[i].name, sizeof(out[i].name) - 1);
        out[i].name[sizeof(out[i].name) - 1] = 0;
        out[i].size = readdir_scratch[i].size;
        out[i].cluster = readdir_scratch[i].cluster;
        out[i].is_dir = readdir_scratch[i].is_dir ? 1u : 0u;
        out[i].is_hidden = readdir_scratch[i].is_hidden ? 1u : 0u;
    }
    return n;
}

static int32_t sys_gettime_ms_impl(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
    (void)a0;
    (void)a1;
    (void)a2;
    (void)a3;
    (void)a4;
    return (int32_t)pit_ms();
}

static int32_t sys_sleep_ms_impl(uint32_t ms, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
    (void)a1;
    (void)a2;
    (void)a3;
    (void)a4;
    pit_sleep_ms(ms);
    return 0;
}

static int32_t sys_brk_impl(uint32_t addr, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
    (void)a1;
    (void)a2;
    (void)a3;
    (void)a4;
    if (g_brk_limit == 0 || g_brk_base == 0) return -1;
    if (addr == 0) return (int32_t)g_brk_current;
    if (addr < g_brk_base) return -1;
    if (addr > g_brk_limit) return -1;
    g_brk_current = addr;
    return (int32_t)g_brk_current;
}

static int32_t sys_getpid_impl(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
    (void)a0;
    (void)a1;
    (void)a2;
    (void)a3;
    (void)a4;
    return 1;
}

static int32_t sys_isatty_impl(uint32_t fd, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
    (void)a1;
    (void)a2;
    (void)a3;
    (void)a4;
    if (fd <= 2) return 1;
    return 0;
}

static int32_t sys_getcwd_impl(uint32_t buf_ptr, uint32_t size, uint32_t a2, uint32_t a3, uint32_t a4) {
    (void)a2;
    (void)a3;
    (void)a4;
    if (size < 2) return -1;
    char *buf = (char *)translate_app_ptr(buf_ptr, size);
    if (!ptr_ok(buf)) return -1;
    buf[0] = '/';
    buf[1] = 0;
    return 0;
}

static int32_t sys_chdir_impl(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4) {
    (void)a0;
    (void)a1;
    (void)a2;
    (void)a3;
    (void)a4;
    return SYS_ENOSYS;
}

static syscall_fn_t syscall_table[SYSCALL_TABLE_SIZE];

void syscall_register(int num, syscall_fn_t fn) {
    if (num > 0 && num < SYSCALL_TABLE_SIZE)
        syscall_table[num] = fn;
}

void syscall_dispatch_handler(void *regs_ptr) {
    isr_regs_t *regs = (isr_regs_t *)regs_ptr;
    uint32_t num = regs->eax;
    if (num == 0 || num >= SYSCALL_TABLE_SIZE || syscall_table[num] == NULL) {
        regs->eax = (uint32_t)SYS_ENOSYS;
        return;
    }
    regs->eax = (uint32_t)syscall_table[num](regs->ebx, regs->ecx, regs->edx, regs->esi, regs->edi);
}

void syscall_init(void) {
    for (uint32_t i = 0; i < SYSCALL_TABLE_SIZE; i++) syscall_table[i] = NULL;
    syscall_table[SYS_write] = sys_write_impl;
    syscall_table[SYS_read] = sys_read_impl;
    syscall_table[SYS_open] = sys_open_impl;
    syscall_table[SYS_close] = sys_close_impl;
    syscall_table[SYS_seek] = sys_seek_impl;
    syscall_table[SYS_fstat] = sys_fstat_impl;
    syscall_table[SYS_readfile] = sys_readfile_impl;
    syscall_table[SYS_exit] = sys_exit_impl;
    syscall_table[SYS_mkdir] = sys_mkdir_impl;
    syscall_table[SYS_unlink] = sys_unlink_impl;
    syscall_table[SYS_rename] = sys_rename_impl;
    syscall_table[SYS_readdir] = sys_readdir_impl;
    syscall_table[SYS_gettime_ms] = sys_gettime_ms_impl;
    syscall_table[SYS_sleep_ms] = sys_sleep_ms_impl;
    syscall_table[SYS_brk] = sys_brk_impl;
    syscall_table[SYS_getpid] = sys_getpid_impl;
    syscall_table[SYS_isatty] = sys_isatty_impl;
    syscall_table[SYS_getcwd] = sys_getcwd_impl;
    syscall_table[SYS_chdir] = sys_chdir_impl;
    gui_sdk_register_syscalls();
    idt_register_handler(0x80, syscall_dispatch_handler);
}

void syscall_app_begin(void) {
    g_app_exit_requested = false;
    g_app_exit_code = 0;
}

bool syscall_app_exit_requested(void) {
    return g_app_exit_requested;
}

int syscall_app_exit_code(void) {
    return (int)g_app_exit_code;
}

void syscall_set_app_image(const void *base, uint32_t size) {
    g_app_image_base = (const uint8_t *)base;
    g_app_image_size = size;
}

void syscall_set_app_heap(uint32_t brk_base, uint32_t brk_limit) {
    g_brk_base = brk_base;
    g_brk_current = brk_base;
    g_brk_limit = brk_limit;
}

void syscall_set_output_cb(void (*cb)(const char *, void *), void *ud) {
    g_out_cb = cb;
    g_out_ud = ud;
}

void syscall_debug_puts(const char *s) {
    if (g_out_cb) { g_out_cb(s, g_out_ud); } else { extern void vga_puts(const char *); vga_puts(s); }
}

void syscall_set_input_cb(int (*cb)(void *), void *ud) {
    g_in_cb = cb;
    g_in_ud = ud;
}

// GUI-aware blocking key read for shell builtins.
// In GUI mode (g_in_cb set): yields to GUI task each time the keyboard queue
// is empty, so the desktop stays live and redraws while waiting for input.
// In VGA text mode: identical to keyboard_wait_event().
key_event_t syscall_wait_key(void) {
    for (;;) {
        if (keyboard_has_event()) {
            key_event_t ev = keyboard_get_event();
            if (ev.released) continue;
            return ev;
        }
        if (g_in_cb) {
            sched_yield();   // let GUI pump a frame and collect keypresses
        } else {
            asm volatile("hlt");  // VGA text mode: wait for IRQ
        }
    }
}
