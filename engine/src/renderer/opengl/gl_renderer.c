#include "renderer/opengl/gl_renderer.h"

#include "asset/asset.h"
#include "core/logger.h"
#include "platform/platform.h"
#include "vendor/glad/glad.h"
#include "core/event.h"
#include "util/rl_math.h"
#include "renderer/opengl/gl_shader.h"

typedef struct opengl_context {
    platform_window *window;
    GL_Shader default_shader;
    u32 default_vao;
} opengl_context;

static opengl_context context;

void update_viewport() {
    glViewport(
        0, 0,
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

    // Shader init
    opengl_shader_setup("default.vert", "default.frag", &context.default_shader);

    vec3 vertices[] = {
        // V1
        vec3_create(0.5f, 0.5f, 0.0f),
        vec3_create(1.0f, 0.0f, 0.0f),

        // V2
        vec3_create(0.5f, -0.5f, 0.0f),
        vec3_create(0.0f, 1.0f, 0.0f),

        // V3
        vec3_create(-0.5f, -0.5f, 0.0f),
        vec3_create(0.0f, 0.0f, 1.0f),

        // V4
        vec3_create(-0.5f, 0.5f, 0.0f),
        vec3_create(1.0f, 0.0f, 0.0f),
    };
    u32 indices[] = {
        0, 1, 3, // First triangle
        1, 2, 3, // Second triangle
    };

    // Create vao & bind
    glGenVertexArrays(1, &context.default_vao);
    glBindVertexArray(context.default_vao);

    // Create vbo & bind
    u32 vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // EBO (indices)
    u32 ebo;
    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // Attributes
    // position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(f32), (void *)0);
    glEnableVertexAttribArray(0);
    // color attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(f32), (void *)(3 * sizeof(f32)));
    glEnableVertexAttribArray(1);

    return true;
}

void opengl_destroy() {
}

void opengl_begin_frame() {
    glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    //glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    glUseProgram(context.default_shader.program_id);
    glBindVertexArray(context.default_vao);
    glDrawElements(GL_TRIANGLES, 9, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

void opengl_end_frame() {
}

void opengl_swap_buffers() {
    platform_swap_buffers(context.window);
}
