#pragma once

#include "core/camera.h"
#include "defines.h"
#include "memory/arena.h"
#include <realm_app_api.h>

typedef struct rl_font rl_font;

typedef struct rl_game_cfg {
    b8 vsync;
    RENDERER_BACKEND renderer_backend;
    i32 width;
    i32 height;
    i32 x, y;
} rl_game_cfg;

typedef struct rl_game {
    rl_game_cfg config;
    rl_camera camera;
    rl_arena frame_arena;
    rl_font *font_jetbrains;
    const realm_app_context *app_context;
} rl_game;

b8 game_init(rl_game *game, const realm_app_context *ctx, rl_game_cfg config);
void game_update(rl_game *game, f64 dt);
void game_render(rl_game *game, f64 dt);
void game_destroy(rl_game *game);
void game_on_resize(rl_game *game, f32 width, f32 height);
