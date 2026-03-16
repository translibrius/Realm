#include "core/project.h"

#include "asset/asset.h"
#include "core/logger.h"
#include "memory/arena.h"
#include "platform/io/file_io.h"
#include "util/str.h"
#include "util/toml.h"

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

    toml_table *t = toml_parse_file(project_file.cstr);
    if (!t) {
        RL_ERROR("project_open: failed to parse '%s'", project_file.cstr);
        arena_scratch_release(scratch);
        return nullptr;
    }

    // Parse
    memset(&state, 0, sizeof(state));
    cstr_copy(state.root_path, sizeof(state.root_path), root);
    snprintf(state.asset_path, sizeof(state.asset_path), "%sassets/", root);
    snprintf(state.scenes_path, sizeof(state.scenes_path), "%sscenes/", root);

    cstr_copy(state.name, sizeof(state.name), toml_get_string(t, "project", "name", "Untitled"));
    cstr_copy(state.default_scene, sizeof(state.default_scene), toml_get_string(t, "project", "default_scene", ""));

    toml_free(t);
    arena_scratch_release(scratch);

    // Default scene path if not specified in project file
    if (!state.default_scene[0]) {
        cstr_copy(state.default_scene, sizeof(state.default_scene), "scenes/default.scene");
    }

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
