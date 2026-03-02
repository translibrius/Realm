#include "renderer/opengl/gl_renderer.h"

#include "gl_gui.h"
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

void opengl_submit_frame_data(rl_frame_data *frame_data) {
    if (!frame_data) {
        return;
    }

    if (frame_data->camera.valid) {
        mat4 view = {0};
        mat4 projection = {0};
        vec3 position = {0};

        glm_mat4_copy(frame_data->camera.view, view);
        glm_mat4_copy(frame_data->camera.projection, projection);
        glm_vec3_copy(frame_data->camera.position, position);

        opengl_set_view_projection(view, projection, position);
    }

    rl_frame_point_light light = {
        .position = {1.2f, 1.0f, 2.0f},
        .ambient  = {0.2f, 0.2f, 0.2f},
        .diffuse  = {0.5f, 0.5f, 0.5f},
        .specular = {1.0f, 1.0f, 1.0f},
    };
    if (frame_data->point_light_count > 0 && frame_data->point_lights) {
        light = frame_data->point_lights[0];
    }

    for (u32 i = 0; frame_data->meshes && i < frame_data->mesh_count; i++) {
        rl_frame_mesh *mesh = &frame_data->meshes[i];
        if (mesh->primitive != RL_FRAME_PRIMITIVE_CUBE) {
            continue;
        }

        if (mesh->kind == RL_FRAME_MESH_KIND_LIT) {
            opengl_shader_use(&context.default_shader);
            opengl_shader_set_i32(&context.default_shader, "material.diffuse", 0);
            glActiveTexture(GL_TEXTURE0);
            u32 tex_id = context.wood_texture.id;
            if (mesh->material.diffuse_map == ASSET_ID_TEXTURE_WOOD_CONTAINER2) {
                tex_id = context.wood_texture2.id;
            }
            glBindTexture(GL_TEXTURE_2D, tex_id);
            opengl_shader_set_vec3(&context.default_shader, "material.specular", mesh->material.specular);
            opengl_shader_set_f32(&context.default_shader, "material.shininess", mesh->material.shininess);
            opengl_shader_set_vec3(&context.default_shader, "light.position", light.position);
            opengl_shader_set_vec3(&context.default_shader, "light.ambient", light.ambient);
            opengl_shader_set_vec3(&context.default_shader, "light.diffuse", light.diffuse);
            opengl_shader_set_vec3(&context.default_shader, "light.specular", light.specular);
            opengl_shader_set_vec3(&context.default_shader, "view_pos", context.pos);
            opengl_shader_set_mat4(&context.default_shader, "model", mesh->model);
            opengl_shader_set_mat4(&context.default_shader, "view", context.view);
            opengl_shader_set_mat4(&context.default_shader, "projection", context.projection);
        } else {
            opengl_shader_use(&context.light_shader);
            opengl_shader_set_mat4(&context.light_shader, "model", mesh->model);
            opengl_shader_set_mat4(&context.light_shader, "view", context.view);
            opengl_shader_set_mat4(&context.light_shader, "projection", context.projection);
        }

        if (mesh->wireframe) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        } else {
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }

        gl_mesh_draw(&context.cube_mesh);

        if (mesh->wireframe) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }
    }

    if (frame_data->texts && frame_data->text_count > 0) {
        opengl_render_text_batch(frame_data->texts, frame_data->text_count);
    }
}

b8 opengl_initialize(platform_window *platform_window, b8 vsync) {
    (void)vsync; // vsync is handled at platform surface level
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
    if (!opengl_shader_setup(ASSET_ID_SHADER_DEFAULT_VERT, ASSET_ID_SHADER_DEFAULT_FRAG, &context.default_shader)) {
        RL_ERROR("opengl_shader_setup() failed");
        return false;
    }
    if (!opengl_shader_setup(ASSET_ID_SHADER_DEFAULT_VERT, ASSET_ID_SHADER_LIGHT_FRAG, &context.light_shader)) {
        RL_ERROR("opengl_shader_setup() failed");
        return false;
    }

    // Texture init
    if (!opengl_texture_generate(ASSET_ID_TEXTURE_WOOD_CONTAINER, &context.wood_texture)) {
        RL_ERROR("opengl_texture_generate() failed");
        return false;
    }
    if (!opengl_texture_generate(ASSET_ID_TEXTURE_WOOD_CONTAINER2, &context.wood_texture2)) {
        RL_ERROR("opengl_texture_generate() failed");
        return false;
    }

    // Text pipeline
    opengl_text_pipeline_init(&context);

    // GUI pipeline
    if (!opengl_gui_pipeline_init(&context)) {
        RL_ERROR("opengl_gui_pipeline_init() failed");
        return false;
    }

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

    glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
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
