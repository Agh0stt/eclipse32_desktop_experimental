// =============================================================================
// Eclipse32 - FAT32 Filesystem Driver
// Full implementation: mount, open, read, write, seek, dir listing, create
// =============================================================================
#include "fat32.h"
#include "../../kernel.h"
#include "../../mm/heap.h"
#include "../../drivers/disk/ata.h"

// ---- BPB / VBR structures ----
typedef struct {
    uint8_t  jmp[3];
    uint8_t  oem[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  fat_count;
    uint16_t root_entry_count;     // 0 for FAT32
    uint16_t total_sectors_16;
    uint8_t  media_type;
    uint16_t sectors_per_fat_16;   // 0 for FAT32
    uint16_t sectors_per_track;
    uint16_t head_count;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    // FAT32 extended
    uint32_t sectors_per_fat_32;
    uint16_t ext_flags;
    uint16_t fs_version;
    uint32_t root_cluster;
    uint16_t fs_info_sector;
    uint16_t backup_boot_sector;
    uint8_t  reserved[12];
    uint8_t  drive_number;
    uint8_t  reserved2;
    uint8_t  boot_signature;
    uint32_t volume_id;
    char     volume_label[11];
    char     fs_type[8];
} PACKED fat32_bpb_t;

// Directory entry (32 bytes)
typedef struct {
    char     name[8];
    char     ext[3];
    uint8_t  attr;
    uint8_t  reserved;
    uint8_t  ctime_cs;
    uint16_t ctime;
    uint16_t cdate;
    uint16_t adate;
    uint16_t cluster_hi;
    uint16_t mtime;
    uint16_t mdate;
    uint16_t cluster_lo;
    uint32_t size;
} PACKED fat32_dirent_t;

// LFN entry
typedef struct {
    uint8_t  seq;
    uint16_t name1[5];
    uint8_t  attr;
    uint8_t  type;
    uint8_t  checksum;
    uint16_t name2[6];
    uint16_t cluster;
    uint16_t name3[2];
} PACKED fat32_lfn_t;

// File attributes
#define FAT_ATTR_READONLY   0x01
#define FAT_ATTR_HIDDEN     0x02
#define FAT_ATTR_SYSTEM     0x04
#define FAT_ATTR_VOLID      0x08
#define FAT_ATTR_DIR        0x10
#define FAT_ATTR_ARCHIVE    0x20
#define FAT_ATTR_LFN        0x0F

// ---- Filesystem state ----
typedef struct {
    bool     mounted;
    int      drive_idx;
    uint32_t lba_start;         // partition start LBA
    uint32_t bytes_per_sector;
    uint32_t sectors_per_cluster;
    uint32_t reserved_sectors;
    uint32_t fat_count;
    uint32_t sectors_per_fat;
    uint32_t root_cluster;
    uint32_t data_start_lba;    // LBA of cluster 2
    uint32_t fat_start_lba;
    uint32_t total_clusters;
    uint8_t  sector_buf[512];   // scratch sector buffer
} fat32_fs_t;

static fat32_fs_t fs;

// ---- Open file table ----
#define MAX_OPEN_FILES  32

typedef struct {
    bool     used;
    uint32_t first_cluster;
    uint32_t cur_cluster;
    uint32_t cur_sector;        // sector within cluster (0-based)
    uint32_t pos;               // byte position in file
    uint32_t size;
    bool     writable;
    bool     is_dir;
    uint8_t  buf[512];          // sector cache
    bool     buf_dirty;
    uint32_t buf_lba;
    // Directory entry location so we can write size back on close
    uint32_t de_lba;
    uint32_t de_offset;
} fat32_file_t;

static fat32_file_t file_table[MAX_OPEN_FILES];

// ---- Helper: cluster to LBA ----
static uint32_t cluster_lba(uint32_t cluster) {
    return fs.data_start_lba + (cluster - 2) * fs.sectors_per_cluster;
}

// ---- Read a sector (with simple LRU-less cache) ----
static int read_sector(uint32_t lba, void *buf) {
    return ata_read_sectors(fs.drive_idx, lba, 1, buf);
}

static int write_sector(uint32_t lba, const void *buf) {
    return ata_write_sectors(fs.drive_idx, lba, 1, buf);
}

// ---- FAT access ----
static uint32_t fat_get_entry(uint32_t cluster) {
    uint32_t fat_offset = cluster * 4;
    uint32_t sector = fs.fat_start_lba + (fat_offset / 512);
    uint32_t offset = fat_offset % 512;

    if (read_sector(sector, fs.sector_buf) != 0) return 0x0FFFFFFF;
    uint32_t val = *(uint32_t *)(fs.sector_buf + offset);
    return val & 0x0FFFFFFF;
}

static int fat_set_entry(uint32_t cluster, uint32_t value) {
    uint32_t fat_offset = cluster * 4;
    uint32_t sector = fs.fat_start_lba + (fat_offset / 512);
    uint32_t offset = fat_offset % 512;

    if (read_sector(sector, fs.sector_buf) != 0) return -1;
    uint32_t *entry = (uint32_t *)(fs.sector_buf + offset);
    *entry = (*entry & 0xF0000000) | (value & 0x0FFFFFFF);
    return write_sector(sector, fs.sector_buf);
}

// Allocate a new cluster (next free)
static uint32_t fat_alloc_cluster(void) {
    for (uint32_t c = 2; c < fs.total_clusters + 2; c++) {
        if (fat_get_entry(c) == 0) {
            fat_set_entry(c, 0x0FFFFFFF);   // EOC
            return c;
        }
    }
    return 0;  // no free clusters
}

// ---- Mounted check ----
bool fs_is_mounted(void) { return fs.mounted; }

// ---- Mount ----
int fat32_mount(int drive_idx) {
    fs.drive_idx = drive_idx;
    fs.mounted   = false;

    // Read sector 0 — could be MBR or raw VBR
    uint8_t mbr[512];
    if (ata_read_sectors(drive_idx, 0, 1, mbr) != 0) return -1;

    // Find FAT partition in MBR partition table
    // Types: 0x0B=FAT32 CHS, 0x0C=FAT32 LBA, 0x0E=FAT16 LBA, 0x06=FAT16, 0x04=FAT16 small
    uint32_t part_lba = 0;
    for (int i = 0; i < 4; i++) {
        uint8_t *pe = mbr + 0x1BE + i * 16;
        uint8_t type = pe[4];
        if (type == 0x0B || type == 0x0C ||
            type == 0x0E || type == 0x06 || type == 0x04) {
            part_lba = *(uint32_t *)(pe + 8);
            break;
        }
    }

    // Fallback: sector 0 itself may be a raw VBR (no partition table)
    // Detect by checking the OEM name / BPB signature directly
    if (part_lba == 0) {
        fat32_bpb_t *test = (fat32_bpb_t *)mbr;
        if (test->bytes_per_sector == 512 && test->fat_count >= 1 &&
            test->sectors_per_cluster > 0) {
            part_lba = 0;   // VBR is at LBA 0
        } else {
            return -1;      // No valid FAT found anywhere
        }
    }

    // Read VBR
    uint8_t vbr[512];
    if (ata_read_sectors(drive_idx, part_lba, 1, vbr) != 0) return -1;

    fat32_bpb_t *bpb = (fat32_bpb_t *)vbr;

    // Validate BPB fields
    if (bpb->bytes_per_sector != 512) return -1;
    if (bpb->fat_count < 1)           return -1;
    if (bpb->sectors_per_cluster == 0) return -1;

    fs.lba_start           = part_lba;
    fs.bytes_per_sector    = bpb->bytes_per_sector;
    fs.sectors_per_cluster = bpb->sectors_per_cluster;
    fs.reserved_sectors    = bpb->reserved_sectors;
    fs.fat_count           = bpb->fat_count;
    // FAT32 uses sectors_per_fat_32; FAT16 stores it in sectors_per_fat_16
    fs.sectors_per_fat     = bpb->sectors_per_fat_32
                           ? bpb->sectors_per_fat_32
                           : bpb->sectors_per_fat_16;
    fs.root_cluster        = bpb->root_cluster ? bpb->root_cluster : 2;
    fs.fat_start_lba       = part_lba + bpb->reserved_sectors;
    fs.data_start_lba      = fs.fat_start_lba
                           + bpb->fat_count * fs.sectors_per_fat;
    // total_sectors_32 is relative to partition start; data_start_lba is absolute.
    // Use total_sectors_16 as fallback for small FAT16 volumes.
    uint32_t total_sectors = bpb->total_sectors_32
                           ? bpb->total_sectors_32
                           : bpb->total_sectors_16;
    uint32_t data_sectors  = total_sectors - (fs.data_start_lba - part_lba);
    fs.total_clusters      = data_sectors / bpb->sectors_per_cluster;

    // Clear file table
    for (int i = 0; i < MAX_OPEN_FILES; i++) file_table[i].used = false;

    fs.mounted = true;
    return 0;
}

// ---- 8.3 name helpers ----
static void name_to_83(const char *path, char out[11]) {
    for (int i = 0; i < 11; i++) out[i] = ' ';
    int pos = 0;

    // Find the last component
    const char *last = path;
    for (const char *p = path; *p; p++) {
        if (*p == '/') last = p + 1;
    }
for (int i = 0; i < 8 && last[i] && last[i] != '.'; i++) {
    char ch = last[i];
    if (ch >= 'a' && ch <= 'z') ch = (char)(ch - ('a' - 'A'));
    out[pos++] = ch;
}

    const char *dot = (const char *)0;
    for (const char *p = last; *p; p++) { if (*p == '.') { dot = p; break; } }
    if (dot) {
        pos = 8;
for (int i = 1; i <= 3 && dot[i]; i++) {
    char ch = dot[i];
    if (ch >= 'a' && ch <= 'z') ch = (char)(ch - ('a' - 'A'));
    out[pos++] = ch;
} 
   }
}
static bool match_83(const fat32_dirent_t *de, const char name83[11]) {
    for (int i = 0; i < 11; i++) {
        if (de->name[i] != name83[i]) return false;
    }
    return true;
}

// ---- Traverse path, return file's directory entry ----
// Returns dirent and the LBA+offset of that dirent (for write-back)
typedef struct {
    fat32_dirent_t de;
    uint32_t       de_lba;
    uint32_t       de_offset;
} fat32_found_t;

static bool find_in_dir(uint32_t dir_cluster, const char name83[11],
                         fat32_found_t *found) {
    uint32_t cluster = dir_cluster;

    while (cluster < 0x0FFFFFF8) {
        for (uint32_t s = 0; s < fs.sectors_per_cluster; s++) {
            uint32_t lba = cluster_lba(cluster) + s;
            if (read_sector(lba, fs.sector_buf) != 0) return false;

            fat32_dirent_t *de = (fat32_dirent_t *)fs.sector_buf;
            for (int i = 0; i < 16; i++, de++) {
                if (de->name[0] == 0x00) return false;    // end of dir
                if ((uint8_t)de->name[0] == 0xE5) continue;  // deleted
                if (de->attr == FAT_ATTR_LFN) continue;    // skip LFN

                if (match_83(de, name83)) {
                    found->de = *de;
                    found->de_lba    = lba;
                    found->de_offset = (uint32_t)((uint8_t *)de - fs.sector_buf);
                    return true;
                }
            }
        }
        cluster = fat_get_entry(cluster);
    }
    return false;
}

// Resolve full path, return starting cluster and size
static bool fat32_resolve(const char *path, fat32_found_t *out) {
    if (!fs.mounted) return false;

    // Tokenize path
    char component[13];
    uint32_t cur_cluster = fs.root_cluster;
    const char *p = path;

    // Skip leading slash
    if (*p == '/') p++;
    if (*p == 0) {
        // Root directory
        out->de.cluster_hi = (uint16_t)(fs.root_cluster >> 16);
        out->de.cluster_lo = (uint16_t)(fs.root_cluster & 0xFFFF);
        out->de.attr = FAT_ATTR_DIR;
        out->de.size = 0;
        return true;
    }

    while (*p) {
        // Extract next component
        int len = 0;
        while (*p && *p != '/' && len < 12) {
            component[len++] = *p++;
        }
        component[len] = 0;
        if (*p == '/') p++;

        char name83[11];
        name_to_83(component, name83);

        fat32_found_t found;
        if (!find_in_dir(cur_cluster, name83, &found)) return false;

        if (*p == 0) {
            *out = found;
            return true;
        }

        // Must be a directory
        if (!(found.de.attr & FAT_ATTR_DIR)) return false;
        cur_cluster = ((uint32_t)found.de.cluster_hi << 16) | found.de.cluster_lo;
    }
    return false;
}

// ---- Find parent dir cluster from path, return last component ----
static uint32_t path_parent_cluster(const char *path, char *last_component) {
    char tmp[256];
    int len = 0;
    while (path[len] && len < 255) { tmp[len] = path[len]; len++; }
    tmp[len] = 0;

    // Find last slash
    int slash = -1;
    for (int i = len - 1; i >= 0; i--) {
        if (tmp[i] == '/') { slash = i; break; }
    }

    if (slash < 0) {
        // No slash at all - use whole string as name, parent is root
        for (int i = 0; i <= len; i++) last_component[i] = tmp[i];
        return fs.root_cluster;
    }

    // Extract last component (everything after the final slash)
    int j = 0;
    for (int i = slash + 1; tmp[i]; i++) last_component[j++] = tmp[i];
    last_component[j] = 0;

    // Parent is root if slash is at position 0
    if (slash == 0) return fs.root_cluster;

    // Otherwise resolve the parent directory
    tmp[slash] = 0;  // tmp now holds the parent path
    fat32_found_t found;
    if (!fat32_resolve(tmp, &found)) return 0;
    if (!(found.de.attr & FAT_ATTR_DIR)) return 0;
    return ((uint32_t)found.de.cluster_hi << 16) | found.de.cluster_lo;
}

// Write a new directory entry into dir_cluster for a file/dir
static int dir_add_entry(uint32_t dir_cluster, const char name83[11],
                          uint8_t attr, uint32_t first_cluster, uint32_t size) {
    uint32_t cluster = dir_cluster;
    while (cluster < 0x0FFFFFF8) {
        for (uint32_t s = 0; s < fs.sectors_per_cluster; s++) {
            uint32_t lba = cluster_lba(cluster) + s;
            if (read_sector(lba, fs.sector_buf) != 0) return -1;

            fat32_dirent_t *de = (fat32_dirent_t *)fs.sector_buf;
            for (int i = 0; i < 16; i++, de++) {
                if (de->name[0] == 0x00 || (uint8_t)de->name[0] == 0xE5) {
                    // Free slot — write entry here
                    for (int j = 0; j < 8; j++) de->name[j] = name83[j];
                    for (int j = 0; j < 3; j++) de->ext[j]  = name83[8+j];
                    de->attr       = attr;
                    de->reserved   = 0;
                    de->ctime_cs   = 0;
                    de->ctime      = 0;
                    de->cdate      = 0;
                    de->adate      = 0;
                    de->cluster_hi = (uint16_t)(first_cluster >> 16);
                    de->mtime      = 0;
                    de->mdate      = 0;
                    de->cluster_lo = (uint16_t)(first_cluster & 0xFFFF);
                    de->size       = size;
                    return write_sector(lba, fs.sector_buf);
                }
            }
        }
        uint32_t next = fat_get_entry(cluster);
        if (next >= 0x0FFFFFF8) {
            // Extend dir with new cluster
            uint32_t new_c = fat_alloc_cluster();
            if (!new_c) return -1;
            fat_set_entry(cluster, new_c);
            // Zero out new cluster
            uint8_t zero[512] = {0};
            for (uint32_t s = 0; s < fs.sectors_per_cluster; s++)
                write_sector(cluster_lba(new_c) + s, zero);
            cluster = new_c;
        } else {
            cluster = next;
        }
    }
    return -1;
}

// ---- Open file ----
int fat32_open(const char *path, int flags) {
    if (!fs.mounted) return -1;

    fat32_found_t found;
    bool exists = fat32_resolve(path, &found);

    if (!exists) {
        if (!(flags & FAT32_O_CREAT)) return -1;

        // Create new file: find parent dir, add entry
        char last[13];
        uint32_t parent_cluster = path_parent_cluster(path, last);
        if (!parent_cluster) return -1;

        char name83[11];
        name_to_83(last, name83);

        // Allocate first cluster for the file
        uint32_t first_c = fat_alloc_cluster();
        if (!first_c) return -1;

        if (dir_add_entry(parent_cluster, name83, FAT_ATTR_ARCHIVE, first_c, 0) != 0) {
            fat_set_entry(first_c, 0); // free it back
            return -1;
        }

        // Re-resolve to populate found
        if (!fat32_resolve(path, &found)) return -1;
    }

    // Allocate fd
    int fd = -1;
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (!file_table[i].used) { fd = i; break; }
    }
    if (fd < 0) return -1;

    fat32_file_t *f = &file_table[fd];
    f->used          = true;
    f->first_cluster = ((uint32_t)found.de.cluster_hi << 16) | found.de.cluster_lo;
    f->cur_cluster   = f->first_cluster;
    f->cur_sector    = 0;
    f->pos           = 0;
    f->size          = found.de.size;
    f->writable      = (flags & FAT32_O_WRONLY) || (flags & FAT32_O_RDWR);
    f->is_dir        = (found.de.attr & FAT_ATTR_DIR) != 0;
    f->buf_dirty     = false;
    f->buf_lba       = 0xFFFFFFFF;
    f->de_lba        = found.de_lba;
    f->de_offset     = found.de_offset;

    if (flags & FAT32_O_TRUNC) {
        // Free every cluster in the old chain so we don't leak FAT entries
        uint32_t c = f->first_cluster;
        while (c >= 2 && c < 0x0FFFFFF8) {
            uint32_t next = fat_get_entry(c);
            fat_set_entry(c, 0);    // mark free
            c = next;
        }
        // Allocate a fresh first cluster so the file has somewhere to write
        uint32_t new_c = fat_alloc_cluster();
        if (new_c) {
            f->first_cluster = new_c;
            f->cur_cluster   = new_c;
        }
        f->size = 0;
        f->pos  = 0;
    }
    if (flags & FAT32_O_APPEND) {
        f->pos = f->size;
    }

    return fd;
}

// ---- Close ----
int fat32_close(int fd) {
    if (fd < 0 || fd >= MAX_OPEN_FILES || !file_table[fd].used) return -1;
    fat32_file_t *f = &file_table[fd];

    // Flush sector cache
    if (f->buf_dirty) {
        write_sector(f->buf_lba, f->buf);
        f->buf_dirty = false;
    }

    // Write updated file size back to directory entry on disk
    if (f->writable && f->de_lba != 0) {
        uint8_t tmp[512];
        if (read_sector(f->de_lba, tmp) == 0) {
            fat32_dirent_t *de = (fat32_dirent_t *)(tmp + f->de_offset);
            de->size = f->size;
            // Also update first cluster in case file was empty on open (CREAT)
            de->cluster_hi = (uint16_t)(f->first_cluster >> 16);
            de->cluster_lo = (uint16_t)(f->first_cluster & 0xFFFF);
            write_sector(f->de_lba, tmp);
        }
    }

    f->used = false;
    return 0;
}

// ---- Read ----
int fat32_read(int fd, void *buf, uint32_t count) {
    if (fd < 0 || fd >= MAX_OPEN_FILES || !file_table[fd].used) return -1;
    fat32_file_t *f = &file_table[fd];

    // Guard: nothing to read if already at or past EOF
    if (f->pos >= f->size) return 0;

    uint32_t remaining = count;
    if (f->pos + remaining > f->size) remaining = f->size - f->pos;
    uint32_t total_read = 0;
    uint8_t *out = (uint8_t *)buf;

    while (remaining > 0) {
        uint32_t sector_size = 512;
        uint32_t sector_offset = f->pos % sector_size;
        uint32_t to_copy = sector_size - sector_offset;
        if (to_copy > remaining) to_copy = remaining;

        // Calculate which sector to read
        uint32_t cluster_offset = f->pos / sector_size;
        uint32_t spc = fs.sectors_per_cluster;
        uint32_t cluster_idx = cluster_offset / spc;
        uint32_t sec_in_cluster = cluster_offset % spc;

        // Walk cluster chain to the right cluster
        uint32_t cluster = f->first_cluster;
        for (uint32_t i = 0; i < cluster_idx; i++) {
            cluster = fat_get_entry(cluster);
            if (cluster >= 0x0FFFFFF8) return total_read;
        }

        uint32_t lba = cluster_lba(cluster) + sec_in_cluster;
        if (f->buf_lba != lba) {
            if (f->buf_dirty) { write_sector(f->buf_lba, f->buf); f->buf_dirty = false; }
            if (read_sector(lba, f->buf) != 0) return -1;
            f->buf_lba = lba;
        }

        for (uint32_t i = 0; i < to_copy; i++) {
            out[total_read + i] = f->buf[sector_offset + i];
        }

        f->pos    += to_copy;
        total_read += to_copy;
        remaining  -= to_copy;
    }
    return (int)total_read;
}

// ---- Write ----
int fat32_write(int fd, const void *buf, uint32_t count) {
    if (fd < 0 || fd >= MAX_OPEN_FILES) return -1;
    fat32_file_t *f = &file_table[fd];
    if (!f->used || !f->writable) return -1;

    const uint8_t *in = (const uint8_t *)buf;
    uint32_t total_written = 0;
    uint32_t remaining = count;

    while (remaining > 0) {
        uint32_t sector_size = 512;
        uint32_t sector_offset = f->pos % sector_size;
        uint32_t to_write = sector_size - sector_offset;
        if (to_write > remaining) to_write = remaining;

        uint32_t cluster_offset = f->pos / sector_size;
        uint32_t spc = fs.sectors_per_cluster;
        uint32_t cluster_idx = cluster_offset / spc;
        uint32_t sec_in_cluster = cluster_offset % spc;

        uint32_t cluster = f->first_cluster;
        if (cluster == 0) {
            cluster = fat_alloc_cluster();
            if (!cluster) return total_written;
            f->first_cluster = cluster;
        }

        for (uint32_t i = 0; i < cluster_idx; i++) {
            uint32_t next = fat_get_entry(cluster);
            if (next >= 0x0FFFFFF8) {
                next = fat_alloc_cluster();
                if (!next) return total_written;
                fat_set_entry(cluster, next);
            }
            cluster = next;
        }

        uint32_t lba = cluster_lba(cluster) + sec_in_cluster;
        if (f->buf_lba != lba) {
            if (f->buf_dirty) { write_sector(f->buf_lba, f->buf); f->buf_dirty = false; }
            read_sector(lba, f->buf);
            f->buf_lba = lba;
        }

        for (uint32_t i = 0; i < to_write; i++) {
            f->buf[sector_offset + i] = in[total_written + i];
        }
        f->buf_dirty = true;

        f->pos         += to_write;
        total_written  += to_write;
        remaining      -= to_write;
        if (f->pos > f->size) f->size = f->pos;
    }
    return (int)total_written;
}

// ---- Seek ----
int fat32_seek(int fd, int32_t offset, int whence) {
    if (fd < 0 || fd >= MAX_OPEN_FILES || !file_table[fd].used) return -1;
    fat32_file_t *f = &file_table[fd];

    int32_t new_pos;
    switch (whence) {
    case FAT32_SEEK_SET: new_pos = offset; break;
    case FAT32_SEEK_CUR: new_pos = (int32_t)f->pos + offset; break;
    case FAT32_SEEK_END: new_pos = (int32_t)f->size + offset; break;
    default: return -1;
    }

    if (new_pos < 0) new_pos = 0;
    if ((uint32_t)new_pos > f->size) new_pos = (int32_t)f->size;
    f->pos = (uint32_t)new_pos;
    return new_pos;
}

// ---- Directory listing ----
int fat32_readdir(const char *path, fat32_dir_entry_t *entries, int max_entries) {
    if (!fs.mounted) return -1;

    fat32_found_t found;
    uint32_t dir_cluster;

    if (path[0] == '/' && path[1] == 0) {
        dir_cluster = fs.root_cluster;
    } else {
        if (!fat32_resolve(path, &found)) return -1;
        if (!(found.de.attr & FAT_ATTR_DIR)) return -1;
        dir_cluster = ((uint32_t)found.de.cluster_hi << 16) | found.de.cluster_lo;
    }

    int count = 0;
    uint32_t cluster = dir_cluster;

    while (cluster < 0x0FFFFFF8 && count < max_entries) {
        for (uint32_t s = 0; s < fs.sectors_per_cluster && count < max_entries; s++) {
            uint32_t lba = cluster_lba(cluster) + s;
            if (read_sector(lba, fs.sector_buf) != 0) return count;

            fat32_dirent_t *de = (fat32_dirent_t *)fs.sector_buf;
            for (int i = 0; i < 16 && count < max_entries; i++, de++) {
                if (de->name[0] == 0x00) return count;
                if ((uint8_t)de->name[0] == 0xE5) continue;
                if (de->attr == FAT_ATTR_LFN) continue;
                if (de->attr & FAT_ATTR_VOLID) continue;
                if (de->name[0] == '.') continue;   // skip . and ..

                fat32_dir_entry_t *out = &entries[count++];

                // Convert 8.3 name to string
                int len = 0;
                for (int j = 0; j < 8 && de->name[j] != ' '; j++) {
                    out->name[len++] = de->name[j];
                }
                if (de->ext[0] != ' ') {
                    out->name[len++] = '.';
                    for (int j = 0; j < 3 && de->ext[j] != ' '; j++) {
                        out->name[len++] = de->ext[j];
                    }
                }
                out->name[len] = 0;

                out->size      = de->size;
                out->is_dir    = (de->attr & FAT_ATTR_DIR) != 0;
                out->is_hidden = (de->attr & FAT_ATTR_HIDDEN) != 0;
                out->cluster   = ((uint32_t)de->cluster_hi << 16) | de->cluster_lo;
            }
        }
        cluster = fat_get_entry(cluster);
    }
    return count;
}

// ---- Stat ----
int fat32_stat(const char *path, fat32_stat_t *st) {
    // Always allow stat on root so cd / works even without disk
    if (path[0] == '/' && path[1] == 0) {
        st->size    = 0;
        st->is_dir  = true;
        st->cluster = fs.mounted ? fs.root_cluster : 0;
        return 0;
    }
    if (!fs.mounted) return -1;

    fat32_found_t found;
    if (!fat32_resolve(path, &found)) return -1;

    st->size   = found.de.size;
    st->is_dir = (found.de.attr & FAT_ATTR_DIR) != 0;
    st->cluster = ((uint32_t)found.de.cluster_hi << 16) | found.de.cluster_lo;
    return 0;
}

// ---- Mkdir ----
int fat32_mkdir(const char *path) {
    if (!fs.mounted) return -1;
    fat32_found_t existing;
    if (fat32_resolve(path, &existing)) return -1; // already exists

    char last[13];
    uint32_t parent_cluster = path_parent_cluster(path, last);
    if (!parent_cluster) return -1;

    char name83[11];
    name_to_83(last, name83);

    // Allocate cluster for the new dir
    uint32_t dir_c = fat_alloc_cluster();
    if (!dir_c) return -1;

    // Zero the new cluster
    uint8_t zero[512];
    for (int i = 0; i < 512; i++) zero[i] = 0;
    for (uint32_t s = 0; s < fs.sectors_per_cluster; s++)
        write_sector(cluster_lba(dir_c) + s, zero);

    // Write . and .. entries
    if (read_sector(cluster_lba(dir_c), fs.sector_buf) != 0) return -1;
    fat32_dirent_t *de = (fat32_dirent_t *)fs.sector_buf;

    // "." entry
    for (int i = 0; i < 8; i++) de[0].name[i] = (i == 0) ? '.' : ' ';
    for (int i = 0; i < 3; i++) de[0].ext[i] = ' ';
    de[0].attr = FAT_ATTR_DIR;
    de[0].cluster_hi = (uint16_t)(dir_c >> 16);
    de[0].cluster_lo = (uint16_t)(dir_c & 0xFFFF);
    de[0].size = 0;

    // ".." entry
    for (int i = 0; i < 8; i++) de[1].name[i] = (i < 2) ? '.' : ' ';
    for (int i = 0; i < 3; i++) de[1].ext[i] = ' ';
    de[1].attr = FAT_ATTR_DIR;
    de[1].cluster_hi = (uint16_t)(parent_cluster >> 16);
    de[1].cluster_lo = (uint16_t)(parent_cluster & 0xFFFF);
    de[1].size = 0;

    write_sector(cluster_lba(dir_c), fs.sector_buf);

    return dir_add_entry(parent_cluster, name83, FAT_ATTR_DIR, dir_c, 0);
}

// ---- Delete (mark dir entry as 0xE5) ----
int fat32_delete(const char *path) {
    if (!fs.mounted) return -1;
    fat32_found_t found;
    if (!fat32_resolve(path, &found)) return -1;

    // Mark entry deleted
    if (read_sector(found.de_lba, fs.sector_buf) != 0) return -1;
    fs.sector_buf[found.de_offset] = 0xE5;
    if (write_sector(found.de_lba, fs.sector_buf) != 0) return -1;

    // Free cluster chain
    uint32_t cluster = ((uint32_t)found.de.cluster_hi << 16) | found.de.cluster_lo;
    while (cluster >= 2 && cluster < 0x0FFFFFF8) {
        uint32_t next = fat_get_entry(cluster);
        fat_set_entry(cluster, 0);
        cluster = next;
    }
    return 0;
}

// ---- Rename (copy entry, delete old) ----
int fat32_rename(const char *oldpath, const char *newpath) {
    if (!fs.mounted) return -1;
    fat32_found_t found;
    if (!fat32_resolve(oldpath, &found)) return -1;

    char last[13];
    uint32_t parent_cluster = path_parent_cluster(newpath, last);
    if (!parent_cluster) return -1;

    char name83[11];
    name_to_83(last, name83);

    uint32_t first_cluster = ((uint32_t)found.de.cluster_hi << 16) | found.de.cluster_lo;
    uint8_t attr = found.de.attr;
    uint32_t size = found.de.size;

    // Delete old entry
    if (read_sector(found.de_lba, fs.sector_buf) != 0) return -1;
    fs.sector_buf[found.de_offset] = 0xE5;
    write_sector(found.de_lba, fs.sector_buf);

    // Add new entry (same cluster chain)
    return dir_add_entry(parent_cluster, name83, attr, first_cluster, size);
}
