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

typedef struct platform_file_stamp {
    u64 write_time_ns;
    u64 size;
} platform_file_stamp;

REALM_API b8 platform_file_get_stamp(const char *path, platform_file_stamp *out);

REALM_API b8 platform_file_exists(const char *path);
REALM_API b8 platform_dir_exists(const char *path);
REALM_API b8 platform_file_copy(const char *source_path, const char *dest_path, b8 overwrite);
REALM_API b8 platform_file_delete(const char *path);

REALM_API b8 platform_dir_create(const char *path);
REALM_API b8 platform_dir_remove(const char *path);

REALM_API b8 platform_file_open(const char *path, FILE_PERM perms, rl_file *out_file);
REALM_API b8 platform_file_read_all(rl_file *file);
REALM_API b8 platform_file_write_all(const char *path, const void *data, u64 size);
REALM_API void platform_file_close(rl_file *file);
