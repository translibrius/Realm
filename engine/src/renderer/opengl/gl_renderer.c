#include "renderer/opengl/gl_renderer.h"

#include "gl_texture.h"
#include "renderer/opengl/gl_text.h"
#include "core/logger.h"
#include "platform/platform.h"
#include "vendor/glad/glad.h"
#include "core/event.h"
#include "renderer/renderer_types.h"
#include "renderer/opengl/gl_shader.h"
#include "renderer/opengl/gl_types.h"

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
    opengl_text_pipeline_init(&context.text_pipeline);
    if (!opengl_font_init("evil_empire.otf", &context.fonts)) {
        RL_ERROR("opengl_font_init() failed");
        return false;
    }

    f32 vertices[] = {
        // positions          // colors           // texture coords
        0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, // top right
        0.5f, -0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, // bottom right
        -0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, // bottom left
        -0.5f, 0.5f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f // top left
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
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);
    // color attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    // texture coord attribute
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void *)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    return true;
}

void opengl_destroy() {
    rl_arena_destroy(&context.arena);
}

void opengl_begin_frame(f64 delta_time) {
    (void)delta_time;

    glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    //glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    opengl_shader_use(&context.default_shader);
    glBindTexture(GL_TEXTURE_2D, context.wood_texture.id);

    glBindVertexArray(context.default_vao);
    glDrawElements(GL_TRIANGLES, 9, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

void opengl_end_frame() {
}

void opengl_swap_buffers() {
    platform_swap_buffers(context.window);
}

void opengl_draw_text(rl_font *font, const char *text, vec2 pos, vec4 color) {
    GL_Font *gl_font = font->handle;

    // Bind shader
    opengl_shader_use(&context.text_pipeline.shader);

    opengl_shader_set_f32(&context.text_pipeline.shader, "u_scale", 0.8f);

    // Set uniforms
    opengl_shader_set_vec2(
        &context.text_pipeline.shader,
        "u_screen_size",
        (vec2){
            (f32)context.window->settings.width,
            (f32)context.window->settings.height
        }
        );

    opengl_shader_set_vec4(&context.text_pipeline.shader, "u_color", color);
    opengl_shader_set_i32(&context.text_pipeline.shader, "u_font_atlas", 0);

    // Bind font atlas
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gl_font->texture_id);

    // Enable alpha blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Bind text pipeline
    glBindVertexArray(context.text_pipeline.vao);
    glBindBuffer(GL_ARRAY_BUFFER, context.text_pipeline.vbo);

    // Map buffer for writing
    GL_TextVertex *verts = glMapBufferRange(
        GL_ARRAY_BUFFER,
        0,
        sizeof(GL_TextVertex) * 6 * MAX_TEXT_GLYPHS,
        GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT
        );

    if (!verts) {
        RL_ERROR("Failed to map text VBO");
        return;
    }

    f32 x = pos.x;
    f32 y = pos.y;

    u32 vertex_count = 0;

    for (const char *c = text; *c && vertex_count < 6 * MAX_TEXT_GLYPHS; c++) {
        if (*c < font->first_char ||
            *c >= font->first_char + font->char_count) {
            continue;
        }

        stbtt_aligned_quad q;
        stbtt_GetBakedQuad(
            font->chars,
            font->atlas_w,
            font->atlas_h,
            *c - font->first_char,
            &x,
            &y,
            &q,
            1 // pixel snapping
            );

        // Triangle 1
        verts[vertex_count++] = (GL_TextVertex){q.x0, q.y0, q.s0, q.t0};
        verts[vertex_count++] = (GL_TextVertex){q.x1, q.y0, q.s1, q.t0};
        verts[vertex_count++] = (GL_TextVertex){q.x1, q.y1, q.s1, q.t1};

        // Triangle 2
        verts[vertex_count++] = (GL_TextVertex){q.x0, q.y0, q.s0, q.t0};
        verts[vertex_count++] = (GL_TextVertex){q.x1, q.y1, q.s1, q.t1};
        verts[vertex_count++] = (GL_TextVertex){q.x0, q.y1, q.s0, q.t1};
    }

    glUnmapBuffer(GL_ARRAY_BUFFER);

    // Draw all glyphs in one call
    glDrawArrays(GL_TRIANGLES, 0, vertex_count);

    glBindVertexArray(0);
    glDisable(GL_BLEND);
}