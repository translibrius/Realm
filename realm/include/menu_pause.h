#pragma once

#include <realm_app_api.h>

typedef struct rl_game rl_game;

void menu_pause_render(rl_game *game, const realm_app_context *ctx, realm_app_output *out);
