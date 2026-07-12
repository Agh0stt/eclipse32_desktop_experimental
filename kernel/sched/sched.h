// =============================================================================
// Eclipse32 - Cooperative Task Scheduler
// =============================================================================
// Two tasks only: TASK_GUI (the desktop loop) and TASK_APP (a running e32).
// No preemption — tasks voluntarily yield via sched_yield().
// sys_read(fd=0) calls sched_yield() while waiting for keyboard input;
// the GUI task renders frames and feeds keys into the ring buffer.
// =============================================================================
#pragma once
#include "../kernel.h"

// Task IDs
#define TASK_NONE   (-1)
#define TASK_GUI    0
#define TASK_APP    1
#define SCHED_TASKS 2

// Stack size for the app task (separate kernel stack)
#define SCHED_STACK_SIZE    (16 * 1024)     // 16 KB

// Initialise the scheduler. Must be called once before gui_run().
void sched_init(void);

// Yield the current task — switch to the other one.
// Returns when the other task yields back.
void sched_yield(void);

// Mark the app task as runnable and switch into it.
// Called by the terminal just before shell_exec_line().
// fn(arg) is the app entry — the scheduler runs it in TASK_APP context.
typedef void (*sched_fn_t)(void *arg);
void sched_run_app(sched_fn_t fn, void *arg);

// Called from TASK_APP when the app finishes.
// Switches back to TASK_GUI permanently (marks TASK_APP as not runnable).
void sched_app_done(void);

// Returns the currently running task id (TASK_GUI or TASK_APP).
int  sched_current(void);

// True if an app task is currently active (between sched_run_app and sched_app_done).
bool sched_app_running(void);
