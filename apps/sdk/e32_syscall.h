#pragma once

#include <stddef.h>
#include <stdint.h>

#define SYS_write      1
#define SYS_read       2
#define SYS_open       3
#define SYS_close      4
#define SYS_seek       5
#define SYS_fstat      6
#define SYS_readfile   7
#define SYS_exit       8
#define SYS_mkdir      9
#define SYS_unlink     10
#define SYS_rename     11
#define SYS_readdir    12
#define SYS_gettime_ms 13
#define SYS_sleep_ms   14
#define SYS_brk        15
#define SYS_getpid     16
#define SYS_isatty     17
#define SYS_getcwd     18
#define SYS_chdir      19

#define E32_ENOSYS     (-38)

typedef struct {
    uint32_t size;
    uint32_t is_dir;
} e32_stat_t;

typedef struct {
    char     name[256];
    uint32_t size;
    uint32_t cluster;
    uint32_t is_dir;
    uint32_t is_hidden;
} e32_dirent_t;

static inline int e32_syscall5(int num, int a1, int a2, int a3, int a4, int a5) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(num), "b"(a1), "c"(a2), "d"(a3), "S"(a4), "D"(a5)
        : "memory"
    );
    return ret;
}

static inline int e32_write(int fd, const char *buf, int len) {
    return e32_syscall5(SYS_write, fd, (int)buf, len, 0, 0);
}

static inline int e32_read(int fd, void *buf, int len) {
    return e32_syscall5(SYS_read, fd, (int)buf, len, 0, 0);
}

static inline int e32_open(const char *path, int flags) {
    return e32_syscall5(SYS_open, (int)path, flags, 0, 0, 0);
}

static inline int e32_close(int fd) {
    return e32_syscall5(SYS_close, fd, 0, 0, 0, 0);
}

static inline int e32_seek(int fd, int offset, int whence) {
    return e32_syscall5(SYS_seek, fd, offset, whence, 0, 0);
}

static inline int e32_fstat(const char *path, e32_stat_t *st) {
    return e32_syscall5(SYS_fstat, (int)path, (int)st, 0, 0, 0);
}

static inline int e32_readfile(const char *path, void *buf, int len) {
    return e32_syscall5(SYS_readfile, (int)path, (int)buf, len, 0, 0);
}

static inline int e32_exit(int code) {
    return e32_syscall5(SYS_exit, code, 0, 0, 0, 0);
}

static inline int e32_mkdir(const char *path) {
    return e32_syscall5(SYS_mkdir, (int)path, 0, 0, 0, 0);
}

static inline int e32_unlink(const char *path) {
    return e32_syscall5(SYS_unlink, (int)path, 0, 0, 0, 0);
}

static inline int e32_rename(const char *oldpath, const char *newpath) {
    return e32_syscall5(SYS_rename, (int)oldpath, (int)newpath, 0, 0, 0);
}

static inline int e32_readdir(const char *path, e32_dirent_t *entries, int max_entries) {
    return e32_syscall5(SYS_readdir, (int)path, (int)entries, max_entries, 0, 0);
}

static inline int e32_gettime_ms(void) {
    return e32_syscall5(SYS_gettime_ms, 0, 0, 0, 0, 0);
}

static inline int e32_sleep_ms(unsigned ms) {
    return e32_syscall5(SYS_sleep_ms, (int)ms, 0, 0, 0, 0);
}

static inline int e32_brk(void *addr) {
    return e32_syscall5(SYS_brk, (int)addr, 0, 0, 0, 0);
}

static inline int e32_getpid(void) {
    return e32_syscall5(SYS_getpid, 0, 0, 0, 0, 0);
}

static inline int e32_isatty(int fd) {
    return e32_syscall5(SYS_isatty, fd, 0, 0, 0, 0);
}

static inline int e32_getcwd(char *buf, size_t size) {
    return e32_syscall5(SYS_getcwd, (int)buf, (int)size, 0, 0, 0);
}

static inline int e32_chdir(const char *path) {
    return e32_syscall5(SYS_chdir, (int)path, 0, 0, 0, 0);
}
