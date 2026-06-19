// =============================================================================
// Eclipse32 - Memory Manager Headers
// =============================================================================
#pragma once
#include "../kernel.h"

// ---- PMM ----
void     pmm_init(uint32_t base, uint32_t size);
uint32_t pmm_alloc_page(void);
void     pmm_free_page(uint32_t addr);
uint32_t pmm_alloc_pages(uint32_t count);
void     pmm_free_pages(uint32_t addr, uint32_t count);
uint32_t pmm_free_pages_count_get(void);
uint32_t pmm_total_pages_get(void);

#define pmm_free_count() pmm_free_pages_count_get()
