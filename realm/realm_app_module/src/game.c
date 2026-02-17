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
        game->scene_angle = 0.0f;
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

    if (!state_compatible) {
        game_set_paused(game, false);
    } else {
        game_apply_input_capture(game);
    }

    return true;
}

void game_update(rl_game *game, f64 dt) {
    if (!game) {
        return;
    }

    game->scene_angle += 100.0f * (f32)dt;
    if (game->scene_angle > 360.0f) {
        game->scene_angle -= 360.0f;
    }

    if (game->paused) {
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

    rl_string fps = rl_string_format(&game->frame_arena, "FPS: %d", rl_engine_get_stats().fps);

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
            .color = {0.0f, 1.0f, 1.0f},
        },
    };

    rl_frame_mesh frame_meshes[SCENE_MESH_COUNT] = {0};
    u32 mesh_index = 0;

    rl_frame_mesh *rotating_cube = &frame_meshes[mesh_index++];
    rotating_cube->primitive = RL_FRAME_PRIMITIVE_CUBE;
    rotating_cube->kind = RL_FRAME_MESH_KIND_LIT;
    rotating_cube->color[0] = 1.0f;
    rotating_cube->color[1] = 0.5f;
    rotating_cube->color[2] = 0.31f;
    rotating_cube->wireframe = false;
    glm_mat4_identity(rotating_cube->model);
    glm_rotate(rotating_cube->model, glm_rad(game->scene_angle), (vec3){0.5f, 1.0f, 0.0f});

    for (i32 x = FLOOR_X_MIN; x <= FLOOR_X_MAX; x++) {
        for (i32 z = FLOOR_Z_MIN; z <= FLOOR_Z_MAX; z++) {
            rl_frame_mesh *floor_tile = &frame_meshes[mesh_index++];
            floor_tile->primitive = RL_FRAME_PRIMITIVE_CUBE;
            floor_tile->kind = RL_FRAME_MESH_KIND_LIT;
            floor_tile->color[0] = 1.0f;
            floor_tile->color[1] = 0.5f;
            floor_tile->color[2] = 0.31f;
            floor_tile->wireframe = true;
            glm_mat4_identity(floor_tile->model);
            glm_translate(floor_tile->model, (vec3){(f32)x, -2.0f, (f32)z});
        }
    }

    rl_frame_mesh *light_cube = &frame_meshes[mesh_index++];
    light_cube->primitive = RL_FRAME_PRIMITIVE_CUBE;
    light_cube->kind = RL_FRAME_MESH_KIND_UNLIT;
    light_cube->color[0] = 1.0f;
    light_cube->color[1] = 1.0f;
    light_cube->color[2] = 1.0f;
    light_cube->wireframe = false;
    glm_mat4_identity(light_cube->model);
    glm_translate(light_cube->model, frame_lights[0].position);
    glm_scale(light_cube->model, (vec3){0.2f, 0.2f, 0.2f});

    rl_frame_text frame_texts[1] = {
        {
            .text = fps.cstr,
            .font = game->font_jetbrains,
            .size_px = 20.0f,
            .x = 0.0f,
            .y = 0.0f,
            .color = {1.0f, 1.0f, 1.0f, 1.0f},
        },
    };

    rl_frame_data frame_data = {0};
    frame_data.camera.valid = true;
    glm_mat4_copy(view, frame_data.camera.view);
    glm_mat4_copy(proj, frame_data.camera.projection);
    glm_vec3_copy(game->camera.pos, frame_data.camera.position);
    frame_data.meshes = frame_meshes;
    frame_data.mesh_count = mesh_index;
    frame_data.point_lights = frame_lights;
    frame_data.point_light_count = 1;
    frame_data.texts = frame_texts;
    frame_data.text_count = 1;

    renderer_submit_frame_data(&frame_data);

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
