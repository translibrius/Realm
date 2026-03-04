#pragma once

#include "core/camera.h"
#include "defines.h"
#include "gui/gui_dropdown.h"
#include "memory/arena.h"
#include <realm_app_api.h>

typedef struct rl_font rl_font;

#define RL_GAME_STATE_VERSION 5

typedef enum game_scene {
    SCENE_MAIN_MENU,
    SCENE_GAME,
    SCENE_COUNT,
} game_scene;

typedef struct rl_game {
    u32 version;

    // Scene management
    game_scene active_scene;
    game_scene pending_scene;
    b8 pause_menu_open;
    b8 settings_open;

    // Game scene state
    rl_camera camera;
    f32 scene_angle;
    f32 time_elapsed;

    // Settings widget state
    gui_dropdown_state settings_backend_dropdown;

    // Shared
    rl_arena frame_arena;
    rl_font *font_jetbrains;
    const realm_app_context *app_context;
} rl_game;

b8 game_init(rl_game *game, const realm_app_context *ctx);
void game_update(rl_game *game, const realm_app_context *ctx, realm_app_output *out, f64 dt);
void game_render(rl_game *game, const realm_app_context *ctx, realm_app_output *out);
void game_destroy(rl_game *game);
