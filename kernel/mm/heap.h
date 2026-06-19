// =============================================================================
// Eclipse32 - Heap Header
// =============================================================================
#pragma once
#include "../kernel.h"

void  heap_init(uint32_t virt_start, uint32_t initial_size);
void *kmalloc(size_t size);
void *kcalloc(size_t n, size_t size);
void *krealloc(void *ptr, size_t new_size);
void  kfree(void *ptr);
