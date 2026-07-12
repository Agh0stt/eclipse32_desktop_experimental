#pragma once

#include "../kernel.h"
#include "../drivers/keyboard/keyboard.h"

typedef struct {
    uint32_t gs, fs, es, ds;
    uint32_t edi, esi, ebp, esp_dummy;
    uint32_t ebx, edx, ecx, eax;
    uint32_t int_num, err_code;
    uint32_t eip, cs, eflags;
    uint32_t user_esp, user_ss;
} PACKED isr_regs_t;

enum {
    SYS_write = 1,
    SYS_read = 2,
    SYS_open = 3,
    SYS_close = 4,
    SYS_seek = 5,
    SYS_fstat = 6,
    SYS_readfile = 7,
    SYS_exit = 8,
    SYS_mkdir = 9,
    SYS_unlink = 10,
    SYS_rename = 11,
    SYS_readdir = 12,
    SYS_gettime_ms = 13,
    SYS_sleep_ms = 14,
    SYS_brk = 15,
    SYS_getpid = 16,
    SYS_isatty = 17,
    SYS_getcwd = 18,
    SYS_chdir = 19,
};

#define SYS_ENOSYS  (-38)

typedef struct {
    uint32_t size;
    uint32_t is_dir;
} sys_stat_t;

typedef struct {
    char     name[256];
    uint32_t size;
    uint32_t cluster;
    uint32_t is_dir;
    uint32_t is_hidden;
} PACKED sys_dirent_t;

// Function pointer type for all syscall handlers.
// Args: (ebx, ecx, edx, esi, edi) → return value written back to eax.
typedef int32_t (*syscall_fn_t)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);

void syscall_init(void);
void syscall_dispatch_handler(void *regs);

// Register a single handler into the syscall table.
// Safe to call after syscall_init(); used by subsystems like gui_sdk.
void syscall_register(int num, syscall_fn_t fn);

void syscall_app_begin(void);
bool syscall_app_exit_requested(void);
int syscall_app_exit_code(void);
void syscall_set_app_image(const void *base, uint32_t size);
void *syscall_translate_app_ptr(uint32_t raw_ptr, uint32_t len);
void syscall_set_app_heap(uint32_t brk_base, uint32_t brk_limit);
// `slot` is the scheduler task id (TASK_GUI or an app slot from
// sched_alloc_app_slot()) these callbacks apply to. Callbacks are per-slot
// so multiple terminal windows can each have their own app task running
// concurrently, each redirecting its stdout/stdin to its own window without
// clobbering the others. Pass TASK_GUI's own slot (0) meaninglessly if you
// really want "no redirection" cleared for slot 0, but in practice these
// are always set/cleared for an app slot (1..SCHED_MAX_APPS).
void syscall_set_output_cb(int slot, void (*cb)(const char *, void *), void *ud);
void syscall_debug_puts(const char *s);
void syscall_set_input_cb(int slot, int (*cb)(void *), void *ud);
key_event_t syscall_wait_key(void);  // GUI-aware blocking key read (yields in GUI mode)
