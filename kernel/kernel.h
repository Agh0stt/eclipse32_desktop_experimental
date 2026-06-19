// =============================================================================
// Eclipse32 - kernel.h
// Core kernel types, macros, and declarations
// =============================================================================
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// ---------------------------------------------------------------------------
// Compiler attributes
// ---------------------------------------------------------------------------
#define PACKED          __attribute__((packed))
#define ALIGNED(n)      __attribute__((aligned(n)))
#define NORETURN        __attribute__((noreturn))
#define UNUSED          __attribute__((unused))
#define INLINE          __attribute__((always_inline)) inline
#define NOINLINE        __attribute__((noinline))
#define SECTION(s)      __attribute__((section(s)))
#define WEAK            __attribute__((weak))

// ---------------------------------------------------------------------------
// Useful macros
// ---------------------------------------------------------------------------
#define KERNEL_BASE         0x00100000UL    // 1MB
#define PAGE_SIZE           4096
#define PAGE_MASK           (~(PAGE_SIZE - 1))
#define PAGE_ALIGN_UP(x)    (((x) + PAGE_SIZE - 1) & PAGE_MASK)
#define PAGE_ALIGN_DOWN(x)  ((x) & PAGE_MASK)

#define KB(x)   ((x) * 1024UL)
#define MB(x)   ((x) * 1024UL * 1024UL)
#define GB(x)   ((x) * 1024UL * 1024UL * 1024UL)

#define MIN(a, b)   ((a) < (b) ? (a) : (b))
#define MAX(a, b)   ((a) > (b) ? (a) : (b))
#define CLAMP(v,lo,hi) (MIN(MAX(v,lo),hi))

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#define OFFSET_OF(type, member) __builtin_offsetof(type, member)
#define CONTAINER_OF(ptr, type, member) \
    ((type *)((uint8_t *)(ptr) - OFFSET_OF(type, member)))

#define BIT(n)          (1UL << (n))
#define BITMASK(n)      (BIT(n) - 1)
#define SET_BIT(v, n)   ((v) |= BIT(n))
#define CLR_BIT(v, n)   ((v) &= ~BIT(n))
#define TST_BIT(v, n)   (((v) >> (n)) & 1)

// ---------------------------------------------------------------------------
// Port I/O
// ---------------------------------------------------------------------------
static INLINE void outb(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}
static INLINE void outw(uint16_t port, uint16_t val) {
    asm volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}
static INLINE void outl(uint16_t port, uint32_t val) {
    asm volatile("outl %0, %1" : : "a"(val), "Nd"(port));
}
static INLINE uint8_t inb(uint16_t port) {
    uint8_t val;
    asm volatile("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}
static INLINE uint16_t inw(uint16_t port) {
    uint16_t val;
    asm volatile("inw %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}
static INLINE uint32_t inl(uint16_t port) {
    uint32_t val;
    asm volatile("inl %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}
static INLINE void io_wait(void) {
    outb(0x80, 0);
}

// ---------------------------------------------------------------------------
// CPU control
// ---------------------------------------------------------------------------
static INLINE void cli(void) { asm volatile("cli"); }
static INLINE void sti(void) { asm volatile("sti"); }
static INLINE void hlt(void) { asm volatile("hlt"); }
static INLINE void nop(void) { asm volatile("nop"); }

static INLINE uint32_t read_cr0(void) {
    uint32_t v; asm volatile("mov %%cr0, %0" : "=r"(v)); return v;
}
static INLINE uint32_t read_cr2(void) {
    uint32_t v; asm volatile("mov %%cr2, %0" : "=r"(v)); return v;
}
static INLINE uint32_t read_cr3(void) {
    uint32_t v; asm volatile("mov %%cr3, %0" : "=r"(v)); return v;
}
static INLINE void write_cr3(uint32_t v) {
    asm volatile("mov %0, %%cr3" : : "r"(v) : "memory");
}
static INLINE void flush_tlb(void) {
    write_cr3(read_cr3());
}
static INLINE void flush_tlb_page(uint32_t addr) {
    asm volatile("invlpg (%0)" : : "r"(addr) : "memory");
}

// ---------------------------------------------------------------------------
// Interrupt flag save/restore
// ---------------------------------------------------------------------------
static INLINE uint32_t irq_save(void) {
    uint32_t flags;
    asm volatile("pushf; pop %0; cli" : "=r"(flags));
    return flags;
}
static INLINE void irq_restore(uint32_t flags) {
    asm volatile("push %0; popf" : : "r"(flags) : "memory");
}

// ---------------------------------------------------------------------------
// Assert
// ---------------------------------------------------------------------------
#define KASSERT(expr) \
    do { \
        if (!(expr)) { \
            kpanic("Assertion failed: " #expr " at " __FILE__ ":" \
                   STRINGIFY(__LINE__)); \
        } \
    } while(0)
#define STRINGIFY(x) STRINGIFY2(x)
#define STRINGIFY2(x) #x

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------
void kpanic(const char *msg) NORETURN;
void vga_printf(const char *fmt, ...);
void vga_puts(const char *s);

// ---------------------------------------------------------------------------
// Legacy compatibility types (used by ported GUI)
// ---------------------------------------------------------------------------
typedef struct {
    uint32_t addr;
    uint16_t width;
    uint16_t height;
    uint16_t pitch;
    uint8_t  bpp;
    uint8_t  active;
} Framebuffer;

typedef struct {
    int32_t  x, y;
    uint8_t  buttons;   // bit0=left, bit1=right
    int32_t  rel_x, rel_y;
} MouseState;

typedef struct {
    uint32_t mem_total;
    uint32_t mem_free;
    uint32_t mmap_addr;
    uint32_t disk_sectors;
    uint8_t  boot_drive;
    uint8_t  fb_active;
    uint8_t  initramfs_loaded;
    uint32_t initramfs_size;
    uint32_t uptime_ticks;
    char     hostname[64];
    char     version[32];
} KernelState;

extern KernelState kstate;
