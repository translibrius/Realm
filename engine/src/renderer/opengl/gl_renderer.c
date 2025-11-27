#include "renderer/opengl/gl_renderer.h"

#include "core/logger.h"
#include "platform/platform.h"
#include "vendor/glad/glad.h"

static opengl_context context;

b8 opengl_initialize(platform_window *platform_window) {
    context.window = platform_window;

    RL_INFO("Initializing Renderer: OpenGL");
    if (!platform_create_opengl_context(context.window)) {
        RL_ERROR("opengl_initialize() failed: Failed to create OpenGL context");
        return false;
    }

    platform_context_make_current(context.window);

    glViewport(0, 0, 600, 600);
    return true;
}

void opengl_destroy() {
}

void opengl_begin_frame() {
    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void opengl_end_frame() {
}

void opengl_swap_buffers() {
    platform_swap_buffers(context.window);
}
