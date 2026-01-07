#pragma once

#include "defines.h"
#include "asset/font.h"
#include "core/camera.h"
#include "memory/arena.h"
#include "platform/platform.h"

typedef struct game {
    rl_camera camera;
    f32 width, height;
    platform_window *window;
    rl_arena frame_arena;

    rl_font *font_jetbrains;
} game;

b8 game_init(game *game_out, platform_window *window);
void game_update(game *game_inst, f64 dt);
void game_render(game *game_inst, f64 dt);
void game_destroy(game *game_inst);
void game_on_resize(game *game_inst, f32 width, f32 height);