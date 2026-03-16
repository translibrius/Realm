#include "ed_config.h"

#include "core/logger.h"
#include "memory/arena.h"
#include "platform/io/file_io.h"
#include "util/str.h"

#include <stdio.h>
#include <string.h>

static char s_current_table[32];

static void parse_line(ed_config *cfg, const char *line) {
    while (*line == ' ' || *line == '\t') line++;
    if (*line == '\0' || *line == '#' || *line == '\n' || *line == '\r') return;

    // Table header
    if (*line == '[') {
        const char *end = strchr(line + 1, ']');
        if (!end) return;
        u32 len = (u32)(end - line - 1);
        if (len >= sizeof(s_current_table)) return;
        memcpy(s_current_table, line + 1, len);
        s_current_table[len] = '\0';
        return;
    }

    // Key = value
    const char *eq = strchr(line, '=');
    if (!eq) return;

    // Extract key
    char key[64] = {0};
    u32 key_len = (u32)(eq - line);
    if (key_len >= sizeof(key)) return;
    memcpy(key, line, key_len);
    key[key_len] = '\0';
    char *k = key;
    while (*k == ' ' || *k == '\t') k++;
    char *kend = k + strlen(k) - 1;
    while (kend > k && (*kend == ' ' || *kend == '\t')) { *kend = '\0'; kend--; }

    // Build fully-qualified key
    char fqk[96] = {0};
    if (s_current_table[0]) {
        snprintf(fqk, sizeof(fqk), "%s.%s", s_current_table, k);
    } else {
        snprintf(fqk, sizeof(fqk), "%s", k);
    }

    // Extract value
    const char *v = eq + 1;
    while (*v == ' ' || *v == '\t') v++;

    char val_buf[ED_PROJECT_PATH_MAX] = {0};
    u32 vlen = 0;
    while (v[vlen] && v[vlen] != '\n' && v[vlen] != '\r' && v[vlen] != '#' && vlen < sizeof(val_buf) - 1) {
        vlen++;
    }
    memcpy(val_buf, v, vlen);
    val_buf[vlen] = '\0';

    // Trim trailing whitespace
    char *val = val_buf;
    while (*val == ' ' || *val == '\t') val++;
    char *vend = val + strlen(val) - 1;
    while (vend > val && (*vend == ' ' || *vend == '\t' || *vend == '\n' || *vend == '\r')) {
        *vend = '\0';
        vend--;
    }

    // Strip surrounding quotes
    u32 val_len = (u32)strlen(val);
    if (val_len >= 2 && val[0] == '"' && val[val_len - 1] == '"') {
        val[val_len - 1] = '\0';
        val++;
    }

    if (strcmp(fqk, "editor.last_project") == 0) {
        cstr_copy(cfg->last_project, sizeof(cfg->last_project), val);
    } else if (strcmp(fqk, "editor.theme") == 0) {
        cstr_copy(cfg->theme, sizeof(cfg->theme), val);
    } else if (strncmp(fqk, "editor.recent_", 14) == 0) {
        u32 idx = (u32)(fqk[14] - '0');
        if (idx < ED_MAX_RECENT_PROJECTS) {
            cstr_copy(cfg->recent_projects[idx], sizeof(cfg->recent_projects[idx]), val);
            if (idx + 1 > cfg->recent_count) {
                cfg->recent_count = idx + 1;
            }
        }
    }
}

void ed_config_load(ed_config *cfg) {
    if (!cfg) return;
    memset(cfg, 0, sizeof(*cfg));

    rl_file file = {0};
    if (!platform_file_open(ED_STATE_FILENAME, P_FILE_READ, &file)) {
        RL_DEBUG("No editor state file found, starting fresh");
        return;
    }

    if (!platform_file_read_all(&file)) {
        platform_file_close(&file);
        return;
    }

    rl_temp_arena scratch = rl_arena_scratch_get();
    u64 buf_size = file.buf_len + 1;
    char *text = rl_arena_push(scratch.arena, buf_size, false);
    memcpy(text, file.buf, file.buf_len);
    text[file.buf_len] = '\0';

    s_current_table[0] = '\0';
    char *line = text;
    while (line && *line) {
        char *next = strchr(line, '\n');
        if (next) { *next = '\0'; next++; }
        parse_line(cfg, line);
        line = next;
    }

    platform_file_close(&file);
    arena_scratch_release(scratch);

    // Compact: remove empty entries from recent list
    u32 write = 0;
    for (u32 i = 0; i < cfg->recent_count; i++) {
        if (cfg->recent_projects[i][0]) {
            if (write != i) {
                cstr_copy(cfg->recent_projects[write], sizeof(cfg->recent_projects[write]), cfg->recent_projects[i]);
            }
            write++;
        }
    }
    cfg->recent_count = write;

    // Default theme if not set
    if (!cfg->theme[0]) {
        cstr_copy(cfg->theme, sizeof(cfg->theme), "dark");
    }

    RL_DEBUG("Editor state loaded: last_project='%s', %u recent", cfg->last_project, cfg->recent_count);
}

void ed_config_save(const ed_config *cfg) {
    if (!cfg) return;

    char buf[4096];
    i32 offset = 0;

    offset += snprintf(buf + offset, sizeof(buf) - (u64)offset,
        "[editor]\n"
        "last_project = \"%s\"\n"
        "theme = \"%s\"\n",
        cfg->last_project, cfg->theme[0] ? cfg->theme : "dark");

    for (u32 i = 0; i < cfg->recent_count && i < ED_MAX_RECENT_PROJECTS; i++) {
        if (cfg->recent_projects[i][0]) {
            offset += snprintf(buf + offset, sizeof(buf) - (u64)offset,
                "recent_%u = \"%s\"\n", i, cfg->recent_projects[i]);
        }
    }

    if (!platform_file_write_all(ED_STATE_FILENAME, buf, (u64)offset)) {
        RL_ERROR("Failed to save editor state to '%s'", ED_STATE_FILENAME);
    }
}

void ed_config_add_recent(ed_config *cfg, const char *project_path) {
    if (!cfg || !project_path || !project_path[0]) return;

    // Update last_project
    cstr_copy(cfg->last_project, sizeof(cfg->last_project), project_path);

    // Remove duplicate if present
    for (u32 i = 0; i < cfg->recent_count; i++) {
        if (strcmp(cfg->recent_projects[i], project_path) == 0) {
            // Shift everything down
            for (u32 j = i; j > 0; j--) {
                cstr_copy(cfg->recent_projects[j], sizeof(cfg->recent_projects[j]), cfg->recent_projects[j - 1]);
            }
            cstr_copy(cfg->recent_projects[0], sizeof(cfg->recent_projects[0]), project_path);
            return;
        }
    }

    // Shift existing entries down, cap at max
    u32 count = cfg->recent_count;
    if (count >= ED_MAX_RECENT_PROJECTS) count = ED_MAX_RECENT_PROJECTS - 1;

    for (u32 i = count; i > 0; i--) {
        cstr_copy(cfg->recent_projects[i], sizeof(cfg->recent_projects[i]), cfg->recent_projects[i - 1]);
    }

    cstr_copy(cfg->recent_projects[0], sizeof(cfg->recent_projects[0]), project_path);

    cfg->recent_count = count + 1;
    if (cfg->recent_count > ED_MAX_RECENT_PROJECTS) {
        cfg->recent_count = ED_MAX_RECENT_PROJECTS;
    }
}
