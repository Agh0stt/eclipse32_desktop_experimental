// =============================================================================
// Eclipse32 - Cooperative Multi-App Task Scheduler
// =============================================================================
// Task 0 is always TASK_GUI (the desktop loop, running on the original kernel
// stack inherited from kmain). Tasks 1..SCHED_MAX_APPS are "app slots": each
// one is an independent cooperative task with its own stack, used to run a
// shell command / e32 program launched from a terminal window.
//
// Unlike the original design (exactly one global app task), any number of
// app slots up to SCHED_MAX_APPS may be runnable *simultaneously* -- e.g.
// one terminal window running `get bigfile.bin` while another window runs
// `ping 10.0.2.2 -c 20`. Tasks are cooperative only: a task keeps the CPU
// until it calls sched_yield() (directly, or indirectly via sys_read()
// blocking on stdin, tcp_send/tcp_recv polling, etc).
//
// sched_yield() round-robins through every *runnable* task, starting after
// the caller, and switches to the first one found (wrapping back to the
// caller itself, and ultimately to TASK_GUI, if nobody else is runnable).
// Because TASK_GUI is always runnable, a full ring rotation always returns
// control to the desktop eventually, so the UI never permanently starves
// as long as every app task yields in a finite time.
//
// Context save/restore uses __builtin_setjmp / __builtin_longjmp (portable,
// compiler-assisted, no inline asm needed for the resume path). Works with
// zig cc / clang -target i686-*-freestanding.
// =============================================================================
#pragma once
#include "../kernel.h"

// Task IDs. Task 0 is the GUI/desktop task; tasks 1..SCHED_MAX_APPS are app
// slots. SCHED_TASKS is the total ring size (GUI + all app slots).
#define TASK_NONE       (-1)
#define TASK_GUI        0
#define SCHED_MAX_APPS  4                          // <-- the hardcoded limit
#define SCHED_TASKS     (1 + SCHED_MAX_APPS)

// Stack size for each app task (separate kernel stack per slot)
#define SCHED_STACK_SIZE    (16 * 1024)     // 16 KB

// Initialise the scheduler. Must be called once before gui_run().
void sched_init(void);

// Yield the current task -- switch to the next runnable task in the ring.
// Returns (possibly much later) when this task is resumed. If no other
// task is runnable, returns immediately (cheap no-op), exactly like the
// original two-task scheduler did.
void sched_yield(void);

// Find a free app slot (a task id in [1, SCHED_MAX_APPS]) and reserve it.
// Returns TASK_NONE if all SCHED_MAX_APPS slots are already in use --
// callers MUST handle this (e.g. tell the user "too many apps running").
int sched_alloc_app_slot(void);

// Mark a previously-allocated-but-unused slot free again without ever
// having run it (e.g. if launch setup fails after allocating). Does
// nothing if the slot is already running or invalid.
void sched_release_app_slot(int slot);

// Launch fn(arg) in the given app slot (previously returned by
// sched_alloc_app_slot()) and switch into it immediately. Returns when
// that slot's task -- or, per the round-robin ring, some other task that
// ends up yielding back to the caller -- hands control back to the caller.
typedef void (*sched_fn_t)(void *arg);
void sched_run_app(int slot, sched_fn_t fn, void *arg);

// Called from inside an app task's own context when it finishes running.
// Frees the slot and switches to the next runnable task in the ring.
// Never returns.
void sched_app_done(void);

// Returns the currently running task id (TASK_GUI, or 1..SCHED_MAX_APPS).
int sched_current(void);

// True if the CALLING task is itself an app slot (i.e. sched_current() !=
// TASK_GUI). This is the same no-argument check the original scheduler
// exposed, and is what tcp.c / gui_sdk.c use to decide whether it's safe
// and meaningful to yield.
bool sched_app_running(void);

// True if a SPECIFIC slot (as returned by sched_alloc_app_slot()) is still
// allocated / runnable. Used by the GUI task to poll whether a particular
// window's app has finished, now that more than one may be in flight.
bool sched_slot_running(int slot);

// Number of app slots currently in use (0..SCHED_MAX_APPS).
int sched_active_app_count(void);
