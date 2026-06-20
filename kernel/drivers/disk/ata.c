// =============================================================================
// Eclipse32 - ATA PIO Disk Driver
// Primary and secondary ATA buses, 28-bit LBA, read/write
// =============================================================================
#include "ata.h"
#include "../../kernel.h"

#define ATA_PRIMARY_BASE    0x1F0
#define ATA_PRIMARY_CTRL    0x3F6
#define ATA_SECONDARY_BASE  0x170
#define ATA_SECONDARY_CTRL  0x376

// Register offsets from base
#define ATA_REG_DATA        0x00
#define ATA_REG_ERR         0x01
#define ATA_REG_FEATURES    0x01
#define ATA_REG_SECCOUNT    0x02
#define ATA_REG_LBA_LO      0x03
#define ATA_REG_LBA_MID     0x04
#define ATA_REG_LBA_HI      0x05
#define ATA_REG_DRIVE_SEL   0x06
#define ATA_REG_STATUS      0x07
#define ATA_REG_CMD         0x07

// Status bits
#define ATA_SR_BSY   0x80
#define ATA_SR_DRDY  0x40
#define ATA_SR_DRQ   0x08
#define ATA_SR_ERR   0x01

// Commands
#define ATA_CMD_READ_PIO    0x20
#define ATA_CMD_WRITE_PIO   0x30
#define ATA_CMD_IDENTIFY    0xEC
#define ATA_CMD_FLUSH       0xE7

static ata_drive_t drives[4];  // primary master/slave, secondary master/slave
static int  active_drive = 0;

static uint8_t ata_read_status(ata_drive_t *d) {
    return inb(d->base + ATA_REG_STATUS);
}

static void ata_wait_bsy(ata_drive_t *d) {
    uint32_t timeout = 0x100000;
    while ((ata_read_status(d) & ATA_SR_BSY) && --timeout);
}

static bool ata_wait_drq_timeout(ata_drive_t *d) {
    uint32_t timeout = 0x100000;
    while (timeout--) {
        uint8_t s = ata_read_status(d);
        if (s & ATA_SR_DRQ) return true;
        if (s & ATA_SR_ERR) return false;
    }
    return false;
}

static bool ata_identify(ata_drive_t *d) {
    // Select drive
    outb(d->base + ATA_REG_DRIVE_SEL, d->is_master ? 0xA0 : 0xB0);
    io_wait();

    // Clear regs
    outb(d->base + ATA_REG_SECCOUNT, 0);
    outb(d->base + ATA_REG_LBA_LO,   0);
    outb(d->base + ATA_REG_LBA_MID,  0);
    outb(d->base + ATA_REG_LBA_HI,   0);

    outb(d->base + ATA_REG_CMD, ATA_CMD_IDENTIFY);
    io_wait();

    uint8_t status = ata_read_status(d);
    if (status == 0) return false;  // no drive

    ata_wait_bsy(d);

    // Check for ATAPI
    if (inb(d->base + ATA_REG_LBA_MID) || inb(d->base + ATA_REG_LBA_HI)) {
        return false;   // ATAPI, skip for now
    }

    // Wait for DRQ
    uint32_t timeout = 100000;
    while (timeout--) {
        status = ata_read_status(d);
        if (status & ATA_SR_DRQ) break;
        if (status & ATA_SR_ERR) return false;
    }
    if (!timeout) return false;

    // Read 256 words of IDENTIFY data
    uint16_t identify_buf[256];
    for (int i = 0; i < 256; i++) {
        identify_buf[i] = inw(d->base + ATA_REG_DATA);
    }

    // Extract sector count (LBA28: words 60-61)
    d->sectors = ((uint32_t)identify_buf[61] << 16) | identify_buf[60];

    // Extract model string (words 27-46, swapped bytes)
    for (int i = 0; i < 20; i++) {
        d->model[i*2]   = (identify_buf[27+i] >> 8) & 0xFF;
        d->model[i*2+1] = identify_buf[27+i] & 0xFF;
    }
    d->model[40] = 0;

    d->present = true;
    return true;
}

int ata_init(void) {
    // Primary master
    drives[0].base = ATA_PRIMARY_BASE;
    drives[0].ctrl = ATA_PRIMARY_CTRL;
    drives[0].is_master = true;

    // Primary slave
    drives[1].base = ATA_PRIMARY_BASE;
    drives[1].ctrl = ATA_PRIMARY_CTRL;
    drives[1].is_master = false;

    // Secondary master
    drives[2].base = ATA_SECONDARY_BASE;
    drives[2].ctrl = ATA_SECONDARY_CTRL;
    drives[2].is_master = true;

    // Secondary slave
    drives[3].base = ATA_SECONDARY_BASE;
    drives[3].ctrl = ATA_SECONDARY_CTRL;
    drives[3].is_master = false;

    int found = 0;
    for (int i = 0; i < 4; i++) {
        if (ata_identify(&drives[i])) found++;
    }

    return found > 0 ? 0 : -1;
}

// Select a drive for subsequent operations
int ata_select_drive(int drive_idx) {
    if (drive_idx < 0 || drive_idx >= 4 || !drives[drive_idx].present) return -1;
    active_drive = drive_idx;
    return 0;
}

ata_drive_t *ata_get_drive(int idx) {
    if (idx < 0 || idx >= 4) return NULL;
    return drives[idx].present ? &drives[idx] : NULL;
}

// Read sectors using LBA28 PIO
int ata_read_sectors(int drive_idx, uint32_t lba, uint8_t count, void *buf) {
    if (drive_idx < 0 || drive_idx >= 4 || !drives[drive_idx].present) return -1;
    ata_drive_t *d = &drives[drive_idx];

    ata_wait_bsy(d);

    // LBA28 + drive select
    uint8_t drive_sel = d->is_master ? 0xE0 : 0xF0;
    drive_sel |= (lba >> 24) & 0x0F;

    outb(d->base + ATA_REG_DRIVE_SEL, drive_sel);
    io_wait();

    outb(d->base + ATA_REG_SECCOUNT, count);
    outb(d->base + ATA_REG_LBA_LO,  lba & 0xFF);
    outb(d->base + ATA_REG_LBA_MID, (lba >> 8)  & 0xFF);
    outb(d->base + ATA_REG_LBA_HI,  (lba >> 16) & 0xFF);
    outb(d->base + ATA_REG_CMD, ATA_CMD_READ_PIO);

    uint16_t *ptr = (uint16_t *)buf;
    for (int s = 0; s < count; s++) {
        ata_wait_bsy(d);
        if (!ata_wait_drq_timeout(d)) return -1;

        uint8_t status = ata_read_status(d);
        if (status & ATA_SR_ERR) return -1;

        for (int i = 0; i < 256; i++) {
            ptr[s * 256 + i] = inw(d->base + ATA_REG_DATA);
        }
    }
    return 0;
}

int ata_write_sectors(int drive_idx, uint32_t lba, uint8_t count, const void *buf) {
    if (drive_idx < 0 || drive_idx >= 4 || !drives[drive_idx].present) return -1;
    ata_drive_t *d = &drives[drive_idx];

    ata_wait_bsy(d);

    uint8_t drive_sel = d->is_master ? 0xE0 : 0xF0;
    drive_sel |= (lba >> 24) & 0x0F;

    outb(d->base + ATA_REG_DRIVE_SEL, drive_sel);
    io_wait();

    outb(d->base + ATA_REG_SECCOUNT, count);
    outb(d->base + ATA_REG_LBA_LO,  lba & 0xFF);
    outb(d->base + ATA_REG_LBA_MID, (lba >> 8)  & 0xFF);
    outb(d->base + ATA_REG_LBA_HI,  (lba >> 16) & 0xFF);
    outb(d->base + ATA_REG_CMD, ATA_CMD_WRITE_PIO);

    const uint16_t *ptr = (const uint16_t *)buf;
    for (int s = 0; s < count; s++) {
        ata_wait_bsy(d);
        if (!ata_wait_drq_timeout(d)) return -1;

        for (int i = 0; i < 256; i++) {
            outw(d->base + ATA_REG_DATA, ptr[s * 256 + i]);
        }
    }

    // Cache flush
    outb(d->base + ATA_REG_CMD, ATA_CMD_FLUSH);
    ata_wait_bsy(d);
    return 0;
}
