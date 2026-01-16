#include "renderer/opengl/gl_renderer.h"

#include "gl_texture.h"
#include "renderer/opengl/gl_text.h"
#include "core/logger.h"
#include "platform/platform.h"
#include "glad.h"
#include "core/event.h"
#include "renderer/opengl/gl_shader.h"
#include "renderer/opengl/gl_types.h"
#include "core/camera.h"

static GL_Context context;

static f32 angle;

GL_Context *opengl_get_context(void) {
    return &context;
}

void opengl_resize_framebuffer(i32 w, i32 h) {
    glViewport(0, 0, w, h);
}

void opengl_set_view_projection(mat4 view, mat4 projection, vec3 pos) {
    glm_mat4_copy(view, context.view);
    glm_mat4_copy(projection, context.projection);
    glm_vec3_copy(pos, context.pos);
}

b8 opengl_initialize(platform_window *platform_window, b8 vsync) {
    context.window = platform_window;

    da_init(&context.fonts);
    rl_arena_init(&context.arena, MiB(100), MiB(25), MEM_SUBSYSTEM_RENDERER);

    RL_INFO("Initializing Renderer: OpenGL");
    if (!platform_create_opengl_context(context.window)) {
        RL_ERROR("opengl_initialize() failed: Failed to create OpenGL context");
        return false;
    }

    platform_context_make_current(context.window);
    opengl_resize_framebuffer(context.window->settings.width, context.window->settings.height);

    // Shader init
    if (!opengl_shader_setup("default.vert", "default.frag", &context.default_shader)) {
        RL_ERROR("opengl_shader_setup() failed");
        return false;
    }
    if (!opengl_shader_setup("default.vert", "light.frag", &context.light_shader)) {
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

    context.cube_mesh = gl_mesh_create_cube();

    return true;
}

void opengl_destroy() {
    gl_mesh_destroy(&context.cube_mesh);
    rl_arena_deinit(&context.arena);
}

void opengl_begin_frame(f64 delta_time) {
    (void)delta_time;

    if (angle > 360)
        angle = 0.0f;
    angle += 100.0f * delta_time;

    glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    vec3 light_pos = {1.2f, 1.0f, 2.0f};

    opengl_shader_use(&context.default_shader);
    opengl_shader_set_vec3(&context.default_shader, "objectColor", (vec3){1.0f, 0.5f, 0.31f});
    opengl_shader_set_vec3(&context.default_shader, "lightColor", (vec3){0.0f, 1.0f, 1.0f});
    opengl_shader_set_vec3(&context.default_shader, "lightPos", light_pos);
    opengl_shader_set_vec3(&context.default_shader, "view_pos", context.pos);

    mat4 model = {};
    glm_mat4_identity(model);
    glm_rotate(model, glm_rad(angle), (vec3){0.5f, 1.0f, 0.0f});

    opengl_shader_set_mat4(&context.default_shader, "model", model);
    opengl_shader_set_mat4(&context.default_shader, "view", context.view);
    opengl_shader_set_mat4(&context.default_shader, "projection", context.projection);

    gl_mesh_draw(&context.cube_mesh);

    // Draw floor
    for (i32 x = -5; x <= 5; x++) {
        for (i32 z = -5; z <= 5; z++) {
            mat4 floor_model;
            glm_mat4_identity(floor_model);

            glm_translate(floor_model, (vec3){(f32)x, -2.0f, (f32)z});
            opengl_shader_set_mat4(&context.default_shader, "model", floor_model);
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            gl_mesh_draw(&context.cube_mesh);
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }
    }

    // Draw light
    opengl_shader_use(&context.light_shader);

    // position the light somewhere in the world
    mat4 light_model;
    glm_mat4_identity(light_model);
    glm_translate(light_model, light_pos);
    glm_scale(light_model, (vec3){0.2f, 0.2f, 0.2f}); // smaller cube

    opengl_shader_set_mat4(&context.light_shader, "model", light_model);
    opengl_shader_set_mat4(&context.light_shader, "view", context.view);
    opengl_shader_set_mat4(&context.light_shader, "projection", context.projection);

    gl_mesh_draw(&context.cube_mesh);
}

void opengl_end_frame() {
}

void opengl_swap_buffers() {
    platform_swap_buffers(context.window);
}

platform_window *opengl_get_active_window() {
    return context.window;
}

void opengl_set_active_window(platform_window *window) {
    context.window = window;
}