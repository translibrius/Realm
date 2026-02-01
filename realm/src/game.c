#include "game.h"

#include "asset/asset.h"
#include "core/event.h"
#include "core/logger.h"
#include "engine.h"
#include "profiler/profiler.h"
#include "renderer/renderer_frontend.h"

b8 game_init(rl_game *game, rl_game_cfg config) {
    game->config = config;
    camera_init(&game->camera);

    rl_arena_init(&game->frame_arena, KiB(4024), KiB(1024), MEM_ARENA);

    rl_asset *asset = get_asset("JetBrainsMono-Regular.ttf");
    game->font_jetbrains = asset->handle;
    if (game->font_jetbrains == nullptr) {
        RL_ERROR("Failed to load font '%s'", asset->filename);
        return false;
    }
    renderer_set_active_font(game->font_jetbrains);

    return true;
}

void game_update(rl_game *game, f64 dt) {
    RL_PROFILE_ZONE(update_zone, "game_update");
    camera_update(&game->camera, dt);
    RL_PROFILE_ZONE_END(update_zone);
}

void game_render(rl_game *game, f64 dt) {
    RL_PROFILE_ZONE(render_zone, "game_render");
    mat4 view = {};
    mat4 proj = {};

    f32 aspect = (f32)game->config.width / (f32)game->config.height;
    camera_get_view(&game->camera, view);
    camera_get_projection(&game->camera, aspect, proj, game->config.renderer_backend);
    renderer_set_view_projection(view, proj, game->camera.pos);

    // engine_stats stats = engine_get_stats();

    // rl_string fps_str = rl_string_format(&game->frame_arena, "FPS: %u", stats.fps);
    // renderer_render_text(fps_str.cstr, 40, game->width / 2 - 100, game->height - 40, (vec4){1.0f, 1.0f, 1.0f, 1.0f});

    // RL_INFO("FPS: %u", stats.fps);

    // Reset frame arena
    rl_arena_clear(&game->frame_arena);
    RL_PROFILE_ZONE_END(render_zone);
}

void game_destroy(rl_game *game) {
    rl_arena_deinit(&game->frame_arena);
}
