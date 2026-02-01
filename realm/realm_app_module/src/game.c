#include "game.h"

#include "asset/asset.h"
#include "core/logger.h"
#include "renderer/renderer_frontend.h"

b8 game_init(rl_game *game, const realm_app_context *ctx, rl_game_cfg config) {
    if (!game) {
        return false;
    }

    game->app_context = ctx;
    game->config = config;
    camera_init(&game->camera);
    rl_arena_init(&game->frame_arena, KiB(4024), KiB(1024), MEM_ARENA);

    rl_asset *asset = get_asset("JetBrainsMono-Regular.ttf");
    game->font_jetbrains = asset ? asset->handle : nullptr;
    if (!game->font_jetbrains) {
        const char *filename = asset ? asset->filename : "<null>";
        RL_ERROR("Failed to load font '%s'", filename);
        return false;
    }
    renderer_set_active_font(game->font_jetbrains);

    return true;
}

void game_update(rl_game *game, f64 dt) {
    if (!game) {
        return;
    }
    camera_update(&game->camera, dt);
}

void game_render(rl_game *game, f64 dt) {
    (void)dt;
    if (!game) {
        return;
    }

    mat4 view = {};
    mat4 proj = {};

    const realm_app_context *ctx = game->app_context;
    i32 width = ctx ? ctx->width : game->config.width;
    i32 height = ctx ? ctx->height : game->config.height;
    f32 aspect = (f32)width / (f32)height;
    camera_get_view(&game->camera, view);
    camera_get_projection(&game->camera, aspect, proj, game->config.renderer_backend);
    renderer_set_view_projection(view, proj, game->camera.pos);

    // Reset frame arena
    rl_arena_clear(&game->frame_arena);
}

void game_destroy(rl_game *game) {
    if (!game) {
        return;
    }
    rl_arena_deinit(&game->frame_arena);
}
