// =============================================================================
// Eclipse32 - InitRAMFS & Kernel String Library Header
// =============================================================================
#pragma once
#include "../kernel.h"

// ---- InitRAMFS ----
void        initramfs_start(void);
void        env_set(const char *key, const char *value);
const char *env_get(const char *key);
void        env_unset(const char *key);

// ---- Kernel string/memory library ----
int    kstrcmp(const char *a, const char *b);
int    kstrncmp(const char *a, const char *b, size_t n);
char  *kstrcpy(char *dst, const char *src);
char  *kstrncpy(char *dst, const char *src, size_t n);
size_t kstrlen(const char *s);
char  *kstrcat(char *dst, const char *src);
char  *kstrchr(const char *s, int c);
char  *kstrrchr(const char *s, int c);
void  *kmemset(void *s, int c, size_t n);
void  *kmemcpy(void *dst, const void *src, size_t n);
int    kmemcmp(const void *a, const void *b, size_t n);
int    katoi(const char *s);
