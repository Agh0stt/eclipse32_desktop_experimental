// =============================================================================
// Eclipse32 - HTTP/1.0 GET client
// =============================================================================
#include "http.h"
#include "tcp.h"
#include "dns.h"
#include "net.h"
#include "../mm/heap.h"
#include "../fs/fat32/fat32.h"
#include "../arch/x86/pit.h"
#include "../syscall/syscall.h"
#include "../initramfs/initramfs.h"  // kstrlen, kstrcpy, etc.
#include <stdarg.h>

// ---------------------------------------------------------------------------
// Debug tracing that's actually visible: vga_puts()/vga_printf() write to
// the legacy 0xB8000 text plane, which the GUI desktop never displays, so
// this whole file's diagnostics were invisible during normal (GUI) use.
// Route through syscall_debug_puts() instead, which writes to whatever
// terminal is currently attached (falls back to VGA text mode if none is).
// Minimal formatter — only the few conversions this file actually uses.
// ---------------------------------------------------------------------------
static void dbg_putn(uint32_t n, int base) {
    char tmp[12];
    int i = 0;
    if (n == 0) { tmp[i++] = '0'; }
    while (n > 0) {
        uint32_t d = n % (uint32_t)base;
        tmp[i++] = (char)(d < 10 ? ('0' + d) : ('a' + (d - 10)));
        n /= (uint32_t)base;
    }
    char out[13];
    int j = 0;
    while (i > 0) out[j++] = tmp[--i];
    out[j] = '\0';
    syscall_debug_puts(out);
}

static void dbg_printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char ch[2] = {0, 0};

    while (*fmt) {
        if (*fmt != '%') {
            ch[0] = *fmt++;
            syscall_debug_puts(ch);
            continue;
        }
        fmt++; // skip '%'
        switch (*fmt) {
        case 'd': {
            int32_t n = va_arg(args, int32_t);
            if (n < 0) { syscall_debug_puts("-"); n = -n; }
            dbg_putn((uint32_t)n, 10);
            break;
        }
        case 'u':
            dbg_putn(va_arg(args, uint32_t), 10);
            break;
        case 's':
            syscall_debug_puts(va_arg(args, const char *));
            break;
        case '%':
            syscall_debug_puts("%");
            break;
        default:
            ch[0] = '%';
            syscall_debug_puts(ch);
            ch[0] = *fmt;
            syscall_debug_puts(ch);
            break;
        }
        fmt++;
    }
    va_end(args);
}

// ---------------------------------------------------------------------------
// URL parser: splits "http://hostname[:port]/path" into parts.
// ---------------------------------------------------------------------------
typedef struct {
    char     host[256];
    char     path[512];
    uint16_t port;
} http_url_t;

static bool parse_url(const char *url, http_url_t *out) {
    // Must start with http://
    if (kstrncmp(url, "http://", 7) != 0) return false;
    const char *p = url + 7;

    // Find end of host (either '/' or ':' or end of string)
    int hi = 0;
    out->port = 80;
    while (*p && *p != '/' && *p != ':') {
        if (hi >= 255) return false;
        out->host[hi++] = *p++;
    }
    out->host[hi] = '\0';

    if (*p == ':') {
        p++;
        uint16_t port = 0;
        while (*p >= '0' && *p <= '9') {
            port = (uint16_t)(port * 10 + (*p - '0'));
            p++;
        }
        out->port = port;
    }

    if (*p == '/') {
        kstrcpy(out->path, p);
    } else {
        out->path[0] = '/';
        out->path[1] = '\0';
    }

    return true;
}

// ---------------------------------------------------------------------------
// Parse a dotted-quad literal ("10.0.2.2") into an ipv4_addr_t. Returns
// false if the string isn't a clean dotted-quad (e.g. it's a real hostname),
// so callers can fall back to DNS. Doesn't accept leading/trailing garbage —
// the whole string must be consumed.
// ---------------------------------------------------------------------------
static bool parse_ipv4_literal(const char *s, ipv4_addr_t *out) {
    uint8_t parts[4];
    for (int i = 0; i < 4; i++) {
        if (*s < '0' || *s > '9') return false;
        uint32_t v = 0;
        int digits = 0;
        while (*s >= '0' && *s <= '9') {
            v = v * 10 + (uint32_t)(*s - '0');
            s++;
            digits++;
            if (digits > 3 || v > 255) return false;
        }
        parts[i] = (uint8_t)v;
        if (i < 3) {
            if (*s != '.') return false;
            s++;
        }
    }
    if (*s != '\0') return false; // trailing junk -> not a plain IP literal
    *out = ipv4_make(parts[0], parts[1], parts[2], parts[3]);
    return true;
}

// ---------------------------------------------------------------------------
// Build HTTP/1.0 GET request
// ---------------------------------------------------------------------------
static int build_request(const http_url_t *u, char *buf, int max) {
    // HTTP/1.0: server closes after response — no chunked encoding to deal with.
    (void)max;
    char *p = buf;
    // "GET <path> HTTP/1.0\r\n"
    kstrcpy(p, "GET "); p += 4;
    kstrcpy(p, u->path); p += kstrlen(u->path);
    kstrcpy(p, " HTTP/1.0\r\nHost: "); p += 17;
    kstrcpy(p, u->host); p += kstrlen(u->host);
    kstrcpy(p, "\r\nUser-Agent: Eclipse32/0.1\r\nConnection: close\r\n\r\n");
    p += 50;
    return (int)(p - buf);
}

// ---------------------------------------------------------------------------
// Skip HTTP response headers, return pointer to body start.
// Also parses Content-Length if present (sets *content_len, 0 = unknown).
// ---------------------------------------------------------------------------
static const uint8_t *skip_headers(const uint8_t *data, uint32_t len,
                                    uint32_t *body_offset, uint32_t *content_len,
                                    int *status_code) {
    *content_len = 0;
    *status_code = 0;

    // Find \r\n\r\n
    for (uint32_t i = 0; i + 3 < len; i++) {
        if (data[i]=='\r' && data[i+1]=='\n' && data[i+2]=='\r' && data[i+3]=='\n') {
            *body_offset = i + 4;

            // Parse status line: "HTTP/1.x NNN ..."
            if (len > 9 && data[0]=='H' && data[1]=='T' && data[2]=='T' && data[3]=='P') {
                int j = 0;
                while (j < (int)len && data[j] != ' ') j++;
                j++;
                if (j + 3 <= (int)len) {
                    *status_code = (data[j]-'0')*100 + (data[j+1]-'0')*10 + (data[j+2]-'0');
                }
            }

            // Scan headers for Content-Length
            uint32_t hstart = 0;
            while (hstart < i) {
                // Find line end
                uint32_t hend = hstart;
                while (hend < i && !(data[hend]=='\r' && data[hend+1]=='\n')) hend++;
                // Check for Content-Length:
                const char *cl = "Content-Length:";
                int cl_len = 15;
                if ((int)(hend - hstart) > cl_len) {
                    bool match = true;
                    for (int k = 0; k < cl_len; k++) {
                        char a = (char)data[hstart + k];
                        char b = cl[k];
                        // case-insensitive
                        if (a >= 'A' && a <= 'Z') a += 32;
                        if (b >= 'A' && b <= 'Z') b += 32;
                        if (a != b) { match = false; break; }
                    }
                    if (match) {
                        uint32_t v = 0;
                        uint32_t vp = hstart + (uint32_t)cl_len;
                        while (vp < hend && data[vp] == ' ') vp++;
                        while (vp < hend && data[vp] >= '0' && data[vp] <= '9') {
                            v = v * 10 + (data[vp] - '0');
                            vp++;
                        }
                        *content_len = v;
                    }
                }
                hstart = hend + 2; // skip \r\n
            }

            return data + *body_offset;
        }
    }
    *body_offset = len; // no headers found
    return data + len;
}

// ---------------------------------------------------------------------------
// Core: resolve, connect, send GET, read response
// ---------------------------------------------------------------------------
static bool http_do_get(const char *url, http_url_t *u,
                         uint8_t **out_buf, uint32_t *out_len,
                         int dest_fd, bool show_progress) {
    if (!parse_url(url, u)) {
        syscall_debug_puts("[HTTP] Bad URL (must be http://)\n");
        return false;
    }

    // DNS resolve — unless the host is already a literal IP (e.g.
    // "10.0.2.2"), in which case there's nothing to resolve and querying
    // DNS for it would be both pointless and another opportunity to fail.
    ipv4_addr_t server_ip;
    if (parse_ipv4_literal(u->host, &server_ip)) {
        dbg_printf("[HTTP] %s is a literal IP, skipping DNS\n", u->host);
    } else {
        dbg_printf("[HTTP] Resolving %s...\n", u->host);
        if (!dns_resolve_blocking(u->host, &server_ip, 5000)) {
            syscall_debug_puts("[HTTP] DNS failed\n");
            return false;
        }
        dbg_printf("[HTTP] %s -> %d.%d.%d.%d\n", u->host,
                   server_ip.b[0], server_ip.b[1], server_ip.b[2], server_ip.b[3]);
    }

    // TCP connect
    dbg_printf("[HTTP] Connecting to port %u...\n", (uint32_t)u->port);
    if (!tcp_connect(server_ip, u->port)) {
        syscall_debug_puts("[HTTP] TCP connect failed\n");
        return false;
    }
    syscall_debug_puts("[HTTP] Connected\n");

    // Send GET request
    char req[900];
    int req_len = build_request(u, req, (int)sizeof(req));
    if (tcp_send((const uint8_t *)req, (uint16_t)req_len) < req_len) {
        syscall_debug_puts("[HTTP] Send failed\n");
        tcp_close();
        return false;
    }

    // Read full response into a temp buffer.
    // Max download: 2MB. This used to be 4MB — the *entire* kernel heap
    // (see heap_init() in kmain.c, which was 4MB total with no heap growth
    // implemented). Requesting the whole heap in one kmalloc() call left no
    // room for anything else already living in it (GUI desktop, FAT32,
    // scheduler, shell buffers, ...), so this allocation was failing
    // essentially every time — silently, since there was no debug message
    // on this branch — which is what made every get/getpkg look like an
    // instant, untraceable failure right after a perfectly successful TCP
    // connect. The heap is now 16MB, so 2MB here is a comfortable fraction
    // rather than the whole pool.
    #define HTTP_MAX_RESP (2 * 1024 * 1024)
    uint8_t *resp = (uint8_t *)kmalloc(HTTP_MAX_RESP);
    if (!resp) {
        dbg_printf("[HTTP] Out of memory (couldn't allocate %u byte response buffer)\n",
                   (uint32_t)HTTP_MAX_RESP);
        tcp_close();
        return false;
    }

    syscall_debug_puts("[HTTP] Downloading...\n");
    int resp_len = tcp_recv_all(resp, HTTP_MAX_RESP);
    tcp_close();

    if (resp_len <= 0) {
        syscall_debug_puts("[HTTP] No response\n");
        kfree(resp);
        return false;
    }

    // Parse headers
    uint32_t body_offset = 0, content_len = 0;
    int status = 0;
    skip_headers(resp, (uint32_t)resp_len, &body_offset, &content_len, &status);

    if (status != 200) {
        dbg_printf("[HTTP] Server returned %d\n", status);
        kfree(resp);
        return false;
    }

    uint8_t *body = resp + body_offset;
    uint32_t body_len = (uint32_t)resp_len - body_offset;

    if (show_progress) {
        dbg_printf("[HTTP] %u bytes\n", body_len);
    }

    if (dest_fd >= 0) {
        // Write directly to file
        uint32_t written = 0;
        uint32_t chunk = 4096;
        while (written < body_len) {
            uint32_t w = body_len - written;
            if (w > chunk) w = chunk;
            if (fat32_write(dest_fd, body + written, w) < 0) {
                syscall_debug_puts("[HTTP] Write error\n");
                kfree(resp);
                return false;
            }
            written += w;
            if (show_progress && content_len > 0) {
                dbg_printf("\r[%u%%]", written * 100 / content_len);
            }
        }
        if (show_progress) syscall_debug_puts("\n");
        *out_len = body_len;
        kfree(resp);
        return true;
    } else {
        // Return heap buffer
        uint8_t *result = (uint8_t *)kmalloc(body_len + 1);
        if (!result) { kfree(resp); return false; }
        for (uint32_t i = 0; i < body_len; i++) result[i] = body[i];
        result[body_len] = 0;
        *out_buf = result;
        *out_len = body_len;
        kfree(resp);
        return true;
    }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

uint8_t *http_get(const char *url, uint32_t *out_len) {
    http_url_t u;
    uint8_t *buf = NULL;
    uint32_t len = 0;
    if (!http_do_get(url, &u, &buf, &len, -1, false)) return NULL;
    if (out_len) *out_len = len;
    return buf;
}

int http_get_to_file(const char *url, const char *dest_path, bool show_progress) {
    // Open dest file (create/truncate)
    int fd = fat32_open(dest_path, FAT32_O_RDWR | FAT32_O_CREAT | FAT32_O_TRUNC);
    if (fd < 0) {
        dbg_printf("[HTTP] Cannot create file: %s\n", dest_path);
        return -1;
    }

    http_url_t u;
    uint32_t len = 0;
    bool ok = http_do_get(url, &u, NULL, &len, fd, show_progress);
    fat32_close(fd);

    if (!ok) {
        fat32_delete(dest_path);
        return -1;
    }
    return 0;
}
