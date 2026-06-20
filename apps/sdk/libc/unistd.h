#pragma once

#include <stddef.h>
#include <stdint.h>

int write(int fd, const void *buf, size_t count);
int read(int fd, void *buf, size_t count);
void _exit(int code);

int brk(void *addr);
void *sbrk(intptr_t increment);
