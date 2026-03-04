#include "../../include/game.h"
#include "../../include/scene_main_menu.h"
#include "../../include/scene_game.h"

#include "asset/asset.h"
#include "core/camera.h"
#include "core/logger.h"
#include "memory/memory.h"

b8 game_init(rl_game *game, const realm_app_context *ctx) {
    if (!game) {
        return false;
    }

    b8 state_compatible = game->version == RL_GAME_STATE_VERSION;

    if (!state_compatible) {
        game->version = RL_GAME_STATE_VERSION;
        game->active_scene = SCENE_MAIN_MENU;
        game->pending_scene = SCENE_MAIN_MENU;
        game->pause_menu_open = false;
        game->settings_open = false;
        game->pause_freezes_sim = true;
        game->scene_angle = 0.0f;
        game->time_elapsed = 0.0f;
        game->settings_backend_dropdown = (gui_dropdown_state){.selected = -1};
        camera_init(&game->camera);
        game->settings_window_mode_dropdown = (gui_dropdown_state){.selected = -1};
        game->settings_fov_slider = (gui_slider_state){0};
        game->settings_sensitivity_slider = (gui_slider_state){0};
    }

    // Apply persisted config values to camera
    game->camera.fov = ctx->fov;
    game->camera.look_speed = ctx->mouse_sensitivity;

    game->app_context = ctx;

    rl_arena_init(&game->frame_arena, KiB(4024), KiB(1024), MEM_ARENA);

    rl_asset *asset = get_asset_by_id(ASSET_ID_FONT_JETBRAINS_MONO_REGULAR);
    game->font_jetbrains = asset ? asset->handle : nullptr;
    if (!game->font_jetbrains) {
        const char *filename = asset ? asset->filename : "<null>";
        RL_ERROR("Failed to load font '%s'", filename);
        return false;
    }

    return true;
}

void game_update(rl_game *game, const realm_app_context *ctx, realm_app_output *out, f64 dt) {
    if (!game) {
        return;
    }

    // Sync camera with config (handles real-time slider changes)
    game->camera.fov = ctx->fov;
    game->camera.look_speed = ctx->mouse_sensitivity;

    // Process deferred scene transition
    if (game->pending_scene != game->active_scene) {
        game->active_scene = game->pending_scene;
        game->pause_menu_open = false;
        game->settings_open = false;

        if (game->active_scene == SCENE_GAME) {
            camera_init(&game->camera);
            game->camera.fov = ctx->fov;
            game->camera.look_speed = ctx->mouse_sensitivity;
        }
    }

    switch (game->active_scene) {
        case SCENE_MAIN_MENU:
            scene_main_menu_update(game, ctx, out, dt);
            break;
        case SCENE_GAME:
            scene_game_update(game, ctx, out, dt);
            break;
        default:
            break;
    }
}

void game_render(rl_game *game, const realm_app_context *ctx, realm_app_output *out) {
    if (!game) {
        return;
    }

    switch (game->active_scene) {
        case SCENE_MAIN_MENU:
            scene_main_menu_render(game, ctx, out);
            break;
        case SCENE_GAME:
            scene_game_render(game, ctx, out);
            break;
        default:
            break;
    }

    rl_arena_clear(&game->frame_arena);
}

void game_destroy(rl_game *game) {
    if (!game) {
        return;
    }
    rl_arena_deinit(&game->frame_arena);
}
