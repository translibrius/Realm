#include "asset/asset.h"

#include "asset/asset_table.h"

#include "asset/font.h"
#include "core/event.h"
#include "core/logger.h"
#include "platform/thread.h"
#include "platform/io/file_io.h"
#include "platform/splash/splash.h"
#include "asset/shader.h"
#include "asset/texture.h"

#include <string.h>

DA_DEFINE(Assets, rl_asset);

typedef struct asset_system {
    rl_arena asset_arena;
    Assets assets;
} asset_system;

static asset_system *state;

u64 asset_system_size() {
    return sizeof(asset_system);
}

b8 asset_system_start(void *system) {
    state = system;
    rl_arena_create(MiB(25), &state->asset_arena, MEM_SUBSYSTEM_ASSET);
    da_init(&state->assets);
    return true;
}

void asset_system_shutdown() {
    da_free(&state->assets);
    rl_arena_destroy(&state->asset_arena);
    state = nullptr;
}

b8 asset_system_load_all() {
    // Show loading splash & perform loading
    rl_thread thread_splash;
    platform_thread_create(splash_run, nullptr, &thread_splash);

    RL_DEBUG("Loading assets...");
    for (u32 i = 0; i < ASSET_TABLE_TOTAL; i++) {
        asset_system_load(&asset_table[i]);
    }

    // Wait for loading to be finished
    platform_thread_join(&thread_splash);

    return true;
}

b8 asset_system_load(rl_asset *asset) {
    b8 success = false;
    switch (asset->type) {
    case ASSET_FONT:
        success = rl_font_init(&state->asset_arena, asset);
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

    return nullptr;
}

const char *get_assets_dir(ASSET_TYPE asset_type) {
    switch (asset_type) {
    case ASSET_FONT:
        return "../../../assets/fonts/";
    case ASSET_SHADER:
        return "../../../assets/shaders/";
    case ASSET_TEXTURE:
        return "../../../assets/textures/";
    default:
        break;
    }

    return "../../../assets/";
}