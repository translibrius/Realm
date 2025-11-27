#pragma once

#include "defines.h"

#include "platform/platform.h"

typedef struct opengl_context {
    platform_window *window;
} opengl_context;

b8 opengl_initialize(platform_window *platform_window);
void opengl_destroy();
void opengl_begin_frame();
void opengl_end_frame();
void opengl_swap_buffers();
