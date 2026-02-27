#include "platform/io/file_io.h"

#ifdef PLATFORM_LINUX

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <sys/sendfile.h>
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

b8 platform_file_get_stamp(const char *path, platform_file_stamp *out) {
    if (!path || !path[0] || !out) {
        return false;
    }

    struct stat st = {0};
    if (stat(path, &st) != 0) {
        return false;
    }

    out->write_time_ns = (u64)st.st_mtim.tv_sec * 1000000000ull + (u64)st.st_mtim.tv_nsec;
    out->size = (u64)st.st_size;
    return true;
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

    if (!overwrite && platform_file_exists(dest_path)) {
        RL_ERROR("Failed to copy file '%s' -> '%s': destination exists", source_path, dest_path);
        return false;
    }

    int src_fd = open(source_path, O_RDONLY);
    if (src_fd < 0) {
        RL_ERROR("Failed to copy file '%s' -> '%s'. Error: %d", source_path, dest_path, errno);
        return false;
    }

    struct stat st;
    if (fstat(src_fd, &st) != 0) {
        close(src_fd);
        RL_ERROR("Failed to copy file '%s' -> '%s'. Error: %d", source_path, dest_path, errno);
        return false;
    }

    int dst_fd = open(dest_path, O_WRONLY | O_CREAT | O_TRUNC, st.st_mode);
    if (dst_fd < 0) {
        close(src_fd);
        RL_ERROR("Failed to copy file '%s' -> '%s'. Error: %d", source_path, dest_path, errno);
        return false;
    }

    off_t offset = 0;
    ssize_t sent = sendfile(dst_fd, src_fd, &offset, (size_t)st.st_size);

    close(src_fd);
    close(dst_fd);

    if (sent < 0 || sent != st.st_size) {
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
    out_file->buf = NULL;

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

b8 platform_file_write_all(const char *path, const void *data, u64 size) {
    if (!path || !data) {
        RL_ERROR("platform_file_write_all: invalid arguments");
        return false;
    }

    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        RL_ERROR("Failed to open file for writing: '%s'. Error: %d", path, errno);
        return false;
    }

    u64 total_written = 0;
    while (total_written < size) {
        ssize_t written = write(fd, (const u8 *)data + total_written, size - total_written);
        if (written < 0) {
            RL_ERROR("Failed to write file '%s'. Error: %d", path, errno);
            close(fd);
            return false;
        }
        total_written += (u64)written;
    }

    close(fd);
    return true;
}

void platform_file_close(rl_file *file) {
    if (!file || !file->handle) {
        return;
    }

    int fd = (int)(intptr_t)file->handle;
    close(fd);

    file->handle = NULL;
    if (file->buf) {
        if (file->buf_len > 0) {
            mem_free(file->buf, file->buf_len, MEM_FILE_BUFFERS);
        }
        file->buf = NULL;
    }
}

#endif // PLATFORM_LINUX
