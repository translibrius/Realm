#pragma once

#include "core/camera.h"
#include "defines.h"
#include "memory/arena.h"
#include <realm_app_api.h>

typedef struct rl_font rl_font;

#define RL_GAME_STATE_VERSION 4

typedef struct rl_game {
    u32 version;
    rl_camera camera;
    rl_arena frame_arena;
    rl_font *font_jetbrains;
    const realm_app_context *app_context;

    f32 scene_angle;
    f32 time_elapsed;
} rl_game;

b8 game_init(rl_game *game, const realm_app_context *ctx);
void game_update(rl_game *game, const realm_app_context *ctx, f64 dt);
void game_render(rl_game *game, f64 dt);
void game_destroy(rl_game *game);
