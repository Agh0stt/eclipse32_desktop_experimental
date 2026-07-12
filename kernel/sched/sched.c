// =============================================================================
// Eclipse32 - Cooperative Multi-App Task Scheduler  (sched.c)
// =============================================================================
// See sched.h for the ring/round-robin model. This file replaces the old
// fixed two-task (TASK_GUI / TASK_APP) scheduler with a ring of
// SCHED_TASKS = 1 + SCHED_MAX_APPS tasks so several terminal windows can
// each have a command or e32 program running concurrently.
// =============================================================================

#include "sched.h"
#include "../kernel.h"

// ---- Context buffer --------------------------------------------------------
// __builtin_setjmp needs a buffer of at least 5 * sizeof(intptr_t) words.
// We use 32 words to be safe across compiler versions.
#define CTX_WORDS 32
typedef intptr_t ctx_t[CTX_WORDS];

// ---- Task state ------------------------------------------------------------
typedef struct {
    ctx_t    ctx;           // saved register context
    bool     valid;         // context has been saved at least once
    bool     runnable;      // is this task eligible to run
} task_t;

static task_t tasks[SCHED_TASKS];
static int    current_task = TASK_GUI;

// Static stacks, one per app slot (index 0..SCHED_MAX_APPS-1 <-> task id
// 1..SCHED_MAX_APPS).
static uint8_t app_stack[SCHED_MAX_APPS][SCHED_STACK_SIZE] ALIGNED(16);

// Per-slot app function + argument (set by sched_run_app before first switch)
static sched_fn_t app_fn[SCHED_MAX_APPS];
static void      *app_arg[SCHED_MAX_APPS];

// Slot the trampoline should start running as. Only ever read/written while
// no other task is executing (set immediately before the stack-switching
// asm block below, consumed immediately after) so a single static is safe
// even though several slots share this one variable over time.
static int g_tramp_slot;

// ---- Internal helpers -------------------------------------------------------

// Returns the next RUNNABLE task after `from`, walking the ring and
// wrapping around. Because TASK_GUI is always runnable once sched_init()
// has run, this always finds *something* -- worst case it wraps all the
// way back to `from` itself (meaning "nobody else is runnable").
static int next_runnable(int from) {
    for (int i = 1; i <= SCHED_TASKS; i++) {
        int idx = (from + i) % SCHED_TASKS;
        if (tasks[idx].runnable) return idx;
    }
    return from; // unreachable in practice: TASK_GUI is always runnable
}

// Called the very first time we switch into a given app slot.
// Runs on that slot's own stack with a fresh frame.
static NOINLINE void app_trampoline(void) {
    int slot = g_tramp_slot;
    sched_fn_t fn  = app_fn[slot];
    void      *arg = app_arg[slot];
    if (fn) fn(arg);
    // When the app returns, clean up and switch away. Never returns.
    sched_app_done();
    // Satisfy the compiler:
    for (;;) asm volatile("hlt");
}

// Switch execution to `target`. Caller is responsible for having already
// saved its own context (or for not needing to -- e.g. sched_app_done()
// is tearing its task down and will never resume).
static NOINLINE void switch_to(int target) {
    current_task = target;

    if (tasks[target].valid) {
        // Task has a saved context from a previous yield -- resume it.
        __builtin_longjmp(tasks[target].ctx, 1);
    }

    // First-ever entry into this task. Only app slots (task id >= 1) can
    // hit this path -- TASK_GUI's context is always valid after sched_init().
    int slot = target - 1;
    g_tramp_slot = slot;
    uint32_t new_esp = (uint32_t)(app_stack[slot] + SCHED_STACK_SIZE) - 16;
    void (*tramp)(void) = app_trampoline;
    asm volatile(
        "mov %0, %%esp\n\t"     // switch to this slot's own stack
        "call *%1\n\t"          // call trampoline (never returns here)
        :
        : "r"(new_esp), "r"(tramp)
        : "memory"
    );
    // Unreachable
    for (;;) asm volatile("hlt");
}

// ---- Public API ------------------------------------------------------------

void sched_init(void) {
    for (int i = 0; i < SCHED_TASKS; i++) {
        tasks[i].valid    = false;
        tasks[i].runnable = false;
    }
    for (int s = 0; s < SCHED_MAX_APPS; s++) {
        app_fn[s]  = NULL;
        app_arg[s] = NULL;
    }
    // TASK_GUI starts as current (we're already running in it)
    tasks[TASK_GUI].runnable = true;
    current_task = TASK_GUI;
}

int sched_current(void) {
    return current_task;
}

bool sched_app_running(void) {
    return current_task != TASK_GUI;
}

bool sched_slot_running(int slot) {
    if (slot <= TASK_GUI || slot >= SCHED_TASKS) return false;
    return tasks[slot].runnable;
}

int sched_active_app_count(void) {
    int n = 0;
    for (int i = 1; i < SCHED_TASKS; i++) {
        if (tasks[i].runnable) n++;
    }
    return n;
}

int sched_alloc_app_slot(void) {
    for (int i = 1; i < SCHED_TASKS; i++) {
        if (!tasks[i].runnable) {
            // Reserve it now so a second alloc call before the matching
            // sched_run_app() (there shouldn't be one -- single-threaded,
            // cooperative -- but be defensive) won't hand out the same slot.
            tasks[i].runnable = true;
            tasks[i].valid    = false;
            return i;
        }
    }
    return TASK_NONE; // SCHED_MAX_APPS reached -- caller must handle this
}

void sched_release_app_slot(int slot) {
    if (slot <= TASK_GUI || slot >= SCHED_TASKS) return;
    tasks[slot].runnable = false;
    tasks[slot].valid    = false;
    app_fn[slot - 1]  = NULL;
    app_arg[slot - 1] = NULL;
}

// ---------------------------------------------------------------------------
// sched_yield — round-robin through the ring of runnable tasks
// ---------------------------------------------------------------------------
void sched_yield(void) {
    int me     = current_task;
    int target = next_runnable(me);
    if (target == me) return; // nobody else runnable -- cheap no-op

    // Save our context. __builtin_setjmp returns 0 the first time (we just
    // saved), and non-zero when we are later resumed by some other task.
    if (__builtin_setjmp(tasks[me].ctx) != 0) {
        current_task = me;
        return;
    }
    tasks[me].valid = true;

    switch_to(target);
}

// ---------------------------------------------------------------------------
// sched_run_app — launch an app in the given (already-allocated) slot
// ---------------------------------------------------------------------------
void sched_run_app(int slot, sched_fn_t fn, void *arg) {
    if (slot <= TASK_GUI || slot >= SCHED_TASKS) return;

    app_fn[slot - 1]  = fn;
    app_arg[slot - 1] = arg;
    tasks[slot].valid    = false;
    tasks[slot].runnable = true;

    int me = current_task;
    if (__builtin_setjmp(tasks[me].ctx) != 0) {
        current_task = me;
        return;
    }
    tasks[me].valid = true;

    switch_to(slot);
}

// ---------------------------------------------------------------------------
// sched_app_done — called from within an app task when it finishes.
// Frees its slot and hands control to the next runnable task in the ring
// (guaranteed to find at least TASK_GUI). Never returns.
// ---------------------------------------------------------------------------
void sched_app_done(void) {
    int me = current_task;
    sched_release_app_slot(me);

    int target = next_runnable(me);
    switch_to(target);
    // Unreachable
    for (;;) asm volatile("hlt");
}
