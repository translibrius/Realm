#include "gl_gui.h"

#include "asset/asset_internal.h"
#include "asset/font.h"
#include "clay.h"
#include "core/logger.h"
#include "gl_renderer.h"
#include "glad.h"
#include "renderer/opengl/gl_shader.h"
#include "renderer/opengl/gl_text.h"
#include "renderer/opengl/gl_types.h"

#include <string.h>

#define GUI_MAX_RECT_VERTS (6 * 4096)
#define GUI_MAX_TEXT_VERTS (6 * 4096)
#define GUI_MAX_CLIP_DEPTH 16
#define GUI_TEXT_BUF_SIZE 1024

// Software clip rect stack
typedef struct {
    f32 x0, y0, x1, y1;
} gui_clip_rect;

static gui_clip_rect clip_stack[GUI_MAX_CLIP_DEPTH];
static i32 clip_depth;

static void clip_push(f32 x, f32 y, f32 w, f32 h) {
    gui_clip_rect r = {x, y, x + w, y + h};
    // Intersect with parent clip
    if (clip_depth > 0) {
        gui_clip_rect *p = &clip_stack[clip_depth - 1];
        if (r.x0 < p->x0) r.x0 = p->x0;
        if (r.y0 < p->y0) r.y0 = p->y0;
        if (r.x1 > p->x1) r.x1 = p->x1;
        if (r.y1 > p->y1) r.y1 = p->y1;
    }
    if (clip_depth < GUI_MAX_CLIP_DEPTH) {
        clip_stack[clip_depth++] = r;
    }
}

static void clip_pop(void) {
    if (clip_depth > 0) clip_depth--;
}

static b8 clip_active(void) {
    return clip_depth > 0;
}

static gui_clip_rect *clip_current(void) {
    return clip_depth > 0 ? &clip_stack[clip_depth - 1] : nullptr;
}

b8 opengl_gui_pipeline_init(GL_Context *ctx) {
    GL_GuiPipeline *p = &ctx->gui_pipeline;

    if (!opengl_shader_setup(ASSET_ID_SHADER_GUI_VERT, ASSET_ID_SHADER_GUI_FRAG, &p->shader)) {
        RL_ERROR("Failed to compile GUI shader");
        return false;
    }

    p->loc_screen_size = glGetUniformLocation(p->shader.program_id, "u_screen_size");

    glGenVertexArrays(1, &p->vao);
    glBindVertexArray(p->vao);

    glGenBuffers(1, &p->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, p->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(GL_TextVertex) * GUI_MAX_RECT_VERTS, NULL, GL_DYNAMIC_DRAW);

    // pos (vec2)
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(GL_TextVertex), (void *)0);
    glEnableVertexAttribArray(0);

    // uv (vec2) — unused for rects but keeps layout consistent
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(GL_TextVertex), (void *)(2 * sizeof(f32)));
    glEnableVertexAttribArray(1);

    // color (vec4)
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(GL_TextVertex), (void *)(4 * sizeof(f32)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
    return true;
}

static void push_rect(GL_TextVertex *verts, u32 *count,
                       f32 x, f32 y, f32 w, f32 h,
                       f32 r, f32 g, f32 b, f32 a) {
    if (*count + 6 > GUI_MAX_RECT_VERTS) return;

    f32 x0 = x, y0 = y, x1 = x + w, y1 = y + h;

    // Software clip
    if (clip_active()) {
        gui_clip_rect *c = clip_current();
        if (x0 < c->x0) x0 = c->x0;
        if (y0 < c->y0) y0 = c->y0;
        if (x1 > c->x1) x1 = c->x1;
        if (y1 > c->y1) y1 = c->y1;
        if (x0 >= x1 || y0 >= y1) return; // Fully clipped
    }

    GL_TextVertex *v = &verts[*count];
    v[0] = (GL_TextVertex){.pos = {x0, y0}, .uv = {0, 0}, .color = {r, g, b, a}};
    v[1] = (GL_TextVertex){.pos = {x0, y1}, .uv = {0, 1}, .color = {r, g, b, a}};
    v[2] = (GL_TextVertex){.pos = {x1, y1}, .uv = {1, 1}, .color = {r, g, b, a}};
    v[3] = (GL_TextVertex){.pos = {x0, y0}, .uv = {0, 0}, .color = {r, g, b, a}};
    v[4] = (GL_TextVertex){.pos = {x1, y1}, .uv = {1, 1}, .color = {r, g, b, a}};
    v[5] = (GL_TextVertex){.pos = {x1, y0}, .uv = {1, 0}, .color = {r, g, b, a}};

    *count += 6;
}

static void flush_rects(GL_Context *ctx, GL_TextVertex *verts, u32 *vert_count) {
    if (*vert_count == 0) return;

    GL_GuiPipeline *p = &ctx->gui_pipeline;
    opengl_shader_use(&p->shader);
    glBindVertexArray(p->vao);
    glBindBuffer(GL_ARRAY_BUFFER, p->vbo);

    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(GL_TextVertex) * (*vert_count), verts);
    glUniform2f(p->loc_screen_size, (f32)ctx->window->settings.width, (f32)ctx->window->settings.height);

    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)*vert_count);

    *vert_count = 0;
}

static rl_font *gui_get_font(u16 font_id) {
    Assets *assets = get_assets();
    u32 font_index = 0;
    for (u32 i = 0; i < assets->count; i++) {
        rl_asset *asset = &assets->items[i];
        if (asset->type == ASSET_FONT && asset->handle) {
            if (font_index == font_id) {
                return (rl_font *)asset->handle;
            }
            font_index++;
        }
    }
    return nullptr;
}

static void flush_text(GL_Context *ctx, GL_TextVertex *verts, u32 *vert_count, GL_Font *gl_font) {
    if (*vert_count == 0 || !gl_font) return;

    GL_TextPipeline *p = &ctx->text_pipeline;
    opengl_shader_use(&p->shader);
    glBindVertexArray(p->vao);
    glBindBuffer(GL_ARRAY_BUFFER, p->vbo);

    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(GL_TextVertex) * (*vert_count), verts);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gl_font->texture_id);

    glUniform1i(p->loc_font_atlas, 0);
    glUniform2f(p->loc_screen_size, (f32)ctx->window->settings.width, (f32)ctx->window->settings.height);
    glUniform1f(p->loc_px_range, gl_font->font->pixel_range);

    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)*vert_count);

    *vert_count = 0;
}

static void push_text_glyphs(GL_TextVertex *verts, u32 *vert_count, u32 max_verts,
                              GL_Font *gl_font, rl_font *font,
                              const char *chars, i32 len,
                              f32 size_px, f32 x, f32 y, vec4 color,
                              i32 window_height) {
    gui_clip_rect *clip = clip_current();

    // Convert clip rect to bottom-left origin for text coord space
    f32 clip_left = 0, clip_right = 1e9f, clip_bottom = -1e9f, clip_top = 1e9f;
    if (clip) {
        clip_left   = clip->x0;
        clip_right  = clip->x1;
        // Clay top-left Y → GL bottom-left Y
        clip_bottom = (f32)window_height - clip->y1;
        clip_top    = (f32)window_height - clip->y0;
    }

    f32 cursor_x = x;
    for (i32 i = 0; i < len; i++) {
        if (*vert_count + 6 > max_verts) break;

        u32 cp = (u32)(unsigned char)chars[i];
        const rl_glyph *g = (cp < 256) ? gl_font->glyph_map[cp] : rl_font_find_glyph(font, cp);
        if (!g) continue;

        f32 gx0 = cursor_x + g->plane_min_x * size_px;
        f32 gx1 = cursor_x + g->plane_max_x * size_px;
        f32 gy0 = y + g->plane_min_y * size_px;
        f32 gy1 = y + g->plane_max_y * size_px;

        cursor_x += g->advance * size_px;

        // Software clip — skip fully clipped glyphs
        if (clip && (gx1 <= clip_left || gx0 >= clip_right || gy1 <= clip_bottom || gy0 >= clip_top)) {
            continue;
        }

        f32 u0 = g->uv_min_x, v0 = g->uv_min_y;
        f32 u1 = g->uv_max_x, v1 = g->uv_max_y;

        // Clamp partially visible glyphs and adjust UVs proportionally
        if (clip) {
            f32 orig_w = gx1 - gx0;
            f32 orig_h = gy1 - gy0;
            if (gx0 < clip_left && orig_w > 0) {
                u0 += (u1 - u0) * (clip_left - gx0) / orig_w;
                gx0 = clip_left;
            }
            if (gx1 > clip_right && orig_w > 0) {
                u1 -= (u1 - u0) * (gx1 - clip_right) / (gx1 - gx0);
                gx1 = clip_right;
            }
            if (gy0 < clip_bottom && orig_h > 0) {
                v0 += (v1 - v0) * (clip_bottom - gy0) / orig_h;
                gy0 = clip_bottom;
            }
            if (gy1 > clip_top && orig_h > 0) {
                v1 -= (v1 - v0) * (gy1 - clip_top) / (gy1 - gy0);
                gy1 = clip_top;
            }
        }

        GL_TextVertex *v = &verts[*vert_count];
        v[0] = (GL_TextVertex){.pos = {gx0, gy0}, .uv = {u0, v0}, .color = {color[0], color[1], color[2], color[3]}};
        v[1] = (GL_TextVertex){.pos = {gx0, gy1}, .uv = {u0, v1}, .color = {color[0], color[1], color[2], color[3]}};
        v[2] = (GL_TextVertex){.pos = {gx1, gy1}, .uv = {u1, v1}, .color = {color[0], color[1], color[2], color[3]}};
        v[3] = (GL_TextVertex){.pos = {gx0, gy0}, .uv = {u0, v0}, .color = {color[0], color[1], color[2], color[3]}};
        v[4] = (GL_TextVertex){.pos = {gx1, gy1}, .uv = {u1, v1}, .color = {color[0], color[1], color[2], color[3]}};
        v[5] = (GL_TextVertex){.pos = {gx1, gy0}, .uv = {u1, v0}, .color = {color[0], color[1], color[2], color[3]}};

        *vert_count += 6;
    }
}

void opengl_render_gui(void *commands, i32 command_count) {
    if (!commands || command_count <= 0) return;

    GL_Context *ctx = opengl_get_context();
    if (!ctx) return;

    Clay_RenderCommand *cmds = (Clay_RenderCommand *)commands;

    static GL_TextVertex rect_verts[GUI_MAX_RECT_VERTS];
    u32 rect_vert_count = 0;

    static GL_TextVertex text_verts[GUI_MAX_TEXT_VERTS];
    u32 text_vert_count = 0;
    GL_Font *text_batch_font = nullptr;

    clip_depth = 0;
    i32 window_height = ctx->window->settings.height;

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    for (i32 i = 0; i < command_count; i++) {
        Clay_RenderCommand *cmd = &cmds[i];
        Clay_BoundingBox bb = cmd->boundingBox;

        switch (cmd->commandType) {
        case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
            Clay_RectangleRenderData *rect = &cmd->renderData.rectangle;
            f32 r = rect->backgroundColor.r / 255.0f;
            f32 g = rect->backgroundColor.g / 255.0f;
            f32 b = rect->backgroundColor.b / 255.0f;
            f32 a = rect->backgroundColor.a / 255.0f;
            push_rect(rect_verts, &rect_vert_count, bb.x, bb.y, bb.width, bb.height, r, g, b, a);
            break;
        }

        case CLAY_RENDER_COMMAND_TYPE_BORDER: {
            Clay_BorderRenderData *border = &cmd->renderData.border;
            f32 r = border->color.r / 255.0f;
            f32 g = border->color.g / 255.0f;
            f32 b = border->color.b / 255.0f;
            f32 a = border->color.a / 255.0f;

            if (border->width.top > 0) {
                push_rect(rect_verts, &rect_vert_count, bb.x, bb.y, bb.width, (f32)border->width.top, r, g, b, a);
            }
            if (border->width.bottom > 0) {
                push_rect(rect_verts, &rect_vert_count, bb.x, bb.y + bb.height - (f32)border->width.bottom, bb.width, (f32)border->width.bottom, r, g, b, a);
            }
            if (border->width.left > 0) {
                push_rect(rect_verts, &rect_vert_count, bb.x, bb.y, (f32)border->width.left, bb.height, r, g, b, a);
            }
            if (border->width.right > 0) {
                push_rect(rect_verts, &rect_vert_count, bb.x + bb.width - (f32)border->width.right, bb.y, (f32)border->width.right, bb.height, r, g, b, a);
            }
            break;
        }

        case CLAY_RENDER_COMMAND_TYPE_TEXT: {
            Clay_TextRenderData *text = &cmd->renderData.text;
            if (!text->stringContents.chars || text->stringContents.length <= 0) break;

            rl_font *font = gui_get_font(text->fontId);
            if (!font) break;

            GL_Font *gl_font = gl_find_font(ctx, font);
            if (!gl_font) break;

            // Flush text batch if font changed
            if (text_batch_font && text_batch_font != gl_font) {
                flush_rects(ctx, rect_verts, &rect_vert_count);
                flush_text(ctx, text_verts, &text_vert_count, text_batch_font);
            }
            text_batch_font = gl_font;

            // Flush if buffer is getting full (leave room for ~256 chars)
            if (text_vert_count + 6 * 256 > GUI_MAX_TEXT_VERTS) {
                flush_rects(ctx, rect_verts, &rect_vert_count);
                flush_text(ctx, text_verts, &text_vert_count, text_batch_font);
            }

            vec4 color = {
                text->textColor.r / 255.0f,
                text->textColor.g / 255.0f,
                text->textColor.b / 255.0f,
                text->textColor.a / 255.0f,
            };

            f32 text_y = (f32)window_height - bb.y - font->ascender * (f32)text->fontSize;

            push_text_glyphs(text_verts, &text_vert_count, GUI_MAX_TEXT_VERTS,
                             gl_font, font,
                             text->stringContents.chars, text->stringContents.length,
                             (f32)text->fontSize, bb.x, text_y, color,
                             window_height);
            break;
        }

        case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START: {
            clip_push(bb.x, bb.y, bb.width, bb.height);
            break;
        }

        case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END: {
            clip_pop();
            break;
        }

        default:
            break;
        }
    }

    // Final flush — rects first (backgrounds), then text on top
    flush_rects(ctx, rect_verts, &rect_vert_count);
    flush_text(ctx, text_verts, &text_vert_count, text_batch_font);

    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);
}
