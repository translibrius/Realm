#include "renderer/opengl/gl_renderer.h"

#include "core/logger.h"
#include "platform/platform.h"
#include "vendor/glad/glad.h"
#include "util/rand.h"
#include "core/event.h"
#include "util/rl_math.h"

static opengl_context context;

void update_viewport() {
    glViewport(
        context.window->settings.x,
        context.window->settings.y,
        context.window->settings.width,
        context.window->settings.height);
}

b8 resize_callback(void *data) {
    platform_window *window = data;
    if (window->id == context.window->id) {
        /*
        RL_DEBUG("Window #%d resized | POS: %d;%d | Size: %dx%d",
                 window->id,
                 window->settings.x, window->settings.y,
                 window->settings.width, window->settings.height);
        */
        update_viewport();
    }
    return false;
}

b8 opengl_initialize(platform_window *platform_window) {
    context.window = platform_window;

    RL_INFO("Initializing Renderer: OpenGL");
    if (!platform_create_opengl_context(context.window)) {
        RL_ERROR("opengl_initialize() failed: Failed to create OpenGL context");
        return false;
    }

    platform_context_make_current(context.window);
    update_viewport();

    // Listen to window size
    event_register(EVENT_WINDOW_RESIZE, resize_callback);

    return true;
}

void opengl_destroy() {
}

void opengl_begin_frame() {
    glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    vec3 vertices[] = {
        vec3_create(-0.5f, -0.5f, 0.0f),
        vec3_create(0.5f, -0.5f, 0.0f),
        vec3_create(0.5f, 0.5f, 0.0f),
    };
    (void)vertices;
}

void opengl_end_frame() {
}

void opengl_swap_buffers() {
    platform_swap_buffers(context.window);
}
