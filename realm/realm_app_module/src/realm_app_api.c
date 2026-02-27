#include <realm_app_api.h>

#include "../../include/game.h"
#include "core/logger.h"

u32 realm_app_get_api_version(void) {
    return REALM_APP_API_VERSION;
}

u64 realm_app_get_state_size(void) {
    return sizeof(rl_game);
}

u32 realm_app_get_state_version(void) {
    return RL_GAME_STATE_VERSION;
}

void realm_app_init(void *state, const realm_app_context *ctx) {
    rl_game *game = (rl_game *)state;

    if (!game_init(game, ctx)) {
        RL_ERROR("failed to initialize game instance");
    }
}

void realm_app_update(void *state, const realm_app_context *ctx, f64 dt) {
    (void)ctx;
    rl_game *game = (rl_game *)state;
    game_update(game, dt);
}

void realm_app_render(void *state, const realm_app_context *ctx) {
    (void)ctx;
    rl_game *game = (rl_game *)state;
    game_render(game, 0.0);
}

void realm_app_shutdown(void *state, const realm_app_context *ctx) {
    (void)ctx;
    rl_game *game = (rl_game *)state;
    game_destroy(game);
}

void realm_app_set_paused(void *state, b8 paused) {
    rl_game *game = (rl_game *)state;
    game_set_paused(game, paused);
}

void realm_app_set_focused(void *state, b8 focused) {
    rl_game *game = (rl_game *)state;
    game_set_focused(game, focused);
}

void realm_app_push_toast(void *state, realm_app_toast_type type, const char *message) {
    rl_game *game = (rl_game *)state;
    game_push_toast(game, type, message);
}
