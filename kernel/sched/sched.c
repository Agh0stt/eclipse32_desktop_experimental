// =============================================================================
// Eclipse32 - Cooperative Task Scheduler  (sched.c)
// =============================================================================
// Uses __builtin_setjmp / __builtin_longjmp for portable, compiler-assisted
// context saving. No inline asm required. Works with zig cc -target x86-freestanding.
//
// Layout:
//   TASK_GUI (task 0) — the gui_run() / gui_pump() loop, runs on the original
//                        kernel stack inherited from kmain.
//   TASK_APP (task 1) — the e32 app (shell_exec_line call), runs on its own
//                        stack allocated statically below.
//
// Yield protocol:
//   sched_yield() saves the caller's context into tasks[current].ctx, then
//   restores tasks[other].ctx. The other task "returns" from its own
//   sched_yield() call.
//
//   Special case: when TASK_GUI yields into a freshly initialised TASK_APP
//   (first time), we instead longjmp to a tiny trampoline that sets up a
//   new C stack frame and calls the app function. After the app returns the
//   trampoline calls sched_app_done() which switches back to TASK_GUI.
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

// Static stack for TASK_APP
static uint8_t app_stack[SCHED_STACK_SIZE] ALIGNED(16);

// App function + argument (set by sched_run_app before first switch)
static sched_fn_t app_fn  = NULL;
static void      *app_arg = NULL;

// ---- Internal trampoline ---------------------------------------------------
// Called the very first time we switch into TASK_APP.
// Runs on app_stack with a fresh frame.
static NOINLINE void app_trampoline(void) {
    // Execute the app
    if (app_fn) app_fn(app_arg);
    // When app returns, clean up
    sched_app_done();
    // sched_app_done() does not return — it longjmps back to TASK_GUI.
    // Satisfy the compiler:
    for (;;) asm volatile("hlt");
}

// ---- Public API ------------------------------------------------------------

void sched_init(void) {
    for (int i = 0; i < SCHED_TASKS; i++) {
        tasks[i].valid    = false;
        tasks[i].runnable = false;
    }
    // TASK_GUI starts as current (we're already running in it)
    tasks[TASK_GUI].runnable = true;
    current_task = TASK_GUI;
}

int sched_current(void) {
    return current_task;
}

bool sched_app_running(void) {
    return tasks[TASK_APP].runnable;
}

// ---------------------------------------------------------------------------
// sched_yield — the heart of the scheduler
// ---------------------------------------------------------------------------
void sched_yield(void) {
    int me    = current_task;
    int other = (me == TASK_GUI) ? TASK_APP : TASK_GUI;

    // If the other task isn't runnable, nothing to switch to — just return.
    if (!tasks[other].runnable) return;

    // Save our context. __builtin_setjmp returns 0 the first time (we saved),
    // and non-zero when we are restored (resumed by the other task).
    if (__builtin_setjmp(tasks[me].ctx) != 0) {
        // We've been resumed — restore current_task and return to caller.
        current_task = me;
        return;
    }
    tasks[me].valid = true;

    // Switch to the other task
    current_task = other;

    if (tasks[other].valid) {
        // Other task has a saved context — restore it.
        __builtin_longjmp(tasks[other].ctx, 1);
    } else {
        // First time switching into TASK_APP: manually set up its stack
        // and jump to the trampoline.
        // We do this with a small inline asm that switches ESP to the top
        // of app_stack and calls app_trampoline().
        uint32_t new_esp = (uint32_t)(app_stack + SCHED_STACK_SIZE) - 16;
        void (*tramp)(void) = app_trampoline;
        asm volatile(
            "mov %0, %%esp\n\t"     // switch to app stack
            "call *%1\n\t"          // call trampoline (never returns here)
            :
            : "r"(new_esp), "r"(tramp)
            : "memory"
        );
        // Unreachable
        for (;;) asm volatile("hlt");
    }
}

// ---------------------------------------------------------------------------
// sched_run_app — launch an app in TASK_APP context
// ---------------------------------------------------------------------------
void sched_run_app(sched_fn_t fn, void *arg) {
    app_fn  = fn;
    app_arg = arg;

    // Reset TASK_APP so sched_yield knows it's a fresh start
    tasks[TASK_APP].valid    = false;
    tasks[TASK_APP].runnable = true;

    // First switch into the app.  After this returns the app has finished
    // (sched_app_done cleared runnable and longjmped back here).
    // Mid-run yields come back here too — the caller (gui_desktop) must loop
    // calling sched_yield_app() to pump frames until runnable goes false.
    sched_yield();
}

// ---------------------------------------------------------------------------
// sched_app_done — called when the app finishes; returns to TASK_GUI
// ---------------------------------------------------------------------------
void sched_app_done(void) {
    // Mark app as no longer runnable
    tasks[TASK_APP].runnable = false;
    tasks[TASK_APP].valid    = false;
    app_fn  = NULL;
    app_arg = NULL;

    // Switch back to GUI — it must have a valid saved context because it
    // called sched_run_app -> sched_yield and is waiting for us.
    current_task = TASK_GUI;
    if (tasks[TASK_GUI].valid) {
        __builtin_longjmp(tasks[TASK_GUI].ctx, 1);
    }
    // Should never reach here
    for (;;) asm volatile("hlt");
}
