#include "../../include/game.h"
#include "../../include/scene_main_menu.h"
#include "../../include/scene_game.h"

#include "asset/asset.h"
#include "core/behavior.h"
#include "core/camera.h"
#include "core/component.h"
#include "core/logger.h"
#include "core/scene.h"
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
        game->time_elapsed = 0.0f;
        game->settings_backend_dropdown = (gui_dropdown_state){.selected = -1};
        camera_init(&game->camera);
        game->settings_window_mode_dropdown = (gui_dropdown_state){.selected = -1};
        game->settings_fov_slider = (gui_slider_state){0};
        game->settings_sensitivity_slider = (gui_slider_state){0};
    }

    // Init camera from scene entity if available, otherwise use defaults
    if (ctx->scene) {
        rl_entity cam_e = scene_get_main_camera(ctx->scene);
        if (cam_e != RL_ENTITY_INVALID) {
            rl_transform *ct = transform_get(&ctx->scene->components, cam_e);
            rl_camera_component *cc = camera_comp_get(&ctx->scene->components, cam_e);
            if (ct && cc) camera_from_entity(&game->camera, ct, cc);
        }
    }

    // Apply persisted config values to camera
    game->camera.look_speed = ctx->mouse_sensitivity;

    game->app_context = ctx;

    rl_arena_init(&game->frame_arena, KiB(4024), KiB(1024), MEM_ARENA);

    asset_id font_id = asset_find(RL_ASSET_FONT_JETBRAINS_MONO);
    rl_asset *asset = asset_get(font_id);
    game->font_jetbrains = asset ? asset->data : nullptr;
    if (!game->font_jetbrains) {
        const char *filename = asset ? asset->filename : "<null>";
        RL_ERROR("Failed to load font '%s'", filename);
        return false;
    }

    // Register behavior functions (handles both first load and hot-reload)
    behavior_registry_clear();
    scene_game_register_behaviors();

    return true;
}

void game_update(rl_game *game, const realm_app_context *ctx, realm_app_cmd_queue *cmds, f64 dt) {
    if (!game) {
        return;
    }

    // Sync camera with config (handles real-time slider changes from settings menu)
    game->camera.fov = ctx->fov;
    game->camera.look_speed = ctx->mouse_sensitivity;

    // Process deferred scene transition
    if (game->pending_scene != game->active_scene) {
        game->active_scene = game->pending_scene;
        game->pause_menu_open = false;
        game->settings_open = false;

        if (game->active_scene == SCENE_GAME && ctx->scene) {
            rl_entity cam_e = scene_get_main_camera(ctx->scene);
            if (cam_e != RL_ENTITY_INVALID) {
                rl_transform *ct = transform_get(&ctx->scene->components, cam_e);
                rl_camera_component *cc = camera_comp_get(&ctx->scene->components, cam_e);
                if (ct && cc) camera_from_entity(&game->camera, ct, cc);
            } else {
                camera_init(&game->camera);
            }
            game->camera.look_speed = ctx->mouse_sensitivity;
        }
    }

    switch (game->active_scene) {
        case SCENE_MAIN_MENU:
            scene_main_menu_update(game, ctx, cmds, dt);
            break;
        case SCENE_GAME:
            scene_game_update(game, ctx, cmds, dt);
            break;
        default:
            break;
    }
}

void game_render(rl_game *game, const realm_app_context *ctx, realm_app_cmd_queue *cmds) {
    if (!game) {
        return;
    }

    switch (game->active_scene) {
        case SCENE_MAIN_MENU:
            scene_main_menu_render(game, ctx, cmds);
            break;
        case SCENE_GAME:
            scene_game_render(game, ctx, cmds);
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
