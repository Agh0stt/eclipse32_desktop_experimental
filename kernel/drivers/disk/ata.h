#pragma once
#include "../../kernel.h"

typedef struct {
    uint16_t base;
    uint16_t ctrl;
    bool     present;
    bool     is_master;
    uint32_t sectors;
    char     model[41];
} ata_drive_t;

int          ata_init(void);
int          ata_select_drive(int drive_idx);
int          ata_read_sectors(int drive_idx, uint32_t lba, uint8_t count, void *buf);
int          ata_write_sectors(int drive_idx, uint32_t lba, uint8_t count, const void *buf);
ata_drive_t *ata_get_drive(int idx);
