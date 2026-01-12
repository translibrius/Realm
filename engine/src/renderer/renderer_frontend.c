
#include "renderer/renderer_frontend.h"
#include "core/logger.h"
#include "opengl/gl_text.h"
#include "renderer/renderer_types.h"
#include "renderer/opengl/gl_renderer.h"

#include "vulkan/vk_renderer.h"
#include "vulkan/vk_text.h"

typedef struct frontend_state {
    b8 initialized;
} frontend_state;

static renderer_interface interface;
static frontend_state state;

// Forward decl
void prepare_interface(RENDERER_BACKEND backend);

b8 renderer_init(RENDERER_BACKEND backend, platform_window *window, rl_camera *camera) {
    prepare_interface(backend);
    if (!interface.initialize(window, camera)) {
        RL_ERROR("Failed to initialize renderer backend");
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

void renderer_render_text(const char *text, f32 size_px, f32 x, f32 y, vec4 color) {
    if (!state.initialized)
        return;
    interface.render_text(text, size_px, x, y, color);
}

void renderer_set_active_font(rl_font *font) {
    if (!state.initialized)
        return;
    interface.set_active_font(font);
}

void renderer_set_view_projection(mat4 view, mat4 projection) {
    if (!state.initialized)
        return;
    interface.set_view_projection(view, projection);
}

void prepare_interface(RENDERER_BACKEND backend) {
    switch (backend) {
    case BACKEND_OPENGL:
        interface.initialize = &opengl_initialize;
        interface.shutdown = &opengl_destroy;
        interface.begin_frame = &opengl_begin_frame;
        interface.end_frame = &opengl_end_frame;
        interface.swap_buffers = &opengl_swap_buffers;
        interface.render_text = &opengl_render_text;
        interface.set_active_font = &opengl_set_active_font;
        interface.set_view_projection = &opengl_set_view_projection;
        break;
    case BACKEND_VULKAN:
        interface.initialize = &vulkan_initialize;
        interface.shutdown = &vulkan_destroy;
        interface.begin_frame = &vulkan_begin_frame;
        interface.end_frame = &vulkan_end_frame;
        interface.swap_buffers = &vulkan_swap_buffers;
        interface.render_text = &vulkan_render_text; //&vulkan_render_text;
        interface.set_active_font = &vulkan_set_active_font; //&vulkan_set_active_font;
        interface.set_view_projection = &vulkan_set_view_projection;
    }
}
