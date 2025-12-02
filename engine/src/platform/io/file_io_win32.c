#include "platform/io/file_io.h"
#include "util/str.h"
#include "vendor/glad/glad_wgl.h"

#ifdef PLATFORM_WINDOWS

typedef struct file_system_state {
    rl_arena file_arena;
} file_system_state;

b8 file_dir_exists(const char *path) {
    rl_arena scratch;
    b8 found = false;

    rl_arena_create(MiB(1), &scratch);
    rl_string_create(&scratch, path);

    WIN32_FIND_DATA found_data;
    HANDLE h = FindFirstFileA(path, &found_data);

    if (h != INVALID_HANDLE_VALUE)
        found = true;

    rl_arena_destroy(&scratch);
    return found;
}

#endif