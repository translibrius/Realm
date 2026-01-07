#include "game.h"

#include "engine.h"
#include "core/event.h"
#include "renderer/renderer_frontend.h"
#include "profiler/profiler.h"

b8 resize_callback_game(void *event, void *data) {
    platform_window *window = event;
    game *game_inst = data;

    if (window->id == game_inst->window->id) {
        game_inst->width = (f32)window->settings.width;
        game_inst->height = (f32)window->settings.height;
    }

    return false;
}

b8 game_init(game *game_out, platform_window *window) {
    camera_init(&game_out->camera);
    game_out->width = (f32)window->settings.width;
    game_out->height = (f32)window->settings.height;
    game_out->window = window;
    rl_arena_create(KiB(1024), &game_out->frame_arena, MEM_ARENA);

    rl_asset *asset = get_asset("JetBrainsMono-Regular.ttf");
    game_out->font_jetbrains = asset->handle;
    if (game_out->font_jetbrains == nullptr) {
        RL_ERROR("game_init(): Failed to load font '%s'", asset->filename);
        return false;
    }
    renderer_set_active_font(game_out->font_jetbrains);

    event_register(EVENT_WINDOW_RESIZE, resize_callback_game, game_out);
    return true;
}

void game_update(game *game_inst, f64 dt) {
    TracyCZoneN(ctx, "game_update", true);
    camera_update(&game_inst->camera, dt);
    TracyCZoneEnd(ctx);
}

void game_render(game *game_inst, f64 dt) {
    TracyCZoneN(ctx, "game_render", true);
    mat4 view = {};
    mat4 proj = {};

    f32 aspect = game_inst->width / game_inst->height;
    camera_get_view(&game_inst->camera, view);
    camera_get_projection(&game_inst->camera, aspect, proj);
    renderer_set_view_projection(view, proj);

    engine_stats stats = engine_get_stats();

    rl_string fps_str = rl_string_format(&game_inst->frame_arena, "FPS: %u", stats.fps);
    renderer_render_text(fps_str.cstr, 40, game_inst->width / 2 - 100, game_inst->height - 40, (vec4){1.0f, 1.0f, 1.0f, 1.0f});

    //RL_DEBUG("FPS: %u", stats.fps);

    // Reset frame arena
    rl_arena_reset(&game_inst->frame_arena);
    TracyCZoneEnd(ctx);
}

void game_destroy(game *game_inst) {
    rl_arena_destroy(&game_inst->frame_arena);
}