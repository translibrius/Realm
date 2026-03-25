#include "renderer/opengl/gl_text.h"

#include "asset/asset.h"
#include "asset/asset_internal.h"
#include "asset/font.h"
#include "asset/font_atlas.h"
#include "core/logger.h"
#include "gl_renderer.h"
#include "glad.h"
#include "renderer/renderer_types.h"

#include "util/str.h"

#include <string.h>

GL_Font *gl_find_font(GL_Context *ctx, rl_font *font) {
    for (u32 i = 0; i < ctx->fonts.count; i++) {
        if (ctx->fonts.items[i].font == font)
            return &ctx->fonts.items[i];
    }
    return nullptr;
}

b8 opengl_text_pipeline_init(GL_Context *ctx) {
    GL_TextPipeline *pipeline = &ctx->text_pipeline;
    if (!opengl_shader_setup(asset_find(RL_ASSET_SHADER_GL_TEXT_VERT), asset_find(RL_ASSET_SHADER_GL_TEXT_FRAG), &pipeline->shader)) {
        RL_ERROR("opengl_shader_setup() failed");
        return false;
    }

    // Cache uniform locations
    pipeline->loc_font_atlas = glGetUniformLocation(pipeline->shader.program_id, "u_font_atlas");
    pipeline->loc_screen_size = glGetUniformLocation(pipeline->shader.program_id, "u_screen_size");
    pipeline->loc_px_range = glGetUniformLocation(pipeline->shader.program_id, "u_px_range");

    // Create vao & bind
    glGenVertexArrays(1, &pipeline->vao);
    glBindVertexArray(pipeline->vao);

    // Create vbo & bind
    glGenBuffers(1, &pipeline->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, pipeline->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GL_TextVertex) * 6 * 4096, NULL, GL_DYNAMIC_DRAW);

    // Attributes: pos (vec2), uv (vec2), color (vec4)
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(GL_TextVertex), (void *)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(GL_TextVertex), (void *)(2 * sizeof(f32)));
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(GL_TextVertex), (void *)(4 * sizeof(f32)));
    glEnableVertexAttribArray(2);

    // Upload font atlas(es) to GPU
    const rl_texture *combined = rl_font_atlas_get_combined();
    u32 shared_texture_id = 0;

    if (combined) {
        // Single combined atlas for all fonts
        glGenTextures(1, &shared_texture_id);
        glBindTexture(GL_TEXTURE_2D, shared_texture_id);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                     combined->width, combined->height,
                     0, GL_RGBA, GL_UNSIGNED_BYTE, combined->data);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        RL_DEBUG("uploaded combined font atlas %dx%d", combined->width, combined->height);
    }

    Assets *assets = get_assets();
    for (u32 i = 0; i < assets->count; i++) {
        rl_asset *asset = &assets->items[i];
        if (asset->type == ASSET_FONT) {
            rl_font *font = (rl_font *)asset->data;
            RL_DEBUG("loading gl font %s", font->name);

            if (shared_texture_id) {
                // Create GL_Font entry sharing the combined texture
                GL_Font gl_font = {0};
                gl_font.texture_id = shared_texture_id;
                gl_font.font = font;
                for (u32 g = 0; g < font->glyph_count; g++) {
                    u32 cp = (u32)font->glyphs[g].codepoint;
                    if (cp < 256)
                        gl_font.glyph_map[cp] = &font->glyphs[g];
                }
                da_append(&ctx->fonts, gl_font);
            } else {
                if (!gl_font_create(font, ctx)) {
                    RL_WARN("gl_font_create() failed for '%s'", asset->filename);
                }
            }

            if (cstr_eq(font->name, "JetBrainsMono-Regular.ttf")) {
                opengl_set_active_font(font);
            }
        }
    }

    // Unbind VAO
    glBindVertexArray(0);

    return true;
}

b8 gl_font_create(rl_font *font, GL_Context *ctx) {
    GL_Font *gl_font = rl_arena_push(&ctx->arena, sizeof(GL_Font), alignof(GL_Font));

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
    for (u32 i = 0; i < font->glyph_count; i++) {
        u32 cp = (u32)font->glyphs[i].codepoint;
        if (cp < 256)
            gl_font->glyph_map[cp] = &font->glyphs[i];
    }
    da_append(&ctx->fonts, *gl_font);

    return true;
}

void opengl_render_text(const char *text, f32 size_px, f32 x, f32 y, vec4 color) {
    GL_Context *ctx = opengl_get_context();
    GL_Font *gl_font = gl_find_font(ctx, ctx->active_font);

    if (ctx == nullptr || gl_font == nullptr || text == nullptr) {
        return;
    }

    GL_TextPipeline *p = &ctx->text_pipeline;
    rl_font *font = gl_font->font;

    GL_TextVertex verts[6 * MAX_TEXT_GLYPHS];
    u32 vert_count = 0;

    f32 cursor_x = x;
    f32 cursor_y = y;

    for (const char *c = text; *c; ) {
        if (*c == '\n') {
            cursor_x = x;
            cursor_y += font->line_height * size_px;
            c++;
            continue;
        }

        if (vert_count + 6 > 6 * MAX_TEXT_GLYPHS)
            break;

        u32 cp = utf8_decode(&c);
        const rl_glyph *g = (cp < 256) ? gl_font->glyph_map[cp] : rl_font_find_glyph(font, cp);
        if (!g)
            continue;

        const f32 x0 = cursor_x + g->plane_min_x * size_px;
        const f32 x1 = cursor_x + g->plane_max_x * size_px;
        const f32 y0 = cursor_y + g->plane_min_y * size_px;
        const f32 y1 = cursor_y + g->plane_max_y * size_px;

        const f32 u0 = g->uv_min_x;
        const f32 v0 = g->uv_min_y;
        const f32 u1 = g->uv_max_x;
        const f32 v1 = g->uv_max_y;

        verts[vert_count + 0] = (GL_TextVertex){.pos = {x0, y0}, .uv = {u0, v0}, .color = {color[0], color[1], color[2], color[3]}};
        verts[vert_count + 1] = (GL_TextVertex){.pos = {x0, y1}, .uv = {u0, v1}, .color = {color[0], color[1], color[2], color[3]}};
        verts[vert_count + 2] = (GL_TextVertex){.pos = {x1, y1}, .uv = {u1, v1}, .color = {color[0], color[1], color[2], color[3]}};
        verts[vert_count + 3] = (GL_TextVertex){.pos = {x0, y0}, .uv = {u0, v0}, .color = {color[0], color[1], color[2], color[3]}};
        verts[vert_count + 4] = (GL_TextVertex){.pos = {x1, y1}, .uv = {u1, v1}, .color = {color[0], color[1], color[2], color[3]}};
        verts[vert_count + 5] = (GL_TextVertex){.pos = {x1, y0}, .uv = {u1, v0}, .color = {color[0], color[1], color[2], color[3]}};

        vert_count += 6;
        cursor_x += g->advance * size_px;
    }

    if (vert_count == 0)
        return;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    opengl_shader_use(&p->shader);
    glBindVertexArray(p->vao);
    glBindBuffer(GL_ARRAY_BUFFER, p->vbo);

    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(GL_TextVertex) * vert_count, verts);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gl_font->texture_id);

    glUniform1i(p->loc_font_atlas, 0);
    glUniform2f(p->loc_screen_size, (f32)ctx->window->settings.width, (f32)ctx->window->settings.height);
    glUniform1f(p->loc_px_range, font->pixel_range);

    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vert_count);

    glBindVertexArray(0);
}

void opengl_render_text_batch(rl_frame_text *texts, u32 text_count) {
    if (!texts || text_count == 0)
        return;

    GL_Context *ctx = opengl_get_context();
    if (!ctx)
        return;

    GL_TextPipeline *p = &ctx->text_pipeline;

    GL_TextVertex verts[6 * MAX_TEXT_GLYPHS];
    u32 vert_count = 0;

    // All text entries share the same font atlas for now — use the first entry's font
    GL_Font *gl_font = nullptr;

    for (u32 t = 0; t < text_count; t++) {
        rl_frame_text *entry = &texts[t];
        if (!entry->text)
            continue;

        rl_font *font = entry->font ? entry->font : ctx->active_font;
        if (!font)
            continue;

        GL_Font *entry_gl_font = gl_find_font(ctx, font);
        if (!entry_gl_font)
            continue;

        // Use the first valid font as the batch font (all text should use the same atlas)
        if (!gl_font)
            gl_font = entry_gl_font;

        f32 cursor_x = entry->x;
        f32 cursor_y = entry->y;
        f32 size_px = entry->size_px;

        for (const char *c = entry->text; *c; ) {
            if (*c == '\n') {
                cursor_x = entry->x;
                cursor_y += font->line_height * size_px;
                c++;
                continue;
            }

            if (vert_count + 6 > 6 * MAX_TEXT_GLYPHS)
                break;

            u32 cp = utf8_decode(&c);
            const rl_glyph *g = (cp < 256) ? entry_gl_font->glyph_map[cp] : rl_font_find_glyph(font, cp);
            if (!g)
                continue;

            const f32 x0 = cursor_x + g->plane_min_x * size_px;
            const f32 x1 = cursor_x + g->plane_max_x * size_px;
            const f32 y0 = cursor_y + g->plane_min_y * size_px;
            const f32 y1 = cursor_y + g->plane_max_y * size_px;

            const f32 u0 = g->uv_min_x;
            const f32 v0 = g->uv_min_y;
            const f32 u1 = g->uv_max_x;
            const f32 v1 = g->uv_max_y;

            const f32 r = entry->color[0];
            const f32 g_ = entry->color[1];
            const f32 b = entry->color[2];
            const f32 a = entry->color[3];

            verts[vert_count + 0] = (GL_TextVertex){.pos = {x0, y0}, .uv = {u0, v0}, .color = {r, g_, b, a}};
            verts[vert_count + 1] = (GL_TextVertex){.pos = {x0, y1}, .uv = {u0, v1}, .color = {r, g_, b, a}};
            verts[vert_count + 2] = (GL_TextVertex){.pos = {x1, y1}, .uv = {u1, v1}, .color = {r, g_, b, a}};
            verts[vert_count + 3] = (GL_TextVertex){.pos = {x0, y0}, .uv = {u0, v0}, .color = {r, g_, b, a}};
            verts[vert_count + 4] = (GL_TextVertex){.pos = {x1, y1}, .uv = {u1, v1}, .color = {r, g_, b, a}};
            verts[vert_count + 5] = (GL_TextVertex){.pos = {x1, y0}, .uv = {u1, v0}, .color = {r, g_, b, a}};

            vert_count += 6;
            cursor_x += g->advance * size_px;
        }
    }

    if (vert_count == 0 || !gl_font)
        return;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    opengl_shader_use(&p->shader);
    glBindVertexArray(p->vao);
    glBindBuffer(GL_ARRAY_BUFFER, p->vbo);

    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(GL_TextVertex) * vert_count, verts);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gl_font->texture_id);

    glUniform1i(p->loc_font_atlas, 0);
    glUniform2f(p->loc_screen_size, (f32)ctx->window->settings.width, (f32)ctx->window->settings.height);
    glUniform1f(p->loc_px_range, gl_font->font->pixel_range);

    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vert_count);

    glBindVertexArray(0);
}

void opengl_set_active_font(rl_font *font) {
    //RL_INFO("OpenGL Active font: %s", font->name);
    opengl_get_context()->active_font = font;
}
