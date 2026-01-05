#include "renderer/opengl/gl_renderer.h"

#include "gl_texture.h"
#include "renderer/opengl/gl_text.h"
#include "core/logger.h"
#include "platform/platform.h"
#include "../vendor/glad/glad.h"
#include "core/event.h"
#include "renderer/renderer_types.h"
#include "renderer/opengl/gl_shader.h"
#include "renderer/opengl/gl_types.h"

static GL_Context context;

static f32 angle;

GL_Context *opengl_get_context(void) {
    return &context;
}

void update_viewport() {
    glViewport(
        0, 0,
        context.window->settings.width,
        context.window->settings.height);
}

b8 resize_callback(void *data) {
    platform_window *window = data;
    if (window->id == context.window->id) {

        RL_DEBUG("Window #%d resized | POS: %d;%d | Size: %dx%d",
                 window->id,
                 window->settings.x, window->settings.y,
                 window->settings.width, window->settings.height);

        update_viewport();
    }
    return false;
}

void opengl_set_view_projection(mat4 view, mat4 projection) {
    glm_mat4_copy(view, context.view);
    glm_mat4_copy(projection, context.projection);
}

b8 opengl_initialize(platform_window *platform_window) {
    context.window = platform_window;
    da_init(&context.fonts);
    rl_arena_create(MiB(25), &context.arena, MEM_SUBSYSTEM_RENDERER);

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
    if (!opengl_shader_setup("default.vert", "default.frag", &context.default_shader)) {
        RL_ERROR("opengl_shader_setup() failed");
        return false;
    }

    // Texture init
    if (!opengl_texture_generate("wood_container.jpg", &context.wood_texture)) {
        RL_ERROR("opengl_texture_generate() failed");
        return false;
    }

    // Text pipeline
    opengl_text_pipeline_init(&context);

    glEnable(GL_DEPTH_TEST);

    f32 vertices[] = {
        -0.5f, -0.5f, -0.5f, 0.0f, 0.0f,
        0.5f, -0.5f, -0.5f, 1.0f, 0.0f,
        0.5f, 0.5f, -0.5f, 1.0f, 1.0f,
        0.5f, 0.5f, -0.5f, 1.0f, 1.0f,
        -0.5f, 0.5f, -0.5f, 0.0f, 1.0f,
        -0.5f, -0.5f, -0.5f, 0.0f, 0.0f,

        -0.5f, -0.5f, 0.5f, 0.0f, 0.0f,
        0.5f, -0.5f, 0.5f, 1.0f, 0.0f,
        0.5f, 0.5f, 0.5f, 1.0f, 1.0f,
        0.5f, 0.5f, 0.5f, 1.0f, 1.0f,
        -0.5f, 0.5f, 0.5f, 0.0f, 1.0f,
        -0.5f, -0.5f, 0.5f, 0.0f, 0.0f,

        -0.5f, 0.5f, 0.5f, 1.0f, 0.0f,
        -0.5f, 0.5f, -0.5f, 1.0f, 1.0f,
        -0.5f, -0.5f, -0.5f, 0.0f, 1.0f,
        -0.5f, -0.5f, -0.5f, 0.0f, 1.0f,
        -0.5f, -0.5f, 0.5f, 0.0f, 0.0f,
        -0.5f, 0.5f, 0.5f, 1.0f, 0.0f,

        0.5f, 0.5f, 0.5f, 1.0f, 0.0f,
        0.5f, 0.5f, -0.5f, 1.0f, 1.0f,
        0.5f, -0.5f, -0.5f, 0.0f, 1.0f,
        0.5f, -0.5f, -0.5f, 0.0f, 1.0f,
        0.5f, -0.5f, 0.5f, 0.0f, 0.0f,
        0.5f, 0.5f, 0.5f, 1.0f, 0.0f,

        -0.5f, -0.5f, -0.5f, 0.0f, 1.0f,
        0.5f, -0.5f, -0.5f, 1.0f, 1.0f,
        0.5f, -0.5f, 0.5f, 1.0f, 0.0f,
        0.5f, -0.5f, 0.5f, 1.0f, 0.0f,
        -0.5f, -0.5f, 0.5f, 0.0f, 0.0f,
        -0.5f, -0.5f, -0.5f, 0.0f, 1.0f,

        -0.5f, 0.5f, -0.5f, 0.0f, 1.0f,
        0.5f, 0.5f, -0.5f, 1.0f, 1.0f,
        0.5f, 0.5f, 0.5f, 1.0f, 0.0f,
        0.5f, 0.5f, 0.5f, 1.0f, 0.0f,
        -0.5f, 0.5f, 0.5f, 0.0f, 0.0f,
        -0.5f, 0.5f, -0.5f, 0.0f, 1.0f
    };
    u32 vbo;

    // Create vao & bind
    glGenVertexArrays(1, &context.default_vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(context.default_vao);

    // Create vbo & bind
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Attributes
    // position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);

    // texcoord
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    return true;
}

void opengl_destroy() {
    rl_arena_destroy(&context.arena);
}

void opengl_begin_frame(f64 delta_time) {
    (void)delta_time;

    if (angle > 360)
        angle = 0.0f;
    angle += 100.0f * delta_time;

    f32 aspect_ratio = (f32)context.window->settings.width / (f32)context.window->settings.height;
    const f32 fov_x = glm_rad(90.0f);
    const f32 fov_y = 2.0f * atanf(tanf(fov_x * 0.5f) / aspect_ratio);
    constexpr f32 near_z = 0.1f;
    constexpr f32 far_z = 100.0f;

    glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    //glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, context.wood_texture.id);
    opengl_shader_use(&context.default_shader);

    mat4 model = {};
    glm_mat4_identity(model);
    glm_rotate(model, glm_rad(angle), (vec3){0.5f, 1.0f, 0.0f});

    opengl_shader_set_mat4(&context.default_shader, "model", model);
    opengl_shader_set_mat4(&context.default_shader, "view", context.view);
    opengl_shader_set_mat4(&context.default_shader, "projection", context.projection);

    glBindVertexArray(context.default_vao);
    glDrawArrays(GL_TRIANGLES, 0, 36);
}

void opengl_end_frame() {
}

void opengl_swap_buffers() {
    platform_swap_buffers(context.window);
}