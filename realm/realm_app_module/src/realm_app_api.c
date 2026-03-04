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

void realm_app_update(void *state, const realm_app_context *ctx, realm_app_output *out, f64 dt) {
    rl_game *game = (rl_game *)state;
    game_update(game, ctx, out, dt);
}

void realm_app_render(void *state, const realm_app_context *ctx, realm_app_output *out) {
    rl_game *game = (rl_game *)state;
    game_render(game, ctx, out);
}

void realm_app_shutdown(void *state, const realm_app_context *ctx) {
    (void)ctx;
    rl_game *game = (rl_game *)state;
    game_destroy(game);
}
