#pragma once

#include "defines.h"
#include "asset/font.h"

#include "platform/platform.h"
#include "util/math_types.h"

b8 opengl_initialize(platform_window *platform_window);
void opengl_destroy();
void opengl_begin_frame(f64 delta_time);
void opengl_end_frame();
void opengl_swap_buffers();

void opengl_draw_text(rl_font *font, const char *text, vec2 pos, vec4 color);