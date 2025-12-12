#pragma once

#include "defines.h"

#include "platform/platform.h"

b8 opengl_initialize(platform_window *platform_window);
void opengl_destroy();
void opengl_begin_frame();
void opengl_end_frame();
void opengl_swap_buffers();