// =============================================================================
// Eclipse32 - PCI Bus Driver
// Config space access via port I/O "Configuration Mechanism #1" (0xCF8/0xCFC),
// which is the standard, universally-supported PCI access method on x86.
// =============================================================================
#include "pci.h"

#define PCI_CONFIG_ADDR   0xCF8
#define PCI_CONFIG_DATA   0xCFC

#define PCI_VENDOR_NONE   0xFFFF   // reads back as 0xFFFF when no device present

// PCI config space offsets we care about
#define PCI_OFF_VENDOR_ID     0x00
#define PCI_OFF_DEVICE_ID     0x02
#define PCI_OFF_COMMAND       0x04
#define PCI_OFF_STATUS        0x06
#define PCI_OFF_CLASS         0x0B   // base class
#define PCI_OFF_SUBCLASS      0x0A
#define PCI_OFF_PROG_IF       0x09
#define PCI_OFF_HEADER_TYPE   0x0E
#define PCI_OFF_BAR0          0x10
#define PCI_OFF_IRQ_LINE      0x3C

#define PCI_CMD_IO_SPACE      0x0001
#define PCI_CMD_MEM_SPACE     0x0002
#define PCI_CMD_BUS_MASTER    0x0004

static pci_device_t devices[PCI_MAX_DEVICES];
static int          device_count = 0;

// -----------------------------------------------------------------------------
// Raw config space access
// -----------------------------------------------------------------------------
static inline uint32_t pci_make_addr(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    return (uint32_t)0x80000000
         | ((uint32_t)bus  << 16)
         | ((uint32_t)slot << 11)
         | ((uint32_t)func << 8)
         | ((uint32_t)offset & 0xFC);
}

uint32_t pci_cfg_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    outl(PCI_CONFIG_ADDR, pci_make_addr(bus, slot, func, offset));
    return inl(PCI_CONFIG_DATA);
}

uint16_t pci_cfg_read16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t dword = pci_cfg_read32(bus, slot, func, offset & 0xFC);
    return (uint16_t)(dword >> ((offset & 2) * 8));
}

uint8_t pci_cfg_read8(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t dword = pci_cfg_read32(bus, slot, func, offset & 0xFC);
    return (uint8_t)(dword >> ((offset & 3) * 8));
}

void pci_cfg_write32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value) {
    outl(PCI_CONFIG_ADDR, pci_make_addr(bus, slot, func, offset));
    outl(PCI_CONFIG_DATA, value);
}

void pci_cfg_write16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t value) {
    uint32_t dword = pci_cfg_read32(bus, slot, func, offset & 0xFC);
    uint32_t shift = (offset & 2) * 8;
    dword = (dword & ~(0xFFFFu << shift)) | ((uint32_t)value << shift);
    pci_cfg_write32(bus, slot, func, offset & 0xFC, dword);
}

void pci_cfg_write8(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint8_t value) {
    uint32_t dword = pci_cfg_read32(bus, slot, func, offset & 0xFC);
    uint32_t shift = (offset & 3) * 8;
    dword = (dword & ~(0xFFu << shift)) | ((uint32_t)value << shift);
    pci_cfg_write32(bus, slot, func, offset & 0xFC, dword);
}

// -----------------------------------------------------------------------------
// Enumeration
// -----------------------------------------------------------------------------
static void pci_scan_function(uint8_t bus, uint8_t slot, uint8_t func) {
    uint16_t vendor = pci_cfg_read16(bus, slot, func, PCI_OFF_VENDOR_ID);
    if (vendor == PCI_VENDOR_NONE) return;
    if (device_count >= PCI_MAX_DEVICES) return;

    pci_device_t *d = &devices[device_count++];
    d->bus        = bus;
    d->slot       = slot;
    d->func       = func;
    d->vendor_id  = vendor;
    d->device_id  = pci_cfg_read16(bus, slot, func, PCI_OFF_DEVICE_ID);
    d->class_code = pci_cfg_read8(bus, slot, func, PCI_OFF_CLASS);
    d->subclass   = pci_cfg_read8(bus, slot, func, PCI_OFF_SUBCLASS);
    d->prog_if    = pci_cfg_read8(bus, slot, func, PCI_OFF_PROG_IF);
    d->header_type= pci_cfg_read8(bus, slot, func, PCI_OFF_HEADER_TYPE) & 0x7F; // mask multi-function bit
    d->irq_line   = pci_cfg_read8(bus, slot, func, PCI_OFF_IRQ_LINE);
    d->present    = true;

    // Only standard (type 0x00) headers have the 6-BAR layout we parse here.
    // Type 0x01 (PCI-to-PCI bridge) has a different layout; skip BAR parsing.
    if (d->header_type == 0x00) {
        for (int i = 0; i < 6; i++) {
            uint32_t raw = pci_cfg_read32(bus, slot, func, PCI_OFF_BAR0 + i * 4);
            d->bar[i] = raw;
            d->bar_is_io[i] = (raw & 0x1) != 0;
        }
    } else {
        for (int i = 0; i < 6; i++) { d->bar[i] = 0; d->bar_is_io[i] = false; }
    }
}

static void pci_scan_slot(uint8_t bus, uint8_t slot) {
    uint16_t vendor = pci_cfg_read16(bus, slot, 0, PCI_OFF_VENDOR_ID);
    if (vendor == PCI_VENDOR_NONE) return;

    pci_scan_function(bus, slot, 0);

    uint8_t header_type = pci_cfg_read8(bus, slot, 0, PCI_OFF_HEADER_TYPE);
    if (header_type & 0x80) {
        // Multi-function device: scan functions 1..7
        for (uint8_t func = 1; func < 8; func++) {
            pci_scan_function(bus, slot, func);
        }
    }
}

int pci_init(void) {
    device_count = 0;
    for (int i = 0; i < PCI_MAX_DEVICES; i++) devices[i].present = false;

    // Brute-force scan: 256 buses x 32 slots. This is the standard portable
    // approach when there's no ACPI/MCFG table parsing yet — every bus/slot
    // combination is queried directly via config space. Most buses beyond 0
    // won't exist (vendor reads back 0xFFFF) so this is fast in practice.
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            pci_scan_slot((uint8_t)bus, slot);
        }
    }

    return device_count;
}

int pci_device_count(void) {
    return device_count;
}

pci_device_t *pci_get_device(int idx) {
    if (idx < 0 || idx >= device_count) return NULL;
    return &devices[idx];
}

pci_device_t *pci_find_device(uint16_t vendor_id, uint16_t device_id) {
    for (int i = 0; i < device_count; i++) {
        if (devices[i].vendor_id == vendor_id && devices[i].device_id == device_id) {
            return &devices[i];
        }
    }
    return NULL;
}

pci_device_t *pci_find_class(uint8_t class_code, uint8_t subclass) {
    for (int i = 0; i < device_count; i++) {
        if (devices[i].class_code == class_code && devices[i].subclass == subclass) {
            return &devices[i];
        }
    }
    return NULL;
}

void pci_enable_device(pci_device_t *dev) {
    if (!dev) return;
    uint16_t cmd = pci_cfg_read16(dev->bus, dev->slot, dev->func, PCI_OFF_COMMAND);
    cmd |= PCI_CMD_IO_SPACE | PCI_CMD_MEM_SPACE | PCI_CMD_BUS_MASTER;
    pci_cfg_write16(dev->bus, dev->slot, dev->func, PCI_OFF_COMMAND, cmd);
}

uint32_t pci_bar_io_base(pci_device_t *dev, int bar_index) {
    if (!dev || bar_index < 0 || bar_index >= 6) return 0;
    if (!dev->bar_is_io[bar_index]) return 0;
    return dev->bar[bar_index] & 0xFFFFFFFC;  // clear low 2 bits (I/O space marker)
}

uint32_t pci_bar_mem_base(pci_device_t *dev, int bar_index) {
    if (!dev || bar_index < 0 || bar_index >= 6) return 0;
    if (dev->bar_is_io[bar_index]) return 0;
    return dev->bar[bar_index] & 0xFFFFFFF0;  // clear low 4 bits (type/prefetch flags)
}
