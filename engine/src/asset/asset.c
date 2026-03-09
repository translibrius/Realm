#include "asset/asset_internal.h"

#include "asset/asset.h"
#include "asset/asset_table.h"

#include "asset/font.h"
#include "asset/shader.h"
#include "asset/texture.h"
#include "core/event.h"
#include "core/logger.h"
#include "platform/platform.h"
#include "platform/io/file_io.h"
#include "platform/splash/splash.h"
#include "util/str.h"

#include <stdio.h>
#include <string.h>

#define DEFAULT_ASSET_ROOT "../../../assets/"

enum {
    ASSET_ROOT_PATH_MAX = 512,
    ASSET_DIR_PATH_MAX = 640,
};

typedef struct asset_system {
    rl_arena asset_arena;
    Assets assets;
    u32 next_id; // starts at 1; 0 = invalid

    char asset_root[ASSET_ROOT_PATH_MAX];
    char fonts_dir[ASSET_DIR_PATH_MAX];
    char shaders_dir[ASSET_DIR_PATH_MAX];
    char textures_dir[ASSET_DIR_PATH_MAX];
} asset_system;

static asset_system *state;

static u64 asset_hash_bytes(const void *data, u64 length) {
    const u8 *bytes = (const u8 *)data;
    u64 hash = 1469598103934665603ull;
    for (u64 i = 0; i < length; i++) {
        hash ^= bytes[i];
        hash *= 1099511628211ull;
    }

    return hash;
}

static b8 asset_compute_source_hash(rl_asset *asset) {
    if (!state || !asset || !asset->source_path || !asset->source_path[0]) {
        return false;
    }

    rl_temp_arena scratch = rl_arena_scratch_get();
    rl_string absolute_path = rl_string_format(scratch.arena, "%s%s", state->asset_root, asset->source_path);

    rl_file source_file = {0};
    if (!platform_file_open(absolute_path.cstr, P_FILE_READ, &source_file)) {
        RL_ERROR("Failed to open asset source file '%s'", absolute_path.cstr);
        arena_scratch_release(scratch);
        return false;
    }

    if (!platform_file_read_all(&source_file)) {
        RL_ERROR("Failed to read asset source file '%s'", absolute_path.cstr);
        platform_file_close(&source_file);
        arena_scratch_release(scratch);
        return false;
    }

    asset->source_hash = asset_hash_bytes(source_file.buf, source_file.buf_len);

    platform_file_close(&source_file);
    arena_scratch_release(scratch);
    return true;
}

static void asset_set_root(const char *asset_root) {
    if (!state) {
        return;
    }

    const char *source = asset_root;
    if (!source || !source[0]) {
        RL_WARN("Engine config did not provide asset_root, using default '%s'", DEFAULT_ASSET_ROOT);
        source = DEFAULT_ASSET_ROOT;
    }

    u64 source_len = strlen(source);
    u64 max_copy = sizeof(state->asset_root) - 2;

    u64 copy_len = source_len;
    if (copy_len > max_copy) {
        copy_len = max_copy;
        RL_WARN("Asset root path is too long, truncating to %llu characters", copy_len);
    }

    memcpy(state->asset_root, source, copy_len);
    for (u64 i = 0; i < copy_len; i++) {
        if (state->asset_root[i] == '\\') {
            state->asset_root[i] = '/';
        }
    }

    b8 needs_slash = copy_len == 0 || state->asset_root[copy_len - 1] != '/';
    if (needs_slash) {
        state->asset_root[copy_len++] = '/';
    }
    state->asset_root[copy_len] = '\0';

    snprintf(state->fonts_dir, sizeof(state->fonts_dir), "%sfonts/", state->asset_root);
    snprintf(state->shaders_dir, sizeof(state->shaders_dir), "%sshaders/", state->asset_root);
    snprintf(state->textures_dir, sizeof(state->textures_dir), "%stextures/", state->asset_root);
}

static b8 asset_load_data(rl_asset *asset) {
    b8 success = false;
    switch (asset->type) {
    case ASSET_FONT:
        success = rl_font_load(&state->asset_arena, asset);
        break;
    case ASSET_SHADER:
        success = load_shader(&state->asset_arena, asset);
        break;
    case ASSET_TEXTURE:
        success = load_texture(&state->asset_arena, asset);
        break;
    case ASSET_MESH:
        RL_ERROR("Mesh loading not yet implemented for '%s'", asset->filename);
        break;
    }
    return success;
}

u32 get_asset_count() {
    return state->assets.count;
}

Assets *get_assets() {
    return &state->assets;
}

u64 asset_system_size() {
    return sizeof(asset_system);
}

b8 asset_system_start(void *system, const char *asset_root) {
    state = system;
    rl_arena_init(&state->asset_arena, MiB(200), MiB(5), MEM_SUBSYSTEM_ASSET);
    da_init(&state->assets);
    state->next_id = 1;

    asset_set_root(asset_root);

    RL_INFO("Asset root: %s", state->asset_root);

    return true;
}

void asset_system_shutdown() {
    da_free(&state->assets);
    rl_arena_deinit(&state->asset_arena);
    state = nullptr;
}

b8 asset_system_load_all() {
    b8 splash_active = splash_show();
    if (splash_active) {
        splash_update();
    }

    RL_DEBUG("Loading assets...");
    for (u32 i = 0; i < ASSET_TABLE_COUNT; i++) {
        asset_table_entry *entry = &asset_table[i];
        u32 id = asset_load(entry->type, entry->source_path);
        if (id == 0) {
            if (splash_active) {
                splash_hide();
            }
            return false;
        }
        if (splash_active) {
            splash_update();
            platform_pump_messages();
        }
    }

    if (splash_active) {
        splash_hide();
    }

    return true;
}

u32 asset_load(ASSET_TYPE type, const char *source_path) {
    if (!state) {
        RL_ERROR("Asset system is not initialized");
        return 0;
    }

    if (!source_path || !source_path[0]) {
        RL_ERROR("asset_load called with empty source_path");
        return 0;
    }

    // Dedup: return existing if already loaded
    u32 existing = asset_find(source_path);
    if (existing != 0) {
        return existing;
    }

    // Extract filename from path
    const char *filename = source_path;
    for (const char *p = source_path; *p; p++) {
        if (*p == '/' || *p == '\\') {
            filename = p + 1;
        }
    }

    u32 id = state->next_id++;

    rl_asset asset = {
        .id = id,
        .type = type,
        .source_path = source_path,
        .source_version = 1,
        .source_hash = 0,
        .filename = filename,
        .data = nullptr,
    };

    if (!asset_compute_source_hash(&asset)) {
        RL_ERROR("Failed to compute source hash for '%s'", source_path);
        return 0;
    }

    if (!asset_load_data(&asset)) {
        RL_ERROR("Failed to load asset '%s'", source_path);
        return 0;
    }

    RL_TRACE("  '%s' = OK! (id=%u)", asset.filename, id);
    da_append(&state->assets, asset);
    e_splash_payload splash_payload = {.asset_name = asset.filename};
    event_fire(EVENT_SPLASH_INCREMENT, &splash_payload);
    return id;
}

const char *get_asset_root(void) {
    if (!state) {
        return DEFAULT_ASSET_ROOT;
    }

    return state->asset_root;
}

rl_asset *asset_get(u32 id) {
    if (!state) {
        RL_ERROR("Asset system is not initialized");
        return nullptr;
    }

    if (id == 0 || id > state->assets.count) {
        return nullptr;
    }

    return &state->assets.items[id - 1];
}

u32 asset_find(const char *source_path) {
    if (!state || !source_path) {
        return 0;
    }

    for (u64 i = 0; i < state->assets.count; i++) {
        if (strcmp(state->assets.items[i].source_path, source_path) == 0) {
            return state->assets.items[i].id;
        }
    }

    return 0;
}

const char *get_assets_dir(ASSET_TYPE asset_type) {
    if (!state) {
        return DEFAULT_ASSET_ROOT;
    }

    switch (asset_type) {
    case ASSET_FONT:
        return state->fonts_dir;
    case ASSET_SHADER:
        return state->shaders_dir;
    case ASSET_TEXTURE:
        return state->textures_dir;
    default:
        break;
    }

    return state->asset_root;
}
