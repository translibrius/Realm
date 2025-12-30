#include "renderer/opengl/gl_text.h"

#include "asset/font.h"
#include "core/logger.h"
#include "renderer/renderer_types.h"
#include "vendor/glad/glad.h"

b8 opengl_text_pipeline_init(GL_TextPipeline *pipeline) {
    if (!opengl_shader_setup("text.vert", "text.frag", &pipeline->shader)) {
        RL_ERROR("opengl_shader_setup() failed");
        return false;
    }

    // Create vao & bind
    glGenVertexArrays(1, &pipeline->vao);
    glBindVertexArray(pipeline->vao);

    // Create vbo & bind
    glGenBuffers(1, &pipeline->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, pipeline->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GL_TextVertex) * 6 * MAX_TEXT_GLYPHS, NULL, GL_DYNAMIC_DRAW);

    // Attributes
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(GL_TextVertex), (void *)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(GL_TextVertex), (void *)(2 * sizeof(f32)));
    glEnableVertexAttribArray(1);

    return true;
}

b8 opengl_font_init(const char *filename, GL_Fonts *fonts) {
    rl_asset *asset = get_asset(filename);
    if (asset == nullptr) {
        RL_ERROR("Failed to find font asset for file: '%s'", filename);
        return false;
    }

    if (asset->handle == nullptr) {
        RL_ERROR("opengl_font_init(): Failed to initialize font: invalid handle");
        return false;
    }

    rl_font *font = asset->handle;
    (void)fonts;
    (void)font;
    return true;
}