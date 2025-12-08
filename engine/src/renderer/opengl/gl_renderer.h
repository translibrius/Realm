#pragma once

#include "defines.h"

#include "platform/platform.h"

b8 opengl_initialize(platform_window *platform_window);
void opengl_destroy();
void opengl_begin_frame();
void opengl_end_frame();
void opengl_swap_buffers();

u32 opengl_compile_vertex_shader(const char *source);
u32 opengl_compile_fragment_shader(const char *source);