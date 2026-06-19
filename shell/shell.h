// =============================================================================
// Eclipse32 - Eclipse Shell (esh) Header
// =============================================================================
#pragma once
#include "../kernel/kernel.h"
#include <stdarg.h>

void shell_main(void);
int  shell_run_script(const char *path);
void shell_exec_line(const char *line, void (*cb)(const char *, void *), void *ud);
