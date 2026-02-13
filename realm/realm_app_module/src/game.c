#include "../../include/game.h"

#include "asset/asset.h"
#include "core/logger.h"
#include "engine.h"
#include "platform/platform.h"
#include "renderer/renderer_frontend.h"
#include "util/str.h"

static void game_apply_input_capture(rl_game *game);

b8 game_init(rl_game *game, const realm_app_context *ctx, rl_game_cfg config) {
    if (!game) {
        return false;
    }

    b8 state_compatible = game->version == RL_GAME_STATE_VERSION;

    if (!state_compatible) {
        game->version = RL_GAME_STATE_VERSION;
        game->paused = true;
        game->focused = true;
        game->input_captured = false;
        camera_init(&game->camera);
    }

    game->app_context = ctx;
    game->config = config;
    rl_arena_init(&game->frame_arena, KiB(4024), KiB(1024), MEM_ARENA);

    rl_asset *asset = get_asset_by_id(ASSET_ID_FONT_JETBRAINS_MONO_REGULAR);
    game->font_jetbrains = asset ? asset->handle : nullptr;
    if (!game->font_jetbrains) {
        const char *filename = asset ? asset->filename : "<null>";
        RL_ERROR("Failed to load font '%s'", filename);
        return false;
    }
    renderer_set_active_font(game->font_jetbrains);

    if (!state_compatible) {
        game_set_paused(game, false);
    } else {
        game_apply_input_capture(game);
    }

    return true;
}

void game_update(rl_game *game, f64 dt) {
    if (!game || game->paused) {
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
    i32 width = ctx ? ctx->window->settings.width : game->config.width;
    i32 height = ctx ? ctx->window->settings.height : game->config.height;
    f32 aspect = (f32)width / (f32)height;
    camera_get_view(&game->camera, view);
    camera_get_projection(&game->camera, aspect, proj, game->config.renderer_backend);
    renderer_set_view_projection(view, proj, game->camera.pos);

    vec4 color = {1.0f, 1.0f, 1.0f, 1.0f};

    rl_string fps = rl_string_format(&game->frame_arena, "FPSss: %d", rl_engine_get_stats().fps);
    renderer_render_text(fps.cstr, 20, 0, 0, color);

    // Reset frame arena
    rl_arena_clear(&game->frame_arena);
}

void game_destroy(rl_game *game) {
    if (!game) {
        return;
    }
    rl_arena_deinit(&game->frame_arena);
}

void game_set_paused(rl_game *game, b8 paused) {
    if (!game || game->paused == paused) {
        return;
    }
    game->paused = paused;
    game_apply_input_capture(game);
}

void game_set_focused(rl_game *game, b8 focused) {
    if (!game || game->focused == focused) {
        return;
    }
    game->focused = focused;
    game_apply_input_capture(game);
}

static void game_apply_input_capture(rl_game *game) {
    if (!game || !game->app_context || !game->app_context->window) {
        return;
    }

    b8 should_capture = game->focused && !game->paused;
    if (should_capture == game->input_captured) {
        return;
    }

    platform_set_cursor_mode(game->app_context->window,
                             should_capture ? CURSOR_MODE_HIDDEN : CURSOR_MODE_NORMAL);
    platform_set_raw_input(game->app_context->window, should_capture);

    game->input_captured = should_capture;
}
