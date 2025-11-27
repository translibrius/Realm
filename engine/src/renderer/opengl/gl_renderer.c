#include "renderer/opengl/gl_renderer.h"

#include "core/logger.h"
#include "platform/platform.h"
#include "vendor/glad/glad.h"

static opengl_context context;

b8 opengl_initialize(platform_window *platform_window) {
    context.window = platform_window;

    if (!platform_create_opengl_context(platform_window)) {
        return false;
    }

    glViewport(0, 0, 600, 600);

    RL_INFO("Initializing Renderer:");
    RL_DEBUG("-- Backend: OpenGL");
    RL_DEBUG("-- Window id: %d", platform_window->id);
    return true;
}

void opengl_destroy() {
}

void opengl_begin_frame() {
}

void opengl_end_frame() {
}

void opengl_swap_buffers() {
}
