// =============================================================================
// Eclipse32 - Boot Menu (splash F-key hotkeys)
// =============================================================================
#pragma once
#include "../kernel.h"

// Result of the boot menu check - tells kmain.c how to continue booting.
typedef enum {
    BOOTMENU_CONTINUE,      // No key pressed (or returned from a stub) - normal boot
    BOOTMENU_FORCE_TEXT,    // F3 was pressed - force VGA text mode path
} bootmenu_result_t;

// Persisted panic record. Lives at a fixed physical address (PANIC_INFO_ADDR)
// so it survives a warm reboot (the BIOS keyboard-controller reset does not
// clear RAM). kpanic() fills this in right before halting; F1's upload-mode
// screen reads it back after the next boot.
#define PANIC_INFO_MAGIC   0xDEAD0001u
#define PANIC_MSG_MAX      128

typedef struct {
    uint32_t magic;             // PANIC_INFO_MAGIC if a panic record is valid
    char     message[PANIC_MSG_MAX];
} PACKED panic_info_t;

// Called by kpanic() right before halting, to persist the panic reason.
void bootmenu_record_panic(const char *msg);

// Call after VBE init (and after the splash is drawn). Polls the keyboard
// for a short window; if F1-F5 is pressed, handles that mode (which may
// block indefinitely, e.g. menus/confirmations) and only returns once the
// kernel should resume its normal boot sequence.
bootmenu_result_t bootmenu_check(void);

// Reboots the machine via the keyboard controller reset line. Does not return.
void machine_reboot(void) NORETURN;
