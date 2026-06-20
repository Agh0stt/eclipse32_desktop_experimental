// =============================================================================
// Eclipse32 - Physical Memory Manager (PMM)
// Bitmap-based page frame allocator
// =============================================================================
#include "pmm.h"
#include "../kernel.h"
#include "../drivers/vga/vga.h"

#define PMM_BITMAP_SIZE     (128 * 1024 / 32)   // supports up to 512MB

static uint32_t pmm_bitmap[PMM_BITMAP_SIZE];
static uint32_t pmm_total_pages = 0;
static uint32_t pmm_free_pages_count = 0;
static uint32_t pmm_base_addr = 0;

static void pmm_set_bit(uint32_t page) {
    pmm_bitmap[page / 32] |= (1 << (page % 32));
}

static void pmm_clear_bit(uint32_t page) {
    pmm_bitmap[page / 32] &= ~(1 << (page % 32));
}

static bool pmm_test_bit(uint32_t page) {
    return (pmm_bitmap[page / 32] >> (page % 32)) & 1;
}

void pmm_init(uint32_t base, uint32_t size) {
    pmm_base_addr = PAGE_ALIGN_UP(base);
    pmm_total_pages = size / PAGE_SIZE;
    pmm_free_pages_count = pmm_total_pages;

    // Mark all pages as free (bit=0 means free)
    uint32_t words = (pmm_total_pages + 31) / 32;
    for (uint32_t i = 0; i < words; i++) {
        pmm_bitmap[i] = 0;
    }
    // Mark bits beyond total_pages as used
    uint32_t rem = pmm_total_pages % 32;
    if (rem) {
        pmm_bitmap[pmm_total_pages / 32] = ~((1 << rem) - 1);
    }

    vga_printf("[PMM] Base=0x%08X Pages=%u (%u KB)\n",
               pmm_base_addr, pmm_total_pages,
               pmm_total_pages * 4);
}

uint32_t pmm_alloc_page(void) {
    for (uint32_t i = 0; i < (pmm_total_pages + 31) / 32; i++) {
        if (pmm_bitmap[i] != 0xFFFFFFFF) {
            // Find first free bit
            uint32_t bits = ~pmm_bitmap[i];
            uint32_t bit = __builtin_ctz(bits);
            uint32_t page = i * 32 + bit;
            if (page < pmm_total_pages) {
                pmm_set_bit(page);
                pmm_free_pages_count--;
                return pmm_base_addr + page * PAGE_SIZE;
            }
        }
    }
    return 0;   // OOM
}

void pmm_free_page(uint32_t addr) {
    uint32_t page = (addr - pmm_base_addr) / PAGE_SIZE;
    if (page >= pmm_total_pages) return;
    if (!pmm_test_bit(page)) return;    // double free!
    pmm_clear_bit(page);
    pmm_free_pages_count++;
}

uint32_t pmm_alloc_pages(uint32_t count) {
    uint32_t consecutive = 0;
    uint32_t start = 0;

    for (uint32_t i = 0; i < pmm_total_pages; i++) {
        if (!pmm_test_bit(i)) {
            if (consecutive == 0) start = i;
            consecutive++;
            if (consecutive == count) {
                // Mark all as used
                for (uint32_t j = start; j < start + count; j++) {
                    pmm_set_bit(j);
                }
                pmm_free_pages_count -= count;
                return pmm_base_addr + start * PAGE_SIZE;
            }
        } else {
            consecutive = 0;
        }
    }
    return 0;
}

void pmm_free_pages(uint32_t addr, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
        pmm_free_page(addr + i * PAGE_SIZE);
    }
}

uint32_t pmm_free_pages_count_get(void) {
    return pmm_free_pages_count;
}

uint32_t pmm_total_pages_get(void) {
    return pmm_total_pages;
}
