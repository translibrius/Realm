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
    i32 asset_index_by_id[ASSET_ID_TOTAL];

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

    for (u32 i = 0; i < ASSET_ID_TOTAL; i++) {
        state->asset_index_by_id[i] = -1;
    }

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
    for (u32 i = 0; i < ASSET_TABLE_TOTAL; i++) {
        b8 success = asset_system_load(&asset_table[i]);
        if (!success) {
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

b8 asset_system_load(rl_asset *asset) {
    if (!asset || !asset->filename || !asset->source_path) {
        RL_ERROR("Asset table contains invalid entry");
        return false;
    }

    if (!asset->source_path[0]) {
        RL_ERROR("Asset '%s' has empty source path", asset->filename);
        return false;
    }

    if (asset->source_version == 0) {
        RL_ERROR("Asset '%s' has invalid source version=0", asset->filename);
        return false;
    }

    if (asset->id >= ASSET_ID_TOTAL) {
        RL_ERROR("Asset '%s' has out-of-range id=%d", asset->filename, asset->id);
        return false;
    }

    if (state->asset_index_by_id[asset->id] >= 0) {
        RL_ERROR("Duplicate asset id=%d for '%s'", asset->id, asset->filename);
        return false;
    }

    if (!asset_compute_source_hash(asset)) {
        RL_ERROR("Failed to compute source hash for '%s'", asset->source_path);
        return false;
    }

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
    }

    RL_TRACE("  '%s' = %s", asset->filename, success ? "OK!" : "Failed");
    da_append(&state->assets, *asset);
    state->asset_index_by_id[asset->id] = (i32)(state->assets.count - 1);
    event_fire(EVENT_SPLASH_INCREMENT, nullptr);
    return success;
}

const char *get_asset_root(void) {
    if (!state) {
        return DEFAULT_ASSET_ROOT;
    }

    return state->asset_root;
}

rl_asset *get_asset_by_id(ASSET_ID id) {
    if (!state) {
        RL_ERROR("Asset system is not initialized");
        return nullptr;
    }

    if (id >= ASSET_ID_TOTAL) {
        RL_ERROR("get_asset_by_id() called with invalid id=%d", id);
        return nullptr;
    }

    i32 index = state->asset_index_by_id[id];
    if (index < 0 || (u64)index >= state->assets.count) {
        RL_ERROR("Asset id=%d has not been loaded", id);
        return nullptr;
    }

    return &state->assets.items[index];
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
