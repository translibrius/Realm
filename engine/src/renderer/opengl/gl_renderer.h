#pragma once

#include "defines.h"
#include "gl_types.h"
#include "platform/platform.h"

b8 opengl_initialize(platform_window *platform_window, b8 vsync);
void opengl_destroy();
void opengl_begin_frame(f64 delta_time);
void opengl_end_frame();
void opengl_swap_buffers();
void opengl_set_view_projection(mat4 view, mat4 projection, vec3 pos);

GL_Context *opengl_get_context(void);

platform_window *opengl_get_active_window();
void opengl_set_active_window(platform_window *window);
void opengl_resize_framebuffer(i32 w, i32 h);