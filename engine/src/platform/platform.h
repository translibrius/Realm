#pragma once

#include "core/logger.h"
#include "defines.h"

#ifdef PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

REALM_API b8 platform_system_start();

REALM_API void platform_system_shutdown();

REALM_API b8 platform_pump_messages();

REALM_API b8 platform_create_window(const char *title, i32 x, i32 y, i32 width, i32 height, HWND out_window);

REALM_API void platform_console_write(const char *message, LOG_LEVEL level);

REALM_API f64 platform_get_absolute_time();
