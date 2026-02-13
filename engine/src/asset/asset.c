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
    char asset_root[ASSET_ROOT_PATH_MAX];
    char fonts_dir[ASSET_DIR_PATH_MAX];
    char shaders_dir[ASSET_DIR_PATH_MAX];
    char textures_dir[ASSET_DIR_PATH_MAX];
} asset_system;

static asset_system *state;

static void asset_set_root(const char *asset_root) {
    if (!state) {
        return;
    }

    const char *source = asset_root;
    if (!source || !source[0]) {
        source = DEFAULT_ASSET_ROOT;
    }

    u64 source_len = strlen(source);
    b8 needs_slash = source_len == 0 || source[source_len - 1] != '/';
    u64 max_copy = sizeof(state->asset_root) - 1;
    if (needs_slash) {
        max_copy -= 1;
    }

    u64 copy_len = source_len;
    if (copy_len > max_copy) {
        copy_len = max_copy;
        RL_WARN("Asset root path is too long, truncating to %llu characters", copy_len);
    }

    memcpy(state->asset_root, source, copy_len);
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
    event_fire(EVENT_SPLASH_INCREMENT, nullptr);
    return success;
}

rl_asset *get_asset(const char *filename) {
    for (u32 i = 0; i < state->assets.count; i++) {
        if (strcmp(state->assets.items[i].filename, filename) == 0) {
            return &state->assets.items[i];
        }
    }

    RL_FATAL("FAILED TO FIND ASSET '%s'", filename);
    return nullptr;
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
