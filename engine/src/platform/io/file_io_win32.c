#include "core/logger.h"
#include "glad_wgl.h"
#include "platform/io/file_io.h"
#include "util/str.h"

#ifdef PLATFORM_WINDOWS

typedef struct file_system_state {
    rl_arena file_arena;
} file_system_state;

// Forward decl
DWORD access_perms(const FILE_PERM *perms);

b8 platform_file_exists(const char *path) {
    DWORD attrs = GetFileAttributesA(path);
    return (attrs != INVALID_FILE_ATTRIBUTES) && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
}

b8 platform_dir_exists(const char *path) {
    DWORD attrs = GetFileAttributesA(path);
    return (attrs != INVALID_FILE_ATTRIBUTES) && (attrs & FILE_ATTRIBUTE_DIRECTORY);
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

    rl_string path_str = rl_path_sanitize(scratch.arena, path);

    Strings split;
    da_init(&split);
    rl_string_split(scratch.arena, &path_str, "/", &split);
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
