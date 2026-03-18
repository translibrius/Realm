#include "platform/platform.h"

#ifdef PLATFORM_WINDOWS
#include "core/logger.h"
#include "glad_wgl.h"
#include "platform/io/file_io.h"
#include "util/assert.h"
#include "util/str.h"

typedef struct file_system_state {
    rl_arena file_arena;
} file_system_state;

// Forward decl
DWORD access_perms(const FILE_PERM *perms);

b8 platform_file_get_stamp(const char *path, platform_file_stamp *out) {
    if (!path || !path[0] || !out) {
        return false;
    }

    WIN32_FILE_ATTRIBUTE_DATA attrs = {0};
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &attrs)) {
        return false;
    }

    ULARGE_INTEGER write_time = {0};
    write_time.LowPart = attrs.ftLastWriteTime.dwLowDateTime;
    write_time.HighPart = attrs.ftLastWriteTime.dwHighDateTime;

    ULARGE_INTEGER size = {0};
    size.LowPart = attrs.nFileSizeLow;
    size.HighPart = attrs.nFileSizeHigh;

    out->write_time_ns = write_time.QuadPart * 100;
    out->size = size.QuadPart;
    return true;
}

b8 platform_file_exists(const char *path) {
    DWORD attrs = GetFileAttributesA(path);
    return (attrs != INVALID_FILE_ATTRIBUTES) && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
}

b8 platform_dir_exists(const char *path) {
    DWORD attrs = GetFileAttributesA(path);
    return (attrs != INVALID_FILE_ATTRIBUTES) && (attrs & FILE_ATTRIBUTE_DIRECTORY);
}

b8 platform_dir_create(const char *path) {
    if (!path || !path[0]) return false;
    if (CreateDirectoryA(path, NULL)) return true;
    DWORD err = GetLastError();
    if (err == ERROR_ALREADY_EXISTS) return true;
    RL_ERROR("Failed to create directory '%s'. Error: %lu", path, err);
    return false;
}

b8 platform_dir_remove(const char *path) {
    if (!path || !path[0]) return false;

    char search[MAX_PATH];
    snprintf(search, sizeof(search), "%s/*", path);

    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(search, &fd);
    if (h == INVALID_HANDLE_VALUE) {
        return RemoveDirectoryA(path) != 0;
    }

    do {
        if (cstr_eq(fd.cFileName, ".") || cstr_eq(fd.cFileName, "..")) continue;

        char child[MAX_PATH];
        cstr_format_buf(child, sizeof(child), "%s/%s", path, fd.cFileName);

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            platform_dir_remove(child);
        } else {
            DeleteFileA(child);
        }
    } while (FindNextFileA(h, &fd));

    FindClose(h);
    return RemoveDirectoryA(path) != 0;
}

b8 platform_file_copy(const char *source_path, const char *dest_path, b8 overwrite) {
    if (!source_path || !dest_path) {
        RL_ERROR("Failed to copy file: invalid path(s)");
        return false;
    }

    BOOL success = CopyFileA(source_path, dest_path, overwrite ? FALSE : TRUE);
    if (!success) {
        RL_ERROR("Failed to copy file '%s' -> '%s'. Error: %lu", source_path, dest_path, GetLastError());
        return false;
    }

    return true;
}

b8 platform_file_delete(const char *path) {
    if (!path) {
        RL_ERROR("Failed to delete file: invalid path");
        return false;
    }

    if (!DeleteFileA(path)) {
        DWORD err = GetLastError();
        if (err == ERROR_FILE_NOT_FOUND) {
            return true;
        }
        RL_ERROR("Failed to delete file '%s'. Error: %lu", path, err);
        return false;
    }

    return true;
}

b8 platform_file_open(const char *path, FILE_PERM perms, rl_file *out_file) {
    RL_ASSERT_MSG(!out_file->handle, "Trying to open a non-closed file");
    rl_temp_arena scratch = rl_arena_scratch_get();

    out_file->buf_len = 0;

    HANDLE h = CreateFileA(
        path,
        access_perms(&perms),
        FILE_SHARE_READ, // Only allow other processes to read while we have it open
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);

    if (h == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        if (err == 2) {
            RL_ERROR("Failed to open file='%s'. Error: File not found", path);
        } else {
            RL_ERROR("Failed to open file='%s'. Error: %d", path, err);
        }
        return false;
    }
    SetFilePointer(h, 0, nullptr, FILE_BEGIN);

    rl_string path_str = rl_path_normalize(scratch.arena, rl_str(path));

    Strings split;
    da_init(&split);
    rl_str_split(path_str, rl_str("/"), &split);
    RL_ASSERT(split.count > 0);

    out_file->path = path;
    out_file->name = split.items[split.count - 1].cstr;
    out_file->handle = h;

    LARGE_INTEGER l_int_size;
    GetFileSizeEx(h, &l_int_size);
    out_file->size = l_int_size.QuadPart;

    // RL_DEBUG("Successfully opened file. Name='%s' Size=%llu", out_file->name, out_file->size);

    da_free(&split);
    arena_scratch_release(scratch);
    return true;
}

void platform_file_close(rl_file *file) {
    if (!CloseHandle(file->handle)) {
        RL_ERROR("Failed to close file='%s'", file->name);
    }

    file->handle = nullptr;
    if (file->buf) {
        if (file->buf_len > 0) {
            mem_free(file->buf, file->buf_len, MEM_FILE_BUFFERS);
        }
        file->buf = nullptr;
    }
}

b8 platform_file_read_all(rl_file *file) {
    DWORD bytes_read = 0;

    file->buf = mem_alloc(file->size, MEM_FILE_BUFFERS);

    BOOL success = ReadFile(file->handle, file->buf, file->size, &bytes_read, nullptr);
    if (!success) {
        RL_ERROR("Failed to read file='%s'. Error: %d", file->name, GetLastError());
        return false;
    }

    if (bytes_read != file->size) {
        RL_ERROR("Failed to read file='%s'. Expected %llu bytes, got %llu", file->name, file->size, bytes_read);
        return false;
    }

    file->buf_len = bytes_read;
    return true;
}

b8 platform_file_write_all(const char *path, const void *data, u64 size) {
    if (!path || !data) {
        RL_ERROR("platform_file_write_all: invalid arguments");
        return false;
    }

    HANDLE h = CreateFileA(
        path,
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);

    if (h == INVALID_HANDLE_VALUE) {
        RL_ERROR("Failed to open file for writing: '%s'. Error: %lu", path, GetLastError());
        return false;
    }

    DWORD bytes_written = 0;
    BOOL success = WriteFile(h, data, (DWORD)size, &bytes_written, nullptr);
    CloseHandle(h);

    if (!success || bytes_written != (DWORD)size) {
        RL_ERROR("Failed to write file '%s'. Error: %lu", path, GetLastError());
        return false;
    }

    return true;
}

// Private
DWORD access_perms(const FILE_PERM *perms) {
    switch (*perms) {
    case P_FILE_READ:
        return GENERIC_READ;
    case P_FILE_WRITE:
        return GENERIC_WRITE;
    case P_FILE_EXECUTE:
        return GENERIC_EXECUTE;
    case P_FILE_ALL:
        return GENERIC_ALL;
    }

    // Fallback
    return GENERIC_READ;
}

#endif
