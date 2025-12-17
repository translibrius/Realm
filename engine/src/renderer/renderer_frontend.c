
#include "renderer/renderer_frontend.h"
#include "core/logger.h"
#include "renderer/renderer_types.h"
#include "renderer/opengl/gl_renderer.h"

typedef struct frontend_state {
    b8 initialized;
} frontend_state;

static renderer_interface interface;
static frontend_state state;

// Forward decl
void prepare_interface(RENDERER_BACKEND backend);

b8 renderer_init(RENDERER_BACKEND backend, platform_window *window) {
    prepare_interface(backend);
    if (!interface.initialize(window)) {
        RL_FATAL("Failed to initialize renderer backend");
        return false;
    }

    state.initialized = true;
    return true;
}

void renderer_destroy() {
    if (!state.initialized)
        return;
    interface.shutdown();
}

void renderer_begin_frame(f64 delta_time) {
    if (!state.initialized)
        return;
    interface.begin_frame(delta_time);
}
void renderer_end_frame() {
    if (!state.initialized)
        return;
    interface.end_frame();
}
void renderer_swap_buffers() {
    if (!state.initialized)
        return;
    interface.swap_buffers();
}

void renderer_draw_text(rl_font *font, const char *text, vec2 pos, vec4 color) {
    if (!state.initialized)
        return;
    interface.draw_text(font, text, pos, color);
}

void prepare_interface(RENDERER_BACKEND backend) {
    switch (backend) {
    case BACKEND_OPENGL:
        interface.initialize = &opengl_initialize;
        interface.shutdown = &opengl_destroy;
        interface.begin_frame = &opengl_begin_frame;
        interface.end_frame = &opengl_end_frame;
        interface.swap_buffers = &opengl_swap_buffers;
        interface.draw_text = &opengl_draw_text;
        break;
    }
}
