#include <realm_app_api.h>

#include "core/logger.h"
#include "game.h"

u32 realm_app_get_api_version(void) {
    return REALM_APP_API_VERSION;
}

u64 realm_app_get_state_size(void) {
    return sizeof(rl_game);
}

void realm_app_init(void *state, const realm_app_context *ctx) {
    rl_game *game = (rl_game *)state;
    rl_game_cfg game_cfg = {
        .vsync = ctx->vsync,
        .renderer_backend = ctx->renderer_backend,
        .width = ctx->width,
        .height = ctx->height,
        .x = ctx->x,
        .y = ctx->y};

    if (!game_init(game, ctx, game_cfg)) {
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
