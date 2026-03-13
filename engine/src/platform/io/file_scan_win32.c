#include "platform/platform.h"

#ifdef PLATFORM_WINDOWS

#include "platform/io/file_scan.h"

#include "core/logger.h"
#include "util/str.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string.h>

static b8 matches_filter(const char *name, const char *ext_filter) {
    if (!ext_filter) return true;

    const char *exts[16];
    u32 ext_count = 0;

    char buf[256];
    u32 len = cstr_len(ext_filter);
    if (len >= sizeof(buf)) len = sizeof(buf) - 1;
    memcpy(buf, ext_filter, len);
    buf[len] = '\0';

    char *tok = buf;
    while (*tok && ext_count < 16) {
        while (*tok == ' ' || *tok == ',') tok++;
        if (!*tok) break;
        exts[ext_count] = tok;
        char *sep = strchr(tok, ',');
        if (sep) {
            *sep = '\0';
            tok = sep + 1;
        } else {
            tok += strlen(tok);
        }
        ext_count++;
    }

    for (u32 i = 0; i < ext_count; i++) {
        if (cstr_ends_with(name, exts[i])) return true;
    }
    return false;
}

b8 platform_dir_scan(const char *path,
                      const char *ext_filter,
                      rl_arena *arena,
                      DirEntries *out) {
    if (!path || !out) return false;

    char search_path[1024];
    snprintf(search_path, sizeof(search_path), "%s\\*", path);

    WIN32_FIND_DATAA ffd;
    HANDLE h = FindFirstFileA(search_path, &ffd);
    if (h == INVALID_HANDLE_VALUE) {
        RL_ERROR("platform_dir_scan: failed to open '%s'", path);
        return false;
    }

    do {
        const char *name = ffd.cFileName;

        if (name[0] == '.') continue;

        b8 is_dir = (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

        if (!is_dir && !matches_filter(name, ext_filter)) continue;

        u32 name_len = cstr_len(name);
        char *arena_name = rl_arena_push(arena, name_len + 1, false);
        memcpy(arena_name, name, name_len + 1);

        platform_dir_entry de = {.name = arena_name, .is_dir = is_dir};
        da_append(out, de);
    } while (FindNextFileA(h, &ffd));

    FindClose(h);
    return true;
}

#endif // PLATFORM_WINDOWS
