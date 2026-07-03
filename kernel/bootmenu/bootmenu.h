// =============================================================================
// Eclipse32 - Machine control (panic persistence, reboot, power-off)
//
// The old interactive F-key boot menu has been removed. This header now
// only exposes the low-level pieces other subsystems still rely on.
// =============================================================================
#pragma once
#include "../kernel.h"

// Persisted panic record. Lives at a fixed physical address (PANIC_INFO_ADDR)
// so it survives a warm reboot (the BIOS keyboard-controller reset does not
// clear RAM). kpanic() fills this in right before halting.
#define PANIC_INFO_MAGIC   0xDEAD0001u
#define PANIC_MSG_MAX      128

typedef struct {
    uint32_t magic;             // PANIC_INFO_MAGIC if a panic record is valid
    char     message[PANIC_MSG_MAX];
} PACKED panic_info_t;

// Called by kpanic() right before halting, to persist the panic reason.
void bootmenu_record_panic(const char *msg);

// Reboots the machine via the keyboard controller reset line. Does not return.
void machine_reboot(void) NORETURN;

// Powers the machine off. Best-effort: tries the fixed ACPI/hypervisor
// shutdown ports known to work under QEMU, Bochs, and VirtualBox. Falls
// back to an infinite halt if none of them are recognised (e.g. real
// hardware without a full ACPI driver). Does not return.
void machine_poweroff(void) NORETURN;
