// =============================================================================
// Eclipse32 - Machine control (panic persistence, reboot, power-off)
//
// The old interactive F-key boot menu (F1 Upload Mode, F2 Boot Menu TUI,
// F3 Force VGA Text, F4 Software Update) has been removed entirely. This
// file now only keeps the low-level bits other subsystems still need:
//   - bootmenu_record_panic() - kpanic() persists the panic message here
//   - machine_reboot()        - warm reboot via keyboard controller
//   - machine_poweroff()      - best-effort ACPI/hypervisor power-off
// =============================================================================
#include "bootmenu.h"
#include "../kernel.h"

// kernel-internal string helper (defined in initramfs.c, used implicitly
// across the kernel the same way shell.c / gui_desktop.c do)
size_t kstrlen(const char *s);

// ---------------------------------------------------------------------------
// Panic info persistence (survives warm reboot - BIOS keyboard-controller
// reset does not clear RAM). Fixed low-memory scratch address, well below
// the kernel load address and above the BIOS/IVT area.
// ---------------------------------------------------------------------------
#define PANIC_INFO_ADDR  0x00090000u

static panic_info_t *panic_info(void) {
    return (panic_info_t *)PANIC_INFO_ADDR;
}

void bootmenu_record_panic(const char *msg) {
    panic_info_t *pi = panic_info();
    pi->magic = PANIC_INFO_MAGIC;
    size_t n = kstrlen(msg);
    if (n >= PANIC_MSG_MAX) n = PANIC_MSG_MAX - 1;
    for (size_t i = 0; i < n; i++) pi->message[i] = msg[i];
    pi->message[n] = 0;
}

// ---------------------------------------------------------------------------
// Machine reboot - pulse the keyboard controller's reset line (port 0x64,
// command 0xFE). Standard PC reset trick, works under QEMU and real HW.
// ---------------------------------------------------------------------------
void machine_reboot(void) {
    cli();
    // Wait for the controller's input buffer to be clear first.
    for (int timeout = 0; timeout < 100000; timeout++) {
        if (!(inb(0x64) & 0x02)) break;
    }
    outb(0x64, 0xFE);
    // Fallback if the controller reset didn't take: halt forever.
    for (;;) hlt();
}

// ---------------------------------------------------------------------------
// Machine power-off - no full ACPI driver yet, so this pokes the small set
// of fixed I/O ports that QEMU, Bochs, and VirtualBox all recognise as a
// "power off now" signal. Harmless no-ops on hardware that doesn't
// recognise them, so we fall back to a halt loop afterwards.
// ---------------------------------------------------------------------------
void machine_poweroff(void) {
    cli();
    outw(0x604,  0x2000);   // QEMU (older versions / -no-acpi builds)
    outw(0xB004, 0x2000);   // Bochs / older QEMU "Bochs shutdown" port
    outw(0x4004, 0x3400);   // VirtualBox
    for (;;) hlt();
}
