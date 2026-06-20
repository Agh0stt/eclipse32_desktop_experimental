// =============================================================================
// Eclipse32 - FAT32 Filesystem Header
// =============================================================================
#pragma once
#include "../../kernel.h"

// Open flags
#define FAT32_O_RDONLY  0x00
#define FAT32_O_WRONLY  0x01
#define FAT32_O_RDWR    0x02
#define FAT32_O_CREAT   0x40
#define FAT32_O_TRUNC   0x200
#define FAT32_O_APPEND  0x400

// Seek whence
#define FAT32_SEEK_SET  0
#define FAT32_SEEK_CUR  1
#define FAT32_SEEK_END  2

typedef struct {
    char     name[256];
    uint32_t size;
    uint32_t cluster;
    bool     is_dir;
    bool     is_hidden;
} fat32_dir_entry_t;

typedef struct {
    uint32_t size;
    uint32_t cluster;
    bool     is_dir;
} fat32_stat_t;

int  fat32_mount(int drive_idx);
bool fs_is_mounted(void);
int  fat32_open(const char *path, int flags);
int  fat32_close(int fd);
int  fat32_read(int fd, void *buf, uint32_t count);
int  fat32_write(int fd, const void *buf, uint32_t count);
int  fat32_seek(int fd, int32_t offset, int whence);
int  fat32_readdir(const char *path, fat32_dir_entry_t *entries, int max_entries);
int  fat32_stat(const char *path, fat32_stat_t *st);
int  fat32_mkdir(const char *path);
int  fat32_delete(const char *path);
int  fat32_rename(const char *oldpath, const char *newpath);
