#pragma once

#include <realm_app_api.h>

typedef struct rl_game rl_game;

void scene_game_register_behaviors(void);
void scene_game_update(rl_game *game, const realm_app_context *ctx, realm_app_output *out, f64 dt);
void scene_game_render(rl_game *game, const realm_app_context *ctx, realm_app_output *out);
