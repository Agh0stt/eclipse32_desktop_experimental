// =============================================================================
// Eclipse32 - Virtual Memory Manager
// Manages 32-bit paging, page directory / table manipulation
// =============================================================================
#include "vmm.h"
#include "pmm.h"
#include "../kernel.h"

#define PDE_PRESENT     0x01
#define PDE_WRITABLE    0x02
#define PDE_USER        0x04
#define PDE_WRITETHROUGH 0x08
#define PDE_CACHE_OFF   0x10
#define PDE_ACCESSED    0x20
#define PDE_LARGE       0x80

#define PAGE_DIR_ADDR   0x70000
#define PAGE_TAB_BASE   0x71000

static uint32_t *page_dir = (uint32_t *)PAGE_DIR_ADDR;

void vmm_init(boot_info_t *boot_info) {
    (void)boot_info;
    // Page directory was set up by stage2; we work with it here
    // Map VBE framebuffer if needed
    if (boot_info && boot_info->vbe_framebuffer) {
        uint32_t fb = boot_info->vbe_framebuffer;
        uint32_t fb_size = (uint32_t)boot_info->vbe_pitch * boot_info->vbe_height;
        fb_size = PAGE_ALIGN_UP(fb_size);

        // Identity map framebuffer pages
        for (uint32_t off = 0; off < fb_size; off += PAGE_SIZE) {
            vmm_map(fb + off, fb + off, PDE_PRESENT | PDE_WRITABLE);
        }
    }
}

void vmm_map(uint32_t virt, uint32_t phys, uint32_t flags) {
    uint32_t pd_idx = virt >> 22;
    uint32_t pt_idx = (virt >> 12) & 0x3FF;

    uint32_t *pt;

    if (!(page_dir[pd_idx] & PDE_PRESENT)) {
        // Allocate new page table
        uint32_t new_pt = pmm_alloc_page();
        if (!new_pt) return;

        // Zero the page table
        uint32_t *p = (uint32_t *)new_pt;
        for (int i = 0; i < 1024; i++) p[i] = 0;

        page_dir[pd_idx] = new_pt | PDE_PRESENT | PDE_WRITABLE;
        pt = p;
    } else {
        pt = (uint32_t *)(page_dir[pd_idx] & PAGE_MASK);
    }

    pt[pt_idx] = (phys & PAGE_MASK) | (flags & 0xFFF) | PDE_PRESENT;
    flush_tlb_page(virt);
}

void vmm_unmap(uint32_t virt) {
    uint32_t pd_idx = virt >> 22;
    uint32_t pt_idx = (virt >> 12) & 0x3FF;

    if (!(page_dir[pd_idx] & PDE_PRESENT)) return;

    uint32_t *pt = (uint32_t *)(page_dir[pd_idx] & PAGE_MASK);
    pt[pt_idx] = 0;
    flush_tlb_page(virt);
}

uint32_t vmm_get_phys(uint32_t virt) {
    uint32_t pd_idx = virt >> 22;
    uint32_t pt_idx = (virt >> 12) & 0x3FF;

    if (!(page_dir[pd_idx] & PDE_PRESENT)) return 0;
    uint32_t *pt = (uint32_t *)(page_dir[pd_idx] & PAGE_MASK);
    if (!(pt[pt_idx] & PDE_PRESENT)) return 0;
    return (pt[pt_idx] & PAGE_MASK) | (virt & ~PAGE_MASK);
}
