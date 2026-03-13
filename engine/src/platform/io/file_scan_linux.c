#include "platform/io/file_scan.h"

#ifdef PLATFORM_LINUX

#include "core/logger.h"
#include "util/str.h"

#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

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

    DIR *dir = opendir(path);
    if (!dir) {
        RL_ERROR("platform_dir_scan: failed to open '%s'", path);
        return false;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        const char *name = entry->d_name;

        if (name[0] == '.') continue;

        b8 is_dir = false;
        if (entry->d_type == DT_DIR) {
            is_dir = true;
        } else if (entry->d_type == DT_UNKNOWN) {
            char full[1024];
            snprintf(full, sizeof(full), "%s/%s", path, name);
            struct stat st;
            if (stat(full, &st) == 0 && S_ISDIR(st.st_mode)) {
                is_dir = true;
            }
        }

        if (!is_dir && !matches_filter(name, ext_filter)) continue;

        u32 name_len = cstr_len(name);
        char *arena_name = rl_arena_push(arena, name_len + 1, false);
        memcpy(arena_name, name, name_len + 1);

        platform_dir_entry de = {.name = arena_name, .is_dir = is_dir};
        da_append(out, de);
    }

    closedir(dir);
    return true;
}

#endif // PLATFORM_LINUX
