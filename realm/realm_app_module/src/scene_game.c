#include "../../include/scene_game.h"
#include "../../include/game.h"
#include "../../include/menu_pause.h"

#include "core/camera.h"
#include "core/component.h"
#include "core/scene.h"
#include "platform/input.h"
#include "renderer/renderer_frontend.h"

void scene_game_update(rl_game *game, const realm_app_context *ctx, realm_app_output *out, f64 dt) {
    (void)ctx;

    if (input_key_pressed(KEY_ESCAPE)) {
        if (game->settings_open) {
            game->settings_open = false;
        } else {
            game->pause_menu_open = !game->pause_menu_open;
        }
    }

    out->show_debug_panel = true;

    b8 paused = game->pause_menu_open || !ctx->focused;

    if (paused) {
        out->wants_cursor_visible = true;
        if (!game->pause_freezes_sim) {
            game->scene_angle += 100.0f * (f32)dt;
            if (game->scene_angle > 360.0f) {
                game->scene_angle -= 360.0f;
            }
        }
        return;
    }

    out->wants_cursor_visible = false;
    game->scene_angle += 100.0f * (f32)dt;
    if (game->scene_angle > 360.0f) {
        game->scene_angle -= 360.0f;
    }

    // Update rotating cube transform from scene_angle
    if (ctx->scene && game->rotating_cube_entity != RL_ENTITY_INVALID) {
        rl_transform *t = transform_get(&ctx->scene->components, game->rotating_cube_entity);
        if (t) {
            t->rotation[1] = game->scene_angle;
            t->dirty = true;
        }
    }

    camera_update(&game->camera, dt);
}

void scene_game_render(rl_game *game, const realm_app_context *ctx, realm_app_output *out) {
    if (!ctx->scene) {
        if (game->pause_menu_open) {
            menu_pause_render(game, ctx, out);
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
        menu_pause_render(game, ctx, out);
    }
}
