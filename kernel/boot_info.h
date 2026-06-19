// =============================================================================
// Eclipse32 - Boot Info Structure
// Passed from Stage 2 bootloader to kernel
// =============================================================================
#pragma once
#include "kernel.h"

#define BOOT_INFO_MAGIC  0xEC320001

typedef struct {
    uint32_t magic;
    uint32_t vbe_framebuffer;
    uint16_t vbe_width;
    uint16_t vbe_height;
    uint16_t vbe_pitch;
    uint8_t  vbe_bpp;
    uint8_t  _pad;
    uint16_t mem_lower;         // KB below 1MB
    uint32_t mem_upper;         // KB above 1MB
    uint32_t gdt_addr;
    uint8_t  boot_drive;
} PACKED boot_info_t;
