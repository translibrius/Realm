#include "../../include/scene_game.h"
#include "../../include/game.h"
#include "../../include/menu_pause.h"

#include "asset/asset.h"
#include "core/camera.h"
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

    camera_update(&game->camera, dt);
}

void scene_game_render(rl_game *game, const realm_app_context *ctx, realm_app_output *out) {
    i32 width = ctx->window->settings.width;
    i32 height = ctx->window->settings.height;
    f32 aspect = (f32)width / (f32)height;

    mat4 view = {};
    mat4 proj = {};
    camera_get_view(&game->camera, view);
    camera_get_projection(&game->camera, aspect, proj, ctx->renderer_backend);

    enum {
        FLOOR_X_MIN = -5,
        FLOOR_X_MAX = 5,
        FLOOR_Z_MIN = -5,
        FLOOR_Z_MAX = 5,
        FLOOR_TILE_COUNT = (FLOOR_X_MAX - FLOOR_X_MIN + 1) * (FLOOR_Z_MAX - FLOOR_Z_MIN + 1),
        SCENE_MESH_COUNT = 2 + FLOOR_TILE_COUNT,
    };

    rl_frame_point_light frame_lights[1] = {
        {
            .position = {1.2f, 1.0f, 2.0f},
            .ambient  = {0.2f, 0.2f, 0.2f},
            .diffuse  = {0.5f, 0.5f, 0.5f},
            .specular = {1.0f, 1.0f, 1.0f},
        },
    };

    rl_frame_mesh frame_meshes[SCENE_MESH_COUNT] = {0};
    u32 mesh_index = 0;

    rl_frame_mesh *rotating_cube = &frame_meshes[mesh_index++];
    rotating_cube->primitive = RL_FRAME_PRIMITIVE_CUBE;
    rotating_cube->kind = RL_FRAME_MESH_KIND_LIT;
    rotating_cube->material = (rl_material){
        .diffuse_map = ASSET_ID_TEXTURE_WOOD_CONTAINER2,
        .specular = {0.5f, 0.5f, 0.5f},
        .shininess = 32.0f,
    };
    rotating_cube->wireframe = false;
    glm_mat4_identity(rotating_cube->model);
    glm_rotate(rotating_cube->model, glm_rad(game->scene_angle), (vec3){0.5f, 1.0f, 0.0f});

    for (i32 x = FLOOR_X_MIN; x <= FLOOR_X_MAX; x++) {
        for (i32 z = FLOOR_Z_MIN; z <= FLOOR_Z_MAX; z++) {
            rl_frame_mesh *floor_tile = &frame_meshes[mesh_index++];
            floor_tile->primitive = RL_FRAME_PRIMITIVE_CUBE;
            floor_tile->kind = RL_FRAME_MESH_KIND_LIT;
            floor_tile->material = (rl_material){
                .diffuse_map = ASSET_ID_TEXTURE_WOOD_CONTAINER2,
                .specular = {0.5f, 0.5f, 0.5f},
                .shininess = 32.0f,
            };
            floor_tile->wireframe = false;
            glm_mat4_identity(floor_tile->model);
            glm_translate(floor_tile->model, (vec3){(f32)x, -2.0f, (f32)z});
        }
    }

    rl_frame_mesh *light_cube = &frame_meshes[mesh_index++];
    light_cube->primitive = RL_FRAME_PRIMITIVE_CUBE;
    light_cube->kind = RL_FRAME_MESH_KIND_UNLIT;
    light_cube->material = (rl_material){
        .specular = {0.0f, 0.0f, 0.0f},
        .shininess = 1.0f,
    };
    light_cube->wireframe = false;
    glm_mat4_identity(light_cube->model);
    glm_translate(light_cube->model, frame_lights[0].position);
    glm_scale(light_cube->model, (vec3){0.2f, 0.2f, 0.2f});

    rl_frame_data frame_data = {0};
    frame_data.camera.valid = true;
    glm_mat4_copy(view, frame_data.camera.view);
    glm_mat4_copy(proj, frame_data.camera.projection);
    glm_vec3_copy(game->camera.pos, frame_data.camera.position);
    frame_data.meshes = frame_meshes;
    frame_data.mesh_count = mesh_index;
    frame_data.point_lights = frame_lights;
    frame_data.point_light_count = 1;
    frame_data.texts = nullptr;
    frame_data.text_count = 0;

    renderer_submit_frame_data(&frame_data);

    // Render pause overlay on top of 3D scene
    if (game->pause_menu_open) {
        menu_pause_render(game, ctx, out);
    }
}
