#pragma once

#include "defines.h"
#include "memory/arena.h"

typedef enum FILE_PERM {
    P_FILE_READ,
    P_FILE_WRITE,
    P_FILE_EXECUTE,
    P_FILE_ALL,
} FILE_PERM;

typedef struct rl_file {
    void *handle;
    void *buf;
    const char *name;
    const char *path;
    u64 size;
    u64 buf_len;
} rl_file;

b8 platform_file_exists(const char *path);
b8 platform_dir_exists(const char *path);

b8 platform_file_open(const char *path, FILE_PERM perms, rl_file *out_file);
b8 platform_file_read_all(rl_file *file);
void platform_file_close(rl_file *file);