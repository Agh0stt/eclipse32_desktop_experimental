#include "../e32_syscall.h"
#include "errno.h"
#include "unistd.h"

int write(int fd, const void *buf, size_t count) {
    int r = e32_syscall5(SYS_write, fd, (int)buf, (int)count, 0, 0);
    if (r < 0) errno = EIO;
    return r;
}

int read(int fd, void *buf, size_t count) {
    int r = e32_syscall5(SYS_read, fd, (int)buf, (int)count, 0, 0);
    if (r < 0) errno = EIO;
    return r;
}

void _exit(int code) {
    e32_syscall5(SYS_exit, code, 0, 0, 0, 0);
    for (;;) { }
}

int brk(void *addr) {
    int r = e32_brk(addr);
    if (r < 0) {
        errno = ENOMEM;
        return -1;
    }
    return 0;
}

void *sbrk(intptr_t increment) {
    if (increment == 0) {
        int cur = e32_brk(0);
        if (cur < 0) {
            errno = ENOMEM;
            return (void *)-1;
        }
        return (void *)(intptr_t)cur;
    }

    int old = e32_brk(0);
    if (old < 0) {
        errno = ENOMEM;
        return (void *)-1;
    }

    intptr_t new_top = (intptr_t)old + increment;
    if (increment > 0 && new_top < (intptr_t)old) {
        errno = ENOMEM;
        return (void *)-1;
    }
    if (increment < 0 && new_top > (intptr_t)old) {
        errno = EINVAL;
        return (void *)-1;
    }

    int r = e32_brk((void *)new_top);
    if (r < 0) {
        errno = (increment > 0) ? ENOMEM : EINVAL;
        return (void *)-1;
    }
    return (void *)(intptr_t)old;
}
