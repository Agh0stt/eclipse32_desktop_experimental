#pragma once

#include "../kernel.h"

#define E32_MAGIC 0x32454345u

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t header_size;
    uint32_t flags;
    uint32_t entry_offset;
    uint32_t code_offset;
    uint32_t code_size;
    uint32_t data_offset;
    uint32_t data_size;
    uint32_t bss_size;
    uint32_t image_size;
} PACKED e32_header_t;

int e32_exec_file(const char *path);
int e32_exec_file_argv(const char *path, int argc, char **argv);
