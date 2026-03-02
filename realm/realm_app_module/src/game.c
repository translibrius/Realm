#include "../../include/game.h"

#include "asset/asset.h"
#include "core/logger.h"
#include "engine.h"
#include "gui/gui_clay.h"
#include "memory/memory.h"
#include "renderer/renderer_frontend.h"
#include "util/str.h"

b8 game_init(rl_game *game, const realm_app_context *ctx) {
    if (!game) {
        return false;
    }

    b8 state_compatible = game->version == RL_GAME_STATE_VERSION;

    if (!state_compatible) {
        game->version = RL_GAME_STATE_VERSION;
        game->scene_angle = 0.0f;
        game->time_elapsed = 0.0f;
        camera_init(&game->camera);
    }

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

void game_update(rl_game *game, const realm_app_context *ctx, f64 dt) {
    if (!game) {
        return;
    }

    game->scene_angle += 100.0f * (f32)dt;
    if (game->scene_angle > 360.0f) {
        game->scene_angle -= 360.0f;
    }

    if (ctx->paused) {
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
    i32 width = ctx->window->settings.width;
    i32 height = ctx->window->settings.height;
    f32 aspect = (f32)width / (f32)height;
    camera_get_view(&game->camera, view);
    camera_get_projection(&game->camera, aspect, proj, ctx->renderer_backend);

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

    // GUI overlay
    gui_layout_begin((f32)dt);

    Clay_ElementDeclaration root_decl = {
        .layout = GUI_ROOT_LAYOUT(CLAY_ALIGN_X_RIGHT, CLAY_ALIGN_Y_TOP),
    };
    Clay_ElementDeclaration panel_decl = {
        .layout = {
            .sizing = {.width = CLAY_SIZING_FIXED(220), .height = CLAY_SIZING_FIT(0)},
            .padding = CLAY_PADDING_ALL(12),
            .childGap = 8,
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
        },
        .backgroundColor = GUI_RGBA(30, 30, 30, 200),
    };

    u16 font_jb = gui_font_id(ASSET_ID_FONT_JETBRAINS_MONO_REGULAR);

    CLAY(CLAY_ID("Root"), root_decl) {
        CLAY(CLAY_ID("DebugPanel"), panel_decl) {
            CLAY_TEXT(CLAY_STRING("Debug Panel"), GUI_TEXT_CFG_FONT(GUI_WHITE, 18, font_jb));
            CLAY_TEXT(GUI_STRING(fps), GUI_TEXT_CFG_FONT(GUI_HEX(0xB4FFB4), 14, font_jb));
        }
    }

    gui_layout_end();

    // Reset frame arena
    rl_arena_clear(&game->frame_arena);
}

void game_destroy(rl_game *game) {
    if (!game) {
        return;
    }
    rl_arena_deinit(&game->frame_arena);
}
