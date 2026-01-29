#pragma once

#include "asset/font.h"
#include "cglm.h"
#include "defines.h"
#include "platform/platform.h"
#include "renderer/renderer_backend.h"

REALM_API b8 renderer_init(platform_window *window, RENDERER_BACKEND backend, b8 vsync);
REALM_API void renderer_destroy();
REALM_API void renderer_begin_frame(f64 delta_time);
REALM_API void renderer_end_frame();
REALM_API void renderer_swap_buffers();

REALM_API void renderer_render_text(const char *text, f32 size_px, f32 x, f32 y, vec4 color);
REALM_API void renderer_set_active_font(rl_font *font);

void renderer_set_view_projection(mat4 view, mat4 projection, vec3 pos);

platform_window *renderer_get_active_window();
void renderer_set_active_window(platform_window *window);

void renderer_resize_framebuffer(i32 w, i32 h);
