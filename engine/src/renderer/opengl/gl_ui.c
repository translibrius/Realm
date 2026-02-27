#include "gl_ui.h"

#include "gl_renderer.h"
#include "gl_types.h"
#include "glad.h"
#include "core/logger.h"
#include "renderer/opengl/gl_text.h"
#include "ui/ui.h"

#include <string.h>

#define MAX_UI_QUADS 512
#define MAX_UI_TEXT_GLYPHS 256

// Same vertex layout as GL_TextVertex so we can share text.vert
typedef struct GL_UIVertex {
    vec2 pos;
    vec2 uv;
    vec4 color;
} GL_UIVertex;

// Per-context VAO cache (main + debug window)
typedef struct GL_UIVAOSlot {
    u16 window_id;
    u32 vao;
} GL_UIVAOSlot;

static GL_UIVAOSlot ui_vao_slots[2] = {0};

static u32 ui_get_or_create_vao(GL_UIPipeline *pipeline, u16 window_id) {
    // Look up existing
    for (u32 i = 0; i < 2; i++) {
        if (ui_vao_slots[i].window_id == window_id && ui_vao_slots[i].vao != 0) {
            return ui_vao_slots[i].vao;
        }
    }

    // Create new VAO for this context
    u32 vao = 0;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, pipeline->vbo);

    // pos (vec2)
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(GL_UIVertex), (void *)0);
    glEnableVertexAttribArray(0);
    // uv (vec2)
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(GL_UIVertex), (void *)(2 * sizeof(f32)));
    glEnableVertexAttribArray(1);
    // color (vec4)
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(GL_UIVertex), (void *)(4 * sizeof(f32)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);

    // Store in first available slot
    for (u32 i = 0; i < 2; i++) {
        if (ui_vao_slots[i].vao == 0) {
            ui_vao_slots[i].window_id = window_id;
            ui_vao_slots[i].vao = vao;
            return vao;
        }
    }

    // All slots full — overwrite slot 1 (debug window)
    if (ui_vao_slots[1].vao) {
        glDeleteVertexArrays(1, &ui_vao_slots[1].vao);
    }
    ui_vao_slots[1].window_id = window_id;
    ui_vao_slots[1].vao = vao;
    return vao;
}

b8 opengl_ui_pipeline_init(GL_Context *ctx) {
    GL_UIPipeline *pipeline = &ctx->ui_pipeline;

    if (!opengl_shader_setup(ASSET_ID_SHADER_TEXT_VERT, ASSET_ID_SHADER_UI_FRAG, &pipeline->shader)) {
        RL_ERROR("Failed to set up UI shader");
        return false;
    }

    pipeline->loc_screen_size = glGetUniformLocation(pipeline->shader.program_id, "u_screen_size");

    // Create shared VBO (shared across GL contexts)
    glGenBuffers(1, &pipeline->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, pipeline->vbo);
    u32 max_verts = 6 * MAX_UI_QUADS;
    u32 text_verts = 6 * MAX_UI_TEXT_GLYPHS;
    u32 total = max_verts > text_verts ? max_verts : text_verts;
    glBufferData(GL_ARRAY_BUFFER, sizeof(GL_UIVertex) * total, NULL, GL_DYNAMIC_DRAW);

    // Create VAO for the main window context
    u16 main_id = ctx->window ? ctx->window->id : 0;
    ui_get_or_create_vao(pipeline, main_id);

    return true;
}

static void flush_quads(GL_UIPipeline *pipeline, GL_UIVertex *verts, u32 vert_count, f32 screen_w, f32 screen_h, u16 window_id) {
    if (vert_count == 0) return;

    opengl_shader_use(&pipeline->shader);

    u32 vao = ui_get_or_create_vao(pipeline, window_id);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, pipeline->vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(GL_UIVertex) * vert_count, verts);

    glUniform2f(pipeline->loc_screen_size, screen_w, screen_h);

    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vert_count);
    glBindVertexArray(0);
}

// Find the GL_Font for a given rl_font in the context's font list
static GL_Font *ui_find_gl_font(GL_Context *ctx, rl_font *font) {
    for (u32 i = 0; i < ctx->fonts.count; i++) {
        if (ctx->fonts.items[i].font == font)
            return &ctx->fonts.items[i];
    }
    return nullptr;
}

static const rl_glyph *ui_font_find_glyph(const rl_font *font, u32 codepoint) {
    for (u32 i = 0; i < font->glyph_count; i++) {
        if ((u32)font->glyphs[i].codepoint == codepoint)
            return &font->glyphs[i];
    }
    return nullptr;
}

// Render a text command using the UI pipeline's VAO (context-safe)
static void render_text_inline(GL_Context *ctx, GL_UIPipeline *pipeline, rl_ui_cmd_text *text_cmd,
                               f32 screen_w, f32 screen_h, u16 window_id) {
    rl_font *font = text_cmd->font ? text_cmd->font : ctx->active_font;
    if (!font || !text_cmd->text) return;

    GL_Font *gl_font = ui_find_gl_font(ctx, font);
    if (!gl_font) return;

    GL_UIVertex verts[6 * MAX_UI_TEXT_GLYPHS];
    u32 vert_count = 0;

    f32 cursor_x = text_cmd->x;
    f32 cursor_y = screen_h - text_cmd->y;
    f32 size_px = text_cmd->size_px;

    for (const unsigned char *c = (const unsigned char *)text_cmd->text; *c; c++) {
        if (*c == '\n') {
            cursor_x = text_cmd->x;
            cursor_y -= (f32)font->line_height * size_px;
            continue;
        }

        if (vert_count + 6 > 6 * MAX_UI_TEXT_GLYPHS) break;

        u32 cp = *c;
        const rl_glyph *g = (cp < 256) ? gl_font->glyph_map[cp] : ui_font_find_glyph(font, cp);
        if (!g) continue;

        f32 x0 = cursor_x + g->plane_min_x * size_px;
        f32 x1 = cursor_x + g->plane_max_x * size_px;
        f32 y0 = cursor_y - g->plane_max_y * size_px;
        f32 y1 = cursor_y - g->plane_min_y * size_px;

        f32 u0 = g->uv_min_x, v0 = g->uv_min_y;
        f32 u1 = g->uv_max_x, v1 = g->uv_max_y;

        f32 r = text_cmd->color[0], g_ = text_cmd->color[1];
        f32 b = text_cmd->color[2], a = text_cmd->color[3];

        verts[vert_count + 0] = (GL_UIVertex){.pos = {x0, y0}, .uv = {u0, v0}, .color = {r, g_, b, a}};
        verts[vert_count + 1] = (GL_UIVertex){.pos = {x0, y1}, .uv = {u0, v1}, .color = {r, g_, b, a}};
        verts[vert_count + 2] = (GL_UIVertex){.pos = {x1, y1}, .uv = {u1, v1}, .color = {r, g_, b, a}};
        verts[vert_count + 3] = (GL_UIVertex){.pos = {x0, y0}, .uv = {u0, v0}, .color = {r, g_, b, a}};
        verts[vert_count + 4] = (GL_UIVertex){.pos = {x1, y1}, .uv = {u1, v1}, .color = {r, g_, b, a}};
        verts[vert_count + 5] = (GL_UIVertex){.pos = {x1, y0}, .uv = {u1, v0}, .color = {r, g_, b, a}};

        vert_count += 6;
        cursor_x += g->advance * size_px;
    }

    if (vert_count == 0) return;

    // Use text shader + font atlas, but with the UI pipeline's VAO (context-safe)
    GL_TextPipeline *tp = &ctx->text_pipeline;
    opengl_shader_use(&tp->shader);

    u32 vao = ui_get_or_create_vao(pipeline, window_id);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, pipeline->vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(GL_UIVertex) * vert_count, verts);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gl_font->texture_id);

    glUniform1i(tp->loc_font_atlas, 0);
    glUniform2f(tp->loc_screen_size, screen_w, screen_h);
    glUniform1f(tp->loc_px_range, font->pixel_range);

    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vert_count);
    glBindVertexArray(0);
}

void opengl_draw_ui(rl_ui_draw_list *list) {
    if (!list || list->count == 0) return;

    GL_Context *ctx = opengl_get_context();
    if (!ctx) return;

    GL_UIPipeline *pipeline = &ctx->ui_pipeline;
    platform_window *active_window = opengl_get_active_window();
    u16 window_id = active_window ? active_window->id : 0;

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    GL_UIVertex quad_batch[6 * MAX_UI_QUADS];
    u32 quad_vert_count = 0;

    for (u32 i = 0; i < list->count; i++) {
        rl_ui_cmd *cmd = &list->commands[i];

        if (cmd->type == RL_UI_CMD_QUAD) {
            if (quad_vert_count + 6 > 6 * MAX_UI_QUADS) {
                flush_quads(pipeline, quad_batch, quad_vert_count, list->screen_w, list->screen_h, window_id);
                quad_vert_count = 0;
            }

            f32 x0 = cmd->quad.x;
            f32 y0 = list->screen_h - cmd->quad.y - cmd->quad.h;
            f32 x1 = cmd->quad.x + cmd->quad.w;
            f32 y1 = list->screen_h - cmd->quad.y;
            f32 r = cmd->quad.color[0];
            f32 g = cmd->quad.color[1];
            f32 b = cmd->quad.color[2];
            f32 a = cmd->quad.color[3];

            quad_batch[quad_vert_count + 0] = (GL_UIVertex){.pos = {x0, y0}, .uv = {0, 0}, .color = {r, g, b, a}};
            quad_batch[quad_vert_count + 1] = (GL_UIVertex){.pos = {x0, y1}, .uv = {0, 0}, .color = {r, g, b, a}};
            quad_batch[quad_vert_count + 2] = (GL_UIVertex){.pos = {x1, y1}, .uv = {0, 0}, .color = {r, g, b, a}};
            quad_batch[quad_vert_count + 3] = (GL_UIVertex){.pos = {x0, y0}, .uv = {0, 0}, .color = {r, g, b, a}};
            quad_batch[quad_vert_count + 4] = (GL_UIVertex){.pos = {x1, y1}, .uv = {0, 0}, .color = {r, g, b, a}};
            quad_batch[quad_vert_count + 5] = (GL_UIVertex){.pos = {x1, y0}, .uv = {0, 0}, .color = {r, g, b, a}};

            quad_vert_count += 6;
        } else if (cmd->type == RL_UI_CMD_TEXT) {
            // Flush pending quads before switching to text
            if (quad_vert_count > 0) {
                flush_quads(pipeline, quad_batch, quad_vert_count, list->screen_w, list->screen_h, window_id);
                quad_vert_count = 0;
            }

            render_text_inline(ctx, pipeline, &cmd->text, list->screen_w, list->screen_h, window_id);
        }
    }

    // Flush remaining quads
    if (quad_vert_count > 0) {
        flush_quads(pipeline, quad_batch, quad_vert_count, list->screen_w, list->screen_h, window_id);
    }

    glEnable(GL_DEPTH_TEST);
}
