#include "asset/asset.h"
#include "asset/asset_table.h"

#include "asset/font.h"
#include "core/event.h"
#include "core/logger.h"
#include "platform/thread.h"
#include "platform/splash/splash.h"
#include "vendor/glad/glad_wgl.h"

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
    rl_arena_create(MiB(128), &state->asset_arena);
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
    switch (asset->type) {
    case ASSET_FONT:
        rl_asset_font main_font;
        rl_font_init(asset->filename, &main_font);
        asset->handle = &main_font;
        break;
    }

    da_append(&state->assets, *asset);
    event_fire(EVENT_SPLASH_INCREMENT, nullptr);
    return true;
}

const char *get_assets_dir(ASSET_TYPE asset_type) {
    switch (asset_type) {
    case ASSET_FONT:
        return "../../../assets/fonts/";
    }

    return "../../../assets/";
}