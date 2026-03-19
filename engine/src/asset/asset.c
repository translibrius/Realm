#include "asset/asset_internal.h"

#include "asset/asset.h"
#include "asset/asset_table.h"

#include "asset/font.h"
#include "asset/mesh.h"
#include "asset/model.h"
#include "asset/shader.h"
#include "asset/texture.h"
#include "core/event.h"
#include "core/logger.h"
#include "engine.h"
#include "platform/platform.h"
#include "platform/io/file_io.h"
#include "platform/splash/splash.h"
#include "util/hash.h"
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
    asset_id next_id; // starts at 1; 0 = invalid

    char asset_root[ASSET_ROOT_PATH_MAX];
    char fonts_dir[ASSET_DIR_PATH_MAX];
    char shaders_dir[ASSET_DIR_PATH_MAX];
    char textures_dir[ASSET_DIR_PATH_MAX];
    char models_dir[ASSET_DIR_PATH_MAX];

    // Content root override (set by project system)
    char content_root[ASSET_ROOT_PATH_MAX];
    b8 has_content_root;

    // First content asset id (set after engine assets loaded)
    asset_id content_start_id;
} asset_system;

static asset_system *state;

static b8 asset_compute_source_hash(rl_asset *asset) {
    if (!state || !asset || !asset->source_path || !asset->source_path[0]) {
        return false;
    }

    rl_temp_arena scratch = rl_arena_scratch_get();
    const char *root = asset_get_resolve_root(asset->type);
    rl_string absolute_path = rl_str_format(scratch.arena, "%s%s", root, asset->source_path);

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

    asset->source_hash = hash_fnv1a(source_file.buf, source_file.buf_len);

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

    u64 source_len = cstr_len(source);
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

    cstr_format_buf(state->fonts_dir, sizeof(state->fonts_dir), "%sfonts/", state->asset_root);
    cstr_format_buf(state->shaders_dir, sizeof(state->shaders_dir), "%sshaders/", state->asset_root);
    cstr_format_buf(state->textures_dir, sizeof(state->textures_dir), "%stextures/", state->asset_root);
    cstr_format_buf(state->models_dir, sizeof(state->models_dir), "%smodels/", state->asset_root);
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
        success = load_mesh(&state->asset_arena, asset);
        break;
    case ASSET_MODEL:
        success = load_model(&state->asset_arena, asset);
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
    state->has_content_root = false;
    state->content_root[0] = '\0';
    state->content_start_id = 0;

    asset_set_root(asset_root);

    RL_INFO("Asset root: %s", state->asset_root);

    return true;
}

void asset_system_shutdown() {
    da_free(&state->assets);
    rl_arena_deinit(&state->asset_arena);
    state = nullptr;
}

b8 asset_system_load_engine() {
    b8 splash_active = false;
    if (!rl_engine_get_skip_splash()) {
        splash_active = splash_show();
        if (splash_active) {
            splash_update();
        }
    }

    RL_DEBUG("Loading engine assets...");
    for (u32 i = 0; i < ENGINE_ASSET_TABLE_COUNT; i++) {
        asset_table_entry *entry = &engine_asset_table[i];
        asset_id id = asset_load(entry->type, entry->source_path);
        if (id == 0) {
            if (splash_active) {
                splash_hide();
            }
            return false;
        }
        if (splash_active) {
            e_splash_payload splash_payload = {.asset_name = entry->filename};
            event_fire(EVENT_SPLASH_INCREMENT, &splash_payload);
            splash_update();
            platform_pump_messages();
        }
    }

    if (splash_active) {
        splash_hide();
    }

    state->content_start_id = state->next_id;
    return true;
}

void asset_system_clear_content(void) {
    if (!state || state->content_start_id == 0) return;

    // Truncate back to engine-only assets
    u32 engine_count = state->content_start_id - 1;
    if (state->assets.count > engine_count) {
        state->assets.count = engine_count;
    }
    state->next_id = state->content_start_id;
}

asset_id asset_load(ASSET_TYPE type, const char *source_path) {
    if (!state) {
        RL_ERROR("Asset system is not initialized");
        return 0;
    }

    if (!source_path || !source_path[0]) {
        RL_ERROR("asset_load called with empty source_path");
        return 0;
    }

    // Dedup: return existing if already loaded
    asset_id existing = asset_find(source_path);
    if (existing != 0) {
        return existing;
    }

    // Copy source_path to asset arena so it persists for the asset's lifetime
    // (dynamic paths from scratch arenas would otherwise dangle)
    const char *owned_path = cstr_format(&state->asset_arena, "%s", source_path);

    // Extract filename from the arena-owned copy
    const char *filename = owned_path;
    const char *slash = cstr_find_last_char(owned_path, '/');
    if (!slash) slash = cstr_find_last_char(owned_path, '\\');
    if (slash) filename = slash + 1;

    asset_id id = state->next_id++;

    rl_asset asset = {
        .id = id,
        .type = type,
        .source_path = owned_path,
        .source_version = 1,
        .source_hash = 0,
        .filename = filename,
        .data = nullptr,
    };

    if (!asset_compute_source_hash(&asset)) {
        RL_ERROR("Failed to compute source hash for '%s'", source_path);
        return 0;
    }

    // Reserve slot before loading data — load_mesh may recursively call
    // asset_load (e.g. for textures), so the slot must exist at the correct
    // index (id-1) before any child assets are appended.
    da_append(&state->assets, asset);

    if (!asset_load_data(&asset)) {
        RL_ERROR("Failed to load asset '%s'", source_path);
        // Remove the reserved slot
        state->assets.count--;
        return 0;
    }

    // Write back the fully loaded asset (data pointer filled in by loader)
    state->assets.items[id - 1] = asset;

    RL_TRACE("  '%s' = OK! (id=%u)", asset.filename, id);
    return id;
}

const char *get_asset_root(void) {
    if (!state) {
        return DEFAULT_ASSET_ROOT;
    }

    return state->asset_root;
}

const char *asset_get_resolve_root(ASSET_TYPE type) {
    if (!state) {
        return DEFAULT_ASSET_ROOT;
    }

    if (state->has_content_root && (type == ASSET_TEXTURE || type == ASSET_MESH || type == ASSET_MODEL)) {
        return state->content_root;
    }

    return state->asset_root;
}

void asset_set_content_root(const char *path) {
    if (!state || !path || !path[0]) return;

    u64 len = cstr_len(path);
    u64 max_copy = sizeof(state->content_root) - 2;
    if (len > max_copy) len = max_copy;

    memcpy(state->content_root, path, len);

    // Normalize slashes
    for (u64 i = 0; i < len; i++) {
        if (state->content_root[i] == '\\') {
            state->content_root[i] = '/';
        }
    }

    // Ensure trailing slash
    if (len == 0 || state->content_root[len - 1] != '/') {
        state->content_root[len++] = '/';
    }
    state->content_root[len] = '\0';
    state->has_content_root = true;

    RL_INFO("Content root set to: %s", state->content_root);
}

void asset_clear_content_root(void) {
    if (!state) return;
    state->content_root[0] = '\0';
    state->has_content_root = false;
}

rl_asset *asset_get(asset_id id) {
    if (!state) {
        RL_ERROR("Asset system is not initialized");
        return nullptr;
    }

    if (id == 0 || id > state->assets.count) {
        return nullptr;
    }

    return &state->assets.items[id - 1];
}

asset_id asset_find(const char *source_path) {
    if (!state || !source_path) {
        return 0;
    }

    for (u64 i = 0; i < state->assets.count; i++) {
        if (cstr_eq(state->assets.items[i].source_path, source_path)) {
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
    case ASSET_MESH:
    case ASSET_MODEL:
        return state->models_dir;
    default:
        break;
    }

    return state->asset_root;
}
