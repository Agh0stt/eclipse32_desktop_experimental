// =============================================================================
// Eclipse32 - Kernel Heap Allocator
// Boundary-tag (header+footer) free list allocator
// =============================================================================
#include "heap.h"
#include "pmm.h"
#include "vmm.h"
#include "../kernel.h"

// Block header: stored before each allocated/free chunk
typedef struct block_hdr {
    uint32_t        size;           // size of data area (not including headers)
    bool            free;
    struct block_hdr *next;
    struct block_hdr *prev;
    uint32_t        magic;
} block_hdr_t;

// Block footer: stored after data area for coalescence
typedef struct {
    uint32_t size;
    bool     free;
} block_ftr_t;

#define HEAP_MAGIC  0xEC32BEEF
#define HDR_SIZE    sizeof(block_hdr_t)
#define FTR_SIZE    sizeof(block_ftr_t)
#define MIN_BLOCK   (HDR_SIZE + 16 + FTR_SIZE)

static uint8_t  *heap_start = NULL;
static uint8_t  *heap_end   = NULL;
static uint8_t  *heap_max   = NULL;
static block_hdr_t *free_list = NULL;

static block_ftr_t *get_footer(block_hdr_t *hdr) {
    return (block_ftr_t *)((uint8_t *)hdr + HDR_SIZE + hdr->size);
}

static block_hdr_t *get_header_from_footer(block_ftr_t *ftr) {
    return (block_hdr_t *)((uint8_t *)ftr - HDR_SIZE - ftr->size);
}

static block_hdr_t *next_block(block_hdr_t *hdr) {
    return (block_hdr_t *)((uint8_t *)hdr + HDR_SIZE + hdr->size + FTR_SIZE);
}

static block_hdr_t *prev_block(block_hdr_t *hdr) {
    block_ftr_t *prev_ftr = (block_ftr_t *)((uint8_t *)hdr - FTR_SIZE);
    if ((uint8_t *)prev_ftr < heap_start) return NULL;
    return get_header_from_footer(prev_ftr);
}

void heap_init(uint32_t virt_start, uint32_t initial_size) {
    // Allocate contiguous physical pages for the heap
    uint32_t pages = PAGE_ALIGN_UP(initial_size) / PAGE_SIZE;
    uint32_t phys = pmm_alloc_pages(pages);
    if (!phys) {
        // Fallback: identity-map a static area just above kernel
        virt_start = 0x400000;
        phys       = virt_start;
    }

    heap_start = (uint8_t *)virt_start;
    heap_end   = heap_start;
    heap_max   = heap_start + initial_size;

    // Map each physical page into the virtual address range.
    // Without this, writing the heap header to virt_start (e.g. 0xC0000000)
    // triggers a page fault immediately after VBE init and the OS resets.
    for (uint32_t i = 0; i < pages; i++) {
        vmm_map(virt_start + i * PAGE_SIZE,
                phys       + i * PAGE_SIZE,
                0x03);  // present | writable
    }

    // Create one big free block covering the whole initial size
    block_hdr_t *initial = (block_hdr_t *)heap_start;
    initial->size  = initial_size - HDR_SIZE - FTR_SIZE;
    initial->free  = true;
    initial->magic = HEAP_MAGIC;
    initial->next  = NULL;
    initial->prev  = NULL;

    block_ftr_t *ftr = get_footer(initial);
    ftr->size = initial->size;
    ftr->free = true;

    free_list = initial;
    heap_end  = heap_start + initial_size;
}

// Find a free block using first-fit
static block_hdr_t *find_free(uint32_t size) {
    block_hdr_t *cur = free_list;
    while (cur) {
        if (cur->free && cur->size >= size) return cur;
        cur = cur->next;
    }
    return NULL;
}

// Remove block from free list
static void fl_remove(block_hdr_t *hdr) {
    if (hdr->prev) hdr->prev->next = hdr->next;
    else free_list = hdr->next;
    if (hdr->next) hdr->next->prev = hdr->prev;
    hdr->next = hdr->prev = NULL;
}

// Add block to free list (front)
static void fl_add(block_hdr_t *hdr) {
    hdr->next = free_list;
    hdr->prev = NULL;
    if (free_list) free_list->prev = hdr;
    free_list = hdr;
}

void *kmalloc(size_t size) {
    if (size == 0) return NULL;

    // Align to 8 bytes
    size = (size + 7) & ~7;

    block_hdr_t *blk = find_free(size);
    if (!blk) return NULL;  // TODO: expand heap

    fl_remove(blk);

    // Split block if large enough
    if (blk->size >= size + MIN_BLOCK) {
        uint32_t old_size = blk->size;

        // Resize current block
        blk->size = size;
        block_ftr_t *ftr = get_footer(blk);
        ftr->size = size;
        ftr->free = false;

        // Create new free block from remainder
        block_hdr_t *new_blk = next_block(blk);
        new_blk->size  = old_size - size - HDR_SIZE - FTR_SIZE;
        new_blk->free  = true;
        new_blk->magic = HEAP_MAGIC;
        block_ftr_t *new_ftr = get_footer(new_blk);
        new_ftr->size = new_blk->size;
        new_ftr->free = true;
        fl_add(new_blk);
    }

    blk->free = false;
    block_ftr_t *ftr = get_footer(blk);
    ftr->free = false;

    return (void *)((uint8_t *)blk + HDR_SIZE);
}

void *kcalloc(size_t n, size_t size) {
    void *ptr = kmalloc(n * size);
    if (ptr) {
        uint8_t *p = (uint8_t *)ptr;
        for (size_t i = 0; i < n * size; i++) p[i] = 0;
    }
    return ptr;
}

void *krealloc(void *ptr, size_t new_size) {
    if (!ptr) return kmalloc(new_size);
    if (new_size == 0) { kfree(ptr); return NULL; }

    block_hdr_t *hdr = (block_hdr_t *)((uint8_t *)ptr - HDR_SIZE);
    if (hdr->size >= new_size) return ptr;

    void *new_ptr = kmalloc(new_size);
    if (!new_ptr) return NULL;

    // Copy data
    uint8_t *src = (uint8_t *)ptr;
    uint8_t *dst = (uint8_t *)new_ptr;
    uint32_t copy_size = hdr->size < new_size ? hdr->size : new_size;
    for (uint32_t i = 0; i < copy_size; i++) dst[i] = src[i];

    kfree(ptr);
    return new_ptr;
}

void kfree(void *ptr) {
    if (!ptr) return;

    block_hdr_t *hdr = (block_hdr_t *)((uint8_t *)ptr - HDR_SIZE);
    if (hdr->magic != HEAP_MAGIC) return;    // invalid free

    hdr->free = true;
    block_ftr_t *ftr = get_footer(hdr);
    ftr->free = true;

    // Coalesce with next block
    block_hdr_t *nxt = next_block(hdr);
    if ((uint8_t *)nxt < heap_end && nxt->free && nxt->magic == HEAP_MAGIC) {
        fl_remove(nxt);
        hdr->size += HDR_SIZE + FTR_SIZE + nxt->size;
        block_ftr_t *nxt_ftr = get_footer(hdr);
        nxt_ftr->size = hdr->size;
        nxt_ftr->free = true;
    }

    // Coalesce with previous block
    block_hdr_t *prv = prev_block(hdr);
    if (prv && prv->free && prv->magic == HEAP_MAGIC) {
        fl_remove(prv);
        prv->size += HDR_SIZE + FTR_SIZE + hdr->size;
        block_ftr_t *prv_ftr = get_footer(prv);
        prv_ftr->size = prv->size;
        prv_ftr->free = true;
        fl_add(prv);
        return;
    }

    fl_add(hdr);
}
