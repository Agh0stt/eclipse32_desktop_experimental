// =============================================================================
// Eclipse32 Kernel - kmain.c
// Main kernel entry point. Initializes all subsystems and launches initramfs.
// =============================================================================

#include "kernel.h"
#include "boot_info.h"
#include "../drivers/vbe/vbe.h"
#include "../drivers/vga/vga.h"
#include "../drivers/keyboard/keyboard.h"
#include "../drivers/mouse/mouse.h"
#include "../drivers/disk/ata.h"
#include "../drivers/pci/pci.h"
#include "../drivers/net/rtl8139.h"
#include "../net/net.h"
#include "../mm/pmm.h"
#include "../mm/vmm.h"
#include "../mm/heap.h"
#include "../arch/x86/gdt.h"
#include "../arch/x86/idt.h"
#include "../arch/x86/pic.h"
#include "../arch/x86/pit.h"
#include "../syscall/syscall.h"
#include "../fs/fat32/fat32.h"
#include "../initramfs/initramfs.h"
#include "../gui/gui_desktop.h"
#include "../gui/gui_sdk.h"
#include "../sched/sched.h"

// Defined in linker script
extern uint32_t kernel_start;
extern uint32_t kernel_end;

static boot_info_t *g_boot_info = NULL;

// =============================================================================
// Kernel panic - halt with message
// =============================================================================
void kpanic(const char *msg) {
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_RED);
    vga_puts("\n\n[KERNEL PANIC] ");
    vga_puts(msg);
    vga_puts("\nSystem halted.");
    asm volatile("cli; hlt");
    for(;;);
}

// =============================================================================
// kmain - called from entry.asm with EAX = boot_info*
// =============================================================================
void kmain(boot_info_t *boot_info) {
    g_boot_info = boot_info;

    // -------------------------------------------------------------------------
    // Phase 1: Basic hardware init (no memory allocator yet)
    // -------------------------------------------------------------------------

    // Initialize VGA text mode first for early debug output
    vga_init();
    vga_clear();
    vga_set_color(VGA_COLOR_CYAN, VGA_COLOR_BLACK);
    vga_puts("Eclipse32 Kernel v0.1\n");
    vga_puts("======================\n\n");

    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    vga_printf("[INIT] Boot info at 0x%08X\n", (uint32_t)boot_info);

    // Validate boot info magic
    if (!boot_info || boot_info->magic != BOOT_INFO_MAGIC) {
        kpanic("Invalid boot info structure from stage2!");
    }

    // -------------------------------------------------------------------------
    // Phase 2: CPU structures - GDT, IDT, TSS
    // -------------------------------------------------------------------------
    vga_puts("[INIT] Setting up GDT...\n");
    gdt_init();

    vga_puts("[INIT] Setting up IDT...\n");
    idt_init();

    vga_puts("[INIT] Configuring PIC...\n");
    pic_remap(0x20, 0x28);

    vga_puts("[INIT] Configuring PIT (1000 Hz)...\n");
    pit_init(1000);

    vga_puts("[INIT] Registering syscall dispatcher...\n");
    syscall_init();

    // Enable interrupts
    asm volatile("sti");
    vga_puts("[INIT] Interrupts enabled\n");

    // -------------------------------------------------------------------------
    // Phase 3: Memory Management
    // -------------------------------------------------------------------------
    vga_puts("[INIT] Initializing Physical Memory Manager...\n");

    // Detect memory from BIOS e820 data or use hardcoded 32MB for now
    // Kernel occupies ~1MB-2MB, free memory starts at 2MB
    uint32_t mem_start = (uint32_t)&kernel_end;
    uint32_t mem_start_aligned = (mem_start + 0xFFF) & ~0xFFF;
    uint32_t mem_total = 32 * 1024 * 1024;  // assume 32MB

    pmm_init(mem_start_aligned, mem_total - mem_start_aligned);
    vga_printf("[INIT] PMM: %u KB free\n", pmm_free_count() * 4);
    vga_puts("[INIT] Initializing Virtual Memory Manager...\n");
    vmm_init(boot_info);

    vga_puts("[INIT] Initializing Heap (kmalloc/kfree)...\n");
    heap_init(0xC0000000, 4 * 1024 * 1024);  // 4MB heap at 0xC0000000

    // -------------------------------------------------------------------------
    // Phase 4: Device Drivers
    // -------------------------------------------------------------------------
    vga_puts("[INIT] Initializing keyboard driver...\n");
    keyboard_init();
    mouse_init();

    vga_puts("[INIT] Initializing ATA disk driver...\n");
    if (ata_init() != 0) {
        vga_set_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
        vga_puts("[WARN] ATA init failed - disk I/O unavailable\n");
        vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    }

    // -------------------------------------------------------------------------
    // Phase 4.5: PCI bus + network stack
    // -------------------------------------------------------------------------
    vga_puts("[INIT] Scanning PCI bus...\n");
    int pci_count = pci_init();
    vga_printf("[INIT] PCI: %d device(s) found\n", pci_count);

    vga_puts("[INIT] Initializing RTL8139 NIC...\n");
    if (rtl8139_init()) {
        mac_addr_t mac = rtl8139_get_mac();
        vga_printf("[INIT] RTL8139 OK: MAC %02X:%02X:%02X:%02X:%02X:%02X\n",
                   mac.b[0], mac.b[1], mac.b[2], mac.b[3], mac.b[4], mac.b[5]);

        // Static IP config matching QEMU user-mode networking's default
        // 10.0.2.0/24 subnet: gateway/DHCP at 10.0.2.2, DNS server at
        // 10.0.2.3 (these are two DIFFERENT addresses in slirp — querying
        // the gateway for DNS gets back ICMP port-unreachable), guest
        // typically at 10.0.2.15 under DHCP — hardcoded here since we don't
        // have a DHCP client yet.
        ipv4_addr_t my_ip      = ipv4_make(10, 0, 2, 15);
        ipv4_addr_t gateway_ip = ipv4_make(10, 0, 2, 2);
        ipv4_addr_t dns_ip     = ipv4_make(10, 0, 2, 3);
        net_init(mac, my_ip, gateway_ip, dns_ip);
        vga_printf("[INIT] Network: %d.%d.%d.%d (gateway %d.%d.%d.%d, dns %d.%d.%d.%d)\n",
                   my_ip.b[0], my_ip.b[1], my_ip.b[2], my_ip.b[3],
                   gateway_ip.b[0], gateway_ip.b[1], gateway_ip.b[2], gateway_ip.b[3],
                   dns_ip.b[0], dns_ip.b[1], dns_ip.b[2], dns_ip.b[3]);
    } else {
        vga_set_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
        vga_puts("[WARN] RTL8139 not found - networking unavailable\n");
        vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    }

    // -------------------------------------------------------------------------
    // Phase 5: VBE Framebuffer
    // -------------------------------------------------------------------------
    if (boot_info->vbe_framebuffer != 0) {
        vga_puts("[INIT] Initializing VBE framebuffer...\n");
        vbe_init(boot_info->vbe_framebuffer,
                 boot_info->vbe_width,
                 boot_info->vbe_height,
                 boot_info->vbe_pitch,
                 boot_info->vbe_bpp);
        if (vbe_active()) {
            vga_printf("[INIT] VBE OK: %ux%u@%ubpp\n",
                       boot_info->vbe_width, boot_info->vbe_height, boot_info->vbe_bpp);
        } else {
            vga_puts("[WARN] VBE inactive, falling back\n");
        }
    } else {
        vga_puts("[WARN] No VBE from bootloader - VGA text mode\n");
    }

    // -------------------------------------------------------------------------
    // Phase 6: FAT32 Filesystem
    // -------------------------------------------------------------------------
    vga_puts("[INIT] Mounting FAT32 filesystem...\n");
    if (fat32_mount(0) != 0) {
        vga_set_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
        vga_puts("[WARN] FAT32 mount failed - filesystem unavailable\n");
        vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    } else {
        vga_puts("[INIT] FAT32 mounted OK\n");
    }

    // -------------------------------------------------------------------------
    // Phase 7: Launch EclipseGUI
    // -------------------------------------------------------------------------
    if (vbe_active()) {
        vga_puts("[INIT] Initialising scheduler...\n");
        sched_init();
        vga_puts("[INIT] Initialising GUI App SDK...\n");
        gui_sdk_init();
        vga_puts("[INIT] Launching EclipseGUI desktop...\n");
        // Use ported old GUI (proven working)
        extern void gui_run(void);
        gui_run();   // never returns
        kpanic("GUI returned unexpectedly!");
    } else {
        // Fallback: text shell
        vga_puts("[INIT] Launching InitRAMFS (text mode)...\n\n");
        initramfs_start();
        kpanic("InitRAMFS returned unexpectedly!");
    }
}
