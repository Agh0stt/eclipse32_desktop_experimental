/*
 * meow — a "cat" clone under a different name.
 *
 * Deployed to /bin (see Makefile: injectsh MEOW.E32 -> meow), so it's
 * launched through esh's normal /bin command lookup like any other
 * shell command.
 *
 * File arguments are resolved from root ("/"): an absolute path
 * (starting with '/') is used as-is; a bare name like "hi.txt" is
 * looked up as "/hi.txt", not relative to wherever esh's cwd happens
 * to be.
 *
 * Usage: meow <file> [file2 ...]
 *        meow            (reads stdin until an empty line, like esh's
 *                         builtin cat with no args)
 */
#include <stdio.h>
#include "../sdk/e32_syscall.h"
#include "../sdk/libc/unistd.h"

static void resolve_path(const char *in, char *out, int outsz) {
    if (in[0] == '/') {
        int i = 0;
        while (in[i] && i < outsz - 1) { out[i] = in[i]; i++; }
        out[i] = 0;
        return;
    }
    /* bare name -> always look from root, regardless of esh's cwd */
    int i = 0;
    out[i++] = '/';
    int j = 0;
    while (in[j] && i < outsz - 1) { out[i++] = in[j++]; }
    out[i] = 0;
}

static int cat_file(const char *arg) {
    char path[256];
    resolve_path(arg, path, sizeof(path));

    int fd = e32_open(path, 0 /* O_RDONLY */);
    if (fd < 0) {
        printf("meow: %s: No such file\n", arg);
        return 1;
    }
    char buf[512];
    int n;
    while ((n = e32_read(fd, buf, sizeof(buf))) > 0) {
        write(1, buf, n);
    }
    e32_close(fd);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        char line[256];
        for (;;) {
            char *g = gets(line);
            if (!g || !line[0]) break;
            printf("%s\n", line);
        }
        return 0;
    }

    int rc = 0;
    for (int a = 1; a < argc; a++) {
        int r = cat_file(argv[a]);
        if (r) rc = r;
    }
    return rc;
}
