#pragma once

#include "defines.h"
#include "asset/font.h"
#include "renderer/opengl/gl_types.h"

b8 opengl_text_pipeline_init(GL_Context *ctx);
b8 gl_font_create(rl_font *font, GL_Context *ctx);

void opengl_set_active_font(rl_font *font);
void opengl_render_text(const char *text, f32 size_px, f32 x, f32 y, vec4 color);