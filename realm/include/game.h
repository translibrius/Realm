#pragma once

#include "core/camera.h"
#include "defines.h"
#include "memory/arena.h"
#include <realm_app_api.h>

typedef struct rl_font rl_font;

#define RL_GAME_STATE_VERSION 3
#define RL_GAME_MAX_TOASTS 6
#define RL_GAME_TOAST_TEXT_MAX 96

typedef struct rl_game_toast {
    char message[RL_GAME_TOAST_TEXT_MAX];
    f32 ttl_seconds;
    realm_app_toast_type type;
    b8 active;
} rl_game_toast;

typedef struct rl_game {
    u32 version;
    rl_camera camera;
    rl_arena frame_arena;
    rl_font *font_jetbrains;
    const realm_app_context *app_context;

    b8 paused;
    b8 focused;
    b8 input_captured;
    f32 scene_angle;
    f32 time_elapsed;
    rl_game_toast toasts[RL_GAME_MAX_TOASTS];
} rl_game;

b8 game_init(rl_game *game, const realm_app_context *ctx);
void game_update(rl_game *game, f64 dt);
void game_render(rl_game *game, f64 dt);
void game_destroy(rl_game *game);
void game_on_resize(rl_game *game, f32 width, f32 height);
void game_set_paused(rl_game *game, b8 paused);
void game_set_focused(rl_game *game, b8 focused);
void game_push_toast(rl_game *game, realm_app_toast_type type, const char *message);
