#pragma once

#include "core/logger.h"
#include "defines.h"

#define MAX_WINDOWS 10

typedef struct platform_window {
    u16 id;
} platform_window;

typedef struct platform_window_settings {
    const char *title;
    i32 x;
    i32 y;
    i32 width;
    i32 height;
    b8 stop_on_close;
    platform_window *out_handle;
} platform_window_settings;

REALM_API b8 platform_system_start();

REALM_API void platform_system_shutdown();

REALM_API b8 platform_pump_messages();

REALM_API b8 platform_create_window(platform_window_settings settings);

REALM_API b8 platform_destroy_window(const platform_window *handle);

REALM_API void platform_console_write(const char *message, LOG_LEVEL level);

REALM_API f64 platform_get_absolute_time();
