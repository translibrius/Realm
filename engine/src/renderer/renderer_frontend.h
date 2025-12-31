#pragma once

#include "defines.h"
#include "asset/font.h"
#include "renderer/renderer_types.h"
#include "platform/platform.h"
#include "util/math_types.h"

REALM_API b8 renderer_init(RENDERER_BACKEND backend, platform_window *window);
REALM_API void renderer_destroy();
REALM_API void renderer_begin_frame(f64 delta_time);
REALM_API void renderer_end_frame();
REALM_API void renderer_swap_buffers();

REALM_API void renderer_render_text(const char *text, f32 size_px, f32 x, f32 y, vec4 color);
REALM_API void renderer_set_active_font(rl_font *font);