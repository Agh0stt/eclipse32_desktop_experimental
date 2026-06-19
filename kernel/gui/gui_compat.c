#pragma once
#include "../../shell/shell.h"
#include "../kernel.h"
#include "../initramfs/initramfs.h"
#include "../arch/x86/pit.h"

static inline void kutoa(uint32_t val, char *buf, int base) {
    static const char digits[] = "0123456789ABCDEF";
    char tmp[32]; int i = 0;
    if (!val) { buf[0]='0'; buf[1]=0; return; }
    while (val) { tmp[i++]=digits[val%(unsigned)base]; val/=(unsigned)base; }
    int j=0; while(i>0) buf[j++]=tmp[--i]; buf[j]=0;
}
static inline void kitoa(int32_t val, char *buf, int base) {
    if (val < 0) { *buf++ = '-'; val = -val; }
    kutoa((uint32_t)val, buf, base);
}
static inline char *kstrncat(char *dst, const char *src, size_t n) {
    char *d=dst; while(*d) d++;
    while(n-- && *src) *d++=*src++; *d=0; return dst;
}
static inline uint32_t get_ticks(void) { return pit_ticks(); }
#define g_tick (pit_ticks())
#define FM_MAX 32
#ifndef EFS_FILENAME_MAX
#define EFS_FILENAME_MAX 64
#endif
// Real fs_list: enumerate root directory via FAT32, return count of entries
#include "../fs/fat32/fat32.h"
#include "../exec/e32_loader.h"
static inline int fs_list(char names[][EFS_FILENAME_MAX], int max) {
    if (!fs_is_mounted()) return 0;
    fat32_dir_entry_t entries[64];
    int n = fat32_readdir("/", entries, 64);
    if (n < 0) n = 0;
    int out = 0;
    for (int i = 0; i < n && out < max; i++) {
        if (entries[i].is_dir) continue;      // skip subdirs
        if (entries[i].is_hidden) continue;   // skip hidden
        // copy name, truncate to EFS_FILENAME_MAX-1
        int j = 0;
        while (j < EFS_FILENAME_MAX - 1 && entries[i].name[j]) {
            names[out][j] = entries[i].name[j]; j++;
        }
        names[out][j] = 0;
        out++;
    }
    return out;
}
static inline uint32_t fs_size(const char *n) {
    if (!fs_is_mounted() || !n) return 0;
    fat32_stat_t st;
    if (fat32_stat(n, &st) < 0) return 0;
    return st.size;
}
static inline int32_t kernel_exec_os32(const char *f) {
    if (!f) return -1;
    return (int32_t)e32_exec_file(f);
}
static inline void sleep_ms(uint32_t ms) { pit_sleep_ms(ms); }



Framebuffer g_fb;
MouseState g_mouse;

static inline int ksprintf(char *buf, const char *fmt, ...) {
    __builtin_va_list ap; __builtin_va_start(ap, fmt);
    char *out=buf; char tmp[32];
    while(*fmt){
        if(*fmt!='%'){*out++=*fmt++;continue;}
        fmt++;
        switch(*fmt){
        case 'd':{int32_t n=__builtin_va_arg(ap,int32_t);if(n<0){*out++='-';n=-n;}kutoa((uint32_t)n,tmp,10);for(char*p=tmp;*p;)*out++=*p++;break;}
        case 'u':{kutoa(__builtin_va_arg(ap,uint32_t),tmp,10);for(char*p=tmp;*p;)*out++=*p++;break;}
        case 'x':{kutoa(__builtin_va_arg(ap,uint32_t),tmp,16);for(char*p=tmp;*p;)*out++=*p++;break;}
        case 's':{const char*p=__builtin_va_arg(ap,const char*);if(!p)p="(null)";while(*p)*out++=*p++;break;}
        case 'c':{*out++=(char)__builtin_va_arg(ap,int);break;}
        case '%':{*out++='%';break;}
        default:{*out++='%';*out++=*fmt;break;}
        }
        fmt++;
    }
    *out=0; __builtin_va_end(ap);
    return (int)(out-buf);
}
