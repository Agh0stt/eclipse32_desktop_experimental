#pragma once

#include <stdarg.h>
#include <stddef.h>

int putchar(int c);
int getchar(void);
int puts(const char *s);
int fputc(int c, int fd);
int fgetc(int fd);
int fputs(const char *s, int fd);
int printf(const char *fmt, ...);
int vprintf(const char *fmt, va_list ap);
int fprintf(int fd, const char *fmt, ...);
int vsnprintf(char *buf, size_t size, const char *fmt, va_list ap);
int snprintf(char *buf, size_t size, const char *fmt, ...);

/* Input */
char *gets(char *buf);
char *fgets(char *buf, int size, int fd);

/* Scanning */
int vsscanf(const char *str, const char *fmt, va_list ap);
int sscanf(const char *str, const char *fmt, ...);
int scanf(const char *fmt, ...);
