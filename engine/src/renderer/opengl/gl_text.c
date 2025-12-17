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

    rl_font *font = asset->handle;
    GL_Font gl_font = {};

    // Bind texture
    glGenTextures(1, &gl_font.texture_id);
    glBindTexture(GL_TEXTURE_2D, gl_font.texture_id);

    // Texture params
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, font->atlas_w, font->atlas_h, 0, GL_RED, GL_UNSIGNED_BYTE, font->atlas);

    da_append(fonts, gl_font);
    font->handle = &fonts->items[fonts->count - 1];
    return true;
}