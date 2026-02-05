#include "platform/io/file_io.h"

#ifdef PLATFORM_MACOS

#include <copyfile.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "core/logger.h"
#include "memory/memory.h"
#include "util/assert.h"

static int file_access_mode(const FILE_PERM *perms) {
    switch (*perms) {
    case P_FILE_READ:
        return O_RDONLY;
    case P_FILE_WRITE:
        return O_WRONLY;
    case P_FILE_EXECUTE:
        return O_RDONLY;
    case P_FILE_ALL:
        return O_RDWR;
    }
    return O_RDONLY;
}

b8 platform_file_exists(const char *path) {
    if (!path) {
        return false;
    }
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

b8 platform_dir_exists(const char *path) {
    if (!path) {
        return false;
    }
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

b8 platform_file_copy(const char *source_path, const char *dest_path, b8 overwrite) {
    if (!source_path || !dest_path) {
        RL_ERROR("Failed to copy file: invalid path(s)");
        return false;
    }

    copyfile_state_t state_copy = copyfile_state_alloc();
    copyfile_flags_t flags = COPYFILE_ALL;
    if (!overwrite) {
        flags |= COPYFILE_EXCL;
    }

    int result = copyfile(source_path, dest_path, state_copy, flags);
    copyfile_state_free(state_copy);

    if (result != 0) {
        RL_ERROR("Failed to copy file '%s' -> '%s'. Error: %d", source_path, dest_path, errno);
        return false;
    }

    return true;
}

b8 platform_file_delete(const char *path) {
    if (!path) {
        RL_ERROR("Failed to delete file: invalid path");
        return false;
    }

    if (unlink(path) != 0) {
        if (errno == ENOENT) {
            return true;
        }
        RL_ERROR("Failed to delete file '%s'. Error: %d", path, errno);
        return false;
    }

    return true;
}

b8 platform_file_open(const char *path, FILE_PERM perms, rl_file *out_file) {
    RL_ASSERT_MSG(out_file && !out_file->handle, "Trying to open a non-closed file");

    out_file->buf_len = 0;
    out_file->buf = nullptr;

    int fd = open(path, file_access_mode(&perms));
    if (fd < 0) {
        RL_ERROR("Failed to open file='%s'. Error: %d", path, errno);
        return false;
    }

    struct stat st;
    if (fstat(fd, &st) != 0) {
        RL_ERROR("Failed to stat file='%s'. Error: %d", path, errno);
        close(fd);
        return false;
    }

    const char *name = strrchr(path, '/');
    if (name) {
        name += 1;
    } else {
        name = path;
    }

    out_file->path = path;
    out_file->name = name;
    out_file->handle = (void *)(intptr_t)fd;
    out_file->size = (u64)st.st_size;

    return true;
}

b8 platform_file_read_all(rl_file *file) {
    if (!file || !file->handle) {
        return false;
    }

    int fd = (int)(intptr_t)file->handle;
    file->buf = mem_alloc(file->size, MEM_FILE_BUFFERS);

    u64 total_read = 0;
    while (total_read < file->size) {
        ssize_t bytes = read(fd, (u8 *)file->buf + total_read, file->size - total_read);
        if (bytes <= 0) {
            RL_ERROR("Failed to read file='%s'. Error: %d", file->name, errno);
            return false;
        }
        total_read += (u64)bytes;
    }

    file->buf_len = total_read;
    return true;
}

void platform_file_close(rl_file *file) {
    if (!file || !file->handle) {
        return;
    }

    int fd = (int)(intptr_t)file->handle;
    close(fd);

    file->handle = nullptr;
    if (file->buf) {
        if (file->buf_len > 0) {
            mem_free(file->buf, file->buf_len, MEM_FILE_BUFFERS);
        }
        file->buf = nullptr;
    }
}

#endif // PLATFORM_MACOS
