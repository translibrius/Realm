#pragma once

#include "core/logger.h"
#include "defines.h"

#include "../vendor/cglm/cglm.h"

#define MAX_WINDOWS 10

enum platform_window_flag {
    WINDOW_FLAG_DEFAULT = 0,
    WINDOW_FLAG_NO_DECORATION = 1 << 0,
    WINDOW_FLAG_NO_INPUT = 1 << 1,
    WINDOW_FLAG_ON_TOP = 1 << 2,
    WINDOW_FLAG_TRANSPARENT = 1 << 3,
};

typedef struct platform_window_settings {
    const char *title;
    i32 x;
    i32 y;
    i32 width;
    i32 height;
    b8 start_center;
    b8 stop_on_close;
    u32 window_flags;
} platform_window_settings;

typedef struct platform_window {
    u16 id;
    platform_window_settings settings;
    void *handle;
} platform_window;

typedef enum platform_cursor_mode {
    CURSOR_MODE_NORMAL,
    CURSOR_MODE_HIDDEN,
    CURSOR_MODE_LOCKED
} platform_cursor_mode;

static platform_cursor_mode current_cursor_mode = CURSOR_MODE_NORMAL;

b8 platform_system_start();
void platform_system_shutdown();
b8 platform_pump_messages();
b8 platform_create_window(platform_window *window);
b8 platform_destroy_window(u16 id);
void platform_console_write(const char *message, LOG_LEVEL level);

i64 platform_get_clock_counter();
i64 platform_get_clock_frequency();
void platform_sleep(u32 milliseconds);

b8 platform_create_opengl_context(platform_window *window);
b8 platform_context_make_current(platform_window *window);
b8 platform_swap_buffers(platform_window *window);
u64 platform_get_current_thread_id();

void platform_set_cursor_mode(platform_window *window, platform_cursor_mode mode);
b8 platform_set_cursor_position(platform_window *window, vec2 position);
b8 platform_center_cursor(platform_window *window);