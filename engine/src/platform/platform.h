#pragma once

#include "core/logger.h"
#include "defines.h"

#define MAX_WINDOWS 10

typedef struct platform_window_settings {
    const char *title;
    i32 x;
    i32 y;
    i32 width;
    i32 height;
    b8 stop_on_close;
} platform_window_settings;

typedef struct platform_window {
    u16 id;
    platform_window_settings settings;
} platform_window;

b8 platform_system_start();
void platform_system_shutdown();
b8 platform_pump_messages();
b8 platform_create_window(platform_window *handle);
b8 platform_destroy_window(u16 id);
void platform_console_write(const char *message, LOG_LEVEL level);

i64 platform_get_clock_counter();
i64 platform_get_clock_frequency();

b8 platform_create_opengl_context(platform_window *handle);
b8 platform_context_make_current(platform_window *handle);
b8 platform_swap_buffers(platform_window *handle);
u64 platform_get_current_thread_id();