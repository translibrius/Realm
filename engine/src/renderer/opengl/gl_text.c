#include "renderer/opengl/gl_text.h"

#include "gl_renderer.h"
#include "asset/font.h"
#include "core/logger.h"
#include "renderer/renderer_types.h"
#include "vendor/glad/glad.h"

#include <string.h>

// Helpers
static const rl_glyph *rl_font_find_glyph(const rl_font *font, u32 codepoint) {
    for (u32 i = 0; i < font->glyph_count; i++) {
        if ((u32)font->glyphs[i].codepoint == codepoint)
            return &font->glyphs[i];
    }
    return nullptr;
}

static GL_Font *find_gl_font(GL_Context *ctx, rl_font *font) {
    for (u32 i = 0; i < ctx->fonts.count; i++) {
        if (ctx->fonts.items[i].font == font)
            return &ctx->fonts.items[i];
    }
    return nullptr;
}

b8 opengl_text_pipeline_init(GL_Context *ctx) {
    GL_TextPipeline *pipeline = &ctx->text_pipeline;
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

    // Load all font assets to GPU :)
    Assets *assets = get_assets();
    for (u32 i = 0; i < assets->count; i++) {
        rl_asset *asset = &assets->items[i];
        if (asset->type == ASSET_FONT) {
            rl_font *font = (rl_font *)asset->handle;
            RL_DEBUG("loading gl font %s", font->name);
            if (!gl_font_create(font, ctx)) {
                RL_WARN("gl_font_create() failed for '%s'", asset->filename);
            }

            // Set *default* font as evil_empire.otf
            if (strcmp(font->name, "evil_empire.otf") == 0) {
                opengl_set_active_font(font);
            }
        }
    }

    // Unbind VAO
    glBindVertexArray(0);

    return true;
}

b8 gl_font_create(rl_font *font, GL_Context *ctx) {
    GL_Font *gl_font = rl_arena_alloc(&ctx->arena, sizeof(GL_Font), alignof(GL_Font));

    if (gl_font == nullptr)
        return false;

    glGenTextures(1, &gl_font->texture_id);
    glBindTexture(GL_TEXTURE_2D, gl_font->texture_id);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    GLenum fmt = (font->atlas.channels == 4) ? GL_RGBA : GL_RGB;
    GLint internal = (font->atlas.channels == 4) ? GL_RGBA8 : GL_RGB8;

    glTexImage2D(GL_TEXTURE_2D, 0, internal,
                 font->atlas.width, font->atlas.height,
                 0, fmt, GL_UNSIGNED_BYTE, font->atlas.data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    gl_font->font = font;
    da_append(&ctx->fonts, *gl_font);

    return true;
}

void opengl_render_text(const char *text, f32 size_px, f32 x, f32 y, vec4 color) {
    GL_Context *ctx = opengl_get_context();
    GL_Font *gl_font = find_gl_font(ctx, ctx->active_font);

    if (ctx == nullptr) {
        RL_WARN("opengl_render_text(): No context");
        return;
    }

    if (gl_font == nullptr) {
        RL_WARN("opengl_render_text(): No gl_font");
        return;
    }

    if (text == nullptr) {
        RL_WARN("opengl_render_text(): No text to render");
        return;
    }

    GL_TextPipeline *p = &ctx->text_pipeline;
    rl_font *font = gl_font->font;

    // Build vertices CPU-side
    GL_TextVertex verts[6 * MAX_TEXT_GLYPHS];
    u32 vert_count = 0;

    f32 cursor_x = x;
    f32 cursor_y = y;

    for (const unsigned char *c = (const unsigned char *)text; *c; c++) {
        if (*c == '\n') {
            cursor_x = x;
            cursor_y += font->line_height * size_px;
            continue;
        }

        if (vert_count + 6 > 6 * MAX_TEXT_GLYPHS)
            break;

        const rl_glyph *g = rl_font_find_glyph(font, (u32)*c);
        if (!g)
            continue;

        // y-up screen space: baseline at cursor_y
        const f32 x0 = cursor_x + g->plane_min_x * size_px;
        const f32 x1 = cursor_x + g->plane_max_x * size_px;

        // bottom and top in y-up space
        const f32 y0 = cursor_y + g->plane_min_y * size_px; // bottom
        const f32 y1 = cursor_y + g->plane_max_y * size_px; // top

        const f32 u0 = g->uv_min_x;
        const f32 v0 = g->uv_min_y;
        const f32 u1 = g->uv_max_x;
        const f32 v1 = g->uv_max_y;

        // Two triangles (TL, BL, BR) (TL, BR, TR)
        verts[vert_count + 0] = (GL_TextVertex){.pos = {x0, y0}, .uv = {u0, v0}};
        verts[vert_count + 1] = (GL_TextVertex){.pos = {x0, y1}, .uv = {u0, v1}};
        verts[vert_count + 2] = (GL_TextVertex){.pos = {x1, y1}, .uv = {u1, v1}};

        verts[vert_count + 3] = (GL_TextVertex){.pos = {x0, y0}, .uv = {u0, v0}};
        verts[vert_count + 4] = (GL_TextVertex){.pos = {x1, y1}, .uv = {u1, v1}};
        verts[vert_count + 5] = (GL_TextVertex){.pos = {x1, y0}, .uv = {u1, v0}};

        vert_count += 6;

        // Advance cursor
        cursor_x += g->advance * size_px;
    }

    if (vert_count == 0)
        return;

    // GL draw
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    opengl_shader_use(&p->shader);
    glBindVertexArray(p->vao);
    glBindBuffer(GL_ARRAY_BUFFER, p->vbo);

    // Update buffer and draw
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(GL_TextVertex) * vert_count, verts);

    // Bind texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gl_font->texture_id);

    // Uniforms (adapt these to your shader uniform API)
    opengl_shader_set_i32(&p->shader, "u_font_atlas", 0);
    opengl_shader_set_vec4(&p->shader, "u_color", color);
    opengl_shader_set_vec2(&p->shader, "u_screen_size",
                           (vec2){(f32)ctx->window->settings.width, (f32)ctx->window->settings.height});

    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vert_count);

    glBindVertexArray(0);
}

void opengl_set_active_font(rl_font *font) {
    RL_INFO("OpenGL Active font: %s", font->name);
    opengl_get_context()->active_font = font;
}
