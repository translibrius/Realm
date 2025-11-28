#include "renderer/opengl/gl_renderer.h"

#include "core/logger.h"
#include "platform/platform.h"
#include "vendor/glad/glad.h"
#include "util/rand.h"

static opengl_context context;

b8 opengl_initialize(platform_window *platform_window) {
    context.window = platform_window;

    RL_INFO("Initializing Renderer: OpenGL");
    if (!platform_create_opengl_context(context.window)) {
        RL_ERROR("opengl_initialize() failed: Failed to create OpenGL context");
        return false;
    }

    platform_context_make_current(context.window);

    glViewport(
        context.window->settings.x,
        context.window->settings.y,
        context.window->settings.width,
        context.window->settings.height);

    return true;
}

void opengl_destroy() {
}

void opengl_begin_frame() {
    float r = rand_float01();
    float g = rand_float01();
    float b = rand_float01();

    glClearColor(r, g, b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void opengl_end_frame() {
}

void opengl_swap_buffers() {
    platform_swap_buffers(context.window);
}
