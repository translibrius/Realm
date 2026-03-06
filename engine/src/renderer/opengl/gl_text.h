#pragma once

#include "defines.h"
#include "asset/font.h"
#include "renderer/frame_data.h"
#include "renderer/opengl/gl_types.h"

b8 opengl_text_pipeline_init(GL_Context *ctx);
b8 gl_font_create(rl_font *font, GL_Context *ctx);

GL_Font *gl_find_font(GL_Context *ctx, rl_font *font);

void opengl_set_active_font(rl_font *font);
void opengl_render_text(const char *text, f32 size_px, f32 x, f32 y, vec4 color);
void opengl_render_text_batch(rl_frame_text *texts, u32 text_count);