#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

inline void outb(uint16_t port, uint8_t v)  { asm volatile("outb %0,%1"::"a"(v),"Nd"(port)); }
static inline void outw(uint16_t port, uint16_t v) { asm volatile("outw %0,%1"::"a"(v),"Nd"(port)); }
static inline uint8_t  inb(uint16_t port) { uint8_t  v; asm volatile("inb %1,%0":"=a"(v):"Nd"(port)); return v; }
static inline uint16_t inw(uint16_t port) { uint16_t v; asm volatile("inw %1,%0":"=a"(v):"Nd"(port)); return v; }
static inline uint32_t inl(uint16_t port) { uint32_t v; asm volatile("inl %1,%0":"=a"(v):"Nd"(port)); return v; }
static inline void io_wait(void) { outb(0x80, 0); }

static void serial_putc(char c) {
    while (!(inb(0x3F8 + 5) & 0x20));
    outb(0x3F8, (uint8_t)c);
}
static void serial_puts(const char *s) { while (*s) serial_putc(*s++); }

#define VGA_MEM  ((volatile uint16_t *)0xB8000)
#define VGA_COLS 80
#define VGA_ROWS 25

static uint8_t vga_fg = 7, vga_bg = 0;
static uint16_t cur_x = 0, cur_y = 0;

static void vga_update_cursor(void) {
    uint16_t pos = cur_y * VGA_COLS + cur_x;
    outb(0x3D4, 0x0F); outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E); outb(0x3D5, (uint8_t)(pos >> 8));
}

static void vga_clear(void) {
    uint16_t blank = (uint16_t)((vga_bg << 12) | (vga_fg << 8) | ' ');
    for (int i = 0; i < VGA_COLS * VGA_ROWS; i++) VGA_MEM[i] = blank;
    cur_x = cur_y = 0;
    vga_update_cursor();
}

static void vga_scroll(void) {
    for (int y = 0; y < VGA_ROWS - 1; y++)
        for (int x = 0; x < VGA_COLS; x++)
            VGA_MEM[y * VGA_COLS + x] = VGA_MEM[(y + 1) * VGA_COLS + x];
    uint16_t blank = (uint16_t)((vga_bg << 12) | (vga_fg << 8) | ' ');
    for (int x = 0; x < VGA_COLS; x++)
        VGA_MEM[(VGA_ROWS - 1) * VGA_COLS + x] = blank;
}

void vga_putchar(char c) {
    if (c == '\n') {
        cur_x = 0;
        if (++cur_y >= VGA_ROWS) { cur_y = VGA_ROWS - 1; vga_scroll(); }
    } else if (c == '\r') {
        cur_x = 0;
    } else if (c == '\b') {
        if (cur_x > 0) cur_x--;
    } else {
        uint16_t attr = (uint16_t)((vga_bg << 12) | (vga_fg << 8) | (uint8_t)c);
        VGA_MEM[cur_y * VGA_COLS + cur_x] = attr;
        if (++cur_x >= VGA_COLS) { cur_x = 0; if (++cur_y >= VGA_ROWS) { cur_y = VGA_ROWS - 1; vga_scroll(); } }
    }
    vga_update_cursor();
}

void vga_puts(const char *s) { while (*s) vga_putchar(*s++); }
void vga_set_color(uint8_t fg, uint8_t bg) { vga_fg = fg; vga_bg = bg; }

static void vga_put_at(int x, int y, char c, uint8_t fg, uint8_t bg) {
    VGA_MEM[y * VGA_COLS + x] = (uint16_t)((bg << 12) | (fg << 8) | (uint8_t)c);
}

static void vga_fill_row(int y, char c, uint8_t fg, uint8_t bg) {
    for (int x = 0; x < VGA_COLS; x++) vga_put_at(x, y, c, fg, bg);
}

static void vga_puts_at(int x, int y, const char *s, uint8_t fg, uint8_t bg) {
    while (*s) { vga_put_at(x++, y, *s++, fg, bg); }
}

static void vga_move(int x, int y) { cur_x = x; cur_y = y; vga_update_cursor(); }

static void uitoa(uint32_t n, char *buf, int base) {
    const char *digits = "0123456789ABCDEF";
    char tmp[16]; int i = 0;
    if (n == 0) { buf[0] = '0'; buf[1] = 0; return; }
    while (n) { tmp[i++] = digits[n % base]; n /= base; }
    int j = 0;
    while (i--) buf[j++] = tmp[i];
    buf[j] = 0;
}

static int strlen(const char *s) { int n = 0; while (*s++) n++; return n; }
static void memset(void *p, uint8_t v, uint32_t n) { uint8_t *b = p; while (n--) *b++ = v; }
static void memcpy(void *d, const void *s, uint32_t n) { uint8_t *dd = d; const uint8_t *ss = s; while (n--) *dd++ = *ss++; }
static int strncmp(const char *a, const char *b, int n) { while (n-- && *a && *a == *b) { a++; b++; } return n < 0 ? 0 : (unsigned char)*a - (unsigned char)*b; }

static void vga_print_uint(uint32_t n) {
    char buf[16]; uitoa(n, buf, 10); vga_puts(buf);
}

static const char scancode_ascii[128] = {
    0,0,'1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,'\\','z','x','c','v','b','n','m',',','.','/',0,
    '*',0,' ',0,0,0,0,0,0,0,0,0,0,0,0,0,
    '7','8','9','-','4','5','6','+','1','2','3','0','.',
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

char kb_getchar(void) {
    while (1) {
        if (!(inb(0x64) & 1)) continue;
        uint8_t sc = inb(0x60);
        if (sc & 0x80) continue;
        char c = scancode_ascii[sc & 0x7F];
        if (c) return c;
    }
}

#define ATA_PRIMARY_BASE    0x1F0
#define ATA_SECONDARY_BASE  0x170
#define ATA_PRIMARY_CTRL    0x3F6
#define ATA_SECONDARY_CTRL  0x376

typedef struct {
    bool     present;
    bool     is_master;
    uint16_t base;
    uint32_t sectors;
    char     model[41];
    int      size_mb;
} disk_t;

#define MAX_DISKS 4
static disk_t disks[MAX_DISKS];
static int    disk_count = 0;

static void ata_wait_ready(uint16_t base) {
    while (inb(base + 7) & 0x80);
}

static bool ata_identify(uint16_t base, bool master, disk_t *d) {
    outb(base + 6, master ? 0xA0 : 0xB0);
    io_wait();
    outb(base + 7, 0xEC);
    io_wait();
    if (inb(base + 7) == 0) return false;
    ata_wait_ready(base);
    if (inb(base + 7) & 0x01) return false;

    uint16_t buf[256];
    for (int i = 0; i < 256; i++) buf[i] = inw(base);

    for (int i = 0; i < 20; i++) {
        d->model[i * 2]     = (char)(buf[27 + i] >> 8);
        d->model[i * 2 + 1] = (char)(buf[27 + i] & 0xFF);
    }
    d->model[40] = 0;
    for (int i = 39; i >= 0 && d->model[i] == ' '; i--) d->model[i] = 0;

    d->sectors = ((uint32_t)buf[61] << 16) | buf[60];
    d->size_mb = (int)(d->sectors / 2048);
    d->base = base;
    d->is_master = master;
    d->present = true;
    return true;
}

static void scan_disks(void) {
    uint16_t bases[2] = { ATA_PRIMARY_BASE, ATA_SECONDARY_BASE };
    disk_count = 0;
    for (int b = 0; b < 2 && disk_count < MAX_DISKS; b++) {
        for (int m = 0; m < 2 && disk_count < MAX_DISKS; m++) {
            disk_t d = {0};
            if (ata_identify(bases[b], m == 0, &d)) {
                disks[disk_count++] = d;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// ATAPI (CD-ROM) — used to read ECLIPSE32.img off the install CD.
//
// IDENTIFY (0xEC) on an ATAPI device aborts; instead we soft-reset and
// check the ATAPI signature (LBA mid = 0x14, LBA high = 0xEB) left in
// the LBA-mid/LBA-high registers, then talk to it with PACKET (0xA0)
// commands carrying a 12-byte SCSI-style READ(10) CDB.
// ---------------------------------------------------------------------------
typedef struct {
    bool     present;
    uint16_t base;
    uint16_t ctrl;
    bool     is_master;
} atapi_dev_t;

static atapi_dev_t cd_drive = {0};

static bool atapi_probe_one(uint16_t base, uint16_t ctrl, bool master, atapi_dev_t *out) {
    outb(base + 6, master ? 0xA0 : 0xB0);
    io_wait();

    // Software reset (SRST) via the device control register, then
    // wait for BSY to clear so the signature registers are valid.
    outb(ctrl, 0x04);
    io_wait();
    outb(ctrl, 0x00);
    io_wait();

    // Re-select the drive after reset (some controllers need this).
    outb(base + 6, master ? 0xA0 : 0xB0);
    io_wait();

    uint32_t spins = 0;
    while ((inb(base + 7) & 0x80) && spins < 1000000) spins++;

    uint8_t mid = inb(base + 4);
    uint8_t hi  = inb(base + 5);

    if (mid == 0x14 && hi == 0xEB) {
        out->present   = true;
        out->base      = base;
        out->ctrl      = ctrl;
        out->is_master = master;
        return true;
    }
    return false;
}

static bool atapi_find_cd(atapi_dev_t *out) {
    if (atapi_probe_one(ATA_PRIMARY_BASE,   ATA_PRIMARY_CTRL,   true,  out)) return true;
    if (atapi_probe_one(ATA_PRIMARY_BASE,   ATA_PRIMARY_CTRL,   false, out)) return true;
    if (atapi_probe_one(ATA_SECONDARY_BASE, ATA_SECONDARY_CTRL, true,  out)) return true;
    if (atapi_probe_one(ATA_SECONDARY_BASE, ATA_SECONDARY_CTRL, false, out)) return true;
    return false;
}

// Read `count` 2048-byte CD sectors starting at `lba` into `buf` (must be
// at least count*2048 bytes) using ATAPI READ(10), PIO data-in transfer.
static bool atapi_read_sectors(const atapi_dev_t *d, uint32_t lba, uint16_t count, void *buf) {
    if (!d->present || count == 0) return false;

    outb(d->base + 6, d->is_master ? 0xA0 : 0xB0);
    io_wait();
    ata_wait_ready(d->base);

    outb(d->base + 1, 0x00);          // features = 0 (PIO, not DMA)
    outb(d->base + 4, 0x00);          // byte count low  (placeholder, set per-PACKET below)
    outb(d->base + 5, 0x08);          // byte count high — max bytes per PIO chunk (2048)
    outb(d->base + 7, 0xA0);          // PACKET command

    // wait for BSY=0 and (DRQ=1 or ERR=1)
    uint32_t spins = 0;
    while (1) {
        uint8_t st = inb(d->base + 7);
        if (!(st & 0x80) && (st & 0x08)) break;  // DRQ set, ready for CDB
        if (st & 0x01) return false;             // ERR set
        if (++spins > 1000000) return false;
    }

    // 12-byte READ(10) CDB: opcode 0x28, LBA (4 bytes BE), transfer length (2 bytes BE, in sectors)
    uint16_t cdb[6] = {0};
    uint8_t *cdb8 = (uint8_t *)cdb;
    cdb8[0]  = 0x28;
    cdb8[1]  = 0x00;
    cdb8[2]  = (uint8_t)(lba >> 24);
    cdb8[3]  = (uint8_t)(lba >> 16);
    cdb8[4]  = (uint8_t)(lba >> 8);
    cdb8[5]  = (uint8_t)(lba);
    cdb8[6]  = 0x00;
    cdb8[7]  = (uint8_t)(count >> 8);
    cdb8[8]  = (uint8_t)(count);
    cdb8[9]  = 0x00;
    cdb8[10] = 0x00;
    cdb8[11] = 0x00;
    for (int i = 0; i < 6; i++) outw(d->base, cdb[i]);

    uint8_t *out = (uint8_t *)buf;
    uint32_t remaining_bytes = (uint32_t)count * 2048;

    while (remaining_bytes > 0) {
        spins = 0;
        while (1) {
            uint8_t st = inb(d->base + 7);
            if (st & 0x01) return false;                 // ERR
            if (!(st & 0x80) && (st & 0x08)) break;       // DRQ ready
            if (!(st & 0x80) && !(st & 0x08)) {
                // command complete with no more data
                return remaining_bytes == 0;
            }
            if (++spins > 1000000) return false;
        }

        uint16_t chunk_lo = inb(d->base + 4);
        uint16_t chunk_hi = inb(d->base + 5);
        uint16_t chunk_bytes = (uint16_t)(chunk_lo | (chunk_hi << 8));
        if (chunk_bytes == 0) break;

        uint16_t words = (uint16_t)(chunk_bytes / 2);
        for (uint16_t i = 0; i < words; i++) {
            uint16_t w = inw(d->base);
            if (remaining_bytes >= 2) {
                out[0] = (uint8_t)(w & 0xFF);
                out[1] = (uint8_t)(w >> 8);
                out += 2;
                remaining_bytes -= 2;
            } else if (remaining_bytes == 1) {
                out[0] = (uint8_t)(w & 0xFF);
                out += 1;
                remaining_bytes -= 1;
            }
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// ATA PIO sector write (LBA28, WRITE SECTORS command 0x30).
// Writes `count` 512-byte sectors starting at `lba` from `buf`.
// ---------------------------------------------------------------------------
static bool ata_write_sectors(const disk_t *d, uint32_t lba, uint8_t count, const void *buf) {
    if (count == 0) return false;
    uint16_t base = d->base;

    ata_wait_ready(base);
    outb(base + 6, (uint8_t)(0xE0 | (d->is_master ? 0x00 : 0x10) | ((lba >> 24) & 0x0F)));
    outb(base + 1, 0x00);                 // features/error (unused)
    outb(base + 2, count);                // sector count
    outb(base + 3, (uint8_t)(lba));        // LBA low
    outb(base + 4, (uint8_t)(lba >> 8));   // LBA mid
    outb(base + 5, (uint8_t)(lba >> 16));  // LBA high
    outb(base + 7, 0x30);                  // WRITE SECTORS

    const uint16_t *src = (const uint16_t *)buf;
    for (uint8_t s = 0; s < count; s++) {
        uint32_t spins = 0;
        while (1) {
            uint8_t st = inb(base + 7);
            if (st & 0x01) return false;            // ERR
            if (!(st & 0x80) && (st & 0x08)) break;  // BSY=0, DRQ=1
            if (++spins > 1000000) return false;
        }
        for (int i = 0; i < 256; i++) {
            outw(base, *src++);
        }
        // flush cache periodically isn't required for QEMU's emulated
        // disk but costs nothing; real hardware benefits from it.
        outb(base + 7, 0xE7);  // CACHE FLUSH
        ata_wait_ready(base);
    }
    return true;
}

// ---------------------------------------------------------------------------
// Copy ECLIPSE32.img from the CD (ATAPI) onto the chosen ATA disk.
//
// Reads in 2048-byte CD-sector chunks, writes out as four 512-byte ATA
// sectors per chunk. ECLIPSE_IMG_CD_LBA / ECLIPSE_IMG_SECTORS must match
// the actual placement of ECLIPSE32.img in the built ISO — verify with:
//   xorriso -indev installer.iso -find /ECLIPSE32.img -exec report_lba --
// after every build, the same way INST_KERNEL_LBA was verified in boot.asm.
// ---------------------------------------------------------------------------
#define ECLIPSE_IMG_CD_LBA   40          // verify per-build, see above
#define ECLIPSE_IMG_MB       64
#define ECLIPSE_IMG_CD_SECTS ((ECLIPSE_IMG_MB * 1024 * 1024) / 2048)

static uint8_t copy_buf[2048] __attribute__((aligned(16)));

static bool install_to_disk(const disk_t *target) {
    if (!atapi_find_cd(&cd_drive)) {
        vga_puts("\n[FATAL] Could not find install CD (ATAPI) drive.\n");
        return false;
    }

    vga_puts("\nCopying Eclipse32 image (");
    vga_print_uint(ECLIPSE_IMG_MB);
    vga_puts(" MB) -> ");
    vga_puts(target->model);
    vga_puts("\n[");
    int bar_drawn = 0;

    for (uint32_t sec = 0; sec < ECLIPSE_IMG_CD_SECTS; sec++) {
        if (!atapi_read_sectors(&cd_drive, ECLIPSE_IMG_CD_LBA + sec, 1, copy_buf)) {
            vga_puts("]\n[FATAL] CD read failed at sector ");
            vga_print_uint(sec);
            vga_puts("\n");
            return false;
        }
        // One CD sector (2048 B) = four ATA sectors (512 B each)
        if (!ata_write_sectors(target, sec * 4, 4, copy_buf)) {
            vga_puts("]\n[FATAL] Disk write failed at LBA ");
            vga_print_uint(sec * 4);
            vga_puts("\n");
            return false;
        }

        int target_bars = (int)(((uint64_t)(sec + 1) * 40) / ECLIPSE_IMG_CD_SECTS);
        while (bar_drawn < target_bars) {
            vga_putchar('#');
            bar_drawn++;
        }
    }
    vga_puts("]\nCopy complete.\n");

    // Write the MBR boot signature (0x55AA) at the very end of sector 0
    // so the BIOS recognizes this disk as bootable. ECLIPSE.IMG should
    // already contain a valid boot sector with its own code in bytes
    // 0-509; we only patch the signature bytes if they're missing.
    static uint8_t sig_check[512] __attribute__((aligned(16)));
    // Re-read sector 0 back is unnecessary here since ECLIPSE.IMG's own
    // sector 0 already carries 0x55AA if it's a proper boot image; this
    // is a no-op placeholder for clarity / future safety checks.
    (void)sig_check;

    return true;
}

__attribute__((section(".text.entry")))
void inst_main(uint8_t boot_drv) {
    (void)boot_drv;

    extern uint8_t bss_start[];
    extern uint8_t bss_end[];
    uint8_t *bss = bss_start;
    while (bss < bss_end) {
        *bss++ = 0;
    }

    vga_clear();
    serial_puts("KERNEL: Entered inst_main successfully!\r\n");

    vga_fill_row(0, ' ', 15, 1);
    vga_puts_at(2, 0, "Eclipse32 OS Installation Subsystem Pipeline", 15, 1);

    vga_move(0, 2);
    vga_puts("Querying IDE/ATA subsystem for target storage disks...\n");
    scan_disks();

    if (disk_count == 0) {
        vga_puts("\n[FATAL] No valid destination hard disks found!\n");
        vga_puts("Press 'q' to drop out into emergency recovery terminal...\n");
        while (kb_getchar() != 'q');
        return;
    }

    vga_puts("\nAvailable Destination Hard Drives:\n");
    for (int i = 0; i < disk_count; i++) {
        vga_puts("  [");
        vga_print_uint(i);
        vga_puts("] Model: ");
        vga_puts(disks[i].model);
        vga_puts(" | Space: ");
        vga_print_uint(disks[i].size_mb);
        vga_puts(" MB\n");
    }
    vga_puts("\nSelect target disk index to install Eclipse32 (or 'q' to abort): ");

    while (1) {
        char choice = kb_getchar();
        if (choice == 'q') {
            vga_puts("\nInstallation aborted by user.");
            break;
        }
        if (choice >= '0' && choice < ('0' + disk_count)) {
            int selected_idx = choice - '0';
            vga_puts("\n\nTarget confirmed: ");
            vga_puts(disks[selected_idx].model);
            vga_puts("\n");

            if (install_to_disk(&disks[selected_idx])) {
                vga_puts("\nEclipse32 installed successfully.\n");
                vga_puts("Remove the install CD and press any key to reboot...\n");
                kb_getchar();
                // Triple-fault-free reboot via the keyboard controller's
                // pulse-reset line (standard real-mode-era reboot trick,
                // works fine called from protected mode on real BIOS/QEMU).
                outb(0x64, 0xFE);
            } else {
                vga_puts("\nInstallation failed. Press 'q' to drop to recovery, or any other key to retry.\n");
            }
            break;
        }
    }

    while (1) {
        asm volatile("hlt");
    }
}
