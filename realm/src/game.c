#include "game.h"

#include "renderer/renderer_frontend.h"

void game_init(game *game_out, f32 width, f32 height) {
    camera_init(&game_out->camera);
    game_out->width = width;
    game_out->height = height;
}

void game_update(game *game_inst, f64 dt) {
    camera_update(&game_inst->camera, dt);
}

void game_render(game *game_inst, f64 dt) {
    mat4 view = {};
    mat4 proj = {};

    f32 aspect = game_inst->width / game_inst->height;
    camera_get_view(&game_inst->camera, view);
    camera_get_projection(&game_inst->camera, aspect, proj);
    renderer_set_view_projection(view, proj);

    //rl_string fps_str = rl_string_format(&state.frame_arena, "FPS: %u", fps_display);
    //renderer_render_text(fps_str.cstr, 40, (f32)state.render_window->settings.width / 2 - 100, (f32)state.render_window->settings.height - 40, (vec4){1.0f, 1.0f, 1.0f, 1.0f});
}

void game_destroy(game *game_inst) {

}