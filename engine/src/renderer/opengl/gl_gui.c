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
#define GUI_MAX_SCISSOR_DEPTH 16
#define GUI_TEXT_BUF_SIZE 1024

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

static GL_TextVertex *push_rect(GL_TextVertex *verts, u32 *count,
                                f32 x, f32 y, f32 w, f32 h,
                                f32 r, f32 g, f32 b, f32 a) {
    if (*count + 6 > GUI_MAX_RECT_VERTS) return verts;

    GL_TextVertex *v = &verts[*count];
    f32 x1 = x + w;
    f32 y1 = y + h;

    v[0] = (GL_TextVertex){.pos = {x, y},   .uv = {0, 0}, .color = {r, g, b, a}};
    v[1] = (GL_TextVertex){.pos = {x, y1},  .uv = {0, 1}, .color = {r, g, b, a}};
    v[2] = (GL_TextVertex){.pos = {x1, y1}, .uv = {1, 1}, .color = {r, g, b, a}};
    v[3] = (GL_TextVertex){.pos = {x, y},   .uv = {0, 0}, .color = {r, g, b, a}};
    v[4] = (GL_TextVertex){.pos = {x1, y1}, .uv = {1, 1}, .color = {r, g, b, a}};
    v[5] = (GL_TextVertex){.pos = {x1, y},  .uv = {1, 0}, .color = {r, g, b, a}};

    *count += 6;
    return verts;
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
    glBindVertexArray(0);

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

void opengl_render_gui(void *commands, i32 command_count) {
    if (!commands || command_count <= 0) return;

    GL_Context *ctx = opengl_get_context();
    if (!ctx) return;

    Clay_RenderCommand *cmds = (Clay_RenderCommand *)commands;

    GL_TextVertex rect_verts[GUI_MAX_RECT_VERTS];
    u32 rect_vert_count = 0;

    i32 scissor_depth = 0;
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
            // Flush rects so borders draw in correct order
            flush_rects(ctx, rect_verts, &rect_vert_count);

            Clay_BorderRenderData *border = &cmd->renderData.border;
            f32 r = border->color.r / 255.0f;
            f32 g = border->color.g / 255.0f;
            f32 b = border->color.b / 255.0f;
            f32 a = border->color.a / 255.0f;

            // Top
            if (border->width.top > 0) {
                push_rect(rect_verts, &rect_vert_count, bb.x, bb.y, bb.width, (f32)border->width.top, r, g, b, a);
            }
            // Bottom
            if (border->width.bottom > 0) {
                push_rect(rect_verts, &rect_vert_count, bb.x, bb.y + bb.height - (f32)border->width.bottom, bb.width, (f32)border->width.bottom, r, g, b, a);
            }
            // Left
            if (border->width.left > 0) {
                push_rect(rect_verts, &rect_vert_count, bb.x, bb.y, (f32)border->width.left, bb.height, r, g, b, a);
            }
            // Right
            if (border->width.right > 0) {
                push_rect(rect_verts, &rect_vert_count, bb.x + bb.width - (f32)border->width.right, bb.y, (f32)border->width.right, bb.height, r, g, b, a);
            }

            flush_rects(ctx, rect_verts, &rect_vert_count);
            break;
        }

        case CLAY_RENDER_COMMAND_TYPE_TEXT: {
            // Flush pending rects before text
            flush_rects(ctx, rect_verts, &rect_vert_count);

            Clay_TextRenderData *text = &cmd->renderData.text;
            if (!text->stringContents.chars || text->stringContents.length <= 0) break;

            rl_font *font = gui_get_font(text->fontId);
            if (!font) break;

            // Copy to null-terminated buffer
            i32 len = text->stringContents.length;
            if (len >= GUI_TEXT_BUF_SIZE) len = GUI_TEXT_BUF_SIZE - 1;
            char buf[GUI_TEXT_BUF_SIZE];
            memcpy(buf, text->stringContents.chars, (u32)len);
            buf[len] = '\0';

            vec4 color = {
                text->textColor.r / 255.0f,
                text->textColor.g / 255.0f,
                text->textColor.b / 255.0f,
                text->textColor.a / 255.0f,
            };

            // Clay uses top-left origin; the text shader uses bottom-left.
            // Convert Y and offset by the font ascender so the top of
            // the tallest glyph aligns with Clay's bounding-box top.
            f32 text_y = (f32)window_height - bb.y - font->ascender * (f32)text->fontSize;

            rl_font *prev_font = ctx->active_font;
            opengl_set_active_font(font);
            opengl_render_text(buf, (f32)text->fontSize, bb.x, text_y, color);
            opengl_set_active_font(prev_font);
            break;
        }

        case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START: {
            flush_rects(ctx, rect_verts, &rect_vert_count);
            if (scissor_depth == 0) {
                glEnable(GL_SCISSOR_TEST);
            }
            scissor_depth++;
            // OpenGL scissor has bottom-left origin; Clay has top-left
            i32 sx = (i32)bb.x;
            i32 sy = window_height - (i32)(bb.y + bb.height);
            i32 sw = (i32)bb.width;
            i32 sh = (i32)bb.height;
            glScissor(sx, sy, sw, sh);
            break;
        }

        case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END: {
            flush_rects(ctx, rect_verts, &rect_vert_count);
            scissor_depth--;
            if (scissor_depth <= 0) {
                scissor_depth = 0;
                glDisable(GL_SCISSOR_TEST);
            }
            break;
        }

        default:
            break;
        }
    }

    // Final flush
    flush_rects(ctx, rect_verts, &rect_vert_count);

    if (scissor_depth > 0) {
        glDisable(GL_SCISSOR_TEST);
    }

    glEnable(GL_DEPTH_TEST);
}
