#include "../../include/scene_game.h"
#include "../../include/game.h"
#include "../../include/menu_pause.h"

#include "core/behavior.h"
#include "core/camera.h"
#include "core/component.h"
#include "core/scene.h"
#include "platform/input.h"
#include "renderer/renderer_frontend.h"

static void rotate_update(rl_scene *scene, rl_entity entity, f32 dt) {
    rl_transform *t = transform_get(&scene->components, entity);
    if (!t) return;

    t->rotation[1] += 100.0f * dt;
    if (t->rotation[1] > 360.0f) {
        t->rotation[1] -= 360.0f;
    }
    t->dirty = true;
}

void scene_game_register_behaviors(void) {
    behavior_register("rotate", rotate_update);
}

void scene_game_update(rl_game *game, const realm_app_context *ctx, realm_app_cmd_queue *cmds, f64 dt) {
    (void)ctx;

    if (input_key_pressed(KEY_ESCAPE)) {
        if (game->settings_open) {
            game->settings_open = false;
        } else {
            game->pause_menu_open = !game->pause_menu_open;
        }
    }

    realm_app_cmd_push(cmds, (realm_app_cmd){.type = REALM_APP_CMD_SHOW_DEBUG_PANEL, .b = true});

    b8 paused = game->pause_menu_open || !ctx->focused;

    if (paused) {
        realm_app_cmd_push(cmds, (realm_app_cmd){.type = REALM_APP_CMD_SET_CURSOR_VISIBLE, .b = true});
        if (!game->pause_freezes_sim && ctx->scene) {
            behavior_update_all(ctx->scene, (f32)dt);
        }
        return;
    }

    realm_app_cmd_push(cmds, (realm_app_cmd){.type = REALM_APP_CMD_SET_CURSOR_VISIBLE, .b = false});
    if (ctx->scene) {
        behavior_update_all(ctx->scene, (f32)dt);
    }

    camera_update(&game->camera, dt);

    // Sync camera state back to the scene camera entity
    if (ctx->scene) {
        rl_entity cam_e = scene_get_main_camera(ctx->scene);
        if (cam_e != RL_ENTITY_INVALID) {
            rl_transform *ct = transform_get(&ctx->scene->components, cam_e);
            if (ct) camera_sync_to_transform(&game->camera, ct);
        }
    }
}

void scene_game_render(rl_game *game, const realm_app_context *ctx, realm_app_cmd_queue *cmds) {
    if (!ctx->scene) {
        if (game->pause_menu_open) {
            menu_pause_render(game, ctx, cmds);
        }
        return;
    }

    i32 width = ctx->window->settings.width;
    i32 height = ctx->window->settings.height;
    f32 aspect = (f32)width / (f32)height;

    mat4 view = {};
    mat4 proj = {};
    camera_get_view(&game->camera, view);
    camera_get_projection(&game->camera, aspect, proj, ctx->renderer_backend);

    rl_frame_camera fc = {.valid = true};
    glm_mat4_copy(view, fc.view);
    glm_mat4_copy(proj, fc.projection);
    glm_vec3_copy(game->camera.pos, fc.position);

    rl_frame_data frame = {0};
    scene_build_frame_data(ctx->scene, &fc, &frame);
    renderer_submit_frame_data(&frame);

    if (game->pause_menu_open) {
        menu_pause_render(game, ctx, cmds);
    }
}
