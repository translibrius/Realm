#include "core/project.h"

#include "asset/asset.h"
#include "core/logger.h"
#include "memory/arena.h"
#include "platform/io/file_io.h"
#include "util/str.h"

#include <stdio.h>
#include <string.h>

static rl_project state;

// Normalize path into dst: copy, convert backslashes, ensure trailing slash.
static void normalize_path(char *dst, u64 dst_size, const char *src) {
    u64 len = strlen(src);
    if (len >= dst_size - 1) len = dst_size - 2;
    memcpy(dst, src, len);

    for (u64 i = 0; i < len; i++) {
        if (dst[i] == '\\') dst[i] = '/';
    }

    if (len == 0 || dst[len - 1] != '/') {
        dst[len++] = '/';
    }
    dst[len] = '\0';
}

// Simple TOML-like line parser for project.realm
static char s_current_table[32];

static void project_parse_line(const char *line) {
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
    // Trim key
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

    // Strip quotes if present
    char val_buf[256] = {0};
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

    if (strcmp(fqk, "project.name") == 0) {
        cstr_copy(state.name, sizeof(state.name), val);
    } else if (strcmp(fqk, "project.default_scene") == 0) {
        cstr_copy(state.default_scene, sizeof(state.default_scene), val);
    }
}

b8 project_create(const char *path, const char *name) {
    if (!path || !path[0] || !name || !name[0]) {
        RL_ERROR("project_create: invalid path or name");
        return false;
    }

    char root[RL_PROJECT_PATH_MAX];
    normalize_path(root, sizeof(root), path);

    // Create directory structure
    if (!platform_dir_create(root)) return false;

    rl_temp_arena scratch = rl_arena_scratch_get();

    rl_string assets_dir = rl_string_format(scratch.arena, "%sassets", root);
    if (!platform_dir_create(assets_dir.cstr)) { arena_scratch_release(scratch); return false; }

    rl_string tex_dir = rl_string_format(scratch.arena, "%sassets/textures", root);
    if (!platform_dir_create(tex_dir.cstr)) { arena_scratch_release(scratch); return false; }

    rl_string models_dir = rl_string_format(scratch.arena, "%sassets/models", root);
    if (!platform_dir_create(models_dir.cstr)) { arena_scratch_release(scratch); return false; }

    rl_string mat_dir = rl_string_format(scratch.arena, "%sassets/materials", root);
    if (!platform_dir_create(mat_dir.cstr)) { arena_scratch_release(scratch); return false; }

    rl_string scenes_dir = rl_string_format(scratch.arena, "%sscenes", root);
    if (!platform_dir_create(scenes_dir.cstr)) { arena_scratch_release(scratch); return false; }

    // Write project.realm
    char buf[512];
    i32 len = snprintf(buf, sizeof(buf),
        "[project]\n"
        "name = \"%s\"\n"
        "engine_version = \"0.1\"\n"
        "default_scene = \"scenes/default.scene\"\n",
        name);

    if (len < 0 || len >= (i32)sizeof(buf)) {
        RL_ERROR("project_create: buffer overflow writing project file");
        arena_scratch_release(scratch);
        return false;
    }

    rl_string project_file = rl_string_format(scratch.arena, "%s%s", root, RL_PROJECT_FILENAME);
    if (!platform_file_write_all(project_file.cstr, buf, (u64)len)) {
        RL_ERROR("project_create: failed to write '%s'", project_file.cstr);
        arena_scratch_release(scratch);
        return false;
    }

    arena_scratch_release(scratch);
    RL_INFO("Project '%s' created at '%s'", name, root);
    return true;
}

rl_project *project_open(const char *path) {
    if (!path || !path[0]) {
        RL_ERROR("project_open: invalid path");
        return nullptr;
    }

    char root[RL_PROJECT_PATH_MAX];
    normalize_path(root, sizeof(root), path);

    // Check project.realm exists
    rl_temp_arena scratch = rl_arena_scratch_get();
    rl_string project_file = rl_string_format(scratch.arena, "%s%s", root, RL_PROJECT_FILENAME);

    if (!platform_file_exists(project_file.cstr)) {
        RL_ERROR("project_open: '%s' not found", project_file.cstr);
        arena_scratch_release(scratch);
        return nullptr;
    }

    rl_file file = {0};
    if (!platform_file_open(project_file.cstr, P_FILE_READ, &file)) {
        RL_ERROR("project_open: failed to open '%s'", project_file.cstr);
        arena_scratch_release(scratch);
        return nullptr;
    }

    if (!platform_file_read_all(&file)) {
        RL_ERROR("project_open: failed to read '%s'", project_file.cstr);
        platform_file_close(&file);
        arena_scratch_release(scratch);
        return nullptr;
    }

    // Parse
    memset(&state, 0, sizeof(state));
    cstr_copy(state.root_path, sizeof(state.root_path), root);
    snprintf(state.asset_path, sizeof(state.asset_path), "%sassets/", root);
    snprintf(state.scenes_path, sizeof(state.scenes_path), "%sscenes/", root);

    // Parse line by line
    u64 buf_size = file.buf_len + 1;
    char *text = rl_arena_push(scratch.arena, buf_size, false);
    memcpy(text, file.buf, file.buf_len);
    text[file.buf_len] = '\0';

    s_current_table[0] = '\0';
    char *line = text;
    while (line && *line) {
        char *next = strchr(line, '\n');
        if (next) { *next = '\0'; next++; }
        project_parse_line(line);
        line = next;
    }

    platform_file_close(&file);
    arena_scratch_release(scratch);

    // Set content root
    asset_set_content_root(state.asset_path);
    state.open = true;

    RL_INFO("Project '%s' opened from '%s'", state.name, state.root_path);
    return &state;
}

void project_close(void) {
    if (!state.open) return;

    RL_INFO("Closing project '%s'", state.name);
    asset_system_clear_content();
    asset_clear_content_root();
    memset(&state, 0, sizeof(state));
}

b8 project_is_open(void) {
    return state.open;
}

rl_project *project_get(void) {
    return state.open ? &state : nullptr;
}
