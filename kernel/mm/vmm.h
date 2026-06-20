// =============================================================================
// Eclipse32 - VMM Header
// =============================================================================
#pragma once
#include "../kernel.h"
#include "../boot_info.h"

void vmm_init(boot_info_t *boot_info);
void vmm_map(uint32_t virt, uint32_t phys, uint32_t flags);
void vmm_unmap(uint32_t virt);
uint32_t vmm_get_phys(uint32_t virt);
