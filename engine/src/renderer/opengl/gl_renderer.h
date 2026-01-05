#pragma once

#include "defines.h"
#include "gl_types.h"
#include "platform/platform.h"

b8 opengl_initialize(platform_window *platform_window);
void opengl_destroy();
void opengl_begin_frame(f64 delta_time);
void opengl_end_frame();
void opengl_swap_buffers();
void opengl_set_view_projection(mat4 view, mat4 projection);

GL_Context *opengl_get_context(void);