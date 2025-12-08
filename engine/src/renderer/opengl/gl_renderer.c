#include "renderer/opengl/gl_renderer.h"

#include "asset/asset.h"
#include "asset/shader.h"
#include "core/logger.h"
#include "platform/platform.h"
#include "vendor/glad/glad.h"
#include "util/rand.h"
#include "core/event.h"
#include "util/rl_math.h"

typedef struct opengl_context {
    platform_window *window;
    u32 default_shader_program;
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

    rl_asset_shader *default_vert = get_asset("default.vert")->handle;
    rl_asset_shader *default_frag = get_asset("default.frag")->handle;
    u32 default_vertex_shader = opengl_compile_vertex_shader(default_vert->source);
    u32 default_fragment_shader = opengl_compile_fragment_shader(default_frag->source);

    context.default_shader_program = glCreateProgram();
    glAttachShader(context.default_shader_program, default_vertex_shader);
    glAttachShader(context.default_shader_program, default_fragment_shader);
    glLinkProgram(context.default_shader_program);
    // Delete after linking to program
    glDeleteShader(default_vertex_shader);
    glDeleteShader(default_fragment_shader);

    i32 success;
    char info_log[512];
    glGetProgramiv(context.default_shader_program, GL_LINK_STATUS, &success);

    if (!success) {
        glGetProgramInfoLog(context.default_shader_program, 512, nullptr, info_log);
        RL_ERROR("Failed to link shader program with default shaders: %s", info_log);
        return false;
    }

    vec3 vertices[] = {
        vec3_create(-0.5f, -0.5f, 0.0f),
        vec3_create(0.5f, -0.5f, 0.0f),
        vec3_create(0.5f, 0.5f, 0.0f),
    };
    (void)vertices;

    // Create vao & bind
    glGenVertexArrays(1, &context.default_vao);
    glBindVertexArray(context.default_vao);

    // Create vbo & bind
    u32 vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Attribute config while vbo is bound
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(f32), (void *)0);
    glEnableVertexAttribArray(0);

    // Unbind
    glBindVertexArray(0);

    return true;
}

void opengl_destroy() {
}

void opengl_begin_frame() {
    glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(context.default_shader_program);
    glBindVertexArray(context.default_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

void opengl_end_frame() {
}

void opengl_swap_buffers() {
    platform_swap_buffers(context.window);
}

u32 opengl_compile_vertex_shader(const char *source) {
    u32 vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &source, nullptr);
    glCompileShader(vertex_shader);

    i32 success;
    char infoLog[512];
    glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);

    if (!success) {
        glGetShaderInfoLog(vertex_shader, 512, nullptr, infoLog);
        RL_FATAL("Failed to compile default vertex shader: %s", infoLog);
    }

    return vertex_shader;
}

u32 opengl_compile_fragment_shader(const char *source) {
    u32 fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &source, nullptr);
    glCompileShader(fragment_shader);

    i32 success;
    char infoLog[512];
    glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);

    if (!success) {
        glGetShaderInfoLog(fragment_shader, 512, nullptr, infoLog);
        RL_FATAL("Failed to compile default fragment shader: %s", infoLog);
    }

    return fragment_shader;
}
