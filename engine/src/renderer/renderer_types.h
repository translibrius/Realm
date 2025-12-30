#pragma once
#include "defines.h"

#include "platform/platform.h"
#include "util/math_types.h"

#define MAX_TEXT_GLYPHS 256

typedef enum RENDERER_BACKEND {
    BACKEND_OPENGL,
} RENDERER_BACKEND;

typedef struct renderer_interface {
    b8 (*initialize)(platform_window *window);
    void (*shutdown)();
    void (*begin_frame)(f64 delta_time);
    void (*end_frame)();
    void (*swap_buffers)();
} renderer_interface;
