#pragma once

#include "defines.h"
#include "asset/font.h"
#include "renderer/renderer_types.h"
#include "platform/platform.h"
#include "util/math_types.h"

b8 renderer_init(RENDERER_BACKEND backend, platform_window *window);
void renderer_destroy();
void renderer_begin_frame(f64 delta_time);
void renderer_end_frame();
void renderer_swap_buffers();

void renderer_draw_text(rl_font *font, const char *text, vec2 pos, vec4 color);