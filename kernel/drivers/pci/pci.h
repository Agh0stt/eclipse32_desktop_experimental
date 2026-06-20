// =============================================================================
// Eclipse32 - PCI Bus Driver
// Config space access (port I/O mechanism #1) and bus enumeration.
// =============================================================================
#pragma once
#include "../../kernel.h"

#define PCI_MAX_DEVICES   32

typedef struct {
    uint8_t  bus;
    uint8_t  slot;
    uint8_t  func;

    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t  class_code;     // base class
    uint8_t  subclass;
    uint8_t  prog_if;
    uint8_t  header_type;
    uint8_t  irq_line;

    uint32_t bar[6];          // raw BAR values (low bit/flags not stripped for MMIO; stripped for I/O)
    bool     bar_is_io[6];    // true if BAR[n] is an I/O space BAR

    bool     present;
} pci_device_t;

// Raw config space access
uint8_t  pci_cfg_read8 (uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
uint16_t pci_cfg_read16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
uint32_t pci_cfg_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
void     pci_cfg_write8 (uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint8_t value);
void     pci_cfg_write16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t value);
void     pci_cfg_write32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value);

// Enumerate all buses/slots/functions, populate the internal device table.
// Returns the number of devices found.
int      pci_init(void);

// Lookup helpers
int             pci_device_count(void);
pci_device_t   *pci_get_device(int idx);
pci_device_t   *pci_find_device(uint16_t vendor_id, uint16_t device_id);
pci_device_t   *pci_find_class(uint8_t class_code, uint8_t subclass);

// Enable I/O space, memory space, and bus mastering for a device
// (bus mastering is required for any controller that does DMA, incl. RTL8139).
void     pci_enable_device(pci_device_t *dev);

// Get the I/O base address from a BAR that was marked bar_is_io[n] == true
// (masks off the low 2 bits that mark it as an I/O BAR).
uint32_t pci_bar_io_base(pci_device_t *dev, int bar_index);

// Get the MMIO physical base address from a memory BAR
// (masks off the low 4 bits of type/prefetch flags).
uint32_t pci_bar_mem_base(pci_device_t *dev, int bar_index);
