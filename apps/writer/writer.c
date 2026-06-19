/*
 * Eclipse32 Writer — simple line-oriented text editor (userspace .E32)
 * Usage: WRITER [path]
 * Default path: /apps/NOTE.TXT
 */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "e32_syscall.h"

#define O_RDONLY 0x00
#define O_RDWR   0x02
#define O_CREAT  0x40
#define O_TRUNC  0x200

#define MAX_LINES 96
#define LINE_LEN  128

static char g_lines[MAX_LINES][LINE_LEN];
static int  g_count;

static void line_copy(char *dst, const char *src) {
    int i;
    for (i = 0; i < LINE_LEN - 1 && src[i]; i++) dst[i] = src[i];
    dst[i] = 0;
}

static int load_file(const char *path) {
    int fd = e32_open(path, O_RDONLY);
    if (fd < 0) return -1;
    char buf[4096];
    int total = 0;
    for (;;) {
        int n = e32_read(fd, buf + total, (int)(sizeof(buf) - 1 - (size_t)total));
        if (n <= 0) break;
        total += n;
        if (total >= (int)sizeof(buf) - 1) break;
    }
    e32_close(fd);
    buf[total] = 0;

    g_count = 0;
    const char *p = buf;
    while (*p && g_count < MAX_LINES) {
        const char *e = p;
        while (*e && *e != '\n' && *e != '\r') e++;
        int len = (int)(e - p);
        if (len >= LINE_LEN) len = LINE_LEN - 1;
        memcpy(g_lines[g_count], p, (size_t)len);
        g_lines[g_count][len] = 0;
        g_count++;
        if (*e == '\r') e++;
        if (*e == '\n') e++;
        if (!*e) break;
        p = e;
    }
    return 0;
}

static int save_file(const char *path) {
    int fd = e32_open(path, O_RDWR | O_CREAT | O_TRUNC);
    if (fd < 0) return -1;
    for (int i = 0; i < g_count; i++) {
        size_t L = strlen(g_lines[i]);
        e32_write(fd, g_lines[i], (int)L);
        e32_write(fd, "\n", 1);
    }
    e32_close(fd);
    return 0;
}

static void list_all(void) {
    for (int i = 0; i < g_count; i++) printf("%3d | %s\n", i + 1, g_lines[i]);
    if (g_count == 0) printf("  (empty)\n");
}

static int parse_int(const char *s) {
    int n = 0;
    bool neg = false;
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '-') {
        neg = true;
        s++;
    }
    while (*s >= '0' && *s <= '9') {
        n = n * 10 + (*s - '0');
        s++;
    }
    return neg ? -n : n;
}

int main(int argc, char **argv) {
    char path[128];
    {
        const char *def = "/apps/NOTE.TXT";
        int i = 0;
        while (def[i]) {
            path[i] = def[i];
            i++;
        }
        path[i] = 0;
    }
    if (argc >= 2) {
        line_copy(path, argv[1]);
    }
    if (load_file(path) < 0) {
        g_count = 0;
    }

    printf("\n--- Eclipse32 Writer ---\n");
    printf("File: %s\n", path);
    printf("Commands:  l=list  a <text>  d <n>  r <n> <text>  s=save  q=quit  h=help\n\n");

    for (;;) {
        printf(">> ");
        char cmd[320];
        if (!fgets(cmd, (int)sizeof(cmd), 0)) break;
        char *nl = cmd;
        while (*nl) {
            if (*nl == '\n' || *nl == '\r') {
                *nl = 0;
                break;
            }
            nl++;
        }
        if (cmd[0] == 0) continue;
        if (cmd[0] == 'q' && cmd[1] == 0) break;
        if (cmd[0] == 'h' && cmd[1] == 0) {
            printf("l list | a line text | d num | r num text | s save | q quit\n");
            continue;
        }
        if (cmd[0] == 'l' && cmd[1] == 0) {
            list_all();
            continue;
        }
        if (cmd[0] == 's' && cmd[1] == 0) {
            if (save_file(path) == 0) printf("(saved)\n");
            else printf("(save failed)\n");
            continue;
        }
        if (cmd[0] == 'a' && cmd[1] == ' ') {
            if (g_count >= MAX_LINES) {
                printf("(buffer full)\n");
                continue;
            }
            line_copy(g_lines[g_count], cmd + 2);
            g_count++;
            continue;
        }
        if (cmd[0] == 'd') {
            int n = parse_int(cmd + 1);
            if (n < 1 || n > g_count) {
                printf("(bad line)\n");
                continue;
            }
            for (int k = n - 1; k < g_count - 1; k++) line_copy(g_lines[k], g_lines[k + 1]);
            g_count--;
            continue;
        }
        if (cmd[0] == 'r' && (cmd[1] == ' ' || cmd[1] == '\t')) {
            const char *p = cmd + 2;
            while (*p == ' ' || *p == '\t') p++;
            int n = parse_int(p);
            while (*p && *p != ' ' && *p != '\t') p++;
            while (*p == ' ' || *p == '\t') p++;
            if (n < 1 || n > g_count) {
                printf("(bad line)\n");
                continue;
            }
            line_copy(g_lines[n - 1], p);
            continue;
        }
        printf("(unknown — type h)\n");
    }

    printf("Bye.\n");
    return 0;
}
