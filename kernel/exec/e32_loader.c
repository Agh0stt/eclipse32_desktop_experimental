#include "e32_loader.h"

#include "../drivers/vga/vga.h"
#include "../fs/fat32/fat32.h"
#include "../initramfs/initramfs.h"
#include "../syscall/syscall.h"

typedef int (*e32_entry_t)(int argc, char **argv);

#define E32_RUNTIME_MAX (256 * 1024)
static uint8_t g_e32_runtime[E32_RUNTIME_MAX];

static bool e32_validate(const e32_header_t *h, uint32_t file_size) {
    const uint32_t max_image = E32_RUNTIME_MAX;
    if (h->magic != E32_MAGIC) return false;
    if (h->version != 1) return false;
    if (h->header_size < sizeof(e32_header_t)) return false;
    if (h->code_offset < h->header_size) return false;
    if (h->code_size > h->image_size || h->data_size > h->image_size) return false;
    if (h->entry_offset >= h->image_size) return false;
    if (h->code_offset + h->code_size > file_size) return false;
    if (h->data_offset + h->data_size > file_size) return false;
    if (h->image_size == 0 || h->image_size > max_image) return false;
    if (h->bss_size > max_image) return false;
    if (h->image_size + h->bss_size > max_image) return false;
    return true;
}

int e32_exec_file_argv(const char *path, int argc, char **argv) {
    fat32_stat_t st;
    if (fat32_stat(path, &st) != 0 || st.is_dir) return -10;
    if (st.size < sizeof(e32_header_t)) return -11;

    int fd = fat32_open(path, FAT32_O_RDONLY);
    if (fd < 0) return -12;

    e32_header_t h;
    int n = fat32_read(fd, &h, sizeof(h));
    if (n != (int)sizeof(h)) {
        fat32_close(fd);
        return -13;
    }
    if (!e32_validate(&h, st.size)) {
        fat32_close(fd);
        return -14;
    }

    uint8_t *image = g_e32_runtime;
    kmemset(image, 0, h.image_size + h.bss_size);

    if (fat32_seek(fd, (int32_t)h.code_offset, FAT32_SEEK_SET) < 0 ||
        fat32_read(fd, image + h.code_offset, h.code_size) != (int)h.code_size) {
        fat32_close(fd);
        return -16;
    }

    if (h.data_size > 0) {
        if (fat32_seek(fd, (int32_t)h.data_offset, FAT32_SEEK_SET) < 0 ||
            fat32_read(fd, image + h.data_offset, h.data_size) != (int)h.data_size) {
            fat32_close(fd);
            return -17;
        }
    }
    fat32_close(fd);

    syscall_app_begin();
    syscall_set_app_image(image + h.code_offset, (h.image_size - h.code_offset) + h.bss_size);
    {
        uint32_t img_end = (uint32_t)image + h.image_size + h.bss_size;
        uint32_t heap_base = PAGE_ALIGN_UP(img_end);
        uint32_t heap_limit = (uint32_t)g_e32_runtime + E32_RUNTIME_MAX;
        if (heap_base < heap_limit)
            syscall_set_app_heap(heap_base, heap_limit);
        else
            syscall_set_app_heap(0, 0);
    }
    e32_entry_t entry = (e32_entry_t)(image + h.entry_offset);
    int rc = entry(argc, argv);
    if (syscall_app_exit_requested()) rc = syscall_app_exit_code();
    syscall_set_app_image(NULL, 0);
    syscall_set_app_heap(0, 0);
    return rc;
}

int e32_exec_file(const char *path) {
    return e32_exec_file_argv(path, 0, NULL);
}
