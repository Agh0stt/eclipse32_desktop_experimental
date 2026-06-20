#include "stdio.h"

#include <stdbool.h>
#include <stdint.h>

#include "string.h"
#include "unistd.h"

int putchar(int c) {
    char ch = (char)c;
    return write(1, &ch, 1);
}

int getchar(void) {
    char ch = 0;
    int n = read(0, &ch, 1);
    if (n <= 0) return -1;
    return (unsigned char)ch;
}

int puts(const char *s) {
    int n = (int)write(1, s, strlen(s));
    write(1, "\n", 1);
    return n + 1;
}

int fputc(int c, int fd) {
    char ch = (char)c;
    return write(fd, &ch, 1) == 1 ? (unsigned char)ch : -1;
}

int fgetc(int fd) {
    char ch = 0;
    int n = read(fd, &ch, 1);
    if (n <= 0) return -1;
    return (unsigned char)ch;
}

int fputs(const char *s, int fd) {
    return (int)write(fd, s, strlen(s));
}

typedef struct fmt_sink {
    int mode; /* 0 = fd, 1 = buffer */
    int fd;
    char *buf;
    size_t size;
    size_t pos;
} fmt_sink_t;

static void sink_putc(fmt_sink_t *sink, char c) {
    if (sink->mode == 0) {
        write(sink->fd, &c, 1);
        sink->pos++;
        return;
    }
    if (sink->size > 0 && sink->pos + 1 < sink->size) sink->buf[sink->pos] = c;
    sink->pos++;
}

static void sink_puts(fmt_sink_t *sink, const char *s) {
    while (*s) sink_putc(sink, *s++);
}

static void sink_print_uint(fmt_sink_t *sink, unsigned int value, unsigned int base, bool upper) {
    char tmp[32];
    int len = 0;
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    if (value == 0) tmp[len++] = '0';
    while (value > 0 && len < (int)sizeof(tmp)) {
        tmp[len++] = digits[value % base];
        value /= base;
    }
    for (int i = len - 1; i >= 0; i--) sink_putc(sink, tmp[i]);
}

static int sink_vprintf(fmt_sink_t *sink, const char *fmt, va_list ap) {
    while (*fmt) {
        if (*fmt != '%') {
            sink_putc(sink, *fmt++);
            continue;
        }
        fmt++;
        switch (*fmt) {
            case '%':
                sink_putc(sink, '%');
                break;
            case 'c': {
                char c = (char)va_arg(ap, int);
                sink_putc(sink, c);
                break;
            }
            case 's': {
                const char *s = va_arg(ap, const char *);
                if (!s) s = "(null)";
                sink_puts(sink, s);
                break;
            }
            case 'd':
            case 'i': {
                int n = va_arg(ap, int);
                if (n < 0) {
                    sink_putc(sink, '-');
                    sink_print_uint(sink, (unsigned int)(-n), 10, false);
                } else {
                    sink_print_uint(sink, (unsigned int)n, 10, false);
                }
                break;
            }
            case 'u':
                sink_print_uint(sink, va_arg(ap, unsigned int), 10, false);
                break;
            case 'x':
                sink_print_uint(sink, va_arg(ap, unsigned int), 16, false);
                break;
            case 'X':
                sink_print_uint(sink, va_arg(ap, unsigned int), 16, true);
                break;
            case 'p': {
                void *v = va_arg(ap, void *);
                sink_puts(sink, "0x");
                sink_print_uint(sink, (unsigned int)(uintptr_t)v, 16, false);
                break;
            }
            default:
                sink_putc(sink, '%');
                sink_putc(sink, *fmt);
                break;
        }
        fmt++;
    }
    return (int)sink->pos;
}

int vprintf(const char *fmt, va_list ap) {
    fmt_sink_t sink = {0, 1, NULL, 0, 0};
    return sink_vprintf(&sink, fmt, ap);
}

int printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = vprintf(fmt, ap);
    va_end(ap);
    return n;
}

int fprintf(int fd, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fmt_sink_t sink = {0, fd, NULL, 0, 0};
    int n = sink_vprintf(&sink, fmt, ap);
    va_end(ap);
    return n;
}

int vsnprintf(char *buf, size_t size, const char *fmt, va_list ap) {
    fmt_sink_t sink = {1, 0, buf, size, 0};
    int n = sink_vprintf(&sink, fmt, ap);
    if (size > 0) {
        size_t end = sink.pos < (size - 1) ? sink.pos : (size - 1);
        sink.buf[end] = '\0';
    }
    return n;
}

int snprintf(char *buf, size_t size, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, size, fmt, ap);
    va_end(ap);
    return n;
}

/* -------------------------------------------------------------------------
 * gets / fgets  — backed by the kernel's readline-capable read(0,...) syscall
 * ------------------------------------------------------------------------- */

char *gets(char *buf) {
    /* The kernel's sys_read for fd=0 now does full readline with echo and
     * backspace support.  It appends '\n' before returning. */
    int n = read(0, buf, 4095);
    if (n <= 0) { buf[0] = '\0'; return NULL; }
    buf[n] = '\0';
    /* strip trailing newline so behaviour matches standard gets() */
    if (n > 0 && buf[n-1] == '\n') buf[--n] = '\0';
    if (n > 0 && buf[n-1] == '\r') buf[--n] = '\0';
    return buf;
}

char *fgets(char *buf, int size, int fd) {
    if (size <= 0) return NULL;
    int n = read(fd, buf, (size_t)(size - 1));
    if (n <= 0) { buf[0] = '\0'; return NULL; }
    buf[n] = '\0';
    return buf;
}

/* -------------------------------------------------------------------------
 * vsscanf — parse a string according to a format
 * Supports: %d %i %u %x %X %s %c %% and optional * (suppress assignment)
 * ------------------------------------------------------------------------- */

static int sc_isspace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

int vsscanf(const char *str, const char *fmt, va_list ap) {
    const char *s = str;
    int count = 0;

    while (*fmt) {
        if (sc_isspace(*fmt)) {
            while (sc_isspace(*s)) s++;
            fmt++;
            continue;
        }

        if (*fmt != '%') {
            if (*s == *fmt) { s++; fmt++; }
            else break;
            continue;
        }

        fmt++;

        int suppress = 0;
        if (*fmt == '*') { suppress = 1; fmt++; }

        int width = 0;
        while (*fmt >= '0' && *fmt <= '9') { width = width * 10 + (*fmt - '0'); fmt++; }

        char spec = *fmt++;

        if (spec == '%') {
            while (sc_isspace(*s)) s++;
            if (*s == '%') s++;
            else break;
            continue;
        }

        if (spec != 'c') { while (sc_isspace(*s)) s++; }
        if (!*s) break;

        if (spec == 'c') {
            int w = (width == 0) ? 1 : width;
            if (suppress) {
                for (int i = 0; i < w && *s; i++) s++;
            } else {
                char *dest = va_arg(ap, char *);
                for (int i = 0; i < w && *s; i++) *dest++ = *s++;
                count++;
            }
        } else if (spec == 's') {
            if (suppress) {
                int w = 0;
                while (*s && !sc_isspace(*s) && (width == 0 || w < width)) { s++; w++; }
            } else {
                char *dest = va_arg(ap, char *);
                int w = 0;
                while (*s && !sc_isspace(*s) && (width == 0 || w < width)) { *dest++ = *s++; w++; }
                *dest = '\0';
                count++;
            }
        } else if (spec == 'd' || spec == 'i' || spec == 'u' || spec == 'x' || spec == 'X') {
            int neg = 0;
            unsigned int val = 0;
            int digits = 0;
            int base = 10;

            if (spec == 'd' || spec == 'i') {
                if (*s == '-') { neg = 1; s++; }
                else if (*s == '+') s++;
            }
            if (spec == 'x' || spec == 'X') base = 16;
            if (spec == 'i') {
                if (*s == '0') {
                    s++;
                    if (*s == 'x' || *s == 'X') { base = 16; s++; }
                    else base = 8;
                } else base = 10;
            }

            int w = 0;
            while (*s && (width == 0 || w < width)) {
                char c = *s;
                int d;
                if (c >= '0' && c <= '9') d = c - '0';
                else if (c >= 'a' && c <= 'f') d = 10 + (c - 'a');
                else if (c >= 'A' && c <= 'F') d = 10 + (c - 'A');
                else break;
                if (d >= base) break;
                val = val * (unsigned int)base + (unsigned int)d;
                s++; w++; digits++;
            }

            if (digits == 0) break;

            if (!suppress) {
                if (spec == 'u' || spec == 'x' || spec == 'X')
                    *va_arg(ap, unsigned int *) = val;
                else
                    *va_arg(ap, int *) = neg ? -(int)val : (int)val;
                count++;
            }
        } else {
            break;
        }
    }

    return count;
}

int sscanf(const char *str, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = vsscanf(str, fmt, ap);
    va_end(ap);
    return n;
}

int scanf(const char *fmt, ...) {
    char line[512];
    int nr = read(0, line, sizeof(line) - 1);
    if (nr <= 0) return -1;
    line[nr] = '\0';
    va_list ap;
    va_start(ap, fmt);
    int n = vsscanf(line, fmt, ap);
    va_end(ap);
    return n;
}
