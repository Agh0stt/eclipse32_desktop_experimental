// =============================================================================
// Eclipse32 - .INS package unpack + install.cfg
// Format: magic "EINS" (u32), version (u16), nfiles (u16), then for each file:
//   name_len (u16), name[], data_size (u32), payload[]
// Payload is stored raw (no compression). Extension is .ins — inspired by zip-like
// bundles but kept simple for a hobby OS.
// =============================================================================
#include "ins_pkg.h"
#include "../kernel/initramfs/initramfs.h"
#include "../kernel/drivers/vga/vga.h"
#include "../kernel/fs/fat32/fat32.h"

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t nfiles;
} PACKED ins_hdr_t;

typedef struct {
    char dest[INS_MAX_PATH];
    char app[64];
    char version[32];
} ins_cfg_t;

static int read_exact(int fd, void *buf, uint32_t n) {
    uint8_t *p = (uint8_t *)buf;
    uint32_t got = 0;
    while (got < n) {
        int r = fat32_read(fd, p + got, n - got);
        if (r <= 0) return -1;
        got += (uint32_t)r;
    }
    return 0;
}

static int skip_exact(int fd, uint32_t n) {
    if (fat32_seek(fd, (int32_t)n, FAT32_SEEK_CUR) < 0) return -1;
    return 0;
}

static int name_is_install_cfg(const char *name, uint16_t nlen) {
    static const char lit[] = "install.cfg";
    size_t L = kstrlen(lit);
    if ((size_t)nlen != L) return 0;
    for (size_t i = 0; i < L; i++) {
        char a = name[i], b = lit[i];
        if (a >= 'A' && a <= 'Z') a = (char)(a + 32);
        if (a != b) return 0;
    }
    return 1;
}

static void cfg_default(ins_cfg_t *c) {
    kstrncpy(c->dest, "/apps", INS_MAX_PATH);
    c->dest[INS_MAX_PATH - 1] = 0;
    c->app[0] = 0;
    c->version[0] = 0;
}

static void trim_inplace(char *s) {
    char *p = s;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    if (p != s) {
        char *q = s;
        while (*p) *q++ = *p++;
        *q = 0;
    }
    int len = (int)kstrlen(s);
    while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t' || s[len - 1] == '\r' || s[len - 1] == '\n')) {
        s[--len] = 0;
    }
}

static int key_match(const char *key, const char *ref) {
    for (; *ref; key++, ref++) {
        char a = *key, b = *ref;
        if (a >= 'A' && a <= 'Z') a = (char)(a + 32);
        if (b >= 'A' && b <= 'Z') b = (char)(b + 32);
        if (a != b) return 0;
    }
    return *key == 0;
}

// Parse install.cfg into ins_cfg_t (KEY=value lines, # comments).
static void parse_install_cfg(const char *buf, int len, ins_cfg_t *out) {
    cfg_default(out);
    char line[256];
    int li = 0;
    for (int i = 0; i <= len; i++) {
        char c = (i < len) ? buf[i] : '\n';
        if (c == '\r') continue;
        if (c != '\n') {
            if (li < (int)sizeof(line) - 1) line[li++] = c;
            continue;
        }
        line[li] = 0;
        li = 0;
        trim_inplace(line);
        if (!line[0] || line[0] == '#') continue;
        char *eq = kstrchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        char *val = eq + 1;
        trim_inplace(line);
        trim_inplace(val);
        if (key_match(line, "dest")) {
            if (val[0] == '/') kstrncpy(out->dest, val, INS_MAX_PATH);
            else {
                out->dest[0] = '/';
                kstrncpy(out->dest + 1, val, INS_MAX_PATH - 1);
            }
            out->dest[INS_MAX_PATH - 1] = 0;
        } else if (key_match(line, "app")) {
            kstrncpy(out->app, val, sizeof(out->app));
            out->app[sizeof(out->app) - 1] = 0;
        } else if (key_match(line, "version")) {
            kstrncpy(out->version, val, sizeof(out->version));
            out->version[sizeof(out->version) - 1] = 0;
        }
    }
}

static int mkdir_p(const char *dirpath) {
    if (!dirpath || dirpath[0] != '/') return -1;
    char acc[INS_MAX_PATH];
    acc[0] = '/';
    acc[1] = 0;
    const char *p = dirpath + 1;
    while (*p) {
        const char *start = p;
        while (*p && *p != '/') p++;
        int seglen = (int)(p - start);
        if (seglen <= 0) {
            while (*p == '/') p++;
            continue;
        }
        int alen = (int)kstrlen(acc);
        if (alen > 1) acc[alen++] = '/';
        for (int i = 0; i < seglen && alen < INS_MAX_PATH - 1; i++) acc[alen++] = start[i];
        acc[alen] = 0;
        fat32_stat_t st;
        if (fat32_stat(acc, &st) == 0) {
            if (!st.is_dir) {
                return -1;
            }
        } else {
            if (fat32_mkdir(acc) != 0) return -1;
        }
        while (*p == '/') p++;
    }
    return 0;
}

static void path_dirname(const char *path, char *out) {
    kstrncpy(out, path, INS_MAX_PATH);
    out[INS_MAX_PATH - 1] = 0;
    char *last = kstrrchr(out, '/');
    if (!last) {
        out[0] = '/';
        out[1] = 0;
        return;
    }
    if (last == out) {
        out[1] = 0;
        return;
    }
    *last = 0;
}

// DEST + "/" + rel. rel must not start with / (handled).
static void join_dest(const char *dest, const char *rel, char *out) {
    int dl = (int)kstrlen(dest);
    while (dl > 0 && dest[dl - 1] == '/') dl--;
    if (rel[0] == '/') {
        kstrncpy(out, rel, INS_MAX_PATH);
    } else {
        int i = 0;
        for (int j = 0; j < dl && i < INS_MAX_PATH - 1; j++) out[i++] = dest[j];
        if (i > 0 && out[i - 1] != '/') out[i++] = '/';
        kstrncpy(out + i, rel, INS_MAX_PATH - i);
    }
    out[INS_MAX_PATH - 1] = 0;
}

static int copy_out_stream(int fd_in, uint32_t nbytes, const char *outpath) {
    char dir[INS_MAX_PATH];
    path_dirname(outpath, dir);
    if (kstrcmp(dir, "/") != 0 && dir[1] != 0) {
        if (mkdir_p(dir) != 0) return -1;
    }
    int fo = fat32_open(outpath, FAT32_O_RDWR | FAT32_O_CREAT | FAT32_O_TRUNC);
    if (fo < 0) return -1;
    char buf[512];
    while (nbytes > 0) {
        uint32_t chunk = nbytes > sizeof(buf) ? (uint32_t)sizeof(buf) : nbytes;
        if (read_exact(fd_in, buf, chunk) != 0) {
            fat32_close(fo);
            return -1;
        }
        if (fat32_write(fo, buf, chunk) != (int)chunk) {
            fat32_close(fo);
            return -1;
        }
        nbytes -= chunk;
    }
    fat32_close(fo);
    return 0;
}

// First pass: load install.cfg content if present.
static int pass_load_cfg(int fd, char *cfg_buf, int *cfg_len) {
    *cfg_len = 0;
    if (fat32_seek(fd, 0, FAT32_SEEK_SET) < 0) return -1;
    ins_hdr_t hdr;
    if (read_exact(fd, &hdr, sizeof(hdr)) != 0) return -1;
    if (hdr.magic != INS_MAGIC || hdr.version != INS_VERSION || hdr.nfiles == 0 || hdr.nfiles > 512) {
        return -1;
    }
    for (uint16_t i = 0; i < hdr.nfiles; i++) {
        uint16_t nlen = 0;
        if (read_exact(fd, &nlen, sizeof(nlen)) != 0) return -1;
        if (nlen == 0 || nlen > 255) return -1;
        char name[256];
        if (read_exact(fd, name, nlen) != 0) return -1;
        name[nlen] = 0;
        uint32_t dsize = 0;
        if (read_exact(fd, &dsize, sizeof(dsize)) != 0) return -1;
        if (name_is_install_cfg(name, nlen)) {
            if (dsize > (uint32_t)INS_CFG_MAX_SIZE) return -1;
            if (dsize == 0) return 0;
            if (read_exact(fd, cfg_buf, dsize) != 0) return -1;
            *cfg_len = (int)dsize;
            return 0;
        }
        if (skip_exact(fd, dsize) != 0) return -1;
    }
    return 0;
}

int ins_install_from_file(const char *resolved_ins_path) {
    char cfg_buf[INS_CFG_MAX_SIZE];
    ins_cfg_t cfg;

    int fi = fat32_open(resolved_ins_path, FAT32_O_RDONLY);
    if (fi < 0) {
        vga_puts("[install] cannot open package file\n");
        return -2;
    }

    int cfg_len = 0;
    if (pass_load_cfg(fi, cfg_buf, &cfg_len) != 0) {
        fat32_close(fi);
        vga_puts("[install] invalid or corrupt .INS package\n");
        return -1;
    }
    parse_install_cfg(cfg_buf, cfg_len, &cfg);

    if (fat32_seek(fi, 0, FAT32_SEEK_SET) < 0) {
        fat32_close(fi);
        return -1;
    }

    ins_hdr_t hdr;
    if (read_exact(fi, &hdr, sizeof(hdr)) != 0) {
        fat32_close(fi);
        return -1;
    }
    if (hdr.magic != INS_MAGIC || hdr.version != INS_VERSION) {
        fat32_close(fi);
        return -1;
    }

    if (mkdir_p(cfg.dest) != 0) {
        fat32_close(fi);
        vga_puts("[install] cannot create DEST directory\n");
        return -3;
    }

    vga_printf("[install] DEST=%s", cfg.dest);
    if (cfg.app[0]) vga_printf("  APP=%s", cfg.app);
    if (cfg.version[0]) vga_printf("  v%s", cfg.version);
    vga_puts("\n");

    for (uint16_t i = 0; i < hdr.nfiles; i++) {
        uint16_t nlen = 0;
        if (read_exact(fi, &nlen, sizeof(nlen)) != 0) goto fail;
        if (nlen == 0 || nlen > 255) goto fail;
        char name[256];
        if (read_exact(fi, name, nlen) != 0) goto fail;
        name[nlen] = 0;
        uint32_t dsize = 0;
        if (read_exact(fi, &dsize, sizeof(dsize)) != 0) goto fail;

        if (name_is_install_cfg(name, nlen)) {
            if (skip_exact(fi, dsize) != 0) goto fail;
            continue;
        }

        char outpath[INS_MAX_PATH];
        join_dest(cfg.dest, name, outpath);

        vga_printf("[install] -> %s\n", outpath);
        if (copy_out_stream(fi, dsize, outpath) != 0) {
            vga_puts("[install] write failed\n");
            fat32_close(fi);
            return -4;
        }
    }

    fat32_close(fi);

    if (fat32_delete(resolved_ins_path) != 0) {
        vga_puts("[install] warning: could not remove package file\n");
    } else {
        vga_printf("[install] removed %s\n", resolved_ins_path);
    }

    vga_puts("[install] done.\n");
    return 0;

fail:
    fat32_close(fi);
    vga_puts("[install] unexpected EOF or corrupt archive\n");
    return -1;
}
