
#include "renderer/renderer_frontend.h"
#include "core/config.h"
#include "core/event.h"
#include "core/logger.h"
#include <string.h>
#include "opengl/gl_gui.h"
#include "opengl/gl_text.h"
#include "renderer/opengl/gl_renderer.h"
#include "renderer/renderer_types.h"

#include "vulkan/vk_renderer.h"
#include "vulkan/vk_text.h"
#include "vulkan/vk_gui.h"

typedef struct frontend_state {
    b8 initialized;
    b8 wireframe;
} frontend_state;

static renderer_interface interface;
static frontend_state state;

// Forward decl
void prepare_interface(RENDERER_BACKEND backend);

static b8 on_config_changed(void *data, void *user_data) {
    (void)user_data;
    e_config_changed_payload *p = data;
    if (strcmp(p->key, "vsync") == 0) {
        renderer_set_vsync(config_get()->vsync);
    }
    return false;
}

b8 renderer_init(platform_window *window, RENDERER_BACKEND backend, b8 vsync) {
    prepare_interface(backend);
    if (!interface.initialize(window, vsync)) {
        RL_ERROR("Failed to initialize renderer backend");
        return false;
    }

    event_register(EVENT_CONFIG_CHANGED, on_config_changed, nullptr);
    state.initialized = true;

    if (state.wireframe && interface.set_wireframe) {
        interface.set_wireframe(true);
    }

    return true;
}

void renderer_destroy() {
    if (!state.initialized)
        return;
    event_unregister(EVENT_CONFIG_CHANGED, on_config_changed, nullptr);
    interface.shutdown();
    state.initialized = false;
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

void renderer_set_vsync(b8 vsync) {
    if (!state.initialized || !interface.set_vsync)
        return;
    interface.set_vsync(vsync);
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

void renderer_set_view_projection(mat4 view, mat4 projection, vec3 pos) {
    if (!state.initialized)
        return;
    interface.set_view_projection(view, projection, pos);
}

platform_window *renderer_get_active_window() {
    if (!state.initialized)
        return nullptr;
    return interface.get_active_window();
}

void renderer_set_active_window(platform_window *window) {
    if (!state.initialized)
        return;
    interface.set_active_window(window);
}

void renderer_resize_framebuffer(i32 w, i32 h) {
    if (!state.initialized)
        return;
    interface.resize_framebuffer(w, h);
}

void renderer_submit_frame_data(rl_frame_data *frame_data) {
    if (!state.initialized || !interface.submit_frame_data)
        return;
    interface.submit_frame_data(frame_data);
}

void renderer_submit_gui_data(void *commands, i32 command_count) {
    if (!state.initialized || !interface.submit_gui_data)
        return;
    interface.submit_gui_data(commands, command_count);
}

void renderer_toggle_wireframe(void) {
    if (!state.initialized)
        return;
    state.wireframe = !state.wireframe;
    if (interface.set_wireframe) {
        interface.set_wireframe(state.wireframe);
    }
    RL_INFO("Wireframe mode %s", state.wireframe ? "ON" : "OFF");
}

void prepare_interface(RENDERER_BACKEND backend) {
    switch (backend) {
    case BACKEND_OPENGL:
        interface.initialize = &opengl_initialize;
        interface.shutdown = &opengl_destroy;
        interface.begin_frame = &opengl_begin_frame;
        interface.end_frame = &opengl_end_frame;
        interface.swap_buffers = &opengl_swap_buffers;
        interface.set_vsync = &opengl_set_vsync;
        interface.render_text = &opengl_render_text;
        interface.set_active_font = &opengl_set_active_font;
        interface.set_view_projection = &opengl_set_view_projection;
        interface.get_active_window = &opengl_get_active_window;
        interface.set_active_window = &opengl_set_active_window;
        interface.resize_framebuffer = &opengl_resize_framebuffer;
        interface.submit_frame_data = &opengl_submit_frame_data;
        interface.submit_gui_data = &opengl_render_gui;
        interface.set_wireframe = &opengl_set_wireframe;
        break;
    case BACKEND_VULKAN:
        interface.initialize = &vulkan_initialize;
        interface.shutdown = &vulkan_destroy;
        interface.begin_frame = &vulkan_begin_frame;
        interface.end_frame = &vulkan_end_frame;
        interface.swap_buffers = &vulkan_swap_buffers;
        interface.set_vsync = &vulkan_set_vsync;
        interface.render_text = &vulkan_render_text;
        interface.set_active_font = &vulkan_set_active_font;
        interface.set_view_projection = &vulkan_set_view_projection;
        interface.get_active_window = &vulkan_get_active_window;
        interface.set_active_window = &vulkan_set_active_window;
        interface.resize_framebuffer = &vulkan_resize_framebuffer;
        interface.submit_frame_data = &vulkan_submit_frame_data;
        interface.submit_gui_data = &vulkan_render_gui;
        interface.set_wireframe = &vulkan_set_wireframe;
    }
}
