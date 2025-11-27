#pragma once
#include "defines.h"

#include "platform/platform.h"

typedef enum RENDERER_BACKEND {
    BACKEND_OPENGL,
} RENDERER_BACKEND;

typedef struct renderer_interface {
    b8 (*initialize)(platform_window *window);
    void (*shutdown)();
    void (*begin_frame)();
    void (*end_frame)();
    void (*swap_buffers)();
} renderer_interface;
