#include "../../include/game.h"

#include "asset/asset.h"
#include "core/logger.h"
#include "engine.h"
#include "memory/memory.h"
#include "platform/platform.h"
#include "renderer/renderer_frontend.h"
#include "util/str.h"

#include <stdio.h>

static void game_apply_input_capture(rl_game *game);
static const char *game_toast_type_tag(realm_app_toast_type type);
static void game_toast_color(realm_app_toast_type type, vec4 out_color);

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
        game->time_elapsed = 0.0f;
        mem_zero(game->toasts, sizeof(game->toasts));
        camera_init(&game->camera);
    }

    game->app_context = ctx;
    game->config = config;

    // Always reset input_captured so game_apply_input_capture() re-registers
    // raw input with the (possibly new) window after a backend switch or reload.
    game->input_captured = false;

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

    for (u32 i = 0; i < RL_GAME_MAX_TOASTS; i++) {
        rl_game_toast *toast = &game->toasts[i];
        if (!toast->active) {
            continue;
        }

        toast->ttl_seconds -= (f32)dt;
        if (toast->ttl_seconds <= 0.0f) {
            toast->active = false;
        }
    }

    u32 write_index = 0;
    for (u32 read_index = 0; read_index < RL_GAME_MAX_TOASTS; read_index++) {
        if (!game->toasts[read_index].active) {
            continue;
        }

        if (write_index != read_index) {
            game->toasts[write_index] = game->toasts[read_index];
        }
        write_index++;
    }
    for (u32 i = write_index; i < RL_GAME_MAX_TOASTS; i++) {
        game->toasts[i].active = false;
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

    u32 active_toast_count = 0;
    for (u32 i = 0; i < RL_GAME_MAX_TOASTS; i++) {
        if (game->toasts[i].active) {
            active_toast_count++;
        }
    }

    u32 max_text_count = 1 + active_toast_count;
    rl_frame_text *frame_texts = rl_arena_push_aligned(&game->frame_arena, sizeof(rl_frame_text) * max_text_count, _Alignof(rl_frame_text), true);
    u32 text_index = 0;

    frame_texts[text_index++] = (rl_frame_text){
        .text = fps.cstr,
        .font = game->font_jetbrains,
        .size_px = 20.0f,
        .x = 0.0f,
        .y = 0.0f,
        .color = {1.0f, 1.0f, 1.0f, 1.0f},
    };

    for (u32 i = 0; i < RL_GAME_MAX_TOASTS; i++) {
        const rl_game_toast *toast = &game->toasts[i];
        if (!toast->active) {
            continue;
        }

        rl_string toast_text = rl_string_format(&game->frame_arena, "[%s] %s", game_toast_type_tag(toast->type), toast->message);
        vec4 toast_color = {0};
        game_toast_color(toast->type, toast_color);

        frame_texts[text_index++] = (rl_frame_text){
            .text = toast_text.cstr,
            .font = game->font_jetbrains,
            .size_px = 16.0f,
            .x = 0.0f,
            .y = 28.0f + ((f32)i * 20.0f),
            .color = {toast_color[0], toast_color[1], toast_color[2], toast_color[3]},
        };
    }

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
    frame_data.text_count = text_index;

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

void game_push_toast(rl_game *game, realm_app_toast_type type, const char *message) {
    if (!game || !message || !message[0]) {
        return;
    }

    for (u32 i = RL_GAME_MAX_TOASTS - 1; i > 0; i--) {
        game->toasts[i] = game->toasts[i - 1];
    }

    rl_game_toast *toast = &game->toasts[0];
    mem_zero(toast, sizeof(*toast));
    snprintf(toast->message, sizeof(toast->message), "%s", message);
    toast->type = type;
    toast->active = true;

    switch (type) {
    case REALM_APP_TOAST_WARNING:
        toast->ttl_seconds = 3.5f;
        break;
    case REALM_APP_TOAST_ERROR:
        toast->ttl_seconds = 5.0f;
        break;
    case REALM_APP_TOAST_INFO:
    default:
        toast->ttl_seconds = 2.5f;
        break;
    }
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

static const char *game_toast_type_tag(realm_app_toast_type type) {
    switch (type) {
    case REALM_APP_TOAST_WARNING:
        return "WARN";
    case REALM_APP_TOAST_ERROR:
        return "ERROR";
    case REALM_APP_TOAST_INFO:
    default:
        return "INFO";
    }
}

static void game_toast_color(realm_app_toast_type type, vec4 out_color) {
    if (!out_color) {
        return;
    }

    switch (type) {
    case REALM_APP_TOAST_WARNING:
        out_color[0] = 1.0f;
        out_color[1] = 0.75f;
        out_color[2] = 0.2f;
        out_color[3] = 1.0f;
        break;
    case REALM_APP_TOAST_ERROR:
        out_color[0] = 1.0f;
        out_color[1] = 0.35f;
        out_color[2] = 0.35f;
        out_color[3] = 1.0f;
        break;
    case REALM_APP_TOAST_INFO:
    default:
        out_color[0] = 0.5f;
        out_color[1] = 0.9f;
        out_color[2] = 1.0f;
        out_color[3] = 1.0f;
        break;
    }
}
