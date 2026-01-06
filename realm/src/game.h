#pragma once

#include "defines.h"
#include "core/camera.h"

typedef struct game {
    rl_camera camera;
    f32 width, height;
} game;

void game_init(game *game_out, f32 width, f32 height);
void game_update(game *game_inst, f64 dt);
void game_render(game *game_inst, f64 dt);
void game_destroy(game *game_inst);